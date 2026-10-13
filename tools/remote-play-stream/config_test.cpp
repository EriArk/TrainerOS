// SPDX-License-Identifier: GPL-3.0-or-later
#include "common.hpp"
#include <cstdio>
#include <filesystem>

int main() {
    char directory[]="/tmp/traineros-stream-test-XXXXXX";
    if(!mkdtemp(directory))return 1;
    const auto file=std::string(directory)+"/session";
    const std::string valid="address=127.0.0.1\npeer=127.0.0.1\nkey=00000000000000000000000000000000\nping=0123456789abcdef\nconnect=1\n";
    unsigned checks=0,failed=0;
    auto test=[&](const std::string& data,mode_t mode,bool expected) {
        {std::ofstream output(file);output<<data;}chmod(file.c_str(),mode);
        bool accepted=false;try {Config c(file.c_str());accepted=true;}catch(const std::exception&){}
        ++checks;if(accepted!=expected)++failed;
    };
    test(valid,0600,true);
    test(valid+"mode=game\ndisplay=1\ndrop_every=30\n",0600,true);
    test(valid,0644,false);
    test(valid+"connect=2\n",0600,false);
    test(valid+"command=unexpected\n",0600,false);
    test(valid+"seconds=999999999999999999\n",0600,false);
    test(valid+"seconds=10suffix\n",0600,false);
    test(valid+"seconds=4\n",0600,false);
    test(valid+"seconds=121\n",0600,false);
    test(valid+"display=true\n",0600,false);
    test(valid+"mode=desktop\n",0600,false);
    test(valid+"drop_every=1\n",0600,false);
    test(valid+"drop_frame=180\n",0600,true);
    test(valid+"drop_frame=59\n",0600,false);
    test(valid+"drop_frame=601\n",0600,false);
    test(valid+"drop_frame=180tail\n",0600,false);
    test(valid+std::string(4096,'a'),0600,false);
    for(const auto value:{"0","4294967296","-1","1bad"}) {
        auto text=valid;text.replace(text.find("connect=1"),9,std::string("connect=")+value);
        test(text,0600,false);
    }
    test(valid,0600,true);
    const auto link=file+"-link";symlink(file.c_str(),link.c_str());
    bool accepted=false;try {Config c(link.c_str());accepted=true;}catch(const std::exception&){}
    ++checks;if(accepted)++failed;
    std::filesystem::remove(link);std::filesystem::remove(file);std::filesystem::remove(directory);
    std::printf("configuration checks=%u failed=%u\n",checks,failed);
    return failed?1:0;
}
