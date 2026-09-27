#!/usr/bin/env python3
"""Install the explicitly prepared offline Emerald runtime, never user game data.

Prepare Node 24.18.0 from its verified official archive and run npm ci
--ignore-scripts --omit=optional in tools/research/emerald-practice first.
Run only while TrainerOS practice is closed. The application never downloads.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--node', type=Path, required=True)
    parser.add_argument('--node-license', type=Path, required=True)
    parser.add_argument('--dependencies', type=Path, required=True)
    parser.add_argument('--prefix', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    node = args.node.resolve(strict=True)
    dependencies = args.dependencies.resolve(strict=True)
    package = json.loads((dependencies / 'pokemon-showdown/package.json').read_text())
    if package.get('version') != '0.11.11':
        raise SystemExit('Expected the pinned pokemon-showdown 0.11.11 dependency')
    env = {k: v for k, v in os.environ.items() if k not in ('NODE_OPTIONS', 'NODE_PATH')}
    if subprocess.check_output([str(node), '--version'], env=env, text=True).strip() != 'v24.18.0':
        raise SystemExit('Expected the verified Node v24.18.0 binary')
    prefix = args.prefix.absolute()
    if prefix.is_symlink() or prefix.name != 'emerald-v1':
        raise SystemExit('Use a real, dedicated emerald-v1 directory')
    prefix.parent.mkdir(parents=True, exist_ok=True)
    previous = prefix.with_name(prefix.name + '.previous')
    if previous.exists():
        raise SystemExit('A previous runtime backup already exists; preserve/review it before another install')
    stage = Path(tempfile.mkdtemp(prefix='.emerald-runtime-', dir=prefix.parent))
    try:
        (stage / 'bin').mkdir()
        shutil.copy2(node, stage / 'bin' / ('node.exe' if os.name == 'nt' else 'node'))
        shutil.copy2(args.node_license, stage / 'NODE-LICENSE')
        shutil.copytree(dependencies, stage / 'node_modules', ignore=shutil.ignore_patterns('.bin'))
        for relative in ('src/integrations/practice/emerald-worker.cjs',
                         'src/integrations/practice/emerald-engine.cjs',
                         'data/emerald-reference.json',
                         'tools/research/emerald-practice/package.json',
                         'tools/research/emerald-practice/package-lock.json'):
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / relative, target)
        hashes = {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in sorted(stage.rglob('*')) if p.is_file()}
        (stage / 'manifest.json').write_text(json.dumps({'node': '24.18.0', 'engine': '0.11.11', 'sha256': hashes}, indent=2))
        if prefix.exists():
            prefix.rename(previous)
        try:
            stage.rename(prefix)
        except OSError:
            if previous.exists():
                previous.rename(prefix)
            raise
    finally:
        if stage.exists():
            shutil.rmtree(stage)  # Only this invocation's mkdtemp directory.
    print('Offline Emerald practice runtime installed; previous bundle retained if present.')


if __name__ == '__main__':
    main()
