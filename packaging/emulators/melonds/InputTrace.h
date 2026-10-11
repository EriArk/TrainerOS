// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "NativeSession.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace traineros::ds {
// Developer replay evidence only. Contains button/touch input, never cartridge,
// firmware, memory or save data. Not a network or state-import protocol.
constexpr uint32_t MaxTraceFrames = 36008;
using InputTrace = std::vector<std::vector<FrameInput>>;
inline InputTrace ReadInputTrace(const std::string& path, unsigned seats) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in || in.tellg() < 1 || in.tellg() > 8 * 1024 * 1024)
        throw std::runtime_error("Input trace size bounds");
    in.seekg(0);
    std::string line, magic, extra; unsigned count=0;
    std::getline(in,line); std::istringstream header(line);
    if (!(header>>magic>>count) || magic!="TRAINEROS_DS_INPUT_V1" || count!=seats || (header>>extra))
        throw std::runtime_error("Input trace header");
    InputTrace result;
    while (std::getline(in,line)) {
        if (result.size() >= MaxTraceFrames) throw std::runtime_error("Input trace frame bounds");
        std::istringstream row(line); uint32_t frame;
        if (!(row>>frame) || frame!=result.size()) throw std::runtime_error("Input trace order");
        std::vector<FrameInput> inputs(seats);
        for (auto& input:inputs) {
            uint32_t touch,x,y;
            if (!(row>>input.keys>>touch>>x>>y) || input.keys>0xFFF || touch>1 || x>255 || y>191)
                throw std::runtime_error("Input trace input bounds");
            input.frame=frame; input.touching=touch!=0; input.x=uint16_t(x); input.y=uint16_t(y);
        }
        if (row>>extra) throw std::runtime_error("Input trace trailing data");
        result.push_back(std::move(inputs));
    }
    if (result.empty() || in.bad()) throw std::runtime_error("Input trace incomplete");
    return result;
}
inline std::string InputTraceLine(const std::vector<FrameInput>& inputs) {
    std::ostringstream row; row<<inputs.front().frame;
    for (const auto& f:inputs) row<<' '<<f.keys<<' '<<unsigned(f.touching)<<' '<<f.x<<' '<<f.y;
    row<<'\n'; return row.str();
}
}
