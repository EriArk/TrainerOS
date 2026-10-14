// SPDX-License-Identifier: GPL-3.0-or-later
#include "shared_source.hpp"
#include <initializer_list>
#include <cstdio>

int main() {
    gst_init(nullptr,nullptr);
    auto check=[](std::initializer_list<unsigned> types,bool delta,bool expected) {
        std::vector<unsigned char> bytes;
        for(auto type:types)bytes.insert(bytes.end(),{0,0,0,1,static_cast<unsigned char>(type),0x80});
        auto buffer=gst_buffer_new_allocate(nullptr,bytes.size(),nullptr);
        gst_buffer_fill(buffer,0,bytes.data(),bytes.size());
        if(delta)GST_BUFFER_FLAG_SET(buffer,GST_BUFFER_FLAG_DELTA_UNIT);
        const bool result=restartableH264(buffer);gst_buffer_unref(buffer);return result==expected;
    };
    if(restartableH264(nullptr)||!check({9,1},false,false)||!check({7,8,1},false,false)
       ||!check({5},false,false)||!check({7,5},false,false)||!check({8,5},false,false)
       ||!check({9,7,8,5},false,true)||!check({7,8,5},true,false))return 1;
    std::puts("Shared stream requires actual IDR and parameter sets: PASS");return 0;
}
