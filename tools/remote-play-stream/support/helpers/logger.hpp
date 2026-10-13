// TrainerOS GPL-3.0-or-later. Narrow logger adapter for the Wolf packetizers.
#pragma once
#include <fmt/format.h>
#include <cstdio>
namespace logs {
enum severity_level { trace, debug, info, warning, error, fatal };
template<typename... Args>
void log(severity_level level, fmt::format_string<Args...> text, Args&&... args) {
    if (level < warning) return;
    const auto message = fmt::format(text, std::forward<Args>(args)...);
    std::fprintf(stderr, "%s\n", message.c_str());
}
}
