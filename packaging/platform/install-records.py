#!/usr/bin/env python3
"""Install the existing secure record helper without installing an emulator."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path, help='New private transport directory')
args = parser.parse_args()
target = args.output.resolve()
if target.exists():
    parser.error('Output already exists; choose a new directory')
root = Path(__file__).resolve().parents[2]
target.mkdir(parents=True, mode=0o700)
subprocess.run([sys.executable, '-m', 'venv', str(target / 'venv')], check=True)
python = target / ('venv/Scripts/python.exe' if sys.platform == 'win32' else 'venv/bin/python')
subprocess.run([str(python), '-m', 'pip', 'install', '-r',
                str(root / 'packaging/emulators/handheld/linked-save-requirements.txt'), 'noiseprotocol==0.3.1'], check=True)
shutil.copy2(root / 'packaging/emulators/melonds/secure-channel.py', target / 'secure-channel.py')
with (target / 'requirements.lock').open('w', encoding='utf-8') as output:
    subprocess.run([str(python), '-m', 'pip', 'freeze'], stdout=output, check=True)
subprocess.run([str(python), '-c', 'import noise.connection; from wormhole import create; import twisted'], check=True)
print('Installed shared record transport; emulator binaries were not installed.')
