// SPDX-License-Identifier: GPL-3.0-or-later
#include "NativeSession.h"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        melonDS::DetMP wireless;
        traineros::ds::MachineContext context; context.wireless=&wireless;
        auto console=std::make_unique<melonDS::NDS>(melonDS::NDSArgs{},&context); context.console=console.get();
        auto* firmware=console->SPI.GetFirmwareMem();
        firmware->Reset();
        // Dirty the address and byte-position latches through an ordinary read,
        // then reset without completing/releasing that transfer.
        firmware->Write(0x03); firmware->Write(0x01); firmware->Write(0x23);
        firmware->Write(0x45); firmware->Write(0x00);
        firmware->Reset();
        melonDS::Savestate state; firmware->DoSavestate(&state); state.Finish();
        // SPFW has 15 serialized bytes after its section/header (16+16 bytes).
        // Every transfer latch and status field must be neutral after Reset.
        if (state.Error || state.Length()!=47) throw std::runtime_error("Unexpected firmware section");
        const auto* bytes=static_cast<const uint8_t*>(state.Buffer());
        for (unsigned i=32;i<47;++i)
            if (bytes[i]) throw std::runtime_error("Firmware transfer state survives reset");
        std::cout<<"PASS firmware SPI address/position reset without ROM or personal firmware\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
