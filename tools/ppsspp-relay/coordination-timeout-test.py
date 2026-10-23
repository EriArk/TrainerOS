#!/usr/bin/env python3
"""Build an actual-source TCP blackhole fixture (Linux).

Compile with: python3 coordination-timeout-test.py SOURCE_DIR OUTPUT_BINARY
Run ONLY in a disposable network namespace with iptables and loopback enabled:
  sudo unshare --net sh -c 'ip link set lo up; /absolute/test persistent'
Modes: baseline (old source), persistent, brief, quiet (patched source).
The binary refuses the host network namespace. No public service is contacted.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = (args.source / "Core/HLE/proAdhoc.cpp").read_text()
finder = source.split("int friendFinder() {", 1)[1]
cleanup = "auto disconnectServer =" + finder.split("auto disconnectServer =", 1)[1].split("// Finder Loop", 1)[0]
ping = finder.split("// Ping Server", 1)[1].split("// Check for Incoming Data", 1)[0]
receive = finder.split("// Check for Incoming Data", 1)[1].split("// Calculate EnterGameMode Timeout", 1)[0]
init = source.split("int initNetwork(", 1)[1]
setup = "setSockKeepAlive((int)metasocket, true);" + init.split("setSockKeepAlive((int)metasocket, true);", 1)[1].split("// Disable Nagle Algo", 1)[0]
keepalive = "int setSockKeepAlive(" + source.split("int setSockKeepAlive(", 1)[1].split("\n/**", 1)[0]
header = (args.source / "Core/HLE/proAdhoc.h").read_text()
ping_define = next(line for line in header.splitlines() if line.startswith("#define PSP_ADHOCCTL_PING_TIMEOUT"))

harness = r'''
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#define PPSSPP_PLATFORM(x) 0
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SD_BOTH SHUT_RDWR
#define socket_errno errno
#define closesocket close
#define INFO_LOG(...) ((void)0)
#define ERROR_LOG(...) ((void)0)
#define WARN_LOG(...) ((void)0)
#define VERBOSE_LOG(...) ((void)0)
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
using s64 = int64_t;
PING_DEFINE
constexpr uint8_t OPCODE_PING = 0;
int metasocket = -1, rxpos = 0, timeouts = 0;
uint8_t rx[1024];
uint64_t lastping = 0, now = 0;
bool g_adhocServerConnected = false;
std::string notice;
std::recursive_mutex peerlock;
void *friends = nullptr;
void timeoutFriendsRecursive(void *) { ++timeouts; }
namespace OSDType { constexpr int MESSAGE_ERROR = 0; }
struct OSD { void Show(int, const std::string &s) { notice = s; } } g_OSD;
struct Translation { const char *T(const char *s) { return s; } } translation;
auto *n = &translation;
double time_now_d() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
int IsSocketReady(int fd, bool read, bool write) {
    if (fd < 0) return -1;
    pollfd p{fd, (short)((read ? POLLIN : 0) | (write ? POLLOUT : 0)), 0};
    return poll(&p, 1, 0);
}
int setSockKeepAlive(int sock, bool keepalive, int keepinvl = 60, int keepcnt = 20, int keepidle = 180);
KEEPALIVE
void tick() {
    CLEANUP
    PING
    RECEIVE
}
int connectPair(unsigned int expectedTimeout, unsigned int &port) {
    int listener = socket(AF_INET, SOCK_STREAM, 0); CHECK(listener >= 0);
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(listener, (sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(listen(listener, 1) == 0);
    socklen_t length = sizeof(addr); CHECK(getsockname(listener, (sockaddr *)&addr, &length) == 0);
    port = ntohs(addr.sin_port);
    metasocket = socket(AF_INET, SOCK_STREAM, 0); CHECK(metasocket >= 0);
    SETUP
    unsigned int timeout = 0; socklen_t size = sizeof(timeout);
    CHECK(getsockopt(metasocket, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout, &size) == 0);
    CHECK(timeout == expectedTimeout);
    CHECK(connect(metasocket, (sockaddr *)&addr, sizeof(addr)) == 0);
    int peer = accept(listener, nullptr, nullptr); CHECK(peer >= 0); close(listener);
    CHECK(fcntl(metasocket, F_SETFL, O_NONBLOCK) == 0);
    g_adhocServerConnected = true; rxpos = 0; lastping = 0;
    tick(); usleep(100000); // Let the initial ping be ACKed before introducing loss.
    return peer;
}
void drop(unsigned int port, bool on) {
    char command[160];
    snprintf(command, sizeof(command), "iptables %s OUTPUT -o lo -p tcp --dport %u -j DROP", on ? "-A" : "-D", port);
    CHECK(system(command) == 0);
}
void runFor(double seconds) {
    double until = time_now_d() + seconds;
    while (time_now_d() < until && g_adhocServerConnected) { tick(); usleep(20000); }
}
int main(int argc, char **argv) {
    setbuf(stdout, nullptr);
    // Refuse to introduce packet loss in the machine's normal namespace.
    struct stat own{}, host{};
    CHECK(stat("/proc/self/ns/net", &own) == 0 && stat("/proc/1/ns/net", &host) == 0);
    CHECK(own.st_ino != host.st_ino && geteuid() == 0);
    CHECK(argc == 2);
    std::string mode = argv[1];
    CHECK(mode == "baseline" || mode == "persistent" || mode == "brief" || mode == "quiet");
    unsigned int port = 0;
    int peer = connectPair(mode == "baseline" ? 0 : 15000, port);
    double began = time_now_d();
    if (mode == "quiet") {
        // The peer sends no application data; TCP ACKs alone keep this alive.
        runFor(18); CHECK(g_adhocServerConnected && timeouts == 0);
    } else {
        drop(port, true);
        if (mode == "brief") {
            runFor(3); CHECK(g_adhocServerConnected);
            drop(port, false);
            runFor(18); CHECK(g_adhocServerConnected && timeouts == 0);
        } else {
            runFor(mode == "baseline" ? 18 : 22);
            double elapsed = time_now_d() - began;
            if (mode == "baseline") CHECK(g_adhocServerConnected && timeouts == 0);
            else {
                CHECK(!g_adhocServerConnected && metasocket == -1 && rxpos == 0 && timeouts == 1);
                CHECK(notice.find(std::to_string(ETIMEDOUT)) != std::string::npos);
                CHECK(elapsed >= 14 && elapsed < 22);
            }
            printf("%s: %.3f s, connected=%d, peer_timeouts=%d\n", mode.c_str(), elapsed, g_adhocServerConnected, timeouts);
            drop(port, false);
        }
    }
    if (metasocket >= 0) close(metasocket);
    close(peer);
    if (mode != "baseline") {
        peer = connectPair(15000, port); runFor(.2);
        CHECK(g_adhocServerConnected);
        CHECK(send(peer, "fresh", 5, MSG_NOSIGNAL) == 5); runFor(.2);
        CHECK(rxpos == 5 && memcmp(rx, "fresh", 5) == 0);
        close(metasocket); close(peer);
    }
    printf("PASS %s: actual coordination socket setup, ping and receive blocks\n", mode.c_str());
}
'''
for marker, code in [("PING_DEFINE", ping_define), ("KEEPALIVE", keepalive), ("CLEANUP", cleanup),
                     ("PING\n", ping + "\n"), ("RECEIVE", receive), ("SETUP", setup)]:
    harness = harness.replace(marker, code)
with tempfile.TemporaryDirectory(prefix="adhoc-timeout-") as directory:
    cpp = Path(directory)/"test.cpp"
    cpp.write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    str(cpp), "-o", str(args.output.resolve())], check=True)
