// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

struct PadState {
    uint16_t buttons=0;
    uint8_t leftTrigger=0,rightTrigger=0;
    int16_t lx=0,ly=0,rx=0,ry=0;
    bool neutral() const {
        return !buttons&&leftTrigger<=8&&rightTrigger<=8
            &&lx>=-4096&&lx<=4096&&ly>=-4096&&ly<=4096
            &&rx>=-4096&&rx<=4096&&ry>=-4096&&ry<=4096;
    }
};

// Host-owned foreground permission and independently fresh authenticated input.
// This is a local lease, not a lobby grant or a client-selected controller port.
class PadLease {
    uint64_t permissionAt_=0,inputAt_=0;
    bool permitted_=false,neutralRequired_=true,haveSequence_=false;
    uint32_t sequence_=0;
    PadState state_{};
public:
    static constexpr uint64_t TimeoutNs=350000000;
    void permission(bool allowed,uint64_t now) {
        if(!allowed||!permitted_||now<permissionAt_||now-permissionAt_>TimeoutNs) {
            state_={};neutralRequired_=true;
        }
        permissionAt_=now;permitted_=allowed;
    }
    void revoke() {permitted_=false;neutralRequired_=true;state_={};}
    PadState current(uint64_t now) {
        if(!permitted_||now<permissionAt_||now-permissionAt_>TimeoutNs
            ||now<inputAt_||now-inputAt_>TimeoutNs) {state_={};neutralRequired_=true;}
        return state_;
    }
    bool update(PadState input,uint32_t sequence,uint64_t now) {
        current(now);
        if(haveSequence_&&sequence<=sequence_)return false;
        haveSequence_=true;sequence_=sequence;
        // Guide, share, touchpad and paddle events never reach the host OS.
        input.buttons&=0xf3ff;
        if(!permitted_||now<permissionAt_||now-permissionAt_>TimeoutNs)return false;
        inputAt_=now;
        if(neutralRequired_) {
            if(!input.neutral())return false;
            neutralRequired_=false;
        }
        state_=input;return true;
    }
};
