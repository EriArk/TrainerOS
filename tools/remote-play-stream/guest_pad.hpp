// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pad_state.hpp"
#include <SDL.h>
#include <algorithm>
#include <stdexcept>

class GuestPad {
    SDL_GameController* device_=nullptr;
public:
    GuestPad() {
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS,"1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER))throw std::runtime_error(SDL_GetError());
    }
    ~GuestPad(){if(device_)SDL_GameControllerClose(device_);SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);}
    PadState sample(bool& exit) {
        SDL_PumpEvents();SDL_GameControllerUpdate();
        if(device_&&!SDL_GameControllerGetAttached(device_)){SDL_GameControllerClose(device_);device_=nullptr;}
        if(!device_)for(int i=0;i<SDL_NumJoysticks();++i) {
            const char* name=SDL_JoystickNameForIndex(i);
            if(name&&std::string(name).find("TrainerOS Remote P")==0)continue;
            if(SDL_IsGameController(i)&&(device_=SDL_GameControllerOpen(i)))break;
        }
        PadState state{};
        if(device_) {
            const std::pair<SDL_GameControllerButton,uint16_t> buttons[]={
                {SDL_CONTROLLER_BUTTON_DPAD_UP,0x0001},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,0x0002},
                {SDL_CONTROLLER_BUTTON_DPAD_LEFT,0x0004},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,0x0008},
                {SDL_CONTROLLER_BUTTON_START,0x0010},{SDL_CONTROLLER_BUTTON_BACK,0x0020},
                {SDL_CONTROLLER_BUTTON_LEFTSTICK,0x0040},{SDL_CONTROLLER_BUTTON_RIGHTSTICK,0x0080},
                {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,0x0100},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0x0200},
                {SDL_CONTROLLER_BUTTON_A,0x1000},{SDL_CONTROLLER_BUTTON_B,0x2000},
                {SDL_CONTROLLER_BUTTON_X,0x4000},{SDL_CONTROLLER_BUTTON_Y,0x8000}};
            for(auto [button,flag]:buttons)if(SDL_GameControllerGetButton(device_,button))state.buttons|=flag;
            auto axis=[&](SDL_GameControllerAxis code){return SDL_GameControllerGetAxis(device_,code);};
            auto inverted=[](int16_t value){return int16_t(std::clamp(-int(value),-32767,32767));};
            state.lx=axis(SDL_CONTROLLER_AXIS_LEFTX);state.ly=inverted(axis(SDL_CONTROLLER_AXIS_LEFTY));
            state.rx=axis(SDL_CONTROLLER_AXIS_RIGHTX);state.ry=inverted(axis(SDL_CONTROLLER_AXIS_RIGHTY));
            state.leftTrigger=std::max(0,int(axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT)))*255/32767;
            state.rightTrigger=std::max(0,int(axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)))*255/32767;
            exit=SDL_GameControllerGetButton(device_,SDL_CONTROLLER_BUTTON_GUIDE);
        }
        SDL_FlushEvents(SDL_JOYAXISMOTION,SDL_CONTROLLERDEVICEREMAPPED);
        return state;
    }
};
