// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <fstream>
#include <fcntl.h>
#include <sstream>
#include <charconv>
#include <set>
#include <map>
#include <stdexcept>
#include <string>

inline uint64_t monotonicNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Config {
    std::string address, peer, key, keyHex, ping;
    uint32_t connect = 0;
    int seconds = 25;
    bool display = false;
    bool sound = false;
    bool game = false;
    unsigned dropEvery = 0;
    unsigned dropFrame = 0;
    explicit Config(const char* path) {
        const int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
        if(fd<0)throw std::runtime_error("Cannot open private configuration");
        struct stat st{};
        if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != getuid()
            || (st.st_mode & 077) || st.st_size > 4096) {
            close(fd);
            throw std::runtime_error("Expected an owner-only regular configuration file");
        }
        char bytes[4097];const auto length=read(fd,bytes,sizeof(bytes));close(fd);
        if(length<1||length!=st.st_size||length>4096)throw std::runtime_error("Invalid configuration size");
        std::istringstream input(std::string(bytes,length));
        std::map<std::string, std::string> fields;
        const std::set<std::string> allowed{"address","peer","key","ping","connect","seconds","display","sound","mode","drop_every","drop_frame"};
        for (std::string line; std::getline(input, line);) {
            auto pos = line.find('=');
            if (pos == std::string::npos || !allowed.count(line.substr(0,pos)) || !fields.emplace(line.substr(0,pos),line.substr(pos+1)).second)
                throw std::runtime_error("Invalid configuration");
        }
        address=fields.at("address"); peer=fields.at("peer"); keyHex=fields.at("key"); ping=fields.at("ping");
        in_addr ip{};
        if (inet_pton(AF_INET,address.c_str(),&ip)!=1 || inet_pton(AF_INET,peer.c_str(),&ip)!=1
            || keyHex.size()!=32 || ping.size()!=16)
            throw std::runtime_error("Invalid endpoint or session material");
        for (size_t n=0;n<keyHex.size();n+=2) {
            const auto text=keyHex.substr(n,2);
            if (text.find_first_not_of("0123456789abcdef")!=std::string::npos)
                throw std::runtime_error("Invalid key encoding");
            key.push_back(static_cast<char>(std::stoul(text,nullptr,16)));
        }
        auto number=[&](const std::string& name) {
            const auto& text=fields.at(name);uint32_t value=0;
            auto result=std::from_chars(text.data(),text.data()+text.size(),value);
            if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())throw std::runtime_error("Invalid integer");
            return value;
        };
        connect=number("connect");
        if (fields.count("seconds")) {auto value=number("seconds");if(value>120)throw std::runtime_error("Invalid lifetime");seconds=value;}
        if (seconds<5 || seconds>120 || connect==0) throw std::runtime_error("Invalid lifetime");
        if(fields.count("display")&&fields["display"]!="0"&&fields["display"]!="1")throw std::runtime_error("Invalid display");
        if(fields.count("sound")&&fields["sound"]!="0"&&fields["sound"]!="1")throw std::runtime_error("Invalid sound");
        if(fields.count("mode")&&fields["mode"]!="synthetic"&&fields["mode"]!="game")throw std::runtime_error("Invalid source mode");
        display=fields["display"]=="1";
        sound=fields["sound"]=="1";
        game=fields["mode"]=="game";
        if(fields.count("drop_every"))dropEvery=number("drop_every");
        if(dropEvery && (dropEvery<10||dropEvery>600))throw std::runtime_error("Invalid drop interval");
        if(fields.count("drop_frame"))dropFrame=number("drop_frame");
        if(dropFrame && (dropFrame<60||dropFrame>600))throw std::runtime_error("Invalid dropped frame");
    }
};
constexpr int RtspPort=49010, VideoPort=49011, AudioPort=49012, ControlPort=49013;
