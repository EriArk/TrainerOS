// SPDX-License-Identifier: GPL-3.0-or-later
#include "InputTrace.h"
#include <filesystem>
#include <iostream>
using namespace traineros::ds;
int main() {
    auto path=std::filesystem::temp_directory_path()/
        ("traineros-input-trace-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove(path,e); } } cleanup{path};
    auto write=[&](const std::string& text) { std::ofstream out(path,std::ios::binary); out<<text; };
    try {
        std::vector<FrameInput> frame(2); frame[0].keys=0xFFE; frame[1].touching=true; frame[1].x=255; frame[1].y=191;
        write("TRAINEROS_DS_INPUT_V1 2\n"+InputTraceLine(frame));
        auto parsed=ReadInputTrace(path.string(),2);
        if (parsed.size()!=1 || parsed[0][0].keys!=0xFFE || !parsed[0][1].touching || parsed[0][1].x!=255 || parsed[0][1].y!=191)
            throw std::runtime_error("Roundtrip");
        for (const std::string bad:{
            "", "OTHER 2\n0 4095 0 0 0 4095 0 0 0\n", "TRAINEROS_DS_INPUT_V1 3\n",
            "TRAINEROS_DS_INPUT_V1 2 extra\n", "TRAINEROS_DS_INPUT_V1 2\n",
            "TRAINEROS_DS_INPUT_V1 2\n1 4095 0 0 0 4095 0 0 0\n",
            "TRAINEROS_DS_INPUT_V1 2\n0 4096 0 0 0 4095 0 0 0\n",
            "TRAINEROS_DS_INPUT_V1 2\n0 4095 2 0 0 4095 0 0 0\n",
            "TRAINEROS_DS_INPUT_V1 2\n0 4095 1 256 0 4095 0 0 0\n",
            "TRAINEROS_DS_INPUT_V1 2\n0 4095 1 0 192 4095 0 0 0\n",
            "TRAINEROS_DS_INPUT_V1 2\n0 4095 0 0 0 4095 0 0\n",
            "TRAINEROS_DS_INPUT_V1 2\n0 4095 0 0 0 4095 0 0 0 extra\n",
            "TRAINEROS_DS_INPUT_V1 2\n0 -1 0 0 0 4095 0 0 0\n"}) {
            write(bad); bool rejected=false;
            try { ReadInputTrace(path.string(),2); } catch (const std::runtime_error&) { rejected=true; }
            if (!rejected) throw std::runtime_error("Malformed input admitted");
        }
        write(std::string(8*1024*1024+1,'x'));
        try { ReadInputTrace(path.string(),2); throw std::logic_error("Oversize admitted"); } catch (const std::runtime_error&) {}
        std::cout<<"PASS bounded input-only trace roundtrip and malformed/oversize rejection\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
