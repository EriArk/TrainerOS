"""Local bundle manifest. Firmware comes only from the owner's explicit paths.
This does not authorize redistribution or certify the selected operator relay.
"""
import hashlib
import json
from pathlib import Path
import sys
from urllib.parse import urlsplit

root=Path(sys.argv[1]).resolve()
online=sys.argv[2] if sys.argv[2] != "--nearby-only" else ""
url=urlsplit(online)
renderer=sys.argv[3] if len(sys.argv)>3 else 'vulkan'
if online and (url.scheme!='udp' or not url.hostname or not url.port or url.username or url.password or url.path or url.query or url.fragment):
    raise SystemExit('Expected udp://operator-host:port without credentials, path or query')
if renderer not in ('vulkan','opengl'):
    raise SystemExit('Expected vulkan or opengl')
files={}
for name,size in [('bin/flycast',None),('firmware/dc_boot.bin',2*1024*1024),('firmware/dc_flash.bin',128*1024)]:
    p=root/name
    if not p.resolve().is_relative_to(root) or not p.is_file() or not 0<p.stat().st_size<=256*1024*1024 or (size and p.stat().st_size!=size):
        raise SystemExit('Invalid local bundle file: '+name)
    with p.open('rb') as f:
        files[name]=hashlib.file_digest(f,'sha256').hexdigest()
manifest={'protocol':1,'source':'d0e47e572b1e7b355e88bda8308c89d0c5156cbf',
          'renderer':renderer,'onlineRelay':online,'files':files}
if (root/'arcade.py').exists() or (root/'arcade-catalog.json').exists():
    arcade={'protocol':'flycast-arcade-sets-v1'}
    for name in ('arcade.py','arcade-catalog.json'):
        path=root/name
        if not path.is_file() or not path.resolve().is_relative_to(root) or not 0<path.stat().st_size<4*1024*1024:
            raise SystemExit('Invalid arcade package file')
        arcade[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    catalog=json.loads((root/'arcade-catalog.json').read_text())
    if catalog.get('source')!=manifest['source'] or catalog.get('protocol')!=1:
        raise SystemExit('Mismatched native arcade catalog')
    manifest['arcade']=arcade
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
