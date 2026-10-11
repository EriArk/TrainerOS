// SPDX-License-Identifier: GPL-3.0-or-later
#include "NativeSession.h"
#include "NDSCart/CartRetail.h"
#include <iostream>

using namespace traineros::ds;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        // Synthetic cartridge storage, no commercial ROM or personal save.
        std::vector<uint8_t> rom(4096);
        MachineContext first, second;
        melonDS::NDSCart::CartRetail a(rom.data(), uint32_t(rom.size()), 0, false,
            {0,4096,2}, nullptr,0,&first);
        melonDS::NDSCart::CartRetail b(rom.data(), uint32_t(rom.size()), 0, false,
            {0,4096,2}, nullptr,0,&second);
        const std::vector<uint8_t> alice(8192,0x35), bob(8192,0xC7);
        InitializeCartridgeState(a,alice); InitializeCartridgeState(b,bob);
        a.Reset(); b.Reset();
        require(CopyCartridgeState(a)==alice && CopyCartridgeState(b)==bob,"Owner images mixed or reset");
        for (size_t size : {size_t(0),size_t(8191),size_t(8193)}) {
            bool rejected=false;
            try { InitializeCartridgeState(a,std::vector<uint8_t>(size,0x11)); }
            catch (const std::invalid_argument&) { rejected=true; }
            require(rejected && CopyCartridgeState(a)==alice,"Invalid image silently altered progress");
        }
        require(first.saveNotifications==1 && second.saveNotifications==1,"Rejected image reached core setter");
        std::cout<<"PASS exact cartridge size, owner isolation, reset persistence, rejection before mutation\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
