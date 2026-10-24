#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import functools
from pathlib import Path
import struct
import tempfile
import unittest
from disc import identity


def entry(name,lba,size):
    name=name.encode();result=bytearray(34+len(name));result[0]=len(result)
    struct.pack_into('<I',result,2,lba);struct.pack_into('<I',result,10,size)
    result[32]=len(name);result[33:33+len(name)]=name;return result


class DiscChecks(unittest.TestCase):
    def inspect(self, config=b'BOOT2 = cdrom0:\\SLUS_212.40;1\r\n', duplicate=False, lba=22):
        data=bytearray(32*2048);data[16*2048:16*2048+7]=b'\x01CD001\x01'
        struct.pack_into('<I',data,16*2048+158,20);struct.pack_into('<I',data,16*2048+166,2048)
        elf=b'\x7fELF\x01\x01'+bytes(range(58))
        rows=entry('SYSTEM.CNF;1',21,len(config))+entry('SLUS_212.40;1',lba,len(elf))
        if duplicate:rows+=entry('SLUS_212.40;1',22,len(elf))
        data[20*2048:20*2048+len(rows)]=rows;data[21*2048:21*2048+len(config)]=config
        data[22*2048:22*2048+len(elf)]=elf
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'unrelated-title.iso';path.write_bytes(data)
            return identity(path,Path(directory)/'unused.so'),elf

    def test_exact_identity(self):
        found,elf=self.inspect()
        self.assertEqual(found,{'serial':'SLUS-21240','crc':f'{functools.reduce(int.__xor__,struct.unpack("<16I",elf)):08X}','content':None})

    def test_duplicate_entry_rejected(self):
        with self.assertRaises(ValueError):self.inspect(duplicate=True)

    def test_extent_rejected(self):
        with self.assertRaises(ValueError):self.inspect(lba=1000000)

    def test_duplicate_boot_rejected(self):
        with self.assertRaises(ValueError):self.inspect(config=b'BOOT2=cdrom0:\\SLUS_212.40;1\n'*2)

    def test_parent_path_rejected(self):
        with self.assertRaises(ValueError):self.inspect(config=b'BOOT2=cdrom0:\\..\\SLUS_212.40;1\n')


if __name__=='__main__':unittest.main()
