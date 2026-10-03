#!/usr/bin/env python3
"""Stage the pinned native bridge; never overwrite a handheld installation."""
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

source, build, destination = map(Path, sys.argv[1:])
revision = "c77bbaa0f372c3f72281602a8b087206706542cb"
assert subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip() == revision
assert not destination.exists(), "Use a new staging directory."
binary = build / "Binaries/dolphin-emu"
assert binary.is_file()
(destination / "bin").mkdir(parents=True)
shutil.copy2(binary, destination / "bin/dolphin-emu")
shutil.copytree(source / "Data/Sys", destination / "bin/Sys")
manifest = dict(protocol=1, source=revision, sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
(destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(manifest["sha256"])
