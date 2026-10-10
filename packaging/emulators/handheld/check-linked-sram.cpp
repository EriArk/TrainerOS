// SPDX-License-Identifier: GPL-3.0-or-later
// ARM/Linux integration check: runs a self-authored MBC1 cartridge in a real core.
// No commercial ROM, BIOS, game save or Nintendo artwork is required.
// c++ -std=c++17 -I SAMEBOY/libretro/libretro-common/include check-linked-sram.cpp -ldl -o check-linked-sram
// ./check-linked-sram CORE_LIBRETRO_SO
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
        if(!strcmp(v->key,"sameboy_link"))v->value="enabled";
        else if(!strcmp(v->key,"sameboy_screen_layout"))v->value="player 2 only";
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
static unsigned polls=0;
static void video(const void* data,unsigned width,unsigned height,size_t) { if(data){assert(width==160);assert(height==144);} assert(polls==1);polls=0; }
static void audio(int16_t,int16_t) {}
static size_t batch(const int16_t*,size_t count) {return count;}
static void poll() {polls++;}
static unsigned inputFrame=0;
static int16_t input(unsigned port,unsigned,unsigned,unsigned key) {
    return key==RETRO_DEVICE_ID_JOYPAD_A && ((inputFrame+port*3)/7)%2;
}

int main(int argc,char** argv) {
    assert(argc==2);
    void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"%s\n",dlerror());return 1;}
#define API(name) auto name=reinterpret_cast<decltype(&::name)>(dlsym(library,#name));assert(name)
    API(retro_set_environment);API(retro_set_video_refresh);API(retro_set_audio_sample);
    API(retro_set_audio_sample_batch);API(retro_set_input_poll);API(retro_set_input_state);
    API(retro_load_game_special);API(retro_init);API(retro_deinit);API(retro_load_game);API(retro_unload_game);
    API(retro_serialize);API(retro_serialize_size);API(retro_unserialize);API(retro_run);API(retro_get_memory_data);API(retro_get_memory_size);
    retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio);
    retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
    std::vector<uint8_t> rom(32768,0);rom[0x100]=0xc3;rom[0x101]=0x50;rom[0x102]=1;
    memcpy(rom.data()+0x134,"TRAINER PAIR ",13);rom[0x143]=0x80;rom[0x147]=0x03;rom[0x149]=3;
    for(int n=0x134;n<=0x14c;++n)rom[0x14d]=rom[0x14d]-rom[n]-1;
    size_t pc=0x150;
    const auto emit=[&](std::initializer_list<uint8_t> bytes){for(auto b:bytes)rom[pc++]=b;};
    const auto write=[&](uint16_t addr,uint8_t value){emit({0x3e,value,0xea,uint8_t(addr),uint8_t(addr>>8)});};
    emit({0xf3,0x31,0xff,0xdf});write(0,0x0a);
    // Read the owner's boot byte, increment it, then stop changing memory.
    emit({0xfa,0,0xa0,0x3c,0xea,0,0xa0,0x18,0xfe});
    retro_game_info game{"trainer-pair.gb",rom.data(),rom.size(),nullptr};retro_game_info pair[2]={game,game};assert(retro_load_game_special(0x101,pair,2));
    assert(retro_get_memory_size(0x100|RETRO_MEMORY_SAVE_RAM)==32768);
    assert(retro_get_memory_size(0x300|RETRO_MEMORY_SAVE_RAM)==32768);
    auto* ram=static_cast<uint8_t*>(retro_get_memory_data(0x100|RETRO_MEMORY_SAVE_RAM));assert(ram);
    auto* second=static_cast<uint8_t*>(retro_get_memory_data(0x300|RETRO_MEMORY_SAVE_RAM));assert(second);
    memset(ram,0x12,32768);memset(second,0x34,32768);
    // The linked pair must be serializable before its first frame as well.
    std::vector<uint8_t> initial(retro_serialize_size());assert(retro_serialize(initial.data(),initial.size()));
    for(int n=0;n<300;++n)retro_run();
    assert(ram[0]==0x13&&second[0]==0x35);
    std::vector<uint8_t> later(retro_serialize_size());assert(retro_serialize(later.data(),later.size()));
    assert(retro_unserialize(initial.data(),initial.size()));
    assert(ram[0]==0x12&&second[0]==0x34);
    for(int n=0;n<300;++n)retro_run();
    assert(ram[0]==0x13&&second[0]==0x35);
    assert(retro_unserialize(later.data(),later.size()));
    for(int n=0;n<300;++n)retro_run();
    assert(ram[0]==0x13&&second[0]==0x35);
    printf("Two independent 32768-byte batteries: boot, initial serialization, rollback and restore passed\n");
    retro_unload_game();
    // A second original program continuously exchanges serial bytes while
    // accumulating button samples. Predictions cross both key edges and cable
    // transfers; restoring only the two core states is insufficient.
    pc=0x150;
    emit({0xf3,0x31,0xff,0xdf});write(0,0x0a);write(0xff00,0x10);
    emit({0xfa,0,0xa0,0xfe,0x12,0x1e,0x80,0x20,0x02,0x1e,0x81});
    const auto loop=pc;
    emit({0xf0,0x00,0x47,0xfa,0x02,0xa0,0x80,0xea,0x02,0xa0});
    emit({0xf0,0x02,0xcb,0x7f,0x20});rom[pc++]=uint8_t(int(loop)-int(pc)-1);
    emit({0xf0,0x01,0xea,0x01,0xa0,0xfa,0x03,0xa0,0x3c,0xea,0x03,0xa0,0xe0,0x01,0x7b,0xe0,0x02,0x18});
    rom[pc++]=uint8_t(int(loop)-int(pc)-1);
    assert(retro_load_game_special(0x101,pair,2));
    ram=static_cast<uint8_t*>(retro_get_memory_data(0x100|RETRO_MEMORY_SAVE_RAM));
    second=static_cast<uint8_t*>(retro_get_memory_data(0x300|RETRO_MEMORY_SAVE_RAM));
    memset(ram,0x12,32768);memset(second,0x34,32768);
    for(int n=0;n<300;n++)retro_run();
    std::vector<uint8_t> snapshot(retro_serialize_size()),first(32768),other(32768);
    for(inputFrame=1;inputFrame<=200;inputFrame++) {
        assert(retro_serialize(snapshot.data(),snapshot.size()));
        retro_run();memcpy(first.data(),ram,32768);memcpy(other.data(),second,32768);
        const auto actualFrame=inputFrame;
        for(int prediction=0;prediction<7;prediction++){inputFrame++;retro_run();}
        assert(retro_unserialize(snapshot.data(),snapshot.size()));inputFrame=actualFrame;
        retro_run();
        if(memcmp(first.data(),ram,32768)||memcmp(other.data(),second,32768)) {
            fprintf(stderr,"Linked input/serial replay diverged at frame %u\n",inputFrame);return 3;
        }
    }
    assert(ram[3]!=0x12&&second[3]!=0x34);
    printf("200 rollback frames across independent input edges and cable transfers passed\n");
    retro_unload_game();retro_deinit();dlclose(library);
}
