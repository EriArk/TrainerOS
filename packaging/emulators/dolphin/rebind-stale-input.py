#!/usr/bin/env python3
"""One-time migration of an absent Steam pad; retain all user button mappings.

Run while Dolphin is closed, after confirming the SDL device name in its runtime.
This does not modify saves or replace a working controller configuration.
"""
import argparse
from pathlib import Path
import os

parser = argparse.ArgumentParser()
parser.add_argument("config", type=Path)
parser.add_argument("--device", required=True, help="Verified Dolphin SDL device, e.g. SDL/0/...")
args = parser.parse_args()
if not args.device.startswith("SDL/") or any(c in args.device for c in "\r\n"):
    raise SystemExit("Expected a verified Dolphin SDL device name")
for proc in Path("/proc").glob("[0-9]*"):
    try:
        if "dolphin" in (proc / "exe").readlink().name.lower():
            raise SystemExit("Close Dolphin first")
    except (FileNotFoundError, PermissionError):
        pass
for filename in ("GCPadNew.ini", "WiimoteNew.ini"):
    path = args.config / filename
    if not path.is_file():
        continue
    data = path.read_bytes()
    old = b"Device = SDL/0/Steam Virtual Gamepad"
    if old not in data:
        continue
    backup = path.with_suffix(path.suffix + ".before-launch80")
    if backup.exists():
        if backup.read_bytes() != data:
            raise SystemExit("A different migration backup already exists")
    else:
        backup.write_bytes(data)
    temp = path.with_suffix(path.suffix + ".launch80.tmp")
    temp.write_bytes(data.replace(old, ("Device = " + args.device).encode()))
    temp.chmod(path.stat().st_mode)
    os.replace(temp, path)
    print(filename, "controller rebound; mappings preserved")
