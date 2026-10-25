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
recipe = Path(__file__).resolve().parent
for patch in ("traineros-netplay.patch", "traineros-bba-eof.patch", "traineros-content.patch"):
    subprocess.run(["git", "-C", str(source), "apply", "--reverse", "--check",
                    str(recipe / patch)], check=True)
assert (source / "Source/Core/DolphinQt/TrainerNetplay.inc").read_bytes() == (recipe / "TrainerNetplay.inc").read_bytes()
assert not destination.exists(), "Use a new staging directory."
binary = build / "Binaries/dolphin-emu"
tool = build / "Binaries/dolphin-tool"
def architecture(path):
    with path.open("rb") as file:
        header = file.read(20)
    assert header[:6] == b"\x7fELF\x02\x01", "Expected a 64-bit little-endian Linux executable"
    return {183: "aarch64", 62: "x86_64"}[int.from_bytes(header[18:20], "little")]
arch = architecture(binary)
assert architecture(tool) == arch, "The emulator and disc tool must have the same architecture"
(destination / "bin").mkdir(parents=True)
shutil.copy2(binary, destination / "bin/dolphin-emu")
shutil.copytree(source / "Data/Sys", destination / "bin/Sys")
shutil.copy2(tool, destination / "bin/dolphin-tool")
(destination / "notices").mkdir()
shutil.copy2(source / "COPYING", destination / "notices/Dolphin.COPYING")
shutil.copytree(source / "LICENSES", destination / "notices/LICENSES")
(destination / "source-patches").mkdir()
for name in ("traineros-netplay.patch", "traineros-bba-eof.patch", "traineros-content.patch", "TrainerNetplay.inc"):
    shutil.copy2(recipe / name, destination / "source-patches" / name)
(destination / "source-patches/UPSTREAM.txt").write_text(
    "https://github.com/dolphin-emu/dolphin\n" + revision + "\n", encoding="utf-8")
manifest = dict(protocol=1, source=revision, architecture=arch, sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                bba="tapserver-eof-v1", hotseat="dolphin-golf-p1-v1", wiimote="dolphin-wiimote-v1", wad="dolphin-wad-channel-v1",
                executable="dolphin-native-executable-v1", toolSha256=hashlib.sha256(tool.read_bytes()).hexdigest())
(destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(manifest["sha256"])
