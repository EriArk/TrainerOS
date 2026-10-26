#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read native MAME-set dependencies and stage a private Flycast session.

libarchive reads ZIP/7z; Flycast remains the cartridge/GD-ROM decoder and GGPO
implementation. Never extract archive entries or copy personal EEPROM/VMU data.
"""
import ctypes as C
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import sys
import zlib

SOURCE='d0e47e572b1e7b355e88bda8308c89d0c5156cbf'
LIMIT=2*1024**3


class Invalid(ValueError):pass


def safe_name(value):
    return isinstance(value,str) and bool(re.fullmatch(r'[A-Za-z0-9_.-]+',value)) and value not in ('.','..')


def digest(path):
    if not path.is_file() or not 0<path.stat().st_size<=LIMIT:raise Invalid('Unreadable arcade content')
    before=path.stat()
    with path.open('rb') as f:value=hashlib.file_digest(f,'sha256').hexdigest()
    after=path.stat()
    if (before.st_ino,before.st_size,before.st_mtime_ns)!=(after.st_ino,after.st_size,after.st_mtime_ns):
        raise Invalid('Arcade content changed')
    return value


def archive(path):
    lib=C.CDLL(ctypes.util.find_library('archive'))
    signatures={
        'archive_read_new':(C.c_void_p,[]),
        'archive_read_support_format_zip':(C.c_int,[C.c_void_p]),
        'archive_read_support_format_7zip':(C.c_int,[C.c_void_p]),
        'archive_read_open_filename':(C.c_int,[C.c_void_p,C.c_char_p,C.c_size_t]),
        'archive_read_next_header':(C.c_int,[C.c_void_p,C.POINTER(C.c_void_p)]),
        'archive_entry_pathname_utf8':(C.c_char_p,[C.c_void_p]),
        'archive_entry_filetype':(C.c_uint,[C.c_void_p]),
        'archive_entry_size':(C.c_int64,[C.c_void_p]),
        'archive_entry_is_encrypted':(C.c_int,[C.c_void_p]),
        'archive_read_data':(C.c_ssize_t,[C.c_void_p,C.c_void_p,C.c_size_t]),
        'archive_read_free':(C.c_int,[C.c_void_p])}
    for name,(result,args) in signatures.items():
        f=getattr(lib,name);f.restype=result;f.argtypes=args
    handle=lib.archive_read_new();entries=[];total=0
    if not handle:raise Invalid('Archive reader unavailable')
    try:
        lib.archive_read_support_format_zip(handle);lib.archive_read_support_format_7zip(handle)
        if lib.archive_read_open_filename(handle,os.fsencode(path),65536)!=0:raise Invalid('Unreadable arcade archive')
        entry=C.c_void_p();buffer=C.create_string_buffer(1024*1024);seen=set()
        while True:
            result=lib.archive_read_next_header(handle,C.byref(entry))
            if result==1:break
            if result!=0 or len(seen)>=1024:raise Invalid('Invalid arcade archive')
            raw=lib.archive_entry_pathname_utf8(entry)
            if not raw:raise Invalid('Invalid archive name')
            name=raw.decode('utf-8');parts=name.replace('\\','/').split('/')
            if name.startswith('/') or ':' in name or '..' in parts or len(name)>512 or name in seen:
                raise Invalid('Ambiguous archive entry')
            seen.add(name);kind=lib.archive_entry_filetype(entry)
            if kind==0o040000:continue
            size=lib.archive_entry_size(entry)
            if kind!=0o100000 or lib.archive_entry_is_encrypted(entry) or size<0 or size>512*1024**2:
                raise Invalid('Unsupported archive entry')
            count=0;crc=0;sha=hashlib.sha256()
            while True:
                got=lib.archive_read_data(handle,buffer,len(buffer))
                if got<0:raise Invalid('Corrupt arcade archive')
                if got==0:break
                count+=got;total+=got
                if count>size or total>LIMIT:raise Invalid('Arcade archive is too large')
                data=buffer.raw[:got];crc=zlib.crc32(data,crc);sha.update(data)
            if count!=size:raise Invalid('Truncated arcade entry')
            entries.append({'name':name,'length':count,'crc':crc,'sha256':sha.hexdigest()})
    finally:lib.archive_read_free(handle)
    if not entries:raise Invalid('Empty arcade archive')
    return entries


def archive_identity(entries):
    return hashlib.sha256(json.dumps(sorted(entries,key=lambda e:e['name']),sort_keys=True,separators=(',',':')).encode()).hexdigest()


def inspect(request):
    observed=[];files=[];hashes={};archives={}
    def watch(path):
        value=str(path)
        if value not in observed:observed.append(value)
    def add(path,relative):
        watch(path)
        value=digest(path)
        if str(path) not in hashes:
            hashes[str(path)]=value;files.append({'source':str(path),'relative':relative,'sha256':value})
        return value
    def read(path,relative):
        add(path,relative)
        if str(path) not in archives:archives[str(path)]=archive(path)
        if digest(path)!=hashes[str(path)]:raise Invalid('Archive changed while reading')
        return archives[str(path)]
    def choose(folder,name,bios=False):
        if not safe_name(name):raise Invalid('Invalid native dependency name')
        for suffix in (('.zip','.7z') if bios else ('.7z','.7Z','.zip','.ZIP')):
            path=folder/(name+suffix);watch(path)
            if path.exists():return path
        return None
    def selected(blob,sets):
        if blob['kind']=='Copy':return None
        found=None
        if blob['crc']:
            found=next((e for group in sets for e in group if e['crc']==blob['crc']),None)
        if found is None:found=next((e for group in sets for e in group if e['name']==blob['name']),None)
        return found
    def available(blob,sets):
        if blob['kind']=='Copy':return True
        found=selected(blob,sets)
        return (found is None and blob['kind']=='Eeprom') or (found is not None and found['length']>=blob['length'])
    def selected_identity(blobs,sets):
        return [[b['kind'],b['length'],entry['sha256'] if (entry:=selected(b,sets)) else None] for b in blobs]
    try:
        source=Path(request['source']);bundle=Path(request['bundle']);platform=request['platform']
        if not source.is_absolute() or not bundle.is_absolute() or source.suffix.lower() not in ('.zip','.7z'):
            raise Invalid('Use a native ZIP/7z arcade set')
        catalog=bundle/'arcade-catalog.json';watch(catalog)
        data=json.loads(catalog.read_text())
        if data.get('protocol')!=1 or data.get('source')!=SOURCE:raise Invalid('Restore the arcade catalog')
        game=data['games'].get(source.stem)
        if not game or game['platform']!=platform:raise Invalid('Not a native set for this arcade platform')
        child=read(source,'roms/'+source.name);sets=[child]
        parent=choose(source.parent,game['parent']) if game['parent'] else None
        if parent:sets.append(read(parent,'roms/'+parent.name))
        # Use the owner's installed firmware copy. An embedded BIOS in a merged
        # set remains valid; no firmware download or title-specific allowlist.
        firmware=choose(bundle/'firmware',game['bios'],True)
        bios_sets=sets+[read(firmware,'data/'+firmware.name)] if firmware else sets
        definitions=data['bios'][game['bios']]
        regions=list(dict.fromkeys([1]+[b['region'] for b in definitions]))
        region=next((r for r in regions if any(b['region']==r for b in definitions)
                     and all(available(b,bios_sets) for b in definitions if b['region']==r)),None)
        if region is None:raise Invalid('Required arcade BIOS is missing or incomplete')
        if not all(available(b,sets) for b in game['blobs']):raise Invalid('Arcade set or parent archive is incomplete')
        if game['disc']:
            disc_name=game['disc']
            if not safe_name(disc_name):raise Invalid('Invalid native disc name')
            disc=None
            for name in [source.stem]+([game['parent']] if game['parent'] else []):
                for suffix in ('.chd','.gdi'):
                    candidate=source.parent/name/(disc_name+suffix);watch(candidate)
                    if candidate.exists():disc=candidate;break
                if disc:break
            if disc is None:raise Invalid('Required arcade GD-ROM is missing')
            relative='roms/'+disc.parent.name+'/'+disc.name
            add(disc,relative)
            if disc.suffix=='.chd':
                with disc.open('rb') as f:header=f.read(16)
                if header!=bytes.fromhex('4d436f6d707248440000007c00000005'):raise Invalid('Unsupported CHD header')
            else:
                if disc.stat().st_size>8192:raise Invalid('Invalid GDI layout')
                rows=disc.read_text(encoding='utf-8').splitlines();count=int(rows[0]);tracks=[]
                if not 3<=count<=99:raise Invalid('Invalid GDI track count')
                for row in rows[1:]:
                    if not row.strip():continue
                    fields=shlex.split(row)
                    if len(fields)!=6:raise Invalid('Invalid GDI track')
                    number,lba,ctrl,sector=int(fields[0]),int(fields[1]),int(fields[2]),int(fields[3])
                    name,offset=fields[4],int(fields[5])
                    if number!=len(tracks)+1 or not safe_name(name) or sector not in (2048,2352) or not 0<=ctrl<=15 or not 0<=lba<=0xffffff or offset<0:
                        raise Invalid('Invalid GDI track')
                    if tracks and lba<=tracks[-1]:raise Invalid('Invalid GDI track order')
                    path=disc.parent/name;watch(path)
                    size=path.stat().st_size
                    if offset>=size or (size-offset)%sector:raise Invalid('Truncated GDI track')
                    add(path,'roms/'+disc.parent.name+'/'+name);tracks.append(lba)
                if len(tracks)!=count:raise Invalid('Incomplete GDI layout')
        value={'protocol':1,'game':source.stem,'platform':platform,'region':region,'observed':observed,'files':files}
        # Identity follows the native selected data, not ZIP timestamps, codec,
        # optional parent archives or unrelated BIOS files in a merged archive.
        payload={'game':source.stem,'platform':platform,'region':region,
                 'roms':selected_identity(game['blobs'],sets),
                 'bios':selected_identity([b for b in definitions if b['region']==region],bios_sets),
                 'discs':[[v['relative'],v['sha256']] for v in files if v['source'] not in archives]}
        value['content']=hashlib.sha256(json.dumps(payload,separators=(',',':'),sort_keys=True).encode()).hexdigest()
        return value
    except (ValueError,OSError,KeyError,TypeError,UnicodeError) as error:
        raise Invalid(str(error),observed) from error


def prepare(request):
    value=inspect(request)
    if value['content']!=request.get('expected'):raise Invalid('Arcade content changed; choose the game again')
    stage=Path(request['stage'])
    if not stage.is_absolute() or stage.is_symlink() or stage.resolve()!=stage or not stage.is_dir():raise Invalid('Invalid private stage')
    for directory in ('roms','data'):
        if (stage/directory).exists():raise Invalid('Arcade stage is already in use')
        (stage/directory).mkdir()
    for item in value['files']:
        target=stage/item['relative'];target.parent.mkdir(parents=True,exist_ok=True)
        # Archives/discs are read-only inputs. Native EEPROM/NVRAM uses separate
        # files in this private stage; personal progress is never imported.
        target.symlink_to(Path(item['source']).resolve())
    value['entry']=str(stage/'roms'/Path(request['source']).name)
    return value


if __name__=='__main__':
    os.umask(0o077)
    if len(sys.argv)!=3 or sys.argv[1] not in ('inspect','prepare'):sys.exit(2)
    try:
        path=Path(sys.argv[2])
        if not path.is_absolute() or path.is_symlink() or path.stat().st_size>65536:raise Invalid('Invalid request')
        request=json.loads(path.read_text())
        print(json.dumps(inspect(request) if sys.argv[1]=='inspect' else prepare(request),separators=(',',':')))
    except (ValueError,OSError,KeyError,TypeError) as error:
        print(json.dumps({'error':error.args[0] if error.args else 'Arcade preparation failed',
                          'observed':error.args[1] if len(error.args)>1 else []}));sys.exit(3)
