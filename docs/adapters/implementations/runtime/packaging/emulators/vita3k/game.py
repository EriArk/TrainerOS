# SPDX-License-Identifier: GPL-3.0-or-later
"""Installed Vita identity and private settings; never semantic save access."""
import hashlib
import json
import os
from pathlib import Path
import re
import struct

SOURCE='46602447c7f84d53790b44e1869ab5105178bf16'
SETTINGS='native-lan-two-owned-v1'


def digest(path,limit=8*1024**3):
    path=Path(path)
    with path.open('rb') as stream:
        before=os.fstat(stream.fileno())
        if not 0<=before.st_size<=limit:raise ValueError('file bounds')
        value=hashlib.file_digest(stream,'sha256').hexdigest()
        after=os.fstat(stream.fileno())
    if (before.st_size,before.st_mtime_ns,before.st_ino)!=(after.st_size,after.st_mtime_ns,after.st_ino):
        raise ValueError('content changed while checking')
    return value


def title_id(link):
    path=Path(link)
    if path.suffix.lower()!='.psvita' or not path.is_file() or not 9<=path.stat().st_size<=64:
        raise ValueError('installed Vita link')
    value=path.read_text(encoding='utf-8-sig').strip()
    if not re.fullmatch('[A-Z0-9]{9}',value):raise ValueError('installed Vita title')
    return value


def sfo_title(path):
    path=Path(path)
    if not path.is_file() or not 20<=path.stat().st_size<=65536:raise ValueError('title metadata')
    data=path.read_bytes()
    magic,version,keys,values,count=struct.unpack_from('<5I',data)
    if magic!=0x46535000 or count>128 or 20+16*count>keys or not keys<values<=len(data):
        raise ValueError('title metadata header')
    found=[]
    for index in range(count):
        key,kind,size,maximum,offset=struct.unpack_from('<HHIII',data,20+16*index)
        begin=keys+key
        if not keys<=begin<values or size>maximum or values+offset+size>len(data):
            raise ValueError('title metadata entry')
        end=data.find(b'\0',begin,values)
        if end<0:raise ValueError('title metadata key')
        if data[begin:end]==b'TITLE_ID':
            if kind!=0x204 or size!=10:raise ValueError('title metadata identifier')
            raw=data[values+offset:values+offset+size]
            if raw[-1:]!=b'\0':raise ValueError('title metadata terminator')
            found.append(raw[:-1].decode('ascii'))
    if len(found)!=1 or not re.fullmatch('[A-Z0-9]{9}',found[0]):raise ValueError('ambiguous title identifier')
    return found[0]


def owned_settings():
    import yaml
    home=Path.home()
    config=Path(os.environ.get('XDG_CONFIG_HOME',home/'.config'))/'Vita3K/config.yml'
    if config.exists():
        if config.stat().st_size>131072:raise ValueError('native settings bounds')
        value=yaml.safe_load(config.read_text())
        if not isinstance(value,dict):raise ValueError('native settings')
    else:value={}
    default=Path(os.environ.get('XDG_DATA_HOME',home/'.local/share'))/'Vita3K/Vita3K'
    pref=value.get('pref-path',str(default))
    if not isinstance(pref,str) or not Path(pref).is_absolute() or not Path(pref).is_dir():
        raise ValueError('own Vita data unavailable')
    return value,Path(pref).resolve()


def bundle(root):
    root=Path(root).resolve();path=root/'manifest.json'
    if path.is_symlink() or not path.is_file() or path.stat().st_size>262144:raise ValueError('runtime manifest')
    value=json.loads(path.read_text())
    if value.get('protocol')!=1 or value.get('source')!=SOURCE or value.get('settings')!=SETTINGS:
        raise ValueError('runtime version')
    files=value.get('files')
    required={'runtime/usr/bin/Vita3K','transport/game.py','transport/launcher.py',
        'transport/network-session.py','transport/network-worker.py','transport/packets.py',
        'transport/secure-channel.py','transport/profiles.json','transport/requirements.lock','transport/native_log.py'}
    if not isinstance(files,dict) or not required.issubset(files) or not len(required)<=len(files)<=1024:
        raise ValueError('runtime file list')
    identity=hashlib.sha256()
    for name in sorted(files):
        expected=files[name]
        if (not isinstance(name,str) or not name or '\\' in name or Path(name).is_absolute()
            or any(part in ('..','.') for part in name.split('/')) or not re.fullmatch('[a-f0-9]{64}',str(expected))):
            raise ValueError('runtime file entry')
        path=root/name
        if not path.resolve().is_relative_to(root) or digest(path,512*1024**2)!=expected:
            raise ValueError('runtime changed')
        identity.update(name.encode()+b'\0'+expected.encode())
    python=root/'transport/venv/bin/python'
    expected=value.get('interpreterSha256')
    if not re.fullmatch('[a-f0-9]{64}',str(expected)) or digest(python,64*1024**2)!=expected:
        raise ValueError('runtime interpreter changed')
    identity.update(expected.encode()+SETTINGS.encode())
    return identity.hexdigest()


