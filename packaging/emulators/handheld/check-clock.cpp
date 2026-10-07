// SPDX-License-Identifier: GPL-3.0-or-later
// ARM/Linux integration check: runs a self-authored MBC3 cartridge in a real core.
// No commercial ROM, BIOS, game save or Nintendo artwork is required.
// c++ -std=c++17 -I DOUBLECHERRY/libretro/DoubleCherryEngine check-clock.cpp -ldl -o check-clock
// ./check-clock CORE_LIBRETRO_SO
#include <libretro.h>
#include <dlfcn.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

static void logMessage(enum retro_log_level,const char*,...) {}
static bool environment(unsigned command,void* data) {
    switch(command) {
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *static_cast<bool*>(data)=true;return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log=logMessage;return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *static_cast<unsigned*>(data)=2;return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE: *static_cast<unsigned*>(data)=RETRO_LANGUAGE_ENGLISH;return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *static_cast<bool*>(data)=false;return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto* v=static_cast<retro_variable*>(data);
        if(!strcmp(v->key,"dcgb_rtc_use_system_clock"))v->value="2";
        else if(!strcmp(v->key,"dcgb_emulated_gameboys"))v->value="1";
        else if(!strcmp(v->key,"dcgb_singleplayer_linked_devive"))v->value="Off";
        else if(!strcmp(v->key,"gambatte_gb_bootloader"))v->value="disabled";
        else {v->value=nullptr;return false;}
        return true;
    }
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: return true;
    default:return false;
    }
}
static void video(const void*,unsigned,unsigned,size_t) {}
static void audio(int16_t,int16_t) {}
static size_t batch(const int16_t*,size_t count) {return count;}
static void poll() {}
static int16_t input(unsigned,unsigned,unsigned,unsigned) {return 0;}

int main(int argc,char** argv) {
    assert(argc==2);
    void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"%s\n",dlerror());return 1;}
#define API(name) auto name=reinterpret_cast<decltype(&::name)>(dlsym(library,#name));assert(name)
    API(retro_set_environment);API(retro_set_video_refresh);API(retro_set_audio_sample);
    API(retro_set_audio_sample_batch);API(retro_set_input_poll);API(retro_set_input_state);
    API(retro_init);API(retro_deinit);API(retro_load_game);API(retro_unload_game);
    API(retro_run);API(retro_get_memory_data);API(retro_get_memory_size);
    retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio);
    retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
    std::vector<uint8_t> rom(32768,0);rom[0x100]=0xc3;rom[0x101]=0x50;rom[0x102]=1;
    memcpy(rom.data()+0x134,"TRAINER CLOCK",13);rom[0x143]=0x80;rom[0x147]=0x10;rom[0x149]=3;
    for(int n=0x134;n<=0x14c;++n)rom[0x14d]=rom[0x14d]-rom[n]-1;
    size_t pc=0x150;
    const auto emit=[&](std::initializer_list<uint8_t> bytes){for(auto b:bytes)rom[pc++]=b;};
    const auto write=[&](uint16_t addr,uint8_t value){emit({0x3e,value,0xea,uint8_t(addr),uint8_t(addr>>8)});};
    const auto latch=[&]{write(0x6000,0);write(0x6000,1);};
    const auto read=[&](uint8_t reg,uint8_t slot){
        write(0x4000,reg);emit({0xfa,0,0xa0,0x47});write(0x4000,0);emit({0x78,0xea,slot,0xa0});
    };
    emit({0xf3,0x31,0xff,0xdf});write(0,0x0a);latch();
    for(uint8_t reg=8;reg<=12;++reg)read(reg,reg-8);
    write(0x4000,10);write(0xa000,14);latch();read(10,5);write(0xa006,0x5a);emit({0x18,0xfe});
    retro_game_info game{"trainer-clock.gbc",rom.data(),rom.size(),nullptr};assert(retro_load_game(&game));
    assert(retro_get_memory_size(RETRO_MEMORY_RTC)==8);
    auto* rtc=static_cast<uint64_t*>(retro_get_memory_data(RETRO_MEMORY_RTC));assert(rtc);
    auto* ram=static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));assert(ram);
    assert(retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)>=32768);memset(ram,0,32768);
    const uint64_t base=uint64_t(time(nullptr))-(300*86400+13*3600+27*60+19);*rtc=base;
    for(int n=0;n<10;++n)retro_run();
    printf("RTC bytes=%zu; cartridge read %u:%u:%u day=%u high=%u; wrote hour=%u; marker=%02x; epoch shift=%lld\n",
        retro_get_memory_size(RETRO_MEMORY_RTC),ram[2],ram[1],ram[0],ram[3],ram[4],ram[5],ram[6],(long long)(*rtc-base));
    assert(ram[0]>=19&&ram[0]<=21&&ram[1]==27&&ram[2]==13&&ram[3]==44&&(ram[4]&1)==1);
    assert(ram[5]==14&&ram[6]==0x5a&&*rtc==base-3600);
    retro_unload_game();retro_deinit();dlclose(library);
}
