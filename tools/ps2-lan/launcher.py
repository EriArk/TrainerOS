#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Trusted installed runtime preparation; no networking/consent ownership."""
import hashlib
import json
import os
import shutil
from pathlib import Path
import sys
from disc import identity as disc_identity
from session_config import prepare, connection, launch_settings

ROOT=Path(__file__).resolve().parent
RUNTIMES={
    '5be49fd2dbf9d1a0d181707e0968db3c4b1985891493a67038d21ae9c6631166',
    '1c40028a0464bcfab57826518500dcd11ceb472bea16d0900d7146abb11a4224',
}


def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def manifest():
    data=json.loads((ROOT/'manifest.json').read_text())
    if data.get('protocol')!=1 or data.get('settings')!='eclx1-namespace-own-cards-v1':
        raise ValueError('Unreviewed PS2 package')
    runtime=Path(data['runtime']);config=Path(data['configRoot'])
    if not runtime.is_absolute() or not config.is_absolute() or not (config/'inis/PCSX2.ini').is_file():
        raise ValueError('PS2 ordinary installation unavailable')
    if data['runtimeSha256'] not in RUNTIMES or digest(runtime)!=data['runtimeSha256']:
        raise ValueError('PS2 runtime changed')
    return data


def identify(path):
    if not path.is_absolute():raise ValueError('Absolute content path required')
    disc=disc_identity(path,ROOT/'libchdr.so')
    profiles=json.loads((ROOT/'profiles.json').read_text())['games']
    matches=[p for p in profiles if p['serial']==disc['serial'] and p['crc']==disc['crc'] and p['mode']=='lan']
    if len(matches)!=1:return {}
    setup=manifest()
    content=disc['content'] or digest(path)
    return {'id':'runtime.armsx2.lan.v1','content':content,'serial':disc['serial'],
            'crc':disc['crc'],'runtime':setup['runtimeSha256'],'settings':setup['settings'],
            'players':2,'lateJoin':False,'temporaryProgress':False,'connections':['online']}


def stable(value):
    return {k:v for k,v in value.items() if k not in ('label','runtime','connections','controllerModes')}


def request(path):
    if not path.is_absolute() or path.is_symlink() or path.stat().st_size>16384:
        raise ValueError('Invalid PS2 request')
    data=json.loads(path.read_text())
    if set(data)!={'content','expected','connection','host'} or type(data['host']) is not bool:
        raise ValueError('Invalid PS2 request fields')
    return data


def prepare_request(path):
    data=request(path);current=identify(Path(data['content']))
    if not current or stable(current)!=stable(data['expected']):raise ValueError('PS2 game changed')
    config=Path(data['connection'])
    if not config.is_absolute() or config.is_symlink() or config.stat().st_size>4096 or config.stat().st_mode&0o077:
        raise ValueError('Invalid private PS2 connection')
    material=config.read_bytes();values=connection(material)
    if (values['host']=='true')!=data['host'] or values['socket']!=str(config.parent/'ps2.sock'):
        raise ValueError('PS2 connection changed')
    setup=manifest();game_dir=Path(setup['configRoot'])/'gamesettings'
    source=game_dir/(current['serial']+'_'+current['crc']+'.ini')
    if not source.exists():source=game_dir/(current['crc']+'.ini')
    prepare(source if source.exists() else None,path.parent/'game.ini',material)
    # Mount a private settings directory at the ordinary path. The same absolute
    # BIOS/card/input locations remain valid; native startup/settings writes stay
    # private. In particular -batch must not enumerate unavailable NFS libraries.
    inis=path.parent/'inis';inis.mkdir(mode=0o700)
    original=Path(setup['configRoot'])/'inis'
    entries=list(original.iterdir())
    if len(entries)>128:raise ValueError('Oversized PS2 settings directory')
    for entry in entries:
        if entry.is_dir():continue
        if entry.is_symlink() or not entry.is_file() or entry.stat().st_size>1024*1024:
            raise ValueError('Unsupported PS2 settings file')
        target=inis/entry.name;shutil.copyfile(entry,target);target.chmod(0o600)
    base=inis/'PCSX2.ini';base.write_bytes(launch_settings(base.read_bytes()))
    return {'ready':True}


def run(path):
    data=request(path);setup=manifest()
    if not (path.parent/'game.ini').is_file():raise ValueError('PS2 preparation missing')
    # No network interface or route is shared with the game. Files, input, GPU
    # and the owner's original data root remain the same ordinary installation.
    args=['/usr/bin/bwrap','--die-with-parent','--unshare-net','--bind','/','/',
          '--dev-bind','/dev','/dev',
          '--bind',str(path.parent/'inis'),str(Path(setup['configRoot'])/'inis'),
          '--proc','/proc','--',str(ROOT/'ps2_endpoint'),data['connection'],setup['runtime'],
          '-batch','-fullscreen',
          '-gamecfg',str(path.parent/'game.ini'),'-logfile',str(path.parent/'native.log'),
          '--',data['content']]
    # ARMSX2 appends its application folder to the base configuration directory.
    # Passing the already-expanded root to -datapath creates a fresh nested setup.
    if Path(setup['configRoot']).name!='ARMSX2':raise ValueError('Unsupported PS2 data root')
    env=dict(os.environ,APPIMAGE_EXTRACT_AND_RUN='1',QT_QPA_PLATFORM='xcb',
             XDG_CONFIG_HOME=str(Path(setup['configRoot']).parent))
    os.execvpe(args[0],args,env)


if __name__=='__main__':
    try:
        mode,target=sys.argv[1:];path=Path(target)
        if mode=='identity':result=identify(path)
        elif mode=='prepare':result=prepare_request(path)
        elif mode=='run':run(path);raise RuntimeError('exec failed')
        else:raise ValueError('Unknown PS2 operation')
        print(json.dumps(result,separators=(',',':')))
    except (ValueError,OSError,KeyError,TypeError):
        # Neither personal paths nor session material belong in shell logs.
        sys.exit(1)