def installed_app(link,data):
    title=title_id(link);app=data/'ux0/app'/title
    if not app.is_dir() or not (app/'eboot.bin').is_file() or sfo_title(app/'sce_sys/param.sfo')!=title:
        raise ValueError('installed game does not match its link')
    return title,app


def identity(root,link):
    root=Path(root)
    runtime=bundle(root)
    settings,data=owned_settings()
    title,app=installed_app(link,data)
    profiles=json.loads((root/'transport/profiles.json').read_text())
    profile=profiles.get(title)
    if not isinstance(profile,dict) or profile.get('ebootSha256')!=digest(app/'eboot.bin',512*1024**2):
        raise ValueError('no reviewed native LAN profile')
    result=hashlib.sha256();count=0
    def add_tree(path,label,extensions=None):
        nonlocal count
        path=Path(path)
        if not path.exists():result.update(label.encode()+b'\0absent\0');return
        base=path.resolve()
        for file in sorted(path.rglob('*')):
            if file.is_symlink() and not file.resolve().is_relative_to(base):raise ValueError('content link escapes its game directory')
            if file.is_symlink() and file.is_dir():
                result.update((label+'/'+file.relative_to(path).as_posix()).encode()+b'\0link\0'+file.resolve().relative_to(base).as_posix().encode()+b'\0')
            if not file.is_file() or extensions and file.suffix.lower() not in extensions:continue
            count+=1
            if count>50000:raise ValueError('game file count')
            relative=label+'/'+file.relative_to(path).as_posix()
            result.update(relative.encode()+b'\0'+digest(file).encode()+b'\0')
    add_tree(app,'app')
    add_tree(data/'ux0/patch'/title,'patch')
    add_tree(data/'ux0/addcont'/title,'addcont')
    for extra in profile.get('extraContent',[]):
        relative=Path(extra['path'])
        if relative.is_absolute() or '..' in relative.parts:raise ValueError('profile content directory')
        directory=data/relative
        for name in extra.get('required',[]):
            file=directory/name
            if not file.resolve().is_relative_to(directory.resolve()) or not file.is_file():
                raise ValueError('required game data missing')
        add_tree(directory,extra['path'],extra['extensions'])
    return {'id':'runtime.vita3k.lan.v1','content':result.hexdigest(),'runtime':runtime,
        'settings':SETTINGS,'players':2,'lateJoin':False,'temporaryProgress':False}


def prepare_settings(root,link,user,network=False):
    import yaml
    settings,data=owned_settings()
    title,app=installed_app(link,data)
    profiles=json.loads((Path(root)/'transport/profiles.json').read_text())
    profile=profiles.get(title,{})
    # Presentation overrides apply only to the exact reviewed executable. Other
    # ordinary Vita games retain their graphics/audio/own-account preferences.
    if profile.get('ebootSha256')==digest(app/'eboot.bin',512*1024**2):
        settings.update(profile.get('nativeSettings',{}))
    settings.update({'pref-path':str(data),'show-welcome':False,'initial-setup':True,
        'check-for-updates-mode':0,'discord-rich-presence':False,'log-level':3})
    if network:settings['adhoc-addr']=0
    bindings=settings.get('controller-binds',list(range(15)))
    if isinstance(bindings,list) and len(bindings)==15:
        bindings=list(bindings);bindings[5]=-1;settings['controller-binds']=bindings
    settings['keyboard-button-psbutton']='Unbound'
    settings['keyboard-button-psbutton-alt']='Unbound'
    user=Path(user);config=user/'config/Vita3K/config.yml'
    config.parent.mkdir(parents=True)
    with config.open('x') as stream:
        os.chmod(config,0o600)
        yaml.safe_dump(settings,stream,sort_keys=False,allow_unicode=True)
    return title
