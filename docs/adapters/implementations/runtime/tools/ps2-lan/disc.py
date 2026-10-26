# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only PS2 DVD identity using libchdr and ISO9660, never filenames.

CHD v5 DVD raw SHA-1 is a declared image identity, not full image verification
or permission to read/write semantic saves. Only decoded sectors' CRCs are
verified by the pinned library. Unsupported sector layouts/parent CHDs fail closed.
"""
import array
import ctypes as C
import ctypes.util
import functools
import gzip
import hashlib
from pathlib import Path
import re
import struct
import sys
import zlib


class Disc:
    def __init__(self, path: Path, library: Path):
        self.file = path.open('rb')
        self.chd = C.c_void_p()
        self.lib = None
        self.source = None
        self.cso = None
        self.lz4 = None
        try:
            self.open(path, library)
        except BaseException:
            self.close()
            raise

    def open(self, path, library):
        self.stride=2048;self.data_offset=0;self.base=0
        header = self.file.read(124)
        if header[:2] == b'\x1f\x8b':
            self.source = self.file
            self.source.seek(0)
            self.file = gzip.GzipFile(fileobj=self.source,mode='rb')
            # Bounded streaming validation; never trust gzip's modulo-4GiB ISIZE.
            total = 0
            while block := self.file.read(1024*1024):
                total += len(block)
                if total > 16*1024**3:raise ValueError('Oversized gzip disc')
            self.file.seek(0);header=self.file.read(124)
            if header[:4] in (b'CISO',b'ZISO') or header[:8]==b'MComprHD':raise ValueError('Nested compressed disc')
        if header[:4] in (b'CISO',b'ZISO'):
            if len(header)<24:raise ValueError('Truncated CSO')
            header_size,self.size,frame,version,shift=struct.unpack_from('<IQIBB',header,4)
            if header_size!=24 or version>1 or shift>20 or frame<2048 or frame>1024**2 or frame&(frame-1) \
               or not 32768<self.size<=16*1024**3 or self.size%2048:
                raise ValueError('Unsupported CSO layout')
            count=(self.size+frame-1)//frame
            self.file.seek(24);table=self.file.read((count+1)*4)
            if len(table)!=(count+1)*4:raise ValueError('Truncated CSO index')
            indexes=array.array('I');indexes.frombytes(table)
            if sys.byteorder!='little':indexes.byteswap()
            self.file.seek(0,2);packed=self.file.tell()
            for n in range(count):
                start=(indexes[n]&0x7fffffff)<<shift;end=(indexes[n+1]&0x7fffffff)<<shift
                if start<24+len(table) or end<=start or end>packed or end-start>frame+(1<<shift) \
                   or (indexes[n]&0x80000000 and end-start<min(frame,self.size-n*frame)):
                    raise ValueError('Invalid CSO extent')
            self.cso=(indexes,frame,shift);self.cached=-1;self.buffer=b'';self.content=None
            if header[:4]==b'ZISO':
                name=ctypes.util.find_library('lz4') or ctypes.util.find_library('liblz4')
                if not name:raise ValueError('Native LZ4 library unavailable')
                self.lz4=C.CDLL(name)
                self.lz4.LZ4_decompress_safe.argtypes=[C.c_char_p,C.c_void_p,C.c_int,C.c_int]
                self.lz4.LZ4_decompress_safe.restype=C.c_int
            self.raw_size=self.size
            return
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
            if not 32768 < self.size <= 16*1024**3:
                raise ValueError('Unsupported raw DVD layout')
            layouts=[]
            # Same layouts/order as ARMSX2 InputIsoFile::Detect, including NRG.
            for stride,offset,base in ((2048,0,0),(2336,8,0),(2352,24,0),(2448,24,0),
                                       (2048,0,150*2048),(2352,24,150*2048),(2448,24,150*2048)):
                if self.size<=base+17*stride:continue
                self.file.seek(base+16*stride+offset)
                if self.file.read(7)==b'\x01CD001\x01':layouts.append((stride,offset,base))
            if len(layouts)!=1:raise ValueError('Unsupported raw sector layout')
            self.stride,self.data_offset,self.base=layouts[0]
            self.content = None
        self.raw_size=self.size
        if self.chd and self.stride==2448:
            sector=self.raw(16*self.stride,self.stride)
            offsets=[n for n in (0,16,24) if sector[n:n+7]==b'\x01CD001\x01']
            if len(offsets)!=1:raise ValueError('Unsupported CHD sector layout')
            self.data_offset=offsets[0]
            self.size=self.raw_size//self.stride*2048
        else:self.size=(self.raw_size-self.base)//self.stride*2048

    def close(self):
        if self.chd:self.lib.chd_close(self.chd);self.chd=C.c_void_p()
        self.file.close()
        if self.source:self.source.close()

    def read(self, offset, size):
        if offset<0 or size<0 or size>64*1024**2 or offset+size>self.size:
            raise ValueError('Invalid disc extent')
        if self.stride==2048:return self.raw(self.base+offset,size)
        result=bytearray()
        while size:
            sector,position=divmod(offset,2048);count=min(size,2048-position)
            result.extend(self.raw(self.base+sector*self.stride+self.data_offset+position,count))
            offset+=count;size-=count
        return bytes(result)

    def raw(self, offset, size):
        if offset<0 or size<0 or offset+size>self.raw_size:
            raise ValueError('Invalid raw disc extent')
        if self.cso:
            indexes,frame,shift=self.cso;result=bytearray()
            while size:
                number,within=divmod(offset,frame)
                if number!=self.cached:
                    start=(indexes[number]&0x7fffffff)<<shift;end=(indexes[number+1]&0x7fffffff)<<shift
                    self.file.seek(start);packed=self.file.read(end-start)
                    if len(packed)!=end-start:raise ValueError('Truncated CSO block')
                    expected=min(frame,self.raw_size-number*frame)
                    if indexes[number]&0x80000000:self.buffer=packed[:expected]
                    elif self.lz4:
                        decoded=C.create_string_buffer(expected)
                        if self.lz4.LZ4_decompress_safe(packed,decoded,len(packed),expected)!=expected:
                            raise ValueError('Invalid ZSO block')
                        self.buffer=decoded.raw
                    else:
                        decoder=zlib.decompressobj(-15)
                        self.buffer=decoder.decompress(packed,expected+1)
                        if not decoder.eof or decoder.unconsumed_tail:raise ValueError('Invalid CSO block')
                    if len(self.buffer)!=expected:raise ValueError('Invalid CSO block size')
                    self.cached=number
                count=min(size,len(self.buffer)-within)
                result.extend(self.buffer[within:within+count]);offset+=count;size-=count
            return bytes(result)
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
