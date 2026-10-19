#!/usr/bin/env python3
"""Prepare an Odin 2 DTB copy; never modify the supplied boot file in place.

Requires dtc's fdtget/fdtput. Only two audio-routing pairs may change.
The matching UCM profile is needed separately. See docs/AUDIO82.md.
"""
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROUTES = ['DMIC3', 'MIC BIAS3', 'TX SWR_INPUT7', 'DMIC3_OUTPUT']


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def properties(path, node='/'):
    result = {}
    for prop in run('fdtget', '-p', str(path), node).splitlines():
        result[(node, prop)] = run('fdtget', '-t', 'bx', str(path), node, prop)
    for child in run('fdtget', '-l', str(path), node).splitlines():
        result.update(properties(path, node.rstrip('/')+'/'+child))
    return result


def prepare(source, target):
    if source.resolve() == target.resolve() or target.exists():
        raise ValueError('Use a new output path; preserve the source')
    if run('fdtget', str(source), '/', 'model') != 'AYN Odin 2':
        raise ValueError('Only the verified original Odin 2 model is allowed')
    before = properties(source)
    raw = bytes(int(b, 16) for b in before[('/sound', 'audio-routing')].split())
    old = raw.rstrip(b'\0').decode().split('\0')
    if len(old) % 2 or ['TX SWR_INPUT1', 'ADC2_OUTPUT'] not in [old[i:i+2] for i in range(0,len(old),2)]:
        raise ValueError('Unrecognized baseline audio routes')
    if any(s in old for s in ROUTES):
        raise ValueError('Microphone routes already present or baseline changed')
    shutil.copyfile(source, target)
    subprocess.run(['fdtput', '-t', 's', str(target), '/sound', 'audio-routing', *old, *ROUTES], check=True)
    after = properties(target)
    changed = [key for key in before.keys() | after.keys() if before.get(key) != after.get(key)]
    if changed != [('/sound', 'audio-routing')]:
        raise ValueError('Unexpected device-tree changes')
    expected = b'\0'.join(s.encode() for s in old+ROUTES)+b'\0'
    if bytes(int(b,16) for b in after[('/sound','audio-routing')].split()) != expected:
        raise ValueError('Route readback mismatch')
    print(json.dumps({'model':'AYN Odin 2','changed_property':'/sound/audio-routing','added':ROUTES}))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: odin2-mic-dtb.py SOURCE.dtb NEW-CANDIDATE.dtb')
    prepare(*map(Path,sys.argv[1:]))
