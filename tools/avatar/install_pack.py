"""Install a reviewed pack into shared user data, outside games. No downloads."""
import argparse, hashlib, json, os, shutil, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--destination',type=Path)
a=p.parse_args();source=a.source.resolve()
base=Path(os.environ.get('LOCALAPPDATA') or os.environ.get('XDG_DATA_HOME') or Path.home()/'.local/share')
destination=(a.destination or base/'traineros/avatar/packs/trainer-avatar-1').resolve()
manifest=json.loads((source/'manifest.json').read_text(encoding='utf-8'))
assert manifest['schema']==1 and manifest['pack']=='trainer-avatar-1'
files=list(manifest['files'])
assert 'template.glb' in files
for name in files:
    assert Path(name).name == name and name not in ('.', '..'), name
    assert (source/name).is_file(),name
    assert hashlib.sha256((source/name).read_bytes()).hexdigest()==manifest['files'][name],name
destination.parent.mkdir(parents=True,exist_ok=True)
if destination.exists():
    # Idempotent only. Never overwrite an installed revision used by a running game.
    assert all((destination/name).is_file() and hashlib.sha256((destination/name).read_bytes()).digest()==hashlib.sha256((source/name).read_bytes()).digest() for name in [*files,'manifest.json']), 'Different installed pack: choose a new versioned destination'
else:
    with tempfile.TemporaryDirectory(prefix='avatar-stage-',dir=destination.parent) as temp:
        stage=Path(temp)/'pack';stage.mkdir()
        for name in [*files,'manifest.json']:shutil.copy2(source/name,stage/name)
        stage.rename(destination)
print(destination)
