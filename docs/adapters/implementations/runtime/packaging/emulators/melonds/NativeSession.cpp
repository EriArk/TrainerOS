// SPDX-License-Identifier: GPL-3.0-or-later
#include "NativeSession.h"
#include "xxhash/xxhash.h"
#include <stdexcept>

namespace traineros::ds {
NativeSession::NativeSession(std::vector<melonDS::NDSArgs> args,
                             const std::vector<uint8_t>& rom)
{
    if (args.size() < 2 || args.size() > 4 || rom.size() < 0x200 || rom.size() > 512 * 1024 * 1024)
        throw std::invalid_argument("Unsupported local machine count or cartridge size");
    for (size_t i = 0; i < args.size(); ++i) {
        auto machine = std::make_unique<Machine>();
        machine->context.wireless = &wireless;
        machine->context.seat = int(i);
        machine->console = std::make_unique<melonDS::NDS>(std::move(args[i]), &machine->context);
        machine->context.console = machine->console.get();
        auto cart = melonDS::NDSCart::ParseROM(rom.data(), uint32_t(rom.size()), &machine->context);
        if (!cart) throw std::invalid_argument("Cartridge rejected by native core");
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
            // Checkpoints remain local and ephemeral. Only a digest is returned.
            melonDS::Savestate state;
            if (!nds.DoSavestate(&state)) throw std::runtime_error("Checkpoint failed");
            state.Finish();
            if (state.Error || state.Length() > 64 * 1024 * 1024) throw std::runtime_error("Checkpoint bounds");
            FrameResult result {uint32_t(seat), input.frame, nds.GetSysTimestamp(),
                XXH64(state.Buffer(), state.Length(), 0)};
            std::lock_guard<std::mutex> guard(lock);
            if (results.size() >= 64) throw std::runtime_error("Checkpoint consumer stalled");
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
