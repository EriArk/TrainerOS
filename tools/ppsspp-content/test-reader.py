#!/usr/bin/env python3
"""Synthetic format tests against the real native reader, no game assets.

Usage: python3 test-reader.py [qemu-aarch64 -L SYSROOT] /path/psp-content-probe
"""
import json
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

def put(b, offset, fmt, *values):
    struct.pack_into(fmt, b, offset, *values)

def sfo(category=b'UG'):
    b = bytearray(88)
    put(b, 0, '<4sIIII', b'\0PSF', 0x101, 52, 72, 2)
    put(b, 20, '<HHIII', 0, 0x204, 3, 4, 0)
    put(b, 36, '<HHIII', 9, 0x204, 10, 12, 4)
    b[52:69] = b'CATEGORY\0DISC_ID\0'
    b[72:74], b[76:85] = category, b'TEST00001'
    return b

def entry(sector, length, name, directory):
    b = bytearray((34 + len(name)) & ~1)
    b[0], b[25], b[32] = len(b), 2 if directory else 0, len(name)
    for offset, value in ((2, sector), (10, length)):
        put(b, offset, '<I', value); put(b, offset + 4, '>I', value)
    put(b, 28, '<H', 1); put(b, 30, '>H', 1)
    b[33:33+len(name)] = name
    return b

def iso():
    b = bytearray(24 * 2048); pvd = 16 * 2048
    b[pvd:pvd+7] = b'\x01CD001\x01'
    put(b, pvd+80, '<I', 24); put(b, pvd+84, '>I', 24)
    put(b, pvd+128, '<H', 2048); put(b, pvd+130, '>H', 2048)
    for offset, data in ((pvd+156, entry(20, 2048, b'\0', True)),
                         (20*2048, entry(21, 2048, b'PSP_GAME', True)),
                         (21*2048, entry(22, 88, b'PARAM.SFO;1', False)), (22*2048, sfo())):
        b[offset:offset+len(data)] = data
    return b

def cso(data, frame=2048, shift=0, mixed=False):
    count = (len(data)+frame-1)//frame
    b = bytearray(24+(count+1)*4)
    put(b, 0, '<4sIQIBB2x', b'CISO', 24, len(data), frame, 1, shift)
    for n in range(count):
        b.extend(b'\0' * (-len(b) % (1 << shift)))
        plain = mixed and n % 2 == 0
        put(b, 24+n*4, '<I', (len(b) >> shift) | (0x80000000 if plain else 0))
        block = data[n*frame:(n+1)*frame].ljust(frame,b'\0')
        compressor = zlib.compressobj(wbits=-15)
        b.extend(block if plain else compressor.compress(block)+compressor.flush())
    b.extend(b'\0' * (-len(b) % (1 << shift)))
    put(b, 24+count*4, '<I', len(b) >> shift)
    return b

def pbp(category=b'UG', executable=b'\x7fELF'+bytes(60), psar=b''):
    param = sfo(category); end = 40+len(param)
    return bytearray(struct.pack('<4sI8I', b'\0PBP', 0x10000, 40, end, end, end, end, end,
                                 end, end+len(executable)) + param + executable + psar)

def changed(data, offset, fmt, value):
    b=bytearray(data); put(b, offset, fmt, value); return b

cases = []
# Uncompressed CHD v5, native libchdr map and hunk reads (own ISO fixture).
def chd(data):
    b=bytearray(2048)
    put(b,0,'>8sII',b'MComprHD',124,5)
    put(b,32,'>QQQII',len(data),124,0,2048,2048)
    for n in range(len(data)//2048):put(b,124+n*4,'>I',n+1)
    return b+data

base_chd=chd(iso())
cases.append(('umd-chd','chd',base_chd,True))
for name,data in (
    ('chd-parent',changed(base_chd,104,'B',1)),
    ('chd-cd-unit',changed(base_chd,60,'>I',2448)),
    ('chd-huge-hunk',changed(base_chd,56,'>I',0x40000000)),
    ('chd-huge-logical',changed(base_chd,32,'>Q',0x100000000)),
    ('chd-map-outside',changed(base_chd,124+16*4,'>I',0x7fffffff)),
    ('chd-truncated',base_chd[:4096])):
    cases.append((name,'chd',data,False))
for frame, shift, mixed in ((2048,0,False), (2048,2,True), (8192,0,False), (32768,4,True)):
    cases.append((f'cso-{frame}-{shift}-{mixed}', 'cso', cso(iso(),frame,shift,mixed), True))
base = cso(iso())
corrupt = bytearray(base)
start,end=struct.unpack_from('<II',base,24+16*4)
corrupt[start:end]=bytes(end-start)
cases.append(('corrupt-deflate','cso',corrupt,False))
for name, data in (
    ('version', changed(base,20,'B',2)), ('huge-frame',changed(base,16,'<I',0x80000000)),
    ('shift',changed(base,21,'B',63)), ('index-overflow',changed(base,24,'<I',0x7fffffff)),
    ('reversed-index',changed(base,28,'<I',0)), ('short-index',base[:50]),
    ('truncated',base[:-4]), ('deflate',base[:struct.unpack_from('<I',base,24+16*4)[0]]+bytes(20)),
    ('video',cso(iso()[:22*2048]+sfo(b'UV')+iso()[22*2048+88:]))):
    cases.append((name,'cso',data,False))
for category in (b'UG',b'EG',b'MG'):
    cases.append(('psp-'+category.decode(),'pbp',pbp(category),True))
cases += [('encrypted','pbp',pbp(executable=b'~PSP'+bytes(60)),True),
          ('npumd','pbp',pbp(executable=b'',psar=b'NPUMDIMG'+bytes(248)),True),
          ('ps1-category','pbp',pbp(b'ME'),False),
          ('ps1-psar','pbp',pbp(psar=b'PSISOIMG0000'),False),
          ('no-executable','pbp',pbp(executable=b''),False),
          ('offset-order','pbp',changed(pbp(),16,'<I',40),False),
          ('offset-overflow','pbp',changed(pbp(),8,'<I',0xffffffff),False),
          ('duplicate-id','pbp',changed(pbp(),40+36,'<H',0),False),
          ('invalid-id','pbp',changed(pbp(),40+76,'B',ord('1')),False)]
with tempfile.TemporaryDirectory() as folder:
    for name, ext, data, valid in cases:
        path=Path(folder)/(name+'.'+ext);path.write_bytes(data)
        result=subprocess.run(sys.argv[1:]+['--psp-content',str(path)],capture_output=True,timeout=20)
        assert (result.returncode==0)==valid, (name,result.returncode,result.stdout,result.stderr)
        if valid:
            assert json.loads(result.stdout)=={'protocol':1,'disc':'TEST00001','format':ext}, name
        else:
            assert not result.stdout, name
print(f'{len(cases)} native reader cases passed')
