#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Seal an already built private cabinet package; never supply game/BIOS data."""
import hashlib
import json
from pathlib import Path
import sys

root=Path(sys.argv[1]).resolve()
files={}
for name in ('bin/flycast', 'cabinet_endpoint', 'stream_transport'):
    path=root/name
    if path.is_symlink() or not path.is_file() or not 0<path.stat().st_size<=256*1024*1024:
        raise SystemExit('Missing or unsafe package executable: '+name)
    if not path.stat().st_mode & 0o111:
        raise SystemExit('Package file is not executable: '+name)
    files[name]=hashlib.sha256(path.read_bytes()).hexdigest()
manifest={'protocol':1, 'source':'d0e47e572b1e7b355e88bda8308c89d0c5156cbf',
          'settings':'flycast-cabinet-private-v1', 'files':files}
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest))
