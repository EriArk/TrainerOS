// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only unrelated controller, to change device enumeration before P2 joins.
#include <inputtino/input.hpp>
#include <chrono>
#include <thread>
int main(int argc,char**) {
    const bool remote=argc==2;
    auto pad=inputtino::XboxOneJoypad::create({remote?"TrainerOS Remote P2":"TrainerOS Input Probe",
        uint16_t(remote?0x045e:0x046d),uint16_t(remote?0x02ea:0xc216),0x0408,"traineros/probe/unrelated",""});
    if(!pad)return 1;
    std::this_thread::sleep_for(std::chrono::seconds(30));
}
