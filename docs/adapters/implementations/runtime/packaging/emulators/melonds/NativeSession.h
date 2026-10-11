// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Args.h"
#include "NDS.h"
#include "DetMP.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace traineros::ds {
struct FrameInput {
    uint32_t frame = 0;
    uint32_t keys = 0xFFF;
    bool touching = false;
    uint16_t x = 0, y = 0;
};
struct FrameResult {
    uint32_t seat = 0, frame = 0;
    uint64_t cycles = 0, stateHash = 0;
};
// Platform callbacks bind to this context, never to an Android/UI singleton.
struct MachineContext {
    melonDS::DetMP* wireless = nullptr;
    melonDS::NDS* console = nullptr;
    int seat = 0;
    std::atomic<bool> stopped {false};
    std::atomic<uint64_t> saveNotifications {0};
};

// Local mirror worker lifecycle. Transport/admission and storage are outside
// this class. All consoles use caller-supplied locally constructed arguments.
// No incoming BIOS, firmware, savestate or arbitrary file import API exists.
// Submit/Take/Stop and destruction belong to one coordinator thread; only the
// owned machine workers run concurrently. Recreate the session after Stop.
class NativeSession {
public:
    NativeSession(std::vector<melonDS::NDSArgs> args, const std::vector<uint8_t>& rom);
    ~NativeSession();
    NativeSession(const NativeSession&) = delete;
    NativeSession& operator=(const NativeSession&) = delete;
    bool Submit(const std::vector<FrameInput>& frame);
    bool Take(FrameResult& result, std::chrono::milliseconds timeout);
    void Stop();
    bool Failed() const { return failed.load() || wireless.IsAborted(); }
    size_t Size() const { return machines.size(); }
private:
    struct Machine {
        MachineContext context;
        std::unique_ptr<melonDS::NDS> console;
        std::thread worker;
        std::deque<FrameInput> inputs;
    };
    void Run(size_t seat);
    melonDS::DetMP wireless;
    std::vector<std::unique_ptr<Machine>> machines;
    std::mutex lock;
    std::condition_variable changed;
    std::deque<FrameResult> results;
    std::atomic<bool> stopping {false}, failed {false};
    uint32_t nextFrame = 0;
};
}
