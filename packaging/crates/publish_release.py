#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One maintainer publication: GitHub assets, then signed catalogue revision.

No build executes here. Only already reviewed projects from the bundled catalogue
can be updated. Authentication belongs to gh/SSH and the external signing key.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import uuid
from catalogue import atomic,decode,verified
from publish_catalogue import publish


def main():
    p=argparse.ArgumentParser()
    p.add_argument('tag');p.add_argument('--record',type=Path,action='append',required=True)
    p.add_argument('--asset',type=Path,action='append',required=True)
    p.add_argument('--key',type=Path,required=True);p.add_argument('--ssh-target',required=True)
    p.add_argument('--remote-dir',default='traineros-crates/data')
    a=p.parse_args()
    if not re.fullmatch('[a-zA-Z0-9._-]+',a.tag) or not re.fullmatch('[a-zA-Z0-9._/-]+',a.remote_dir) or '..' in a.remote_dir:
        raise SystemExit('Invalid publication destination')
    root=Path(__file__).resolve().parents[2];pub=Path(__file__).with_name('catalogue-key.pem')
    def run(*args,**kwargs):return subprocess.run(args,check=True,**kwargs)
    previous=decode(run('ssh',a.ssh_target,'cat '+a.remote_dir+'/current.json',capture_output=True).stdout)
    value=verified(previous,pub);value=dict(value,revision=value['revision']+1)
    approved={r['id'] for r in json.loads((root/'data/crates/catalogue.json').read_text())['projects']}
    added=[]
    for record in a.record:
        r=decode(record.read_bytes())
        if r['id'] not in approved:raise SystemExit('Project has not been reviewed for this catalogue')
        if not r['url'].startswith('https://github.com/EriArk/TrainerOS/releases/download/'+a.tag+'/'):
            raise SystemExit('Record does not refer to this release')
        if any(x['id']==r['id'] and x['version']==r['version'] for x in value['packages']):
            raise SystemExit('Version is already published; choose a new version')
        archive=next((f for f in a.asset if f.name==r['url'].rsplit('/',1)[-1]),None)
        import hashlib
        if not archive or archive.stat().st_size!=r['size'] or hashlib.sha256(archive.read_bytes()).hexdigest()!=r['sha256']:
            raise SystemExit('Release asset does not match its record')
        added.append(r)
    value['packages']=added+value['packages']
    with tempfile.TemporaryDirectory() as directory:
        d=Path(directory);atomic(d/'before.json',previous);atomic(d/'input.json',value)
        publish(d/'input.json',a.key,d/'signed.json',d/'before.json',pub)
        exists=subprocess.run(['gh','release','view',a.tag,'--repo','EriArk/TrainerOS'],capture_output=True).returncode==0
        if not exists:
            notes=d/'notes.txt';notes.write_text('Experimental TrainerOS ARM64 example packages. Pinned upstream sources, package recipes and SHA-256 checksums are included. This is not a TrainerOS OS release.\n')
            run('gh','release','create',a.tag,'--repo','EriArk/TrainerOS','--prerelease','--latest=false','--title','Crates examples · '+a.tag,'--notes-file',str(notes),*[str(f) for f in a.asset])
        else:
            inventory=json.loads(run('gh','release','view',a.tag,'--repo','EriArk/TrainerOS','--json','assets',capture_output=True).stdout)
            existing={item['name'] for item in inventory['assets']}
            for asset in a.asset:
                if asset.name in existing:
                    verify=d/'verify';verify.mkdir(exist_ok=True)
                    run('gh','release','download',a.tag,'--repo','EriArk/TrainerOS','--pattern',asset.name,'--dir',str(verify))
                    if hashlib.sha256((verify/asset.name).read_bytes()).digest()!=hashlib.sha256(asset.read_bytes()).digest():
                        raise SystemExit('Existing release asset differs; never replace published bytes')
                else:run('gh','release','upload',a.tag,'--repo','EriArk/TrainerOS',str(asset))
        incoming='incoming-'+uuid.uuid4().hex+'.json'
        run('scp',str(d/'signed.json'),a.ssh_target+':'+a.remote_dir+'/'+incoming)
        # The revision comparison and replace happen under the server publication
        # lock. A concurrent publisher cannot silently overwrite a newer release.
        script='''from pathlib import Path
import fcntl,json,os,sys
p=Path(sys.argv[1]);expected=int(sys.argv[2]);incoming=p/sys.argv[3]
sys.path.insert(0,str(p.parent))
from catalogue import load
with (p/'publish.lock').open('a') as lock:
 fcntl.flock(lock,fcntl.LOCK_EX)
 old=load(p/'current.json');new=load(incoming)
 if old['signed']['revision']!=expected or new['signed']['revision']!=expected+1:raise SystemExit('Concurrent publication; retry from the current revision')
 target=p/(str(expected+1)+'.json')
 if target.exists():raise SystemExit('Revision already exists')
 target.write_bytes(incoming.read_bytes())
 with target.open('rb') as f:os.fsync(f.fileno())
 os.replace(incoming,p/'current.json')
 fd=os.open(p,os.O_RDONLY);os.fsync(fd);os.close(fd)
 for item in p.glob('*.json'):
  if item.stem.isdigit() and int(item.stem)<expected-30:item.unlink()
print('Published catalogue revision',expected+1)
'''
        run('ssh',a.ssh_target,'python3 - '+a.remote_dir+' '+str(previous['signed']['revision'])+' '+incoming,input=script.encode())


if __name__=='__main__':main()
