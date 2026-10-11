// SPDX-License-Identifier: GPL-3.0-or-later
#include "NativeSession.h"
#include "xxhash/xxhash.h"
#include <stdexcept>
#include <cstring>

namespace traineros::ds {
NativeSession::NativeSession(std::vector<melonDS::NDSArgs> args,
                             const std::vector<uint8_t>& rom, SessionOptions options,
                             std::optional<CartridgeStartup> startup)
    : options(options)
{
    if (args.size() < 2 || args.size() > 4 || rom.size() < 0x200 || rom.size() > 512 * 1024 * 1024)
        throw std::invalid_argument("Unsupported local machine count or cartridge size");
    if (startup) {
        if (startup->seats.size() != args.size() || startup->ownerSeat >= args.size())
            throw std::invalid_argument("Cartridge owners must match the admitted seats");
        ownerSeat = startup->ownerSeat;
    }
    for (size_t i = 0; i < args.size(); ++i) {
        auto machine = std::make_unique<Machine>();
        machine->context.wireless = &wireless;
        machine->context.seat = int(i);
        machine->console = std::make_unique<melonDS::NDS>(std::move(args[i]), &machine->context);
        machine->context.console = machine->console.get();
        auto cart = melonDS::NDSCart::ParseROM(rom.data(), uint32_t(rom.size()), &machine->context);
        if (!cart) throw std::invalid_argument("Cartridge rejected by native core");
        if (cart->GetSaveMemoryLength() > MaxCartridgeSaveBytes)
            throw std::invalid_argument("Cartridge save exceeds session bounds");
        if (startup && startup->seats[i]) InitializeCartridgeState(*cart, *startup->seats[i]);
        machine->console->SetNDSCart(std::move(cart));
        machine->console->Reset();
        machine->console->SetupDirectBoot("session.nds");
        machine->console->RTC.SetDateTime(2026, 1, 1, 12, 0, 0);
        machine->console->Start();
        machines.push_back(std::move(machine));
    }
    // Register every machine before any worker can observe the shared clock.
    for (size_t i = 0; i < machines.size(); ++i)
        wireless.SetNow(int(i), machines[i]->console->GetSysTimestamp());
    for (size_t i = 0; i < machines.size(); ++i)
        wireless.ResetSession(int(i), machines[i]->console->GetSysTimestamp());
    try {
        for (size_t i = 0; i < machines.size(); ++i)
            machines[i]->worker = std::thread([this, i] { Run(i); });
    } catch (...) { Stop(); throw; }
}

NativeSession::~NativeSession() { Stop(); }

bool NativeSession::Submit(const std::vector<FrameInput>& frame)
{
    std::lock_guard<std::mutex> guard(lock);
    if (stopping || Failed() || frame.size() != machines.size()) return false;
    for (size_t i = 0; i < frame.size(); ++i) {
        const auto& f = frame[i];
        if (f.frame != nextFrame || f.keys > 0xFFF || f.x > 255 || f.y > 191
                || machines[i]->inputs.size() >= 8) return false;
    }
    // All seats advance together in admission order; workers execute in parallel.
    for (size_t i = 0; i < frame.size(); ++i) machines[i]->inputs.push_back(frame[i]);
    ++nextFrame;
    changed.notify_all();
    return true;
}

bool NativeSession::Take(FrameResult& result, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> guard(lock);
    changed.wait_for(guard, timeout, [this] { return !results.empty() || stopping || Failed(); });
    if (results.empty()) return false;
    result = results.front(); results.pop_front();
    return true;
}

bool NativeSession::TakePresentation(size_t seat, PresentationFrame& result)
{
    std::lock_guard<std::mutex> guard(lock);
    if (stopping || seat >= machines.size() || !machines[seat]->presentation) return false;
    result = std::move(*machines[seat]->presentation);
    machines[seat]->presentation.reset();
    return true;
}

bool NativeSession::RequestOwnedReturn(uint32_t frame)
{
    std::lock_guard<std::mutex> guard(lock);
    if (stopping || Failed() || !ownerSeat || returnFrame || frame < nextFrame
            || uint64_t(frame) > uint64_t(nextFrame) + 8) return false;
    const auto* cart = machines[*ownerSeat]->console->NDSCartSlot.GetCart();
    if (!cart || !cart->GetSaveMemoryLength()) return false;
    returnFrame = frame;
    return true;
}

bool NativeSession::TakeOwnedReturn(OwnerCartridgeReturn& result)
{
    OwnerCartridgeReturn candidate;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (stopping || Failed() || !returnFrame || !ownerReturn) return false;
        for (const auto& machine : machines)
            if (!machine->completedFrame || *machine->completedFrame < *returnFrame) return false;
        candidate = std::move(*ownerReturn);
    }
    Stop();
    // A concurrent worker failure during joined teardown invalidates the copy.
    if (failed.load()) return false;
    result = std::move(candidate);
    return true;
}

