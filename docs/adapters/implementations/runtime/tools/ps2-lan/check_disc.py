#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import functools
import ctypes as C
import ctypes.util
import gzip
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from disc import identity


def entry(name,lba,size):
    name=name.encode();result=bytearray(34+len(name));result[0]=len(result)
    struct.pack_into('<I',result,2,lba);struct.pack_into('<I',result,10,size)
    result[32]=len(name);result[33:33+len(name)]=name;return result


class DiscChecks(unittest.TestCase):
    def inspect(self, config=b'BOOT2 = cdrom0:\\SLUS_212.40;1\r\n', duplicate=False, lba=22, format='iso'):
        data=bytearray(32*2048);data[16*2048:16*2048+7]=b'\x01CD001\x01'
        struct.pack_into('<I',data,16*2048+158,20);struct.pack_into('<I',data,16*2048+166,2048)
        elf=b'\x7fELF\x01\x01'+bytes(range(58))
        rows=entry('SYSTEM.CNF;1',21,len(config))+entry('SLUS_212.40;1',lba,len(elf))
        if duplicate:rows+=entry('SLUS_212.40;1',22,len(elf))
        data[20*2048:20*2048+len(rows)]=rows;data[21*2048:21*2048+len(config)]=config
        data[22*2048:22*2048+len(elf)]=elf
        if format in ('bin','img','mdf'):
            data=b''.join(bytes(24)+data[n:n+2048]+bytes(280) for n in range(0,len(data),2048))
        if format=='gz':data=gzip.compress(data)
        if format=='nrg':data=bytes(150*2048)+data+b'NERO'+bytes(4)
        if format in ('cso','zso','bad-cso'):
            count=len(data)//2048;packed=bytearray(24+(count+1)*4)
            struct.pack_into('<4sIQIBB2x',packed,0,b'CISO',24,len(data),2048,1,0)
            if format=='zso':
                packed[:4]=b'ZISO';lz4=C.CDLL(ctypes.util.find_library('lz4') or ctypes.util.find_library('liblz4'))
                lz4.LZ4_compress_default.argtypes=[C.c_char_p,C.c_void_p,C.c_int,C.c_int]
                lz4.LZ4_compress_default.restype=C.c_int
            for n in range(count):
                plain=n%2==0;struct.pack_into('<I',packed,24+n*4,len(packed)|(0x80000000 if plain else 0))
                block=data[n*2048:(n+1)*2048];compress=zlib.compressobj(wbits=-15)
                if format=='zso' and not plain:
                    output=C.create_string_buffer(4096);length=lz4.LZ4_compress_default(bytes(block),output,len(block),4096)
                    self.assertGreater(length,0);packed.extend(output.raw[:length])
                else:packed.extend(block if plain else compress.compress(block)+compress.flush())
            struct.pack_into('<I',packed,24+count*4,len(packed))
            if format=='bad-cso':struct.pack_into('<I',packed,24,0xffffffff)
            data=packed
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/('unrelated-title.'+format);path.write_bytes(data)
            return identity(path,Path(directory)/'unused.so'),elf

    def test_exact_identity(self):
        found,elf=self.inspect()
        self.assertEqual(found,{'serial':'SLUS-21240','crc':f'{functools.reduce(int.__xor__,struct.unpack("<16I",elf)):08X}','content':None})

    def test_duplicate_entry_rejected(self):
        with self.assertRaises(ValueError):self.inspect(duplicate=True)

    def test_native_containers_preserve_boot_identity(self):
        expected,_=self.inspect()
        for format in ('bin','img','mdf','cso','zso','gz','nrg'):
            with self.subTest(format=format):self.assertEqual(self.inspect(format=format)[0],expected)
        with self.assertRaises(ValueError):self.inspect(format='bad-cso')

    def test_extent_rejected(self):
        with self.assertRaises(ValueError):self.inspect(lba=1000000)

    def test_duplicate_boot_rejected(self):
        with self.assertRaises(ValueError):self.inspect(config=b'BOOT2=cdrom0:\\SLUS_212.40;1\n'*2)

    def test_parent_path_rejected(self):
        with self.assertRaises(ValueError):self.inspect(config=b'BOOT2=cdrom0:\\..\\SLUS_212.40;1\n')


if __name__=='__main__':unittest.main()
