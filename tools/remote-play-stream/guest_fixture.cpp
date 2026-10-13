// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit device-test executable; never an installed/user-facing input mode.
// The source controller exists only inside this SDL process, so an emulator
// cannot observe it directly and bypass GameStream's authenticated input path.
#define main streamGuestMain
#include "guest.cpp"
#undef main

struct FixturePad {
    SDL_Joystick* joystick=nullptr;
    uint64_t start=0;
    static void update(void* context) {
        auto& pad=*static_cast<FixturePad*>(context);
        if(!pad.joystick)return;
        const auto elapsed=(monotonicNs()-pad.start)/1000000;
        // Neutral admission, then P2 movement in both directions with pauses.
        const int phase=int(elapsed/1500);
        const int y=phase>=2&&phase<12?(phase%3==0?-24000:phase%3==1?24000:0):0;
        SDL_JoystickSetVirtualAxis(pad.joystick,SDL_CONTROLLER_AXIS_LEFTY,y);
        SDL_JoystickSetVirtualButton(pad.joystick,SDL_CONTROLLER_BUTTON_A,phase==5?1:0);
    }
};
int main(int argc,char** argv) {
    SDL_SetHint("SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT","0xfeed/0x1001");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS,"1");
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER))return 2;
    FixturePad pad;pad.start=monotonicNs();
    SDL_VirtualJoystickDesc desc{};desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;desc.naxes=SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;desc.vendor_id=0xfeed;desc.product_id=0x1001;
    desc.name="TrainerOS process-local test controller";desc.userdata=&pad;desc.Update=FixturePad::update;
    const int index=SDL_JoystickAttachVirtualEx(&desc);
    if(index<0)return 2;
    pad.joystick=SDL_JoystickOpen(index);if(!pad.joystick)return 2;
    SDL_JoystickSetVirtualAxis(pad.joystick,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
    SDL_JoystickSetVirtualAxis(pad.joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
    const int result=streamGuestMain(argc,argv);
    SDL_JoystickClose(pad.joystick);pad.joystick=nullptr;
    SDL_JoystickDetachVirtual(index);SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    return result;
}
