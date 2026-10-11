// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit local cartridge only; fresh volatile saves and generated firmware.
// Reports deterministic boot/replay evidence, not wireless gameplay acceptance.
#include "NativeSession.h"
#include "InputTrace.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace traineros::ds;
using Trace = std::map<std::pair<uint32_t,uint32_t>, FrameResult>;

void compare(const FrameResult& a, const FrameResult& b) {
    if (a.cycles != b.cycles || a.stateHash != b.stateHash || a.wirelessSent != b.wirelessSent || a.wirelessReceived != b.wirelessReceived) {
        std::cerr << "REPLAY_MISMATCH seat=" << a.seat << " frame=" << a.frame << '\n';
        std::cerr << "cycles-equal=" << (a.cycles==b.cycles) << " state-equal=" << (a.stateHash==b.stateHash)
                  << " sent-equal=" << (a.wirelessSent==b.wirelessSent)
                  << " received-equal=" << (a.wirelessReceived==b.wirelessReceived) << '\n';
        for (size_t i=0;i<a.checkpointSections.size() && i<b.checkpointSections.size();++i)
            if (a.checkpointSections[i].hash!=b.checkpointSections[i].hash)
                std::cerr<<"CHECKPOINT_SECTION_MISMATCH "<<a.checkpointSections[i].name<<'\n';
        throw std::runtime_error("Native replay differs");
    }
}

Trace run(const std::vector<uint8_t>& rom, unsigned frames, unsigned seats, bool jit, bool perturb, bool presentation, const InputTrace& recorded, const Trace* expected=nullptr)
{
    std::vector<melonDS::NDSArgs> args(seats);
    for (unsigned i = 0; i < seats; ++i) {
        if (!jit) args[i].JIT = std::nullopt;
        auto& header = args[i].Firmware.GetHeader();
        header.MacAddr = {0x02, 0x00, 0x00, 0x00, 0x00, uint8_t(i + 1)};
        header.UpdateChecksum();
    }
    NativeSession session(std::move(args), rom, SessionOptions{recorded.empty() ? 1u : 60u,presentation});
    std::vector<FrameInput> invalid(seats);
    invalid[0].frame = 1;
    if (session.Submit(invalid) || session.Submit({}))
        throw std::runtime_error("Incomplete or out-of-order frame admitted");
    invalid[0].frame = 0; invalid[0].keys = 0x1000;
    if (session.Submit(invalid)) throw std::runtime_error("Invalid input admitted");
    Trace trace;
    std::vector<bool> presented(seats, false);
    unsigned next = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(recorded.empty() ? 120 : 1200);
    while (trace.size() < size_t(frames) * seats) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Native workers timed out");
        // The small lookahead also lets a slower machine catch up at a frame
        // boundary while another is inside an emulated wireless receive wait.
        while (next < frames + 8) {
            std::vector<FrameInput> input(seats);
            for (unsigned i = 0; i < seats; ++i) {
                input[i].frame = next;
                // Same recorded input in both replays, independent per seat.
                if (recorded.empty() && next >= 90 && next < 94 && i == 0) input[i].keys &= ~1u;
            }
            if (next < recorded.size()) input=recorded[next];
            if (!session.Submit(input)) break;
            if (next == 0 && session.Submit(input)) throw std::runtime_error("Duplicate input frame admitted");
            ++next;
            if (perturb) std::this_thread::sleep_for(std::chrono::microseconds(137));
        }
        FrameResult result;
        if (session.Take(result, std::chrono::milliseconds(1000)) && result.frame < frames) {
            if (expected) compare(expected->at({result.seat,result.frame}),result);
            if (!trace.emplace(std::make_pair(result.seat, result.frame), result).second)
                throw std::runtime_error("Duplicate output frame");
        }
        if (session.Failed()) throw std::runtime_error("Native session aborted");
        if (presentation) for (unsigned seat=0;seat<seats;++seat) {
            PresentationFrame output;
            if (!session.TakePresentation(seat,output)) continue;
            if (output.pixels.size()!=256*192*2 || output.audio.size()>4096*2 || output.audio.size()%2)
                throw std::runtime_error("Presentation output bounds");
            presented[seat]=true;
        }
    }
    if (presentation) for (bool seen:presented) if (!seen) throw std::runtime_error("Missing presentation output");
    session.Stop();
    FrameResult stale;
    PresentationFrame stalePresentation;
    if (session.Take(stale, std::chrono::milliseconds(0)) || session.Submit(std::vector<FrameInput>(seats))
        || session.TakePresentation(0,stalePresentation))
        throw std::runtime_error("Stopped session retained output or accepted new work");
    session.Stop(); // Idempotent owner-thread shutdown, including idle workers.
    return trace;
}

int main(int argc, char** argv)
{
    try {
        if (argc != 5 && argc != 6) throw std::invalid_argument("Usage: native_pair_probe OWN_ROM FRAMES SEATS jit|interpreter|jit-present [LOCAL_INPUT_TRACE]");
        const unsigned frames = std::stoul(argv[2]), seats = std::stoul(argv[3]);
        const std::string mode = argv[4];
        if (frames < 1 || frames > (argc==6 ? 36000u : 3600u) || seats < 2 || seats > 4 || (mode != "jit" && mode != "interpreter" && mode != "jit-present"))
            throw std::invalid_argument("Invalid frame/count/mode bounds");
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        const auto size = input.tellg();
        if (!input || size < 0x200 || size > 512 * 1024 * 1024) throw std::runtime_error("Invalid local cartridge");
        std::vector<uint8_t> rom(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(rom.data()), size)) throw std::runtime_error("Cartridge read failed");
        const auto recorded=argc==6 ? ReadInputTrace(argv[5],seats) : InputTrace{};
        if (argc==6 && recorded.size()<frames) throw std::invalid_argument("Input trace shorter than requested replay");
        const auto start = std::chrono::steady_clock::now();
        auto first = run(rom, frames, seats, mode != "interpreter", false, mode == "jit-present",recorded);
        // A fresh process often receives zero-filled pages. Reused native-object
        // allocations expose fields that Reset/constructors forgot to initialize.
        {
            std::vector<std::unique_ptr<unsigned char[]>> oldHeap;
            for (unsigned i=0;i<seats;++i) {
                auto bytes=std::make_unique<unsigned char[]>(sizeof(melonDS::NDS));
                auto* poison=static_cast<volatile unsigned char*>(bytes.get());
                for (size_t n=0;n<sizeof(melonDS::NDS);++n) poison[n]=0xA5;
                oldHeap.push_back(std::move(bytes));
            }
        }
        auto second = run(rom, frames, seats, mode != "interpreter", true, mode == "jit-present",recorded,&first);
        uint64_t digest = 0, checkpoints = 0;
        for (const auto& [key, a] : first) {
            const auto& b = second.at(key);
            compare(a,b);
            if (recorded.empty() || a.frame % 60 == 0) ++checkpoints;
            digest = (digest << 7 | digest >> 57) ^ a.stateHash;
        }
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout << "PASS native-machines=" << seats << " frames=" << frames
                  << " compared-frame-results=" << first.size() << " state-checkpoints=" << checkpoints << " elapsed-ms=" << ms
                  << " wireless-sent=" << first.rbegin()->second.wirelessSent << " wireless-received=" << first.rbegin()->second.wirelessReceived
                  << " digest=" << std::hex << digest << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
