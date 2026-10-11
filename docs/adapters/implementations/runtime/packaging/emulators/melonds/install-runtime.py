#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage a new DS runtime version; publish/select it only after validation.

No games, firmware, saves or ordinary melonDS settings are copied. The caller
supplies the reviewed frontend hash. An existing version is never overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "linux" or not args.frontend.is_file() or digest(args.frontend) != args.sha256:
        raise SystemExit("Use the verified Linux frontend artifact")
    output = args.directory
    if not output.is_absolute() or output.exists() or output.is_symlink():
        raise SystemExit("Choose a new absolute runtime-version directory")
    recipe = Path(__file__).resolve().parent
    os.umask(0o077)
    output.mkdir(parents=True, mode=0o700)
    (output / "bin").mkdir(mode=0o700)
    frontend = output / "bin/traineros-ds"
    shutil.copyfile(args.frontend, frontend)
    frontend.chmod(0o700)
    if digest(frontend) != args.sha256:
        raise SystemExit("Frontend changed during staging; runtime was not admitted")
    subprocess.run(["sh", str(recipe / "install-transport.sh"), str(output / "transport")], check=True)
    files = ["bin/traineros-ds", "transport/secure-channel.py", "transport/requirements.lock"]
    manifest = {"protocol": 1, "source": "906e9ebb27da8c6a715cd7abab4abfe8a8d29427",
                "settings": "detmp-two-owned-v1", "files": {name: digest(output / name) for name in files},
                "interpreterSha256": digest(output / "transport/venv/bin/python")}
    # Manifest last; interrupted dependency installation cannot enable a profile.
    with (output / "manifest.pending").open("x", encoding="utf-8") as file:
        json.dump(manifest, file, indent=2)
        file.write("\n")
        file.flush()
        os.fsync(file.fileno())
    (output / "manifest.pending").replace(output / "manifest.json")
    fd = os.open(output, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)
    print("DS runtime staged and hashed; no current-version link or personal data changed")


if __name__ == "__main__":
    main()
