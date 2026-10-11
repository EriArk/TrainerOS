// SPDX-License-Identifier: GPL-3.0-or-later
// Existing local cartridge, synthetic SRAM only. No personal-save file access.
#include "NativeSession.h"
#include "xxhash/xxhash.h"
#include <fstream>
#include <iostream>
#include <map>

using namespace traineros::ds;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::vector<melonDS::NDSArgs> arguments(unsigned seats=2) {
    std::vector<melonDS::NDSArgs> args(seats);
    for (unsigned i=0;i<seats;++i) {
        auto& h=args[i].Firmware.GetHeader(); h.MacAddr={0x02,0,0,0,0,uint8_t(i+1)}; h.UpdateChecksum();
    }
    return args;
}
using Trace=std::map<std::pair<uint32_t,uint32_t>,uint64_t>;
Trace run(const std::vector<uint8_t>& rom, CartridgeStartup startup, bool cancel) {
    constexpr unsigned boundary=7;
    const auto expected=*startup.seats[startup.ownerSeat];
    const unsigned seats=unsigned(startup.seats.size());
    NativeSession session(arguments(seats),rom,{1,false},std::move(startup));
    OwnerCartridgeReturn result;
    require(!session.TakeOwnedReturn(result),"Return before request");
    require(session.RequestOwnedReturn(boundary),"Return request rejected");
    require(!session.RequestOwnedReturn(boundary),"Duplicate return request accepted");
    Trace trace;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    unsigned next=0;
    while (trace.size()<(boundary+1)*seats) {
        require(std::chrono::steady_clock::now()<deadline,"Return boundary timeout");
        while(next<boundary+8) {
            std::vector<FrameInput> input(seats);for(auto& f:input) f.frame=next;
            if(!session.Submit(input))break;
            ++next;
        }
        FrameResult f;
        if(session.Take(f,std::chrono::milliseconds(100)) && f.frame<=boundary)
            trace[{f.seat,f.frame}]=f.stateHash;
        require(!session.Failed(),"Session failed before agreed boundary");
    }
    if(cancel) {
        session.Stop(); require(!session.TakeOwnedReturn(result),"Cancelled session exported progress");
    } else {
        require(session.TakeOwnedReturn(result),"Completed owner boundary not returned");
        require(result.frame==boundary && result.bytes==expected,"Wrong owner or frame returned");
        require(session.Size()==0 && !session.TakeOwnedReturn(result),"Return did not join/release exactly once");
    }
    return trace;
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Usage: owned_state_probe OWN_ROM");
        std::ifstream f(argv[1],std::ios::binary|std::ios::ate);const auto size=f.tellg();
        require(f && size>=0x200 && size<=512*1024*1024,"Invalid local cartridge");
        std::vector<uint8_t> rom(static_cast<size_t>(size));f.seekg(0);
        require(bool(f.read(reinterpret_cast<char*>(rom.data()),size)),"Cartridge read failed");
        MachineContext context;
        auto cart=melonDS::NDSCart::ParseROM(rom.data(),uint32_t(rom.size()),&context);
        require(bool(cart),"Cartridge rejected");
        const auto length=cart->GetSaveMemoryLength();
        require(length>0 && length<=MaxCartridgeSaveBytes,"No supported cartridge memory");
        CartridgeStartup startup{{std::vector<uint8_t>(length,0x35),std::vector<uint8_t>(length,0xC7)},0};
        auto first=run(rom,startup,false);
        startup.ownerSeat=1;
        auto second=run(rom,startup,false);
        require(first==second,"Owner selection altered deterministic mirrors");
        run(rom,startup,true);
        auto four=startup;
        four.seats.push_back(std::vector<uint8_t>(length,0x17));
        four.seats.push_back(std::vector<uint8_t>(length,0xE2));four.ownerSeat=3;
        const auto fourTrace=run(rom,four,false);
        four.ownerSeat=0;require(fourTrace==run(rom,four,false),"Four-seat owner affected mirrors");
        {
            NativeSession stalled(arguments(),rom,{0,false},startup);
            require(stalled.RequestOwnedReturn(7),"Failed-session fixture request");
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            unsigned next=0;
            while(!stalled.Failed() && std::chrono::steady_clock::now()<deadline) {
                std::vector<FrameInput> input(2);for(auto& frame:input)frame.frame=next;
                if(stalled.Submit(input))++next;
                else std::this_thread::sleep_for(std::chrono::milliseconds(1));
                // Deliberately do not consume results: bounded overflow aborts.
            }
            OwnerCartridgeReturn value;
            require(stalled.Failed() && !stalled.TakeOwnedReturn(value),"Failed session exported progress");
            stalled.Stop();
        }
        // Every shape error fails before a worker starts; no silent fresh fallback.
        for (unsigned variant=0;variant<4;++variant) {
            auto bad=startup;
            if(variant==0) bad.seats.pop_back();
            if(variant==1) bad.ownerSeat=2;
            if(variant==2) bad.seats[0]=std::vector<uint8_t>{};
            if(variant==3) bad.seats[1]->push_back(0);
            bool rejected=false;
            try {NativeSession session(arguments(),rom,{1,false},std::move(bad));}
            catch(const std::invalid_argument&) {rejected=true;}
            require(rejected,"Invalid startup admitted");
        }
        NativeSession fresh(arguments(),rom);
        require(!fresh.RequestOwnedReturn(0),"Volatile session gained a save owner");fresh.Stop();
        uint64_t digest=0;for(const auto& [key,value]:first)digest=XXH64(&value,sizeof(value),digest);
        std::cout<<"PASS owned-state seats=2,4 bytes="<<length<<" checkpoints="<<first.size()
                 <<" owner-isolation=1 cancelled-return=0 failed-return=0 digest="<<std::hex<<digest<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
