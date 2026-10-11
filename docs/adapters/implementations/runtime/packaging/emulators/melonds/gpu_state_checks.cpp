// SPDX-License-Identifier: GPL-3.0-or-later
#include "NativeSession.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

#if defined(__GNUC__)
__attribute__((noinline))
#endif
void poisonStack() {
    volatile unsigned char bytes[8192];
    for (size_t i=0;i<sizeof(bytes);++i) bytes[i]=0xA5;
}

int main() {
    try {
        melonDS::DetMP wireless;
        traineros::ds::MachineContext context; context.wireless=&wireless;
        auto console=std::make_unique<melonDS::NDS>(melonDS::NDSArgs{},&context); context.console=console.get();
        console->Reset();
        auto& gpu=console->GPU.GPU3D;
        for (bool packed:{false,true}) {
            gpu.Reset(); gpu.SetEnabled(true,true);
            if (packed) { poisonStack(); gpu.Write32(0x04000400,0x20); }
            poisonStack(); gpu.Write32(packed ? 0x04000400 : 0x04000480,0x1234);
            melonDS::Savestate state; gpu.DoSavestate(&state); state.Finish();
            // Header+section, FIFO counters+256 entries, PIPE counters.
            // Each command is a u64 union: 32-bit parameter, 8-bit opcode,
            // then three reserved bytes that are also serialized.
            constexpr size_t entry=16+16+12+256*8+12;
            if (state.Error || state.Length()<entry+8) throw std::runtime_error("GPU checkpoint bounds");
            const auto* bytes=static_cast<const uint8_t*>(state.Buffer());
            uint32_t param=0; std::memcpy(&param,bytes+entry,4);
            if (param!=0x1234 || bytes[entry+4]!=0x20) throw std::runtime_error("GPU command not queued");
            for (size_t i=5;i<8;++i)
                if (bytes[entry+i]) throw std::runtime_error("Uninitialized GPU command bytes in checkpoint");
        }
        std::cout<<"PASS direct/packed GPU command checkpoint bytes independent of old stack\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
