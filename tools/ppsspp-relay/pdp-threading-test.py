#!/usr/bin/env python3
"""Compile the real PPSSPP relay receive/peek wrappers with concurrent callers.

The stand-in relay asserts exclusive access to its mutable stream cursor. The
unpatched wrappers overlap; no network latency or particular game is required.
"""
from pathlib import Path
import subprocess
import sys
source = Path(sys.argv[1]).read_text()
out = Path(sys.argv[2]);out.mkdir(parents=True, exist_ok=True)
def function(signature):
    start = source.index(signature);body = source.index('{', start);depth = 1;end = body+1
    while depth:
        depth += (source[end] == '{')-(source[end] == '}');end += 1
    return source[start:end]
unit = r'''
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdint>
#include <cstdio>
struct SceNetEtherAddr { char bytes[6]; };
struct AdhocSocket { void *postofficeHandle; } socket0{(void*)1};
AdhocSocket *adhocSockets[]{&socket0};
std::recursive_mutex pdpRelayMutex[1];
constexpr int SOCKET_ERROR=-1,AEMU_POSTOFFICE_PDP_BLOCK_MAX=65523;
constexpr int AEMU_POSTOFFICE_CLIENT_SESSION_DEAD=-2,AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK=-3,AEMU_POSTOFFICE_CLIENT_OUT_OF_MEMORY=-4;
#define ERROR_LOG(...) ((void)0)
std::atomic<int> active{0},overlap{0};
struct Operation {
 Operation(){if(++active!=1)++overlap;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 ~Operation(){--active;}
};
void *pdp_postoffice_recover(int){return (void*)1;}
int pdp_peek_next_size(void*){Operation operation;return 4;}
int pdp_recv(void*,char*,int*,char*,int*,bool){Operation operation;return 0;}
void pdp_delete(void*){}
void handle_relay_connect_failure(){}
uint16_t reverse_port_simple(int port){return port;}
'''
unit += function('static int pdp_peek_next_size_postoffice(')+'\n'
unit += function('static int pdp_recv_postoffice(')+'\n'
unit += r'''
int main(){
 std::atomic<int> prepared{0};std::atomic<bool> go{false};
 auto run=[&](bool receive){++prepared;while(!go)std::this_thread::yield();
   for(int i=0;i<100;++i){if(receive){SceNetEtherAddr mac{};uint16_t port=0;char data[4];int size=4;
     pdp_recv_postoffice(0,&mac,&port,data,&size);
   }else pdp_peek_next_size_postoffice(0);}
 };
 std::thread matching(run,true),psp(run,false);while(prepared!=2)std::this_thread::yield();go=true;
 matching.join();psp.join();
 printf("Concurrent relay buffer accesses: %d\n",overlap.load());return overlap?1:0;
}
'''
(out/'test.cpp').write_text(unit)
subprocess.run(['g++', '-std=c++17', '-O2', '-pthread', str(out/'test.cpp'), '-o', str(out/'test')], check=True)
subprocess.run([str(out/'test')], check=True, timeout=10)
