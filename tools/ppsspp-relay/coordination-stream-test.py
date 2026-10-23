#!/usr/bin/env python3
"""Exercise the actual FriendFinder receive block with real TCP sockets.

Only logging/UI and peer timeout notification are stubbed. --legacy proves the
old EOF defect against an unpatched proAdhoc.cpp; it is not a passing repair test.
"""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=pathlib.Path)
parser.add_argument("--legacy", action="store_true")
args = parser.parse_args()
source = args.source.read_text()
body = source.split("int friendFinder() {", 1)[1]
receive = body.split("// Check for Incoming Data", 1)[1].split(
    "// Calculate EnterGameMode Timeout", 1)[0]
cleanup = ""
if not args.legacy:
    cleanup = "auto disconnectServer =" + body.split("auto disconnectServer =", 1)[1].split(
        "// Finder Loop", 1)[0]

harness = r'''
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SD_BOTH SHUT_RDWR
#define socket_errno errno
#define closesocket close
#define INFO_LOG(...) ((void)0)
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
int metasocket = -1, rxpos = 0, timeouts = 0, notices = 0;
uint8_t rx[1024];
bool g_adhocServerConnected = false;
std::recursive_mutex peerlock;
void *friends = nullptr;
void timeoutFriendsRecursive(void *) { ++timeouts; }
namespace OSDType { constexpr int MESSAGE_ERROR = 0; }
struct OSD { void Show(int, const std::string &) { ++notices; } } g_OSD;
struct Translation { const char *T(const char *s) { return s; } } translation;
auto *n = &translation;
bool forceReady = false;
int injectedError = 0;
int IsSocketReady(int fd, bool, bool) {
    if (forceReady) return 1;
    pollfd p{fd, POLLIN, 0};
    return poll(&p, 1, 200);
}
ssize_t checkedRecv(int fd, void *p, size_t n, int flags) {
    if (injectedError) { errno = injectedError; return -1; }
    return recv(fd, p, n, flags);
}
#define recv checkedRecv
void step() {
    CLEANUP
    RECEIVE
}
#undef recv
int connectPair() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listener >= 0);
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(listener, (sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(listen(listener, 1) == 0);
    socklen_t length = sizeof(addr);
    CHECK(getsockname(listener, (sockaddr *)&addr, &length) == 0);
    metasocket = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(connect(metasocket, (sockaddr *)&addr, sizeof(addr)) == 0);
    int peer = accept(listener, nullptr, nullptr); CHECK(peer >= 0); close(listener);
    CHECK(fcntl(metasocket, F_SETFL, O_NONBLOCK) == 0);
    g_adhocServerConnected = true;
    return peer;
}
int main() {
    int peer = connectPair();
    CHECK(send(peer, "old", 3, 0) == 3); step(); CHECK(rxpos == 3);
    forceReady = true; step(); // Actual EAGAIN after a readiness race.
    CHECK(g_adhocServerConnected && rxpos == 3 && timeouts == 0);
    injectedError = EINTR; step(); injectedError = 0; forceReady = false;
    CHECK(g_adhocServerConnected && rxpos == 3 && timeouts == 0);
    close(peer); step();
#ifdef LEGACY
    CHECK(g_adhocServerConnected && metasocket >= 0 && rxpos == 3 && timeouts == 0);
    close(metasocket);
    puts("BASELINE REPRODUCED: EOF retained dead connection and partial packet");
    return 0;
#else
    CHECK(!g_adhocServerConnected && metasocket == -1 && rxpos == 0);
    CHECK(timeouts == 1 && notices == 1);
    peer = connectPair(); // A deliberate native retry gets a fresh stream.
    char full[sizeof(rx)]; memset(full, 'x', sizeof(full));
    CHECK(send(peer, full, sizeof(full), 0) == sizeof(full)); step();
    CHECK(rxpos == sizeof(rx));
    CHECK(send(peer, "next", 4, 0) == 4); step();
    CHECK(g_adhocServerConnected && rxpos == sizeof(rx));
    rxpos = 0; // Existing packet parser frees buffer space between iterations.
    step(); CHECK(rxpos == 4 && memcmp(rx, "next", 4) == 0);
    rxpos = 0;
    CHECK(send(peer, "fresh", 5, 0) == 5); step();
    CHECK(rxpos == 5 && memcmp(rx, "fresh", 5) == 0);
    linger reset{1, 0}; CHECK(setsockopt(peer, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)) == 0);
    close(peer); step();
    CHECK(!g_adhocServerConnected && metasocket == -1 && rxpos == 0);
    CHECK(timeouts == 2 && notices == 2);
    for (int i = 0; i < 20; ++i) {
        peer = connectPair(); close(peer); step();
        CHECK(!g_adhocServerConnected && metasocket == -1 && rxpos == 0);
    }
    CHECK(timeouts == 22 && notices == 22);
    puts("PASS: partial data, EAGAIN, EINTR, EOF, TCP reset, fresh stream, repeated reconnect");
#endif
}
'''.replace("    CLEANUP", cleanup).replace("    RECEIVE", receive)
with tempfile.TemporaryDirectory(prefix="adhoc-stream-") as directory:
    cpp = pathlib.Path(directory) / "test.cpp"
    exe = pathlib.Path(directory) / "test"
    cpp.write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    *(["-DLEGACY"] if args.legacy else []), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
