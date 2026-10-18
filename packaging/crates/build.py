#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Maintainer build recipe; never executed by the handheld installer."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import ports

recipe_root=Path(__file__).resolve().parent
name=sys.argv[1]
recipe=json.loads((recipe_root/'recipes'/(name+'.json')).read_text())
out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=True)
source=out/(name+'-source');build=out/(name+'-build');stage=out/(name+'-stage')
def run(*args,**kwargs): subprocess.run(args,check=True,**kwargs)
if not source.exists(): run('git','clone',recipe['repository'],str(source))
run('git','-C',str(source),'checkout','--detach',recipe['revision'])
if stage.exists(): raise SystemExit('Choose a fresh output directory for packaging')
stage.mkdir();(stage/'bin').mkdir()
if name in ('2048','hextris','breakout'):
    run('cmake','-S',str(recipe_root/'webgame'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release')
    run('cmake','--build',str(build),'-j4')
    shutil.copy2(build/'traineros-webgame',stage/'bin/traineros-webgame')
if name in ('hextris','breakout'):
    ports.web(name,source,stage,recipe_root)
elif name=='2048':
    (stage/'web').mkdir()
    for file in ('index.html','js','style','meta','LICENSE.txt'):
        p=source/file
        if p.is_dir(): shutil.copytree(p,stage/'web'/file)
        else: shutil.copy2(p,stage/'web'/file)
    shutil.copy2(source/'LICENSE.txt',stage/'LICENSE.upstream')
    shutil.copy2(recipe_root.parents[1]/'LICENSE',stage/'LICENSE.runner')
    # A bounded landscape-port stylesheet, kept in this game's recipe.
    html=(stage/'web/index.html').read_text()
    html=html.replace('</head>','<style>html,body{margin:0}body{padding:16px 0}.game-container{margin-top:16px}.game-explanation,hr,.game-explanation~p{display:none}.container:after{content:"Gabriele Cirulli · TrainerOS adaptation · MIT";display:block;font-size:12px;margin-top:12px}</style></head>')
    html=html.replace('</body>','<script>document.addEventListener("keydown",function(e){if(e.key!=="Enter")return;var m=document.querySelector(".game-message");if(m.classList.contains("game-over"))document.querySelector(".retry-button").click();else if(m.classList.contains("game-won"))document.querySelector(".keep-playing-button").click();});</script></body>')
    (stage/'web/index.html').write_text(html)
elif name=='qalculate':
    ports.qalculate(source,build,stage,out,recipe_root)
elif name=='featherpad':
    run('cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_INSTALL_PREFIX=/usr')
    run('cmake','--build',str(build),'-j4')
    dest=out/'dest';run('cmake','--install',str(build),env=dict(os.environ,DESTDIR=str(dest)))
    shutil.copy2(dest/'usr/bin/featherpad',stage/'bin/featherpad')
    (stage/'bin/start').write_text('#!/bin/sh\nset -eu\nexec "$(dirname "$0")/featherpad" --standalone\n')
    (stage/'bin/start').chmod(0o755)
    shutil.copytree(dest/'usr/share',stage/'share')
    shutil.copy2(source/'COPYING',stage/'LICENSE.upstream')
    icons=list((stage/'share/icons').rglob('featherpad.svg'))
    if icons: shutil.copy2(icons[0],stage/'icon.svg')
else: raise SystemExit('No reviewed recipe for '+name)
# Source archive is published beside the binary for reproducibility/licensing.
run('git','-C',str(source),'archive','--format=tar.gz','--output='+str(out/(name+'-source.tar.gz')),'HEAD')
archive=out/(name+'-aarch64.tar.gz')
with tarfile.open(archive,'w:gz') as tar:
    for p in sorted(stage.rglob('*')):
        if p.is_file(): tar.add(p,arcname=p.relative_to(stage),recursive=False)
size=sum(p.stat().st_size for p in stage.rglob('*') if p.is_file())
record=dict(recipe,sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),size=archive.stat().st_size,
            unpackedSize=size+1024*1024,compatibility='untested',channel='Experimental',
            notes='Initial TrainerOS package. User data is kept separately.',
            url='https://github.com/EriArk/TrainerOS/releases/download/'+(sys.argv[3] if len(sys.argv)>3 else 'crates-test-20261018')+'/'+archive.name)
(out/(name+'-record.json')).write_text(json.dumps(record,indent=2))
print(json.dumps(record))
