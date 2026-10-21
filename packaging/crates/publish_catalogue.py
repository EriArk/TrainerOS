#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Maintainer-only publication. Run after publishing the approved release assets.

The private Ed25519 key stays outside the checkout/build machine/server. Transfer
only the resulting signed JSON to the server and atomically replace current.json.
"""
import argparse
from pathlib import Path
from catalogue import atomic,decode,signature,validate,verified


def publish(source,key,output,previous=None,public_key=None):
    value=validate(decode(source.read_bytes()))
    if previous:
        old=decode(previous.read_bytes());verified(old,public_key)
        if value['revision']<=old['signed']['revision']: raise ValueError('Publication revision must increase')
        releases={(r['id'],r['version'],r['architecture']):r for r in old['signed']['packages']}
        for r in value['packages']:
            before=releases.get((r['id'],r['version'],r['architecture']))
            if before and any(before[k]!=r[k] for k in ('sha256','url','size','unpackedSize','entry','architecture','kind')):
                raise ValueError('Published versions are immutable; assign a new version')
    envelope={'signed':value,'signature':signature(value,key)}
    atomic(output,envelope)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--key',type=Path,required=True);p.add_argument('--previous',type=Path);p.add_argument('--public-key',type=Path)
    a=p.parse_args();publish(a.source,a.key,a.output,a.previous,a.public_key)
