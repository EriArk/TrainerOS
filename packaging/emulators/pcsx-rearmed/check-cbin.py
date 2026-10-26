#!/usr/bin/env python3
"""Compile and exercise the real pinned reader functions, without ROM or BIOS."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib


def harness(source):
    text = (source / 'libpcsxcore/cdriso.c').read_text()
    def between(start, end):
        return text[text.index(start):text.index(end, text.index(start))]
    return r'''
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <sys/stat.h>
#include "zlib_wrapper.h"
#define CD_FRAMESIZE_RAW 2352
#define OFF_T_MSB ((off_t)1 << (sizeof(off_t)*8-1))
#define SWAP32(x) (x)
#define SysPrintf(...) fprintf(stderr, __VA_ARGS__)
#define CDRT_DATA 1
static FILE *cdHandle;
static int numtracks;
static struct { int type; unsigned int start_offset, start, length; } ti[100];
''' + between('static struct {', '#ifdef HAVE_CHD') + between('static off_t get_size', '// Some c libs') + between('static int handlecbin(', '#ifdef HAVE_CHD') + between('static int uncompress2_pcsx(', '#ifdef HAVE_CHD') + r'''
int main(int argc, char **argv) {
    if (argc != 4 || !(cdHandle = fopen(argv[1], "rb"))) return 10;
    int status = handlecbin(argv[1]);
    if (!status) {
        unsigned char out[2352];
        status = cdread_compressed(cdHandle, 0, out, atoi(argv[2])) == 2352 ? 0 : 2;
        if (!status) for (int i=0; i<2352; ++i) if (out[i] != atoi(argv[3])) { status=3; break; }
    }
    if (compr_img) { free(compr_img->index_table); free(compr_img); }
    fclose(cdHandle);
    return status ? 1 : 0;
}
'''


def image(plain=False, align=0, decoded=2352):
    blocks=[]
    for value in (17, 34, 51):
        raw=bytes([value])*decoded
        if plain: blocks.append(raw)
        else:
            encoder=zlib.compressobj(wbits=-15)
            blocks.append(encoder.compress(raw)+encoder.flush())
    step=1 << align
    start=(40+step-1)//step*step
    out=bytearray(start); index=[]
    for block in blocks:
        index.append((len(out) >> align) | (0x80000000 if plain else 0))
        out.extend(block); out.extend(bytes((-len(out)) % step))
    index.append(len(out) >> align)
    struct.pack_into('<4sIQIBB2x',out,0,b'CISO',24,3*2352,2352,1,align)
    struct.pack_into('<4I',out,24,*index)
    return out


def check(binary, directory):
    cases=0
    def run(data, sector, expected=0, valid=False, extension='cbin'):
        nonlocal cases
        path=directory/('fixture.'+extension);path.write_bytes(data)
        result=subprocess.run([str(binary),str(path),str(sector),str(expected)],capture_output=True,timeout=10)
        assert (result.returncode==0)==valid, (cases,result.returncode,result.stderr.decode())
        cases+=1
    for plain in (False, True):
        for align in (0, 4, 16):
            for ext in ('cbn','cbin','CBIN'):
                data=image(plain,align)
                for sector,value in enumerate((17,34,51)):run(data,sector,value,True,ext)
                for sector in (-1,3,4,48):run(data,sector)
    for offset,fmt,value in ((4,'I',8),(8,'Q',0),(8,'Q',2353),(8,'Q',1<<40),(16,'I',2048),(20,'B',2),(21,'B',63),(36,'I',0),(28,'I',0xffffffff)):
        data=image();struct.pack_into('<'+fmt,data,offset,value);run(data,2)
    for data in (image()[:36],image()[:-1],image(decoded=12),image(True,decoded=12)):
        run(data,2)
    print(f'Native CBIN reader: {cases} cases passed')


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--cc',default='cc')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='trainer-cbin-check-') as temp:
        directory=Path(temp)
        if args.source:
            code=directory/'reader.c';code.write_text(harness(args.source))
            source=args.source.resolve()
            subprocess.run([args.cc,'-O1','-g','-DUSE_MINIZ','-I'+str(source/'include'),
                            '-I'+str(source/'deps/libchdr/deps/miniz-3.1.2'),'-o',str(args.binary),str(code),
                            str(source/'deps/libchdr/deps/miniz-3.1.2/miniz.c')],check=True)
        check(args.binary.resolve(),directory)
