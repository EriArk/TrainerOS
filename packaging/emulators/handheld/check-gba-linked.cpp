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
static int players=2;
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
 // Owner-supplied Kirby regression: title -> file -> Start -> Multiplayer.
 if(cartridge) {
  const int step=frame;
  if(step>=400&&step<402)return id==RETRO_DEVICE_ID_JOYPAD_START;
  if((step>=600&&step<602)||(step>=800&&step<802)||(step>=1000&&step<1002)||(step>=1400&&step<1402))return id==RETRO_DEVICE_ID_JOYPAD_A;
  if(step>=1200&&step<1202)return id==RETRO_DEVICE_ID_JOYPAD_DOWN;
  if(port==0&&((step>=1700&&step<1702)||(step>=1900&&step<1902)))return id==RETRO_DEVICE_ID_JOYPAD_A;
  if(step>=3000&&step<3050)return id==(port%2?RETRO_DEVICE_ID_JOYPAD_RIGHT:RETRO_DEVICE_ID_JOYPAD_LEFT);
  return 0;
 }
 int t=frame-300-(port==0?0:30);
 return ((t>=120&&t<125)||(t>=145&&t<150)||(port==0&&t>=290&&t<295))?id==RETRO_DEVICE_ID_JOYPAD_RIGHT:
 ((t>=180&&t<185)||(t>=250&&t<255)||(t>=330&&t<335)||(t>=500&&t<505)||(t>=700&&t<705))?id==RETRO_DEVICE_ID_JOYPAD_A:0;
}

int main(int argc,char** argv) {
    assert(argc>=3&&argc<=5);
    for(int n=3;n<argc;++n) {
        if(!strcmp(argv[n],"--cartridge"))cartridge=true;
        else {assert(strlen(argv[n])==1);players=atoi(argv[n]);assert(players>=2&&players<=4);}
    }
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
    retro_game_info game{argv[2],rom.data(),rom.size(),nullptr};retro_game_info machines[4]={game,game,game,game};
    assert(retro_load_game_special(0x200+players-1,machines,players));
    const unsigned memory[4]={0,0x100,0x101,0x102};
    auto state=[&](){std::vector<uint8_t> v(retro_serialize_size());assert(retro_serialize(v.data(),v.size()));return v;};
    for(int p=0;p<players;++p) {
        assert(retro_get_memory_size(memory[p])==131072);
        for(int q=0;q<p;++q)assert(retro_get_memory_data(memory[p])!=retro_get_memory_data(memory[q]));
        memset(retro_get_memory_data(memory[p]),0x51+p,131072);
    }
    auto initial=state();
    for(int p=0;p<players;++p)memset(retro_get_memory_data(memory[p]),0,131072);
    assert(retro_unserialize(initial.data(),initial.size()));
    for(int p=0;p<players;++p) {
        assert(((unsigned char*)retro_get_memory_data(memory[p]))[0]==0x51+p);
        if(cartridge)memset(retro_get_memory_data(memory[p]),0xff,131072);
    }
    fprintf(stdout,"players=%d state_size=%zu\n",players,initial.size());fflush(stdout);
    const int frames=cartridge?3300:3600;
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
            // Resume the original timeline; otherwise scripted key edges in
            // the lookahead would be pressed a second time in the outer loop.
            assert(retro_unserialize(saved.data(),saved.size()));
            frame=old;
        }
    }
    auto finalState=state();size_t offset=12+4*players;
    if(!cartridge)for(int p=0;p<players;++p) {
        uint32_t length;memcpy(&length,finalState.data()+12+4*p,4);
        uint32_t result[13];memcpy(result,finalState.data()+offset+0x21000,sizeof(result));
        printf("LINK player=%u status=%u transfers=%u baud=%u data_errors=%u missed=%u duplicate=%u timeouts=%u\n",result[3],result[2],result[6],result[7],result[8],result[9],result[10],result[12]);
        assert(result[0]==0x31544b4c&&!(result[2]&0x80000000)&&result[6]>100&&result[7]==15);
        assert(result[8]==0&&result[9]==0&&result[10]==0&&result[12]==0);
        offset+=length;
    }
    printf("PASS rollback + %d SRAM slots frames=%d cpu_seconds=%.3f video=%llu\n",players,frames,double(clock()-start)/CLOCKS_PER_SEC,videoHash);
    retro_unload_game();retro_deinit();dlclose(library);
}
