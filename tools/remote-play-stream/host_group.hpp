// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <map>

inline bool connectionId(const std::string& id) {
    if(id.size()!=36)return false;
    for(size_t n=0;n<id.size();++n)if(n==8||n==13||n==18||n==23) {if(id[n]!='-')return false;}
        else if(!std::isxdigit(static_cast<unsigned char>(id[n])))return false;
    return true;
}
struct GroupSession {
    Config config;
    Host host;
    std::vector<std::thread> threads;
    GroupSession(const std::string& path,SharedSource& source,int fd,int port,const std::string& id)
        :config(path.c_str()),host(config,&source,fd,port,id) {
        if(!config.game)throw std::runtime_error("Group requires owned-game mode");
    }
    void start() {
        auto run=[this](auto fn){threads.emplace_back([this,fn]{try{fn();}catch(const std::exception&){host.stop=true;}});};
        run([this]{host.rtsp();});run([this]{host.control();});run([this]{host.media(false);});run([this]{host.media(true);});
    }
    ~GroupSession(){host.stop=true;for(auto& t:threads)if(t.joinable())t.join();if(host.padFd>=0)::close(host.padFd);
        std::fprintf(stderr,"group_port=%d video_packets=%u audio_packets=%u idrs=%u sps=%u requests=%u\n",host.port,host.videoPackets.load(),host.audioPackets.load(),host.encodedIdrs.load(),host.encodedSps.load(),host.idrs.load());}
};
inline int groupMain() {
    const char* controlValue=std::getenv("TRAINEROS_GROUP_FD"),*audioValue=std::getenv("TRAINEROS_GAME_AUDIO_FD");
    if(!controlValue||!audioValue)throw std::runtime_error("Missing owned group pipes");
    const int fd=std::stoi(controlValue),audio=std::stoi(audioValue);int domain=0,type=0;socklen_t size=sizeof(int);
    ucred peer{};socklen_t credSize=sizeof(peer);
    if(fd<3||fd==audio||getsockopt(fd,SOL_SOCKET,SO_DOMAIN,&domain,&size)||domain!=AF_UNIX
       ||getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&size)||type!=SOCK_SEQPACKET
       ||getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&credSize)||peer.uid!=getuid())throw std::runtime_error("Invalid group owner");
    SharedSource source(audio);source.start();std::map<std::string,std::unique_ptr<GroupSession>> sessions;
    auto ended=[](const std::string& id){std::printf("{\"event\":\"ended\",\"connection\":\"%s\"}\n",id.c_str());std::fflush(stdout);};
    while(source.healthy()) {
        pollfd input{fd,POLLIN,0};const auto result=poll(&input,1,50);
        if(result>0) {
            char bytes[2048],ancillary[CMSG_SPACE(4*sizeof(int))]{};iovec data{bytes,sizeof(bytes)};
            msghdr message{};message.msg_iov=&data;message.msg_iovlen=1;message.msg_control=ancillary;message.msg_controllen=sizeof(ancillary);
            const auto count=recvmsg(fd,&message,MSG_CMSG_CLOEXEC);std::vector<int> descriptors;
            for(auto c=CMSG_FIRSTHDR(&message);c;c=CMSG_NXTHDR(&message,c))if(c->cmsg_level==SOL_SOCKET&&c->cmsg_type==SCM_RIGHTS) {
                auto values=reinterpret_cast<int*>(CMSG_DATA(c));const auto n=(c->cmsg_len-CMSG_LEN(0))/sizeof(int);
                descriptors.insert(descriptors.end(),values,values+n);
            }
            auto closeReceived=[&]{for(int n:descriptors)::close(n);descriptors.clear();};
            if(count<=0){closeReceived();break;}
            if(message.msg_flags&(MSG_TRUNC|MSG_CTRUNC)){closeReceived();throw std::runtime_error("Group packet overflow");}
            std::istringstream packet(std::string(bytes,count));std::vector<std::string> fields;for(std::string line;std::getline(packet,line);)fields.push_back(line);
            if(fields.size()==2&&fields[0]=="remove"&&connectionId(fields[1])&&descriptors.empty()) {
                sessions.erase(fields[1]);continue;
            }
            if(fields.size()!=4||fields[0]!="add"||fields[1].size()!=1||fields[1][0]<'2'||fields[1][0]>'4'
               ||!connectionId(fields[2])||fields[3].empty()||fields[3].size()>1024||descriptors.size()!=1) {
                closeReceived();throw std::runtime_error("Invalid local group request");
            }
            const int port=fields[1][0]-'0';bool occupied=sessions.count(fields[2])||sessions.size()>=3;
            for(const auto& entry:sessions)occupied|=entry.second->host.port==port;
            if(occupied){closeReceived();throw std::runtime_error("Duplicate group controller");}
            try {
                auto session=std::make_unique<GroupSession>(fields[3],source,descriptors[0],port,fields[2]);descriptors.clear();
                session->start();sessions.emplace(fields[2],std::move(session));
            }catch(const std::exception&){closeReceived();ended(fields[2]);}
        }
        for(auto it=sessions.begin();it!=sessions.end();)if(it->second->host.stop) {
            const auto id=it->first;it=sessions.erase(it);ended(id);
        }else ++it;
    }
    sessions.clear();return source.stopped?1:0;
}
