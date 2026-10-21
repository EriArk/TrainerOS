"""Installed SDK apps retain host discovery while their own XDG data stays isolated."""
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'packaging/crates'))
import installer

class LaunchTests(unittest.TestCase):
    def test_installed_process_discovers_host_and_keeps_private_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            content = b'#!/usr/bin/python3\nimport json,os\nprint(json.dumps({k:os.environ[k] for k in ("TRAINEROS_SDK_ENDPOINT","XDG_DATA_HOME","TRAINEROS_PACKAGE_DATA")}))\n'
            archive = root / 'app.tar.gz'
            with tarfile.open(archive, 'w:gz') as tar:
                entry = tarfile.TarInfo('bin/app');entry.size=len(content);entry.mode=0o755
                tar.addfile(entry, io.BytesIO(content))
            record = dict(id='sdk-fixture',version='1',title='Fixture',kind='app',architecture=os.uname().machine,
                          entry='bin/app',size=archive.stat().st_size,unpackedSize=4096,
                          sha256=hashlib.sha256(archive.read_bytes()).hexdigest())
            with patch.dict(os.environ, {'HOME':str(root)}):
                store=installer.Store(root/'packages');store.activate(archive, record)
            for explicit in (None, str(root/'custom/endpoint.json')):
                env=dict(os.environ,HOME=str(root),XDG_DATA_HOME=str(root/'host-data'))
                env.pop('TRAINEROS_SDK_ENDPOINT',None)
                if explicit:env['TRAINEROS_SDK_ENDPOINT']=explicit
                output=subprocess.check_output([sys.executable,str(ROOT/'packaging/crates/launch.py'),str(store.root),'sdk-fixture'],env=env)
                actual=json.loads(output)
                self.assertEqual(actual['TRAINEROS_SDK_ENDPOINT'],explicit or str(root/'host-data/traineros/platform-v1/endpoint.json'))
                self.assertEqual(actual['XDG_DATA_HOME'],str(store.root/'data/sdk-fixture/share'))
                self.assertEqual(actual['TRAINEROS_PACKAGE_DATA'],str(store.root/'data/sdk-fixture'))

    def test_only_reviewed_release_repositories(self):
        base=dict(id='session-timer',version='0.1.0',title='Session Timer',kind='app',architecture='x86_64',
                  entry='bin/session-timer',sha256='a'*64,size=100,unpackedSize=1000)
        for repo in ('TrainerOS','crates'):
            installer.validate_records([dict(base,url=f'https://github.com/EriArk/{repo}/releases/download/test/app.tar.gz')])
        for url in ('https://github.com/other/crates/releases/download/test/app.tar.gz',
                    'https://github.com/EriArk/crates-evil/releases/download/test/app.tar.gz',
                    'https://github.com/EriArk/crates/releases/download/test/app.tar.gz?redirect=1',
                    'https://github.com/EriArk/crates/raw/main/app.tar.gz'):
            with self.assertRaises(ValueError):installer.validate_records([dict(base,url=url)])

if __name__=='__main__':unittest.main()
