// SPDX-License-Identifier: GPL-3.0-or-later
// Session-owned launcher for the existing upstream aemu relay. Only loopback
// is reachable; the authenticated TrainerOS connection owns remote admission.
#include "server.h"
#include <chrono>
#include <thread>
#include <csignal>
#include <cstdio>
#include <sys/prctl.h>
#include <unistd.h>
static volatile std::sig_atomic_t stop=0;
int main() {
    std::signal(SIGTERM,[](int){stop=1;});std::signal(SIGINT,[](int){stop=1;});
    std::signal(SIGPIPE,SIG_IGN);
    const auto parent=getppid();
    if(parent==1||prctl(PR_SET_PDEATHSIG,SIGTERM)!=0||getppid()!=parent)return 1;
    aemu_postoffice_server::Config config;config.ip_addr="127.0.0.1";
    aemu_postoffice_server::Server server(config);
    while(!stop) {
        const auto start=std::chrono::steady_clock::now();
        if(server.pump()!=aemu_postoffice_server::ServerPumpStatus::SUCCESS)return 1;
        std::this_thread::sleep_until(start+std::chrono::milliseconds(config.target_tick_interval_ms));
    }
    return 0;
}
