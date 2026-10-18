#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Signed, bounded Crates snapshots. Network work runs outside the GUI process."""
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import urllib.request

MAX_BYTES = 12 * 1024 * 1024
PUBLIC_KEY = Path(__file__).with_name('catalogue-key.pem')
ENDPOINT = 'https://crates.abysstail.art/v1/catalogue'
ID = re.compile(r'[a-z0-9][a-z0-9._-]{0,79}\Z')
HASH = re.compile(r'[0-9a-f]{64}\Z')


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True, allow_nan=False).encode()


def digest(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def decode(data):
    if len(data) > MAX_BYTES:
        raise ValueError('Catalogue exceeds size limit')
    def unique(pairs):
        out = {}
        for k, v in pairs:
            if k in out: raise ValueError('Duplicate catalogue key')
            out[k] = v
        return out
    return json.loads(data, object_pairs_hook=unique, parse_constant=lambda _: (_ for _ in ()).throw(ValueError('Invalid number')))


def atomic(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as f:
        temporary = Path(f.name)
        f.write(canonical(value)); f.flush(); os.fsync(f.fileno())
    try:
        os.replace(temporary, path)
        if os.name != 'nt':
            fd = os.open(path.parent, os.O_RDONLY)
            try: os.fsync(fd)
            finally: os.close(fd)
    finally:
        temporary.unlink(missing_ok=True)


def signature(payload, key, value=None):
    # Use the platform OpenSSL implementation, not a handwritten crypto primitive
    # or a new Python dependency on every handheld. Ed25519 signs the exact bytes.
    with tempfile.TemporaryDirectory() as d:
        p = Path(d); (p/'payload').write_bytes(canonical(payload))
        args = ['openssl', 'pkeyutl', '-rawin', '-inkey', str(key), '-in', str(p/'payload')]
        if value is None:
            result = subprocess.run(args+['-sign'], capture_output=True, timeout=10, check=True)
            return base64.b64encode(result.stdout).decode()
        raw = base64.b64decode(value, validate=True)
        if len(raw) != 64: raise ValueError('Invalid catalogue signature')
        (p/'signature').write_bytes(raw)
        result = subprocess.run(args+['-verify', '-pubin', '-sigfile', str(p/'signature')], capture_output=True, timeout=10)
        if result.returncode: raise ValueError('Catalogue signature does not match')


def validate(snapshot):
    if snapshot.get('schema') != 1 or type(snapshot.get('revision')) is not int or not 0 < snapshot['revision'] < 2**53:
        raise ValueError('Unsupported catalogue revision')
    if set(snapshot) != {'schema', 'revision', 'crates', 'catalogue', 'packages', 'assets'}:
        raise ValueError('Unsupported catalogue fields')
    crates = snapshot['crates']
    if not isinstance(crates, list) or len(crates) != 6 or [c.get('id') for c in crates[:2]] != ['whats-new', 'workshop']:
        raise ValueError('Invalid showcase layout')
    ids = set()
    for c in crates:
        if not ID.fullmatch(c['id']) or c['id'] in ids: raise ValueError('Invalid crate identity')
        ids.add(c['id'])
        if set(c) != {'id','title','subtitle','art','color','lid','icon'}: raise ValueError('Invalid crate fields')
        if type(c['art']) is not int or not 0 <= c['art'] <= 5: raise ValueError('Unknown crate illustration')
        for field in ('title','subtitle','icon'):
            if not isinstance(c[field],str) or not 0 < len(c[field]) <= 160: raise ValueError('Invalid crate text')
        for field in ('color','lid'):
            if not re.fullmatch('#[0-9a-fA-F]{6}', c[field]): raise ValueError('Invalid crate colour')
    assets = snapshot['assets']
    if not isinstance(assets,dict) or len(assets) > 128: raise ValueError('Too many catalogue images')
    total = 0
    for key, value in assets.items():
        if not HASH.fullmatch(key): raise ValueError('Invalid image identity')
        raw = base64.b64decode(value, validate=True); total += len(raw)
        if len(raw)>1024*1024 or total>8*1024*1024 or hashlib.sha256(raw).hexdigest()!=key:
            raise ValueError('Catalogue image integrity failure')
        if raw[:8]!=b'\x89PNG\r\n\x1a\n' or raw[12:16]!=b'IHDR' or len(raw)<33:
            raise ValueError('Only bounded PNG artwork is accepted')
        w,h=struct.unpack('>II',raw[16:24])
        if not 0<w<=2048 or not 0<h<=2048: raise ValueError('Catalogue image dimensions exceed limit')
    cat=snapshot['catalogue']
    if set(cat)!={'schema','projects','contents'} or cat['schema']!=1 or len(canonical(cat))>2*1024*1024:
        raise ValueError('Invalid project catalogue')
    if not isinstance(cat['projects'],list) or len(cat['projects'])>1000: raise ValueError('Too many projects')
    projects=set()
    for p in cat['projects']:
        if not ID.fullmatch(p['id']) or p['id'] in projects or p['kind'] not in ('game','app','module'):
            raise ValueError('Invalid project identity')
        projects.add(p['id'])
        if set(p)-{'id','title','kind','license','sample','creator','summary','description','hero','screenshots','releases'}:
            raise ValueError('Unsupported project fields')
        for field,limit in [('title',160),('creator',160),('summary',300),('description',12000),('license',80)]:
            if not isinstance(p.get(field,''),str) or len(p.get(field,''))>limit: raise ValueError('Invalid project text')
        if not p['title'] or len(p.get('screenshots',[]))>8 or len(p.get('releases',[]))>64: raise ValueError('Invalid project presentation')
        for art in [p.get('hero','')]+p.get('screenshots',[]):
            if not art: continue
            if art.startswith('asset:') and art[6:] in assets: continue
            if not re.fullmatch(r'qrc:/(?:crates|series)/[a-zA-Z0-9_./-]+',art) or '..' in art:
                raise ValueError('Unapproved catalogue image source')
    if not isinstance(cat['contents'],dict) or set(cat['contents'])-ids: raise ValueError('Unknown crate contents')
    for members in cat['contents'].values():
        if not isinstance(members,list) or len(members)>1000 or len(set(members))!=len(members) or set(members)-projects:
            raise ValueError('Invalid crate membership')
    from installer import validate_records
    validate_records(snapshot['packages'])
    for r in snapshot['packages']:
        if r['id'] not in projects: raise ValueError('Release refers to an unknown project')
        if not re.fullmatch('[0-9a-f]{40}',r.get('revision','')) or not r.get('license'):
            raise ValueError('Release provenance is required')
        if not r.get('repository','').startswith('https://github.com/'):
            raise ValueError('Source repository is required')
    return snapshot


def verified(envelope, public_key=PUBLIC_KEY):
    if set(envelope)!={'signed','signature'}: raise ValueError('Invalid signed catalogue')
    signature(envelope['signed'],public_key,envelope['signature'])
    return validate(envelope['signed'])


def load(path, public_key=PUBLIC_KEY):
    with Path(path).open('rb') as f: envelope=decode(f.read(MAX_BYTES+1))
    verified(envelope,public_key)
    return envelope


def delta(old, new):
    a,b=old['signed'],new['signed']
    if a['revision']==b['revision'] and digest(a)==digest(b):
        return {'kind':'unchanged','revision':b['revision'],'digest':digest(b)}
    before={p['id']:p for p in a['catalogue']['projects']}
    after={p['id']:p for p in b['catalogue']['projects']}
    patch={k:b[k] for k in ('crates','packages') if a[k]!=b[k]}
    patch['projects']={'upsert':[p for k,p in after.items() if before.get(k)!=p], 'remove':sorted(before.keys()-after.keys()), 'order':list(after)}
    patch['contents']={k:v for k,v in b['catalogue']['contents'].items() if a['catalogue']['contents'].get(k)!=v}
    patch['removedContents']=sorted(a['catalogue']['contents'].keys()-b['catalogue']['contents'].keys())
    patch['assets']={k:v for k,v in b['assets'].items() if k not in a['assets']}
    patch['removedAssets']=sorted(a['assets'].keys()-b['assets'].keys())
    return {'kind':'delta','baseRevision':a['revision'],'baseDigest':digest(a), 'revision':b['revision'],'patch':patch,'signature':new['signature']}


def apply(response, previous, public_key=PUBLIC_KEY):
    kind=response.get('kind')
    if kind=='unchanged':
        if not previous or response['revision']!=previous['signed']['revision'] or response['digest']!=digest(previous['signed']):
            raise ValueError('Unchanged response conflicts with cache')
        return previous
    if kind=='snapshot': result=response['envelope']
    elif kind=='delta':
        if not previous or response['baseRevision']!=previous['signed']['revision'] or response['baseDigest']!=digest(previous['signed']):
            raise ValueError('Delta base conflicts with cache')
        s=copy.deepcopy(previous['signed']); p=response['patch']; s['revision']=response['revision']
        for k in ('crates','packages'):
            if k in p: s[k]=p[k]
        projects={r['id']:r for r in s['catalogue']['projects']}
        for k in p['projects']['remove']: projects.pop(k,None)
        for r in p['projects']['upsert']: projects[r['id']]=r
        s['catalogue']['projects']=[projects[k] for k in p['projects']['order']]
        s['catalogue']['contents'].update(p['contents'])
        for k in p['removedContents']: s['catalogue']['contents'].pop(k,None)
        s['assets'].update(p['assets'])
        for k in p['removedAssets']: s['assets'].pop(k,None)
        result={'signed':s,'signature':response['signature']}
    else: raise ValueError('Unsupported catalogue response')
    verified(result,public_key)
    if previous:
        a,b=previous['signed'],result['signed']
        if b['revision']<a['revision'] or (b['revision']==a['revision'] and digest(a)!=digest(b)):
            raise ValueError('Catalogue rollback or revision conflict')
    return result


def materialize(cache, envelope):
    cache=Path(cache); assets=cache/'assets'; assets.mkdir(parents=True,exist_ok=True)
    if cache.is_symlink() or assets.is_symlink(): raise ValueError('Unsafe catalogue cache')
    for key,value in envelope['signed']['assets'].items():
        target=assets/(key+'.png'); raw=base64.b64decode(value)
        if target.is_symlink(): raise ValueError('Unsafe catalogue image cache')
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=key:
            temporary=assets/(key+'.pending'); temporary.write_bytes(raw); temporary.replace(target)
    atomic(cache/'current.json',envelope)
    # At most the current bounded artwork set is retained. No package/data cleanup.
    for path in assets.glob('*.png'):
        if HASH.fullmatch(path.stem) and path.stem not in envelope['signed']['assets']: path.unlink()


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args): raise ValueError('Catalogue redirects are not permitted')


def sync(cache, endpoint=ENDPOINT, public_key=PUBLIC_KEY, fetch=None):
    cache=Path(cache)
    # Serialize concurrent clients before reading the base revision.
    import fcntl
    cache.mkdir(parents=True,exist_ok=True)
    with (cache/'sync.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        previous=None
        if (cache/'current.json').exists():
            try: previous=load(cache/'current.json',public_key)
            except (ValueError,KeyError,TypeError): pass  # One full resync, never trust a damaged cursor.
        cursor=str(previous['signed']['revision']) if previous else '0'
        query='?revision='+cursor+'&digest='+(digest(previous['signed']) if previous else '')
        if fetch is None:
            request=urllib.request.Request(endpoint+query,headers={'User-Agent':'TrainerOS-Crates/1','Accept':'application/json'})
            with urllib.request.build_opener(NoRedirect()).open(request,timeout=12) as response:
                response=decode(response.read(MAX_BYTES+1))
        else: response=fetch(query)
        envelope=apply(response,previous,public_key)
        if envelope!=previous: materialize(cache,envelope)
        return envelope,response['kind']


def main():
    cache=Path(sys.argv[2]); mode=sys.argv[1]
    try:
        if mode=='sync': envelope,result=sync(cache)
        elif mode=='load': envelope,result=load(cache/'current.json'),'cached'
        else: raise ValueError('Unknown catalogue operation')
        s=envelope['signed']
        print(json.dumps({'revision':s['revision'],'result':result,'crates':s['crates'], 'catalogue':s['catalogue'],'packages':s['packages']}))
    except Exception as e:
        print(json.dumps({'error':str(e) or type(e).__name__}))
        return 1
    return 0


if __name__=='__main__': sys.exit(main())
