# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only PS2 DVD identity using libchdr and ISO9660, never filenames.

CHD v5 DVD raw SHA-1 is a declared image identity, not full image verification
or permission to read/write semantic saves. Only decoded sectors' CRCs are
verified by the pinned library. Unsupported sector layouts/parent CHDs fail closed.
"""
import array
import ctypes as C
import functools
import hashlib
from pathlib import Path
import re
import struct
import sys


class Disc:
    def __init__(self, path: Path, library: Path):
        self.file = path.open('rb')
        self.chd = C.c_void_p()
        self.lib = None
        try:
            self.open(path, library)
        except BaseException:
            self.close()
            raise

    def open(self, path, library):
        self.stride=2048;self.data_offset=0
        header = self.file.read(124)
        if header[:8] == b'MComprHD':
            if len(header) != 124 or struct.unpack_from('>II', header, 8) != (124, 5):
                raise ValueError('Unsupported CHD version')
            self.size = struct.unpack_from('>Q', header, 32)[0]
            self.hunk, unit = struct.unpack_from('>II', header, 56)
            if unit not in (2048,2448) or not unit <= self.hunk <= 1024*1024 or self.hunk % unit or not 32768 < self.size <= 16*1024**3:
                raise ValueError('Unsupported PS2 DVD layout')
            if any(header[104:124]) or not any(header[64:84]):
                raise ValueError('Parent or unidentified CHD')
            self.content = hashlib.sha256(b'chd-raw-sha1:'+header[64:84]).hexdigest()
            self.lib = C.CDLL(str(library))
            self.lib.chd_open.argtypes = [C.c_char_p,C.c_int,C.c_void_p,C.POINTER(C.c_void_p)]
            self.lib.chd_open.restype = C.c_int
            self.lib.chd_read.argtypes = [C.c_void_p,C.c_uint32,C.c_void_p]
            self.lib.chd_read.restype = C.c_int
            self.lib.chd_close.argtypes = [C.c_void_p]
            if self.lib.chd_open(str(path).encode(),1,None,C.byref(self.chd)):
                raise ValueError('Unreadable CHD')
            self.buffer = C.create_string_buffer(self.hunk)
            self.cached = -1
            self.stride=unit
        else:
            self.file.seek(0,2);self.size=self.file.tell()
            if not 32768 < self.size <= 16*1024**3 or self.size%2048:
                raise ValueError('Unsupported raw DVD layout')
            self.content = None
        self.raw_size=self.size
        if self.stride==2448:
            sector=self.raw(16*self.stride,self.stride)
            offsets=[n for n in (0,16,24) if sector[n:n+7]==b'\x01CD001\x01']
            if len(offsets)!=1:raise ValueError('Unsupported CHD sector layout')
            self.data_offset=offsets[0]
            self.size=self.raw_size//self.stride*2048

    def close(self):
        if self.chd:self.lib.chd_close(self.chd);self.chd=C.c_void_p()
        self.file.close()

    def read(self, offset, size):
        if offset<0 or size<0 or size>64*1024**2 or offset+size>self.size:
            raise ValueError('Invalid disc extent')
        if self.stride==2048:return self.raw(offset,size)
        result=bytearray()
        while size:
            sector,position=divmod(offset,2048);count=min(size,2048-position)
            result.extend(self.raw(sector*self.stride+self.data_offset+position,count))
            offset+=count;size-=count
        return bytes(result)

    def raw(self, offset, size):
        if offset<0 or size<0 or offset+size>self.raw_size:
            raise ValueError('Invalid raw disc extent')
        if not self.chd:
            self.file.seek(offset);value=self.file.read(size)
            if len(value)!=size:raise ValueError('Truncated disc')
            return value
        result=bytearray()
        while size:
            hunk,position=divmod(offset,self.hunk)
            if hunk!=self.cached:
                if self.lib.chd_read(self.chd,hunk,self.buffer):raise ValueError('Corrupt CHD sector')
                self.cached=hunk
            count=min(size,self.hunk-position)
            result.extend(self.buffer.raw[position:position+count]);offset+=count;size-=count
        return bytes(result)

    def directory(self, lba, size):
        if size>256*1024:raise ValueError('Oversized ISO directory')
        data=self.read(lba*2048,size);result={};at=0
        while at<len(data):
            length=data[at]
            if not length:at=(at//2048+1)*2048;continue
            if length<34 or at+length>len(data):raise ValueError('Invalid ISO directory')
            entry=data[at:at+length];name_length=entry[32]
            if 33+name_length>length:raise ValueError('Invalid ISO name')
            name=entry[33:33+name_length].decode('ascii').upper().split(';')[0]
            if name not in ('\0','\1'):
                if name in result:raise ValueError('Ambiguous ISO entry')
                result[name]=(int.from_bytes(entry[2:6],'little'),int.from_bytes(entry[10:14],'little'),bool(entry[25]&2))
            at+=length
        return result

    def inspect(self):
        pvd=self.read(16*2048,2048)
        if pvd[:7]!=b'\x01CD001\x01':raise ValueError('Missing ISO primary volume')
        root=self.directory(int.from_bytes(pvd[158:162],'little'),int.from_bytes(pvd[166:170],'little'))
        extent=root.get('SYSTEM.CNF')
        if not extent or extent[2] or extent[1]>8192:raise ValueError('Missing PS2 boot configuration')
        config=self.read(extent[0]*2048,extent[1]).decode('ascii')
        boots=re.findall(r'^\s*BOOT2\s*=\s*cdrom0:[\\/]([^\r\n;]+)(?:;1)?\s*$',config,re.M|re.I)
        if len(boots)!=1:raise ValueError('Ambiguous PS2 boot')
        components=boots[0].upper().replace('\\','/').split('/')
        if len(components)>8 or any(x in ('','.','..') for x in components):raise ValueError('Invalid boot path')
        serial=re.fullmatch(r'([A-Z]{4})_(\d{3})\.(\d{2})',components[-1])
        if not serial:raise ValueError('Unknown PS2 serial')
        directory=root
        for index,name in enumerate(components):
            item=directory.get(name)
            if not item:raise ValueError('Missing boot executable')
            if index<len(components)-1:
                if not item[2]:raise ValueError('Invalid boot directory')
                directory=self.directory(item[0],item[1])
        if item[2] or item[1]>64*1024**2:raise ValueError('Invalid boot executable')
        elf=self.read(item[0]*2048,item[1])
        if elf[:6]!=b'\x7fELF\x01\x01':raise ValueError('Invalid PS2 ELF')
        words=array.array('I');words.frombytes(elf[:len(elf)//4*4])
        if sys.byteorder!='little':words.byteswap()
        crc=functools.reduce(int.__xor__,words,0)
        return {'serial':serial[1]+'-'+serial[2]+serial[3],'crc':f'{crc:08X}','content':self.content}


def identity(path: Path, library: Path):
    disc=Disc(path,library)
    try:
        return disc.inspect()
    finally:
        disc.close()
