// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit local cartridge only; fresh volatile saves and generated firmware.
// Reports deterministic boot/replay evidence, not wireless gameplay acceptance.
#include "NativeSession.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace traineros::ds;
using Trace = std::map<std::pair<uint32_t,uint32_t>, FrameResult>;

Trace run(const std::vector<uint8_t>& rom, unsigned frames, unsigned seats, bool jit, bool perturb)
{
    std::vector<melonDS::NDSArgs> args(seats);
    for (unsigned i = 0; i < seats; ++i) {
        if (!jit) args[i].JIT = std::nullopt;
        auto& header = args[i].Firmware.GetHeader();
        header.MacAddr = {0x02, 0x00, 0x00, 0x00, 0x00, uint8_t(i + 1)};
        header.UpdateChecksum();
    }
    NativeSession session(std::move(args), rom);
    std::vector<FrameInput> invalid(seats);
    invalid[0].frame = 1;
    if (session.Submit(invalid) || session.Submit({}))
        throw std::runtime_error("Incomplete or out-of-order frame admitted");
    invalid[0].frame = 0; invalid[0].keys = 0x1000;
    if (session.Submit(invalid)) throw std::runtime_error("Invalid input admitted");
    Trace trace;
    unsigned next = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while (trace.size() < size_t(frames) * seats) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Native workers timed out");
        // The small lookahead also lets a slower machine catch up at a frame
        // boundary while another is inside an emulated wireless receive wait.
        while (next < frames + 8) {
            std::vector<FrameInput> input(seats);
            for (unsigned i = 0; i < seats; ++i) {
                input[i].frame = next;
                // Same recorded input in both replays, independent per seat.
                if (next >= 90 && next < 94 && i == 0) input[i].keys &= ~1u;
            }
            if (!session.Submit(input)) break;
            if (next == 0 && session.Submit(input)) throw std::runtime_error("Duplicate input frame admitted");
            ++next;
            if (perturb) std::this_thread::sleep_for(std::chrono::microseconds(137));
        }
        FrameResult result;
        if (session.Take(result, std::chrono::milliseconds(1000)) && result.frame < frames) {
            if (!trace.emplace(std::make_pair(result.seat, result.frame), result).second)
                throw std::runtime_error("Duplicate output frame");
        }
        if (session.Failed()) throw std::runtime_error("Native session aborted");
    }
    session.Stop();
    FrameResult stale;
    if (session.Take(stale, std::chrono::milliseconds(0)) || session.Submit(std::vector<FrameInput>(seats)))
        throw std::runtime_error("Stopped session retained output or accepted new work");
    session.Stop(); // Idempotent owner-thread shutdown, including idle workers.
    return trace;
}

int main(int argc, char** argv)
{
    try {
        if (argc != 5) throw std::invalid_argument("Usage: native_pair_probe OWN_ROM FRAMES SEATS jit|interpreter");
        const unsigned frames = std::stoul(argv[2]), seats = std::stoul(argv[3]);
        const std::string mode = argv[4];
        if (frames < 1 || frames > 3600 || seats < 2 || seats > 4 || (mode != "jit" && mode != "interpreter"))
            throw std::invalid_argument("Invalid frame/count/mode bounds");
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        const auto size = input.tellg();
        if (!input || size < 0x200 || size > 512 * 1024 * 1024) throw std::runtime_error("Invalid local cartridge");
        std::vector<uint8_t> rom(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(rom.data()), size)) throw std::runtime_error("Cartridge read failed");
        const auto start = std::chrono::steady_clock::now();
        auto first = run(rom, frames, seats, mode == "jit", false);
        auto second = run(rom, frames, seats, mode == "jit", true);
        uint64_t digest = 0;
        for (const auto& [key, a] : first) {
            const auto& b = second.at(key);
            if (a.cycles != b.cycles || a.stateHash != b.stateHash) {
                std::cerr << "REPLAY_MISMATCH seat=" << key.first << " frame=" << key.second << '\n';
                return 2;
            }
            digest = (digest << 7 | digest >> 57) ^ a.stateHash;
        }
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout << "PASS native-machines=" << seats << " frames=" << frames
                  << " replay-checkpoints=" << first.size() << " elapsed-ms=" << ms
                  << " digest=" << std::hex << digest << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
