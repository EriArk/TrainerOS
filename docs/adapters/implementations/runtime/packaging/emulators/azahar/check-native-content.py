#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise native metadata and staged installation with generated, key-free data.

These tiny containers carry no game code, artwork, keys or commercial content.
They test the native CIA/ZCIA reader and storage transaction, not game boot.
"""
import argparse
import ctypes
import ctypes.util
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile


def cia(title=0x0004000000001200, encrypted=False, executable=True):
    body=bytearray(0xa00)
    body[0x100:0x104]=b'NCCH';struct.pack_into('<I',body,0x104,len(body)//0x200)
    struct.pack_into('<Q',body,0x108,title);struct.pack_into('<Q',body,0x118,title)
    struct.pack_into('<I',body,0x180,0x400)
    body[0x18d]=2 if executable else 0;body[0x18f]=4
    ticket=bytearray(0x140+0x164+20);struct.pack_into('>I',ticket,0,0x10004)
    struct.pack_into('>Q',ticket,0x140+0x90,1);struct.pack_into('>Q',ticket,0x140+0x9c,title)
    struct.pack_into('>HHIIHHI',ticket,0x140+0x164,1,20,20,20,0,20,0)
    tmd=bytearray(0xb34);struct.pack_into('>I',tmd,0,0x10004)
    struct.pack_into('>Q',tmd,0x18c,title);struct.pack_into('>HH',tmd,0x1dc,0,1)
    struct.pack_into('>IHHQ',tmd,0xb04,42,0,int(encrypted),len(body))
    tmd[0xb14:0xb34]=hashlib.sha256(body).digest()
    header=bytearray(0x2020);struct.pack_into('<IHHIIIIQ',header,0,len(header),0,0,0,len(ticket),len(tmd),0,len(body));header[32]=128
    result=bytes(header)
    for part in (ticket,tmd,body):
        result+=b'\0'*((-len(result))%64)+part
    return result


def zcia(raw):
    lib=ctypes.CDLL(ctypes.util.find_library('zstd'))
    lib.ZSTD_compressBound.argtypes=[ctypes.c_size_t];lib.ZSTD_compressBound.restype=ctypes.c_size_t
    lib.ZSTD_compress.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_int]
    lib.ZSTD_compress.restype=ctypes.c_size_t
    target=ctypes.create_string_buffer(lib.ZSTD_compressBound(len(raw)))
    size=lib.ZSTD_compress(target,len(target),raw,len(raw),3)
    assert size<=len(target)
    packed=target.raw[:size]+struct.pack('<IIIIIBI',0x184d2a5e,17,size,len(raw),1,0,0x8f92eab1)
    return struct.pack('<4s4sBBHIQQ',b'Z3DS',b'CIA\0',1,0,32,0,len(packed),len(raw))+packed


def run(runtime):
    spec=importlib.util.spec_from_file_location('content',Path(__file__).with_name('content.py'))
    content=importlib.util.module_from_spec(spec);spec.loader.exec_module(content)
    with tempfile.TemporaryDirectory(prefix='traineros-native-cia-') as temporary:
        root=Path(temporary);source=root/'base.cia';source.write_bytes(cia())
        request={'runtime':str(runtime),'source':str(source),'nand':str(root/'nand'),
                 'sdmc':str(root/'sdmc'),'state':str(root/'state')}
        before=content.inspect(request);assert before['executable'] and before['package']
        assert before['title']=='0004000000001200'
        assert not (root/'nand').exists() and not (root/'sdmc').exists(), 'inspection wrote personal storage'
        installed=content.install(request);entry=Path(installed['entry'])
        assert entry.is_file() and entry.name=='0000002a.app',installed
        save=entry.parent.parent/'data/sentinel';save.parent.mkdir();save.write_bytes(b'personal save')
        content.install(request)
        compressed=root/'base.zcia';compressed.write_bytes(zcia(source.read_bytes()))
        compressed_request=dict(request,source=str(compressed))
        assert content.inspect(compressed_request)['title']==before['title']
        content.install(compressed_request)
        for high in (0x0004000e,0x0004008c):
            package=root/(f'{high:08x}.cia');package.write_bytes(cia((high<<32)|0x1200))
            value=content.install(dict(request,source=str(package)))
            assert not value['executable'] and Path(value['contentRoot']).is_dir()
        assert content.inspect(request)['fingerprint']!=before['fingerprint']
        assert save.read_bytes()==b'personal save'
        demo=root/'demo.cia';demo.write_bytes(cia(0x0004000200001200))
        assert content.install(dict(request,source=str(demo)))['executable']
        for name,payload in [('truncated',cia()[:0x2050]),('encrypted',cia(encrypted=True)),
                             ('system',cia(0x0004001000001200)),('non-executable',cia(executable=False))]:
            path=root/(name+'.cia');path.write_bytes(payload)
            try:content.inspect(dict(request,source=str(path)))
            except ValueError:pass
            else:raise AssertionError(name+' unexpectedly admitted')
        print('Native CIA/ZCIA: inspect, base/update/DLC install, idempotence, save preservation and 4 invalid cases passed')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--runtime',type=Path,required=True)
    run(parser.parse_args().runtime.resolve())
