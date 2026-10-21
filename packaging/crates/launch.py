#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Hold the package lease for the entire child lifetime, including minimization."""
import os
from pathlib import Path
import signal
import re
import subprocess
import sys
from installer import Store, ID, safe_path

def main():
    root, project = sys.argv[1:3]
    store = Store(root)
    with store.lock(project,exclusive=False):
        r = store.receipt(project)
        if not r or not re.fullmatch(re.escape(project)+'-[0-9a-f]{64}',r['object']): raise ValueError('Package is not installed')
        base = store.root/'objects'/r['object']
        entry = base/str(safe_path(r['entry']))
        if base.is_symlink() or not entry.is_file() or not entry.resolve().is_relative_to(base.resolve()):
            raise ValueError('Invalid package entry')
        data = store.root/'data'/project
        if r['kind']=='game':
            if len(sys.argv)!=4 or not re.fullmatch('[0-9a-f]{64}',sys.argv[3]):
                raise ValueError('A game requires a Trainer data owner')
            data=data/'owners'/sys.argv[3]
        data.mkdir(parents=True,exist_ok=True)
        env = dict(os.environ, XDG_CONFIG_HOME=str(data/'config'), XDG_DATA_HOME=str(data/'share'),
                   XDG_CACHE_HOME=str(data/'cache'), TRAINEROS_PACKAGE_DATA=str(data))
        # Resolve host discovery before assigning the package's private XDG root.
        # Pass only the path; the SDK owns its token and normal admission checks.
        env['TRAINEROS_SDK_ENDPOINT'] = os.environ.get('TRAINEROS_SDK_ENDPOINT') or str(
            Path(os.environ.get('XDG_DATA_HOME') or Path.home()/'.local/share') /
            'traineros/platform-v1/endpoint.json')
        if r['kind']=='game': env['TRAINEROS_CRATES_GAME']='1'
        for key in ('QT_QUICK_BACKEND','QT_SCALE_FACTOR','QSG_RHI_BACKEND'):
            env.pop(key,None)
        # The managed Qt web runner shares the browser's affected Armada GL
        # composition path. Keep this device workaround out of other apps.
        if entry.name == 'traineros-webgame':
            model = Path('/proc/device-tree/model')
            if model.exists() and model.read_bytes().rstrip(b'\0').strip() in (b'AYN Odin 2', b'Retroid Pocket Flip2'):
                env['QSG_RHI_BACKEND'] = 'vulkan'
        child = subprocess.Popen([str(entry)],cwd=base,env=env)
        signal.signal(signal.SIGTERM,lambda *_: child.terminate())
        return child.wait()

if __name__ == '__main__': sys.exit(main())
