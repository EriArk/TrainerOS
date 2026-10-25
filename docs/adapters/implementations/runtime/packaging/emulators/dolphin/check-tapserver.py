#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux regression for the pinned upstream TAP class, not a Dolphin gameplay test.

Pass a directory containing the original TAPServerConnection.cpp/.h from
c77bbaa0f372c3f72281602a8b087206706542cb/Source/Core/Core/HW/EXI/BBA.
Only logging, utility/platform wrappers and unused IP lookup are stubbed. The
actual upstream socket/framing/thread code is compiled before and after patch.
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('upstream', type=Path)
parser.add_argument('--cxx', default='c++')
args = parser.parse_args()
hashes = {'cpp': 'ecdd828a142a874d8ab73dbf6db364004312527173e4a35531096a1428dce78f',
          'h': '593538416cff633797de4e69965da60fd0f4f3cc51f778c135e4ee396ee47a7a'}
for ext, digest in hashes.items():
    if hashlib.sha256((args.upstream / ('TAPServerConnection.' + ext)).read_bytes()).hexdigest() != digest:
        raise ValueError('Unexpected upstream source; review the pin before testing')

stubs = {
    'Common/CommonTypes.h': '#pragma once\n#include <cstdint>\nusing u8=uint8_t;using u32=uint32_t;\n',
    'Common/Flag.h': '#pragma once\n#include <atomic>\nnamespace Common {class Flag {std::atomic_bool v{false};public:void Set(){v=true;}void Clear(){v=false;}bool IsSet(){return v.load();}};}\n',
    'Common/SocketContext.h': '#pragma once\nnamespace Common {struct SocketContext {}; }\n',
    'Common/Logging/Log.h': '#pragma once\n#include <atomic>\nextern std::atomic_int errors;\n#define ERROR_LOG_FMT(...) (++errors)\n#define INFO_LOG_FMT(...) ((void)0)\n',
    'Common/Network.h': '#pragma once\n#include <sys/socket.h>\nnamespace Common {constexpr int SEND_FLAGS=MSG_NOSIGNAL;inline void SetPlatformSocketOptions(int){} }\n',
    'Common/StringUtil.h': '#pragma once\n',
    # Harness uses AF_UNIX; no hostname/IP path participates in this test.
    'SFML/Network/IpAddress.hpp': '#pragma once\n#include <optional>\n#include <string>\n#include <cstdint>\nnamespace sf {struct IpAddress {static std::optional<IpAddress> resolve(const std::string&){return {};}uint32_t toInteger() const{return 0;}};}\n',
}
runner = r'''
#include "Core/HW/EXI/BBA/TAPServerConnection.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
std::atomic_int errors{0};
using namespace std::chrono_literals;
int main() {
    for(int stage=0;stage<3;++stage) {
        errors=0;std::atomic_int received{0};
        const std::string path="/tmp/trainer-bba-proof-"+std::to_string(getpid())+"-"+std::to_string(stage);
        const int server=socket(AF_UNIX,SOCK_STREAM,0);sockaddr_un addr{};addr.sun_family=AF_UNIX;
        std::strcpy(addr.sun_path,path.c_str());
        if(server<0||bind(server,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))||listen(server,1))return 2;
        ExpansionInterface::TAPServerConnection tap(path,[&](std::string&& data){if(data==std::string(60,'x'))++received;},1518);
        if(!tap.Activate())return 3;
        const int peer=accept(server,nullptr,nullptr);if(peer<0)return 4;
        tap.RecvStart();
        std::string packet(2,'\0');packet[0]=60;packet+=std::string(60,'x');
        if(send(peer,packet.data(),packet.size(),MSG_NOSIGNAL)!=62)return 5;
        for(int n=0;n<100&&received==0;++n)std::this_thread::sleep_for(1ms);
        if(received!=1)return 6;
        if(stage==1)send(peer,packet.data(),1,MSG_NOSIGNAL);
        if(stage==2)send(peer,packet.data(),12,MSG_NOSIGNAL);
        std::this_thread::sleep_for(10ms);close(peer);
        std::this_thread::sleep_for(80ms);
        const int count=errors.load();tap.Deactivate();close(server);unlink(path.c_str());
        if(count!=1){std::cerr<<"stage "<<stage<<" repeated errors: "<<count<<'\n';return 1;}
    }
    std::cout<<"BBA EOF: size / partial size / partial frame stopped once\n";
}
'''
with tempfile.TemporaryDirectory(prefix='trainer-bba-check-') as temp:
    root = Path(temp)
    source = root / 'Source/Core/Core/HW/EXI/BBA'
    source.mkdir(parents=True)
    for ext in hashes:
        shutil.copyfile(args.upstream / ('TAPServerConnection.' + ext), source / ('TAPServerConnection.' + ext))
    for name, content in stubs.items():
        target = root / 'Source/Core' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
    (root / 'runner.cpp').write_text(runner)
    def build_run():
        subprocess.run([args.cxx, '-std=c++20', '-pthread', '-I'+str(root / 'Source/Core'),
                        str(source / 'TAPServerConnection.cpp'), str(root / 'runner.cpp'), '-o', str(root / 'check')], check=True)
        return subprocess.run([str(root / 'check')], timeout=5, capture_output=True, text=True)
    before = build_run()
    if before.returncode != 1:
        raise AssertionError('The unchanged upstream source did not reproduce the EOF spin: '+before.stderr)
    print('Original upstream:', before.stderr.strip())
    subprocess.run(['git', 'apply', str(Path(__file__).with_name('traineros-bba-eof.patch').resolve())], cwd=root, check=True)
    after = build_run()
    if after.returncode:
        raise AssertionError('Patched TAP class failed: '+after.stderr)
    print(after.stdout.strip())
