# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit maintainer adaptations, never interpreted from downloaded metadata."""
import json
import os
import re
import shutil
import subprocess


def copy(source,dest,names):
    for name in names:
        src=source/name
        if src.is_dir(): shutil.copytree(src,dest/name)
        else: shutil.copy2(src,dest/name)


def web(name,source,stage,recipes):
    target=stage/'web';target.mkdir()
    if name=='hextris':
        copy(source,target,('index.html','js','style','vendor','images','favicon.ico','LICENSE.md'))
        shutil.copy2(source/'LICENSE.md',stage/'LICENSE.upstream')
        html=(target/'index.html').read_text()
        html=re.sub(r'<script\b[^>]*src=["\']https?://[^>]*>.*?</script>','',html,flags=re.S)
        html=re.sub(r'<script>\s*\(function\(i,s,o,g,r,a,m\).*?</script>','',html,flags=re.S)
        html=re.sub(r'<link[^>]*fonts.googleapis.com[^>]*>','',html)
        html=html.replace('</head>',"<style>#socialShare,#buttonCont,#badges{display:none!important}body,canvas{font-family:sans-serif}</style></head>")
        main=target/'js/main.js'; text=main.read_text()
        text=text.replace("$.get('http://54.183.184.126/' + String(score))",'/* Offline package: no score telemetry. */')
        text=re.sub(r'\(function\(\)\{\s*var script = document.createElement\(\x27script\x27\);.*?\}\)\(\)','',text,flags=re.S)
        main.write_text(text)
        init=target/'js/initialization.js';text=init.read_text()
        text=re.sub(r'\(function\(i, s, o, g, r, a, m\).*?ga\(\x27send\x27, \x27pageview\x27\);','',text,flags=re.S)
        init.write_text(text)
        (stage/'ADAPTATION.txt').write_text('Hextris by Logan Engstrom, Garrett Finucane, Noah Moroze and Michael Yang.\nTrainerOS offline adaptation: external advertising/analytics/score telemetry removed; original gameplay and local saves retained. GPL-3.0-or-later.\n')
    elif name=='breakout':
        copy(source,target,('index.html','game.js','breakout.js','levels.js','breakout.css','images','LICENSE'))
        shutil.copy2(source/'LICENSE',stage/'LICENSE.upstream')
        html=(target/'index.html').read_text()
        html=html.replace('<script>','<script src="traineros-audio.js"></script><script>',1)
        html=html.replace('</head>', '<style>#breakout{width:min(96vw,calc(94vh * 4 / 3));height:auto;aspect-ratio:4/3;margin:2vh auto}#canvas{width:100%;height:100%}</style></head>')
        html=html.replace('<b>space</b> to start','<b>Enter / A</b> to start')
        shutil.copy2(recipes/'webgame/breakout-audio.js',target/'traineros-audio.js')
        (stage/'ADAPTATION.txt').write_text('Javascript Breakout by Jake Gordon and contributors (MIT).\nOriginal Freesound recordings and Flash sound engine are not distributed in this binary package. TrainerOS procedural Web Audio cues are GPL-3.0-or-later; see corresponding runner source.\n')
    else: raise ValueError('Unknown web port')
    # CSP applies even to dynamically injected scripts; local games cannot make
    # analytics requests or navigate to a third-party site from an in-game link.
    html=html.replace('<head>', '<head><meta http-equiv="Content-Security-Policy" content="default-src \'self\' data:; script-src \'self\' \'unsafe-inline\' \'unsafe-eval\'; style-src \'self\' \'unsafe-inline\'; connect-src \'none\'; frame-src \'none\'; object-src \'none\'">',1)
    (target/'index.html').write_text(html)
    (target/'viewport.json').write_text('{"responsive":true}')
    shutil.copy2(recipes.parents[1]/'LICENSE',stage/'LICENSE.runner')


def qalculate(source,build,stage,out,recipes):
    def run(*args,**kwargs):subprocess.run(args,check=True,**kwargs)
    lib=json.loads((recipes/'recipes/qalculate-lib.json').read_text())
    src=out/'libqalculate-source'; prefix=out/'qalculate-deps'
    if not src.exists():run('git','clone',lib['repository'],str(src))
    run('git','-C',str(src),'checkout','--detach',lib['revision'])
    run('sh','autogen.sh','--prefix='+str(prefix),'--disable-textport','--disable-unittests','--disable-static',cwd=src)
    run('make','-j4',cwd=src);run('make','install',cwd=src)
    build.mkdir(exist_ok=True)
    (build/'translations').mkdir(exist_ok=True)
    env=dict(os.environ,PKG_CONFIG_PATH=str(prefix/'lib/pkgconfig'))
    run('qmake6',str(source/'qalculate-qt.pro'),'PREFIX=/usr','COMPILE_RESOURCES=yes',cwd=build,env=env)
    # Upstream translations.qrc names source-relative QM files even in a shadow
    # build. Put only generated translations there before compiling resources.
    for qm in (build/'translations').glob('*.qm'):shutil.copy2(qm,source/'translations'/qm.name)
    run('make','-j4',cwd=build,env=env)
    shutil.copy2(build/'qalculate-qt',stage/'bin/qalculate-qt')
    (stage/'lib').mkdir()
    for p in (prefix/'lib').glob('libqalculate.so.*'):
        # Archives contain regular files only, including the ELF SONAME filename.
        if re.fullmatch(r'libqalculate\.so\.[0-9]+',p.name):
            shutil.copyfile(p,stage/'lib'/p.name);run('strip','--strip-unneeded',str(stage/'lib'/p.name))
    run('strip','--strip-unneeded',str(stage/'bin/qalculate-qt'))
    shutil.copytree(prefix/'share/qalculate',stage/'share/qalculate')
    shutil.copy2(source/'COPYING',stage/'LICENSE.upstream')
    shutil.copy2(src/'COPYING',stage/'LICENSE.libqalculate')
    shutil.copy2(source/'data/scalable/qalculate-qt.svg',stage/'icon.svg')
    (stage/'bin/start').write_text('#!/bin/sh\nset -eu\nbase=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\nexport LD_LIBRARY_PATH="$base/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexport QALCULATE_DEFINITIONS_DIR="$base/share/qalculate"\nexec "$base/bin/qalculate-qt" --new-instance\n')
    (stage/'bin/start').chmod(0o755)
    (stage/'DEPENDENCIES.json').write_text(json.dumps(lib,indent=2))
    run('git','-C',str(src),'archive','--format=tar.gz','--output='+str(out/'libqalculate-source.tar.gz'),'HEAD')
