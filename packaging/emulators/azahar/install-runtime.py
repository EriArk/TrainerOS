#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage a reviewed native Azahar build without selecting it or touching saves."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--requirements", type=Path, required=True,
                        help="Previously reviewed frozen transport requirements")
    args = parser.parse_args()
    if sys.platform != "linux" or digest(args.frontend) != args.sha256:
        raise SystemExit("Use the verified native Linux frontend")
    output = args.directory
    if not output.is_absolute() or output.exists() or output.is_symlink():
        raise SystemExit("Choose a new absolute version directory")
    recipe = Path(__file__).resolve().parent
    os.umask(0o077)
    (output / "bin").mkdir(parents=True, mode=0o700)
    (output / "transport").mkdir(mode=0o700)
    shutil.copyfile(args.frontend, output / "bin/azahar")
    (output / "bin/azahar").chmod(0o700)
    (output / "bin/azahar-launch").write_text(
        '#!/bin/sh\nset -eu\nlauncher=$(readlink -f "$0")\n'
        'export TRAINEROS_AZAHAR_MANAGED=1\n'
        'exec "$(dirname "$launcher")/azahar" "$@"\n')
    (output / "bin/azahar-launch").chmod(0o700)
    if digest(output / "bin/azahar") != args.sha256:
        raise SystemExit("Frontend changed during staging")
    subprocess.run([sys.executable, "-m", "venv", str(output / "transport/venv")], check=True)
    python = output / "transport/venv/bin/python"
    subprocess.run([str(python), "-m", "pip", "install", "-r", str(args.requirements)], check=True)
    freeze = subprocess.check_output([str(python), "-m", "pip", "freeze"])
    if sorted(freeze.splitlines()) != sorted(args.requirements.read_bytes().splitlines()):
        raise SystemExit("Transport dependency versions changed; review before admission")
    (output / "transport/requirements.lock").write_bytes(freeze)
    (output / "transport/dependencies.json").write_bytes(subprocess.check_output([str(python), "-m", "pip", "inspect"]))
    subprocess.run([str(python), "-c", "import noise.connection; from wormhole import create; import twisted"], check=True)
    shutil.copyfile(recipe / "room-session.py", output / "transport/room-session.py")
    # Reuse the reviewed DS channel implementation with a distinct application
    # and protocol namespace. No modification to the installed DS helper.
    channel = (recipe.parent / "melonds/secure-channel.py").read_text()
    assert channel.count('"org.traineros.ds-channel/v1"') == 1
    assert channel.count('"traineros-ds-records-v1"') == 1
    channel = channel.replace('"org.traineros.ds-channel/v1"', '"org.traineros.azahar-channel/v1"')
    channel = channel.replace('"traineros-ds-records-v1"', '"traineros-azahar-records-v1"')
    (output / "transport/secure-channel.py").write_text(channel)
    shutil.copyfile(recipe / "LICENSE.upstream", output / "LICENSE.upstream")
    shutil.copyfile(recipe / "content.py", output / "content.py")
    (output / "source-patches").mkdir()
    for name in ("TrainerRoom.inc", "TrainerContent.inc", "traineros-room.patch",
                 "traineros-content.patch", "libusb-init-failure.patch", "build.sh"):
        shutil.copyfile(recipe / name, output / "source-patches" / name)
    (output / "source-patches/UPSTREAM.txt").write_text(
        "https://github.com/azahar-emu/azahar\n219d743a1e698101fe1ab8bef43e6563126c3c27\n")
    files = ["bin/azahar", "transport/room-session.py", "transport/secure-channel.py", "transport/requirements.lock"]
    manifest = {"protocol": 1, "source": "219d743a1e698101fe1ab8bef43e6563126c3c27",
                "settings": "native-room-two-private-v1", "files": {name: digest(output / name) for name in files},
                "interpreterSha256": digest(python),
                "content": {"protocol": "azahar-content-v1", "sha256": digest(output / "content.py")}}
    with (output / "manifest.pending").open("x") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    (output / "manifest.pending").replace(output / "manifest.json")
    print("Azahar runtime staged; no current-version selection or personal data changed")


if __name__ == "__main__":
    main()
