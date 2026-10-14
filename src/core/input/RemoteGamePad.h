// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL.h>
#include <fstream>
#include <string>

namespace trainer {
inline bool remoteGamePadIdentity(const std::string& name,unsigned vendor,unsigned product,const std::string& kernelName) {
    return vendor==0x045e&&product==0x02ea
        &&(name.rfind("TrainerOS Remote P",0)==0||kernelName.rfind("TrainerOS Remote P",0)==0);
}
inline std::string kernelGamePadName(int index) {
    (void)index;
    std::string kernelName;
#if defined(__linux__) && SDL_VERSION_ATLEAST(2,24,0)
    // SDL normalizes known Xbox names. Its OS path still identifies the uinput
    // device and its original kernel name, set by our host, not a guest packet.
    if(const char* raw=SDL_JoystickPathForIndex(index)) {
        const std::string path(raw),prefix="/dev/input/";
        if(path.rfind(prefix,0)==0) {
            const auto node=path.substr(prefix.size());
            const auto digits=node.rfind("event",0)==0?node.substr(5):node.rfind("js",0)==0?node.substr(2):std::string();
            if(!digits.empty()&&digits.find_first_not_of("0123456789")==std::string::npos) {
                std::ifstream file("/sys/class/input/"+node+"/device/name");std::getline(file,kernelName);
            }
        }
    }
#endif
    return kernelName;
}
inline bool isRemoteGamePad(int index) {
    const char* name=SDL_JoystickNameForIndex(index);
    return remoteGamePadIdentity(name?name:"",SDL_JoystickGetDeviceVendor(index),SDL_JoystickGetDeviceProduct(index),kernelGamePadName(index));
}
}
