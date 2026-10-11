// SPDX-License-Identifier: GPL-3.0-or-later
// Runs actual ported DetMP; synthetic packets do not prove DS game compatibility.
#include "DetMP.h"
#include "Platform.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace melonDS::Platform {
void Log(LogLevel, const char*, ...) {} // no files, ROMs, BIOS or networking
}

using namespace melonDS;
using namespace std::chrono_literals;
void require(bool ok, const char* what)
{
    if (!ok) { std::cerr << "FAIL: " << what << std::endl; std::exit(1); }
}
void setup(DetMP& mp, int count)
{
    // The owner freezes and registers every worker before any can exchange.
    for (int i = 0; i < count; ++i) mp.SetNow(i, 100);
    for (int i = 0; i < count; ++i) mp.ResetSession(i, 100);
}
void advance(DetMP& mp, int count, u64 t)
{
    for (int i = 0; i < count; ++i) mp.SetNow(i, 100 + t);
}

std::vector<u8> schedule(bool reversed)
{
    DetMP mp; setup(mp, 4); advance(mp, 4, 10);
    u8 first = 10, second = 11, other = 20;
    if (reversed) mp.SendPacket(1, &other, 1, 0);
    mp.SendPacket(0, &first, 1, 0);
    mp.SendPacket(0, &second, 1, 0);
    if (!reversed) mp.SendPacket(1, &other, 1, 0);
    std::array<u8, 0x948> buffer {};
    require(mp.RecvPacket(3, buffer.data(), nullptr) == 0, "same-time packet waits for next emulated tick");
    advance(mp, 4, 11);
    std::vector<u8> result;
    while (mp.RecvPacket(3, buffer.data(), nullptr)) result.push_back(buffer[0]);
    require(result == std::vector<u8>({10, 11, 20}), "emulated-time, sender, sequence ordering");
    return result;
}

void waitAndStop(bool abort, bool replies, bool leaveSelf = false)
{
    DetMP mp; setup(mp, 2); mp.SetNow(0, 110);
    auto worker = std::async(std::launch::async, [&] {
        std::array<u8, 15 * 1024> buf {};
        return replies ? int(mp.RecvReplies(0, buf.data(), 0, 2))
                       : mp.RecvPacket(0, buf.data(), nullptr);
    });
    require(worker.wait_for(30ms) == std::future_status::timeout, "clock barrier waits for slow peer");
    if (abort) mp.Abort(); else mp.Leave(leaveSelf ? 0 : 1);
    require(worker.wait_for(1s) == std::future_status::ready, "stop releases clock barrier");
    require(worker.get() == 0, "stop produces no packet");
    if (abort) {
        mp.ResetSession(0, 0); mp.SetNow(0, 1000);
        u8 byte = 1;
        require(mp.IsAborted() && mp.SendPacket(0, &byte, 1, 0) == 0, "aborted session cannot be reused");
    }
}

void repliesAndBounds()
{
    DetMP mp; setup(mp, 2); advance(mp, 2, 10);
    u8 cmd = 9;
    require(mp.SendCmd(0, &cmd, 1, 1000) == 1, "send command");
    advance(mp, 2, 11);
    std::array<u8, 0x948> packet {};
    require(mp.RecvHostPacket(1, packet.data(), nullptr) == 1, "receive command");
    std::array<u8, 1025> reply {}; reply.fill(42);
    require(mp.SendReply(1, reply.data(), 1025, 0, 1) == 0, "reject oversized reply slot");
    require(mp.SendReply(1, reply.data(), 1, 0, 0) == 0, "reject invalid aid");
    require(mp.SendReply(1, reply.data(), 1024, 0, 1) == 1024, "full valid reply slot");
    reply.fill(77);
    require(mp.SendReply(1, reply.data(), 1, 0, 1) == 1, "duplicate queued deterministically");
    require(mp.SendReply(1, reply.data(), 1, 0, 2) == 1, "unrequested aid queued for mask check");
    std::array<u8, 15 * 1024 + 2> output {}; output.fill(0xA5);
    require(mp.RecvReplies(0, output.data() + 1, 0, 2) == 2, "expected reply mask");
    require(output[0] == 0xA5 && output[1] == 42 && output[1024] == 42
            && output[1025] == 0xA5 && output.back() == 0xA5, "reply bounds and first answer retained");
    for (int invalid : {-1, 16, 1000}) {
        mp.SetNow(invalid, 5); mp.ResetSession(invalid, 0);
        mp.Begin(invalid); mp.End(invalid); mp.Leave(invalid);
        require(mp.SendPacket(invalid, &cmd, 1, 0) == 0
                && mp.RecvPacket(invalid, packet.data(), nullptr) == 0
                && mp.RecvReplies(invalid, output.data(), 0, 2) == 0, "invalid instance rejected");
    }
    require(mp.SendPacket(0, nullptr, 1, 0) == 0 && mp.SendPacket(0, &cmd, -1, 0) == 0
            && mp.SendPacket(0, packet.data(), 0x949, 0) == 0
            && mp.RecvPacket(0, nullptr, nullptr) == 0, "invalid buffer and packet bounds");
}

void queueAndEpoch()
{
    DetMP mp; setup(mp, 2); advance(mp, 2, 10); u8 byte = 7;
    for (size_t i = 0; i < DetMP::MaxQueuedFrames; ++i)
        require(mp.SendPacket(0, &byte, 1, 0) == 1, "bounded queue accepts capacity");
    require(mp.SendPacket(0, &byte, 1, 0) == 0 && mp.IsAborted(), "overflow aborts instead of silent loss");

    DetMP replies; setup(replies, 2); advance(replies, 2, 10);
    replies.SendCmd(0, &byte, 1, 0); advance(replies, 2, 11);
    std::array<u8, 0x948> packet {};
    require(replies.RecvHostPacket(1, packet.data(), nullptr) == 1, "reply queue command");
    for (size_t i = 0; i < DetMP::MaxQueuedFrames; ++i)
        require(replies.SendReply(1, &byte, 1, 0, 1) == 1, "bounded reply queue accepts capacity");
    require(replies.SendReply(1, &byte, 1, 0, 1) == 0 && replies.IsAborted(), "reply queue overflow aborts");

    DetMP fresh; setup(fresh, 2); advance(fresh, 2, 10);
    fresh.SendPacket(0, &byte, 1, 0);
    fresh.End(1); advance(fresh, 2, 11); fresh.Begin(1);
    std::array<u8, 0x948> buf {};
    require(fresh.RecvPacket(1, buf.data(), nullptr) == 0, "powered-off packets not replayed");
    fresh.SendPacket(0, &byte, 1, 0);
    // Both workers are parked while the coordinator changes the clock epoch.
    fresh.ResetSession(0, 111); fresh.ResetSession(1, 111);
    advance(fresh, 2, 12);
    require(fresh.RecvPacket(1, buf.data(), nullptr) == 0, "reset clears stale packets");
}

int main()
{
    require(schedule(false) == schedule(true), "opposite host scheduling has identical result");
    for (bool replies : {false, true}) {
        waitAndStop(true, replies); waitAndStop(false, replies); waitAndStop(false, replies, true);
    }
    repliesAndBounds(); queueAndEpoch();
    std::cout << "PASS: deterministic ordering, clock barrier, abort/leave, reply bounds, queue cap, reset\n";
}
