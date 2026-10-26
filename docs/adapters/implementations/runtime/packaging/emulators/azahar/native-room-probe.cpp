// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only executable linked to the pinned native Azahar network libraries.
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>
#include <unistd.h>
#include <json.hpp>
#include "common/logging/backend.h"
#include "network/network.h"

static volatile std::sig_atomic_t stopping=0;
static void stop(int){stopping=1;}
int main() {
    using namespace std::chrono_literals;
    using nlohmann::json;
    std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
    const auto filename=std::getenv("TRAINEROS_AZAHAR_SESSION");if(!filename)return 2;
    json config;std::ifstream(filename)>>config;
    const auto folder=std::filesystem::path(config.at("user").get<std::string>());
    const auto nick=config.at("nickname").get<std::string>();const int seat=std::stoi(nick.substr(6));
    const int players=config.at("players").get<int>();const bool host=config.at("host").get<bool>();
    if(players<2||players>4||seat<1||seat>players||host!=(seat==1))return 2;
    std::ofstream(folder/"pid")<<getpid();
    Common::Log::Initialize((folder/"native-network.log").string());Common::Log::Start();
    if(!Network::Init())return 2;
    const auto room=Network::GetRoom().lock();const auto member=Network::GetRoomMember().lock();
    const auto password=config.at("password").get<std::string>();const auto port=config.at("port").get<u16>();
    if(host) {
        if(!room->Create("TrainerOS group check","","127.0.0.1",port,password,players,
            "Player1","",0,std::make_unique<Network::VerifyUser::NullBackend>()))return 2;
        std::cout<<"[TrainerOS3DS] room-ready"<<std::endl;
    }
    std::mutex mutex;std::set<int> received{seat};bool unicast=false;std::atomic_bool invalid=false;
    const auto handler=member->BindOnWifiPacketReceived([&](const Network::WifiPacket& packet){
        std::lock_guard lock(mutex);
        if(packet.data.size()!=2||packet.data[0]<1||packet.data[0]>players||
           (packet.data[1]!=0&&packet.data[1]!=seat)){invalid=true;return;}
        if(packet.data[1]==0)received.insert(packet.data[0]);else unicast=true;
    });
    const Network::MacAddress mac{2,0,0,0,0,u8(seat)};
    member->Join(nick,"traineros-group-check-"+std::to_string(seat),"127.0.0.1",port,0,mac,password);
    const auto deadline=std::chrono::steady_clock::now()+20s;
    while(!member->IsConnected()&&!stopping&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(20ms);
    if(!member->IsConnected())return 3;
    std::cout<<"[TrainerOS3DS] running"<<std::endl;
    while(!stopping&&member->IsConnected()&&!invalid) {
        Network::WifiPacket packet{};packet.type=Network::WifiPacket::PacketType::Data;packet.channel=1;
        packet.transmitter_address=mac;packet.destination_address=Network::BroadcastMac;packet.data={u8(seat),0};
        member->SendWifiPacket(packet);
        const auto target=u8(seat%players+1);packet.destination_address={2,0,0,0,0,target};packet.data={u8(seat),target};
        member->SendWifiPacket(packet);
        {
            std::lock_guard lock(mutex);
            if(unicast) {
                std::ofstream(folder/"received.tmp")<<json(received).dump();
                std::filesystem::rename(folder/"received.tmp",folder/"received.json");
            }
        }
        std::this_thread::sleep_for(50ms);
    }
    member->Leave();if(host)room->Destroy();Network::Shutdown();Common::Log::Stop();
    return invalid?4:0;
}