void NativeSession::Stop()
{
    stopping.store(true);
    wireless.Abort(); // Wake workers inside an emulated wireless clock barrier.
    changed.notify_all();
    for (auto& machine : machines)
        if (machine->worker.joinable()) machine->worker.join();
    std::lock_guard<std::mutex> guard(lock);
    for (auto& machine : machines) machine->inputs.clear();
    results.clear();
    ownerReturn.reset(); returnFrame.reset();
    machines.clear(); // Release volatile cartridge/state memory at session end.
}

void NativeSession::Run(size_t seat)
{
    auto& machine = *machines[seat];
    try {
        while (!stopping) {
            FrameInput input;
            {
                std::unique_lock<std::mutex> guard(lock);
                changed.wait(guard, [&] { return stopping || !machine.inputs.empty(); });
                if (stopping) break;
                input = machine.inputs.front(); machine.inputs.pop_front();
            }
            auto& nds = *machine.console;
            nds.SetKeyMask(input.keys);
            if (input.touching) nds.TouchScreen(input.x, input.y); else nds.ReleaseScreen();
            nds.RunFrame();
            if (stopping) break;
            if (machine.context.stopped || wireless.IsAborted()) throw std::runtime_error("Core stopped");
            FrameResult result {uint32_t(seat), input.frame, nds.GetSysTimestamp(), 0};
            result.wirelessSent = machine.context.wirelessSent;
            result.wirelessReceived = machine.context.wirelessReceived;
            // Checkpoints remain local and ephemeral. Rendering must not require
            // serializing every console on every frame merely to draw its screen.
            bool captureOwner = false, returnBoundary = false;
            {
                std::lock_guard<std::mutex> guard(lock);
                returnBoundary = returnFrame && *returnFrame == input.frame;
                captureOwner = returnBoundary && ownerSeat && *ownerSeat == seat;
            }
            if (returnBoundary || (options.checkpointInterval && input.frame % options.checkpointInterval == 0)) {
                melonDS::Savestate state;
                if (!nds.DoSavestate(&state)) throw std::runtime_error("Checkpoint failed");
                state.Finish();
                if (state.Error || state.Length() > 128 * 1024 * 1024) throw std::runtime_error("Checkpoint bounds");
                result.stateHash = XXH64(state.Buffer(), state.Length(), 0);
                {
                    const auto* bytes=static_cast<const uint8_t*>(state.Buffer());
                    for (uint32_t offset=16;offset<state.Length();) {
                        if (state.Length()-offset<16) throw std::runtime_error("Checkpoint section header");
                        uint32_t length=0; std::memcpy(&length,bytes+offset+4,4);
                        if (length<16 || length>state.Length()-offset || result.checkpointSections.size()>=64)
                            throw std::runtime_error("Checkpoint section bounds");
                        result.checkpointSections.push_back({std::string(reinterpret_cast<const char*>(bytes+offset),4),
                            XXH64(bytes+offset,length,0)});
                        offset+=length;
                    }
                }
            }
            std::optional<OwnerCartridgeReturn> cartridge;
            if (captureOwner)
                cartridge = OwnerCartridgeReturn{input.frame, CopyCartridgeState(*nds.NDSCartSlot.GetCart())};
            std::optional<PresentationFrame> output;
            if (options.presentation) {
                void* top = nullptr; void* bottom = nullptr;
                if (!nds.GPU.GetFramebuffers(&top, &bottom) || !top || !bottom)
                    throw std::runtime_error("Software frame unavailable");
                output.emplace(); output->frame = input.frame;
                output->pixels.resize(256 * 192 * 2);
                std::memcpy(output->pixels.data(), top, 256 * 192 * sizeof(uint32_t));
                std::memcpy(output->pixels.data() + 256 * 192, bottom, 256 * 192 * sizeof(uint32_t));
                // Drain every machine, including mirrors; only the selected
                // machine's output is played by the frontend. Bound each copy.
                output->audio.resize(4096 * 2);
                int samples = nds.SPU.ReadOutput(output->audio.data(), 4096);
                if (samples < 0 || samples > 4096) throw std::runtime_error("Audio bounds");
                output->audio.resize(size_t(samples) * 2);
            }
            std::lock_guard<std::mutex> guard(lock);
            if (results.size() >= 64) throw std::runtime_error("Checkpoint consumer stalled");
            if (output) machine.presentation = std::move(output);
            if (cartridge) ownerReturn = std::move(cartridge);
            machine.completedFrame = input.frame;
            results.push_back(result);
            changed.notify_all();
        }
    } catch (...) {
        failed.store(true); stopping.store(true); wireless.Abort(); changed.notify_all();
    }
    wireless.Leave(int(seat));
    melonDS::NDS::Current = nullptr;
}
}
