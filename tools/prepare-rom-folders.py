#!/usr/bin/env python3
"""Prepare the ARM64 ROM tree without moving/removing games or following aliases.

Run with the mounted ROM root and --apply. The default only prints the plan.
The same registry drives discovery, platform labels and ordinary launch.
"""
import argparse
import json
from pathlib import Path


def prepare(root, registry, apply=False):
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise ValueError('ROM root is not a directory')
    result = []
    for p in registry['platforms']:
        names = list(dict.fromkeys([p['folder'], p['id']] + p['aliases']))
        if any(not n or n in ('.', '..') or '/' in n or '\\' in n for n in names):
            raise ValueError('Invalid platform folder')
        existing = [root / n for n in names if (root / n).exists() or (root / n).is_symlink()]
        for path in existing:
            if not path.is_dir() or not path.resolve().is_relative_to(root):
                raise ValueError(f'Platform folder needs review: {path.name}')
        # Honor old aliases in an established collection; no parallel empty tree.
        folder = existing[0] if existing else root / p['folder']
        if apply:
            folder.mkdir(exist_ok=True)
        result.append({'platform': p['id'], 'folder': folder.name, 'created': not existing})
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('roms', type=Path)
    parser.add_argument('--registry', type=Path, default=Path(__file__).resolve().parents[1] / 'data/rom-platforms.json')
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    rows = prepare(args.roms, json.loads(args.registry.read_text(encoding='utf8')), args.apply)
    print(json.dumps({'applied': args.apply, 'platforms': len(rows), 'new_folders': sum(r['created'] for r in rows), 'folders': rows}, indent=2))
