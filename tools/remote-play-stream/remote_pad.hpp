// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pad_state.hpp"
#include <inputtino/input.hpp>
#include <memory>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

class RemotePad {
    int leaseFd_;
    PadLease lease_;
    std::unique_ptr<inputtino::XboxOneJoypad> pad_;
    void apply(const PadState& state) {
        pad_->set_pressed_buttons(state.buttons);
        pad_->set_triggers(state.leftTrigger,state.rightTrigger);
        pad_->set_stick(inputtino::Joypad::LS,state.lx,state.ly);
        pad_->set_stick(inputtino::Joypad::RS,state.rx,state.ry);
    }
public:
    explicit RemotePad(int fd):leaseFd_(fd) {
        int domain=0,type=0;socklen_t size=sizeof(int);
        ucred peer{};socklen_t credSize=sizeof(peer);
        if(fd<3||getsockopt(fd,SOL_SOCKET,SO_DOMAIN,&domain,&size)||domain!=AF_UNIX
            ||getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&size)||type!=SOCK_SEQPACKET
            ||getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&credSize)||peer.uid!=getuid())
            throw std::runtime_error("Expected inherited local input lease");
        auto created=inputtino::XboxOneJoypad::create({
            "TrainerOS Remote P2",0x045e,0x02ea,0x0408,"traineros/remote/p2",""});
        if(!created)throw std::runtime_error(created.getErrorMessage());
        pad_=std::make_unique<inputtino::XboxOneJoypad>(std::move(*created));
        apply({});
        for(const auto& node:pad_->get_nodes())std::fprintf(stderr,"remote_pad_node=%s\n",node.c_str());
    }
    ~RemotePad(){if(pad_)apply({});}
    void tick(uint64_t now) {
        // Bounded drain; only the inherited local supervisor can send this grant.
        for(int i=0;i<16;++i) {
            char data[10];auto count=recv(leaseFd_,data,sizeof(data),MSG_DONTWAIT|MSG_TRUNC);
            if(count<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)lease_.revoke();break;}
            if(count!=9||(data[0]!='0'&&data[0]!='1')) {lease_.revoke();break;}
            uint64_t sent=0;memcpy(&sent,data+1,sizeof(sent));
            if(sent>now||now-sent>PadLease::TimeoutNs){lease_.revoke();continue;}
            lease_.permission(data[0]=='1',sent);
        }
        apply(lease_.current(now));
    }
    bool update(const PadState& state,uint32_t sequence,uint64_t now) {
        bool accepted=lease_.update(state,sequence,now);apply(lease_.current(now));return accepted;
    }
};
