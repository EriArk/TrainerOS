// SPDX-License-Identifier: GPL-3.0-or-later
// ARM/Linux regression: paired GBA SRAM, continuous serial transfers and rollback.
// Pass the pinned CC0 gba-link-continuous fixture documented in mgba-splitscreen.md.
// c++ -O2 -std=c++17 -I MGBA/src/platform/libretro check-gba-linked.cpp -ldl -o check-gba-linked
// ./check-gba-linked CORE_LIBRETRO_SO CONTINUOUS_FIXTURE_GBA
#include <libretro.h>
#include <dlfcn.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <fstream>
#include <cstdlib>

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
        if(!strcmp(v->key,"splitscreen_players"))v->value="2";
        else if(!strcmp(v->key,"splitscreen_layout"))v->value="focus";
        else if(!strcmp(v->key,"splitscreen_focus_player"))v->value="1";
        else if(!strcmp(v->key,"splitscreen_audio"))v->value="player 1";
        else if(!strcmp(v->key,"splitscreen_fs_assist")||!strcmp(v->key,"splitscreen_overlays"))v->value="off";
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
static int frame=0;
static bool cartridge=false;
static unsigned long long videoHash;
static void video(const void* data,unsigned width,unsigned height,size_t pitch) {
 if(!data)return;
 videoHash=1469598103934665603ULL;
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width*2;x++)videoHash=(videoHash^((const unsigned char*)data)[y*pitch+x])*1099511628211ULL;
}
static void audio(int16_t,int16_t) {}
static size_t batch(const int16_t*,size_t count) {return count;}
static void poll() {}
static int16_t input(unsigned port,unsigned,unsigned,unsigned id) {
 if(cartridge)return (frame>=100&&frame<120)?id==RETRO_DEVICE_ID_JOYPAD_START:
 ((frame>=250&&frame<270)||(frame>=400&&frame<420))?id==RETRO_DEVICE_ID_JOYPAD_A:0;
 int t=frame-300-(port==0?0:30);
 return ((t>=120&&t<125)||(t>=145&&t<150)||(port==0&&t>=290&&t<295))?id==RETRO_DEVICE_ID_JOYPAD_RIGHT:
 ((t>=180&&t<185)||(t>=250&&t<255)||(t>=330&&t<335)||(t>=500&&t<505)||(t>=700&&t<705))?id==RETRO_DEVICE_ID_JOYPAD_A:0;
}

int main(int argc,char** argv) {
    assert(argc==3||(argc==4&&!strcmp(argv[3],"--cartridge")));
    cartridge=argc==4;
    void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"%s\n",dlerror());return 1;}
#define API(name) auto name=reinterpret_cast<decltype(&::name)>(dlsym(library,#name));assert(name)
    API(retro_set_environment);API(retro_set_video_refresh);API(retro_set_audio_sample);
    API(retro_set_audio_sample_batch);API(retro_set_input_poll);API(retro_set_input_state);
    API(retro_load_game_special);API(retro_init);API(retro_deinit);API(retro_load_game);API(retro_unload_game);
    API(retro_serialize);API(retro_serialize_size);API(retro_unserialize);API(retro_run);API(retro_get_memory_data);API(retro_get_memory_size);
    retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio);
    retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
    std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)),{});
    retro_game_info game{argv[2],rom.data(),rom.size(),nullptr};retro_game_info pair[2]={game,game};assert(retro_load_game_special(0x201,pair,2));

    fprintf(stderr,"sizes=%zu,%zu\n",retro_get_memory_size(0),retro_get_memory_size(0x100));
    assert(retro_get_memory_size(0)==131072);assert(retro_get_memory_size(0x100)==131072);
    assert(retro_get_memory_data(0)!=retro_get_memory_data(0x100));
    auto state=[&](){std::vector<uint8_t> v(retro_serialize_size());assert(retro_serialize(v.data(),v.size()));return v;};
    memset(retro_get_memory_data(0),0x51,131072);memset(retro_get_memory_data(0x100),0xa2,131072);
    auto initial=state();
    memset(retro_get_memory_data(0),0x12,131072);memset(retro_get_memory_data(0x100),0x34,131072);
    assert(retro_unserialize(initial.data(),initial.size()));
    assert(((unsigned char*)retro_get_memory_data(0))[0]==0x51);
    assert(((unsigned char*)retro_get_memory_data(0x100))[0]==0xa2);fprintf(stdout,"state_size=%zu\n",initial.size());fflush(stdout);
    if(cartridge){memset(retro_get_memory_data(0),0xff,131072);memset(retro_get_memory_data(0x100),0xff,131072);}
    const int frames=cartridge?900:3600;
    auto start=clock();
    for(frame=0;frame<frames;frame++) {
        retro_run();
        if(frame%37==0) {
            auto saved=state();int old=frame;
            for(int k=0;k<8;k++){frame=old+k;retro_run();}
            auto expected=state();auto expectedVideo=videoHash;
            assert(retro_unserialize(saved.data(),saved.size()));

            for(int k=0;k<8;k++){frame=old+k;retro_run();}
            auto actual=state();
            if(expected!=actual){size_t diffs=0,first=0;for(size_t i=0;i<actual.size();i++)if(expected[i]!=actual[i]){if(!diffs)first=i;if(diffs<16)fprintf(stderr,"byte %zu: %02x/%02x\n",i,expected[i],actual[i]);diffs++;}fprintf(stderr,"DIVERGENCE frame=%d first=%zu diffs=%zu video=%d\n",old,first,diffs,int(expectedVideo==videoHash));return 3;}
            assert(expectedVideo==videoHash);
            frame=old;
        }
    }
    auto finalState=state();uint32_t firstLength;memcpy(&firstLength,finalState.data()+12,4);
    if(!cartridge)for(size_t offset:{size_t(20),size_t(20+firstLength)}) {
        uint32_t result[13];memcpy(result,finalState.data()+offset+0x21000,sizeof(result));
        printf("LINK player=%u status=%u transfers=%u baud=%u data_errors=%u missed=%u duplicate=%u timeouts=%u\n",result[3],result[2],result[6],result[7],result[8],result[9],result[10],result[12]);
        assert(result[0]==0x31544b4c&&!(result[2]&0x80000000)&&result[6]>100&&result[7]==15);
        assert(result[8]==0&&result[9]==0&&result[10]==0&&result[12]==0);
    }
    printf("PASS rollback + two SRAM slots frames=%d cpu_seconds=%.3f video=%llu\n",frames,double(clock()-start)/CLOCKS_PER_SEC,videoHash);
    retro_unload_game();retro_deinit();dlclose(library);
}
