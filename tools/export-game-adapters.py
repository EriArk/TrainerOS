#!/usr/bin/env python3
"""Refresh/check the portable, exact-game source snapshot in the knowledge base."""
import argparse
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
dest = root / "docs/adapters/implementations/gen3"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
files = {}


def include(path):
    relative = path.relative_to(root).as_posix()
    if relative in files:
        return
    # Git working trees may use CRLF. The exported source is canonical UTF-8/LF.
    text = path.read_text(encoding="utf-8")
    files[relative] = text.encode("utf-8")
    for name in re.findall(r'^#include "([^"]+)"', text, re.M):
        dependency = path.parent / name
        if not dependency.is_file():
            dependency = root / "src" / name
        if not dependency.is_file():
            raise SystemExit(f"Missing source dependency: {name}")
        include(dependency)


for name in ("Gen3Progress", "EmeraldParty", "EmeraldShops", "EmeraldPractice", "EmeraldLink"):
    include(root / f"src/integrations/progress/{name}.cpp")
include(root / "src/integrations/practice/PracticeSession.cpp")
include(root / "src/core/model/AdventureCompletion.h")
for relative in ("src/integrations/practice/emerald-engine.cjs",
                 "src/integrations/practice/emerald-worker.cjs",
                 "tools/research/emerald-practice/.gitignore",
                 "tools/research/emerald-practice/package.json",
                 "tools/research/emerald-practice/package-lock.json"):
    files[relative]=(root/relative).read_text(encoding="utf-8").encode("utf-8")
for name in ("emerald-reference", "emerald-shops"):
    for suffix in (".json", "-source.json"):
        path = root / f"data/{name}{suffix}"
        files[path.relative_to(root).as_posix()] = path.read_text(encoding="utf-8").encode("utf-8")

registry = json.loads((root / "docs/adapters/registry.json").read_text(encoding="utf-8"))
for build in registry["builds"]:
    profile = {k: build[k] for k in ("id", "romSha256", "romBytes", "saveBytes", "edition", "platform")}
    profile["game"] = "emerald" if build["id"] == "emerald-en" else "firered"
    profile["capabilities"] = build["capabilities"]
    profile["unknown"] = build["unknown"]
    profile["evidenceBase"] = "../../../"
    files[f"profiles/{build['id']}.json"] = (json.dumps(profile, indent=2) + "\n").encode()

manifest = {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())}
files["manifest.json"] = (json.dumps({"hash": "SHA-256 of UTF-8/LF export", "files": manifest}, indent=2) + "\n").encode()
managed = {p.relative_to(dest).as_posix() for folder in ("src", "data", "profiles", "tools")
           for p in (dest / folder).rglob("*") if p.is_file() and "node_modules" not in p.parts}
unexpected = managed - set(files)
if unexpected:
    raise SystemExit(f"Review stale export files explicitly: {sorted(unexpected)}")
stale = []
for name, content in files.items():
    target = dest / name
    if args.check:
        if not target.is_file() or target.read_text(encoding="utf-8").encode("utf-8") != content:
            stale.append(name)
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
if stale:
    raise SystemExit("Adapter copy is stale; run tools/export-game-adapters.py:\n" + "\n".join(stale))
print(f"Adapter source snapshot {'checked' if args.check else 'exported'}: {len(manifest)} files, {len(registry['builds'])} exact profiles")

# Runtime modules are a separate source snapshot, not semantic save adapters.
dest = root / "docs/adapters/implementations/runtime"
files = {}
for relative in ("src/integrations/adventure/standalone/DolphinNetplay.cpp",
                 "src/integrations/adventure/standalone/PpssppNetplay.cpp",
                 "src/integrations/adventure/retroarch/RetroArchNetplay.cpp",
                 "src/integrations/adventure/retroarch/RetroArchHandheldLink.cpp",
                 "src/integrations/adventure/retroarch/RetroArchSaveTarget.cpp",
                 "src/integrations/adventure/retroarch/RetroArchDisc.cpp",
                 "src/integrations/adventure/retroarch/RetroArchConfiguration.cpp"):
    include(root / relative)
for profile in registry.get("runtimeProfiles", []):
    exported_profile = dict(profile, evidenceBase="../../../")
    files[f"profiles/{profile['id']}.json"] = (json.dumps(exported_profile, indent=2) + "\n").encode()
for path in sorted((root / "packaging/emulators/dolphin").iterdir()):
    if path.is_file():
        files[path.relative_to(root).as_posix()] = path.read_text(encoding="utf-8").encode("utf-8")
manifest = {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())}
files["manifest.json"] = (json.dumps({"hash": "SHA-256 of UTF-8/LF export", "files": manifest}, indent=2) + "\n").encode()
managed = {p.relative_to(dest).as_posix() for folder in ("src", "profiles", "packaging")
           for p in (dest / folder).rglob("*") if p.is_file()}
if managed - set(files):
    raise SystemExit(f"Review stale runtime export files explicitly: {sorted(managed - set(files))}")
stale = []
for name, content in files.items():
    target = dest / name
    if args.check:
        if not target.is_file() or target.read_text(encoding="utf-8").encode("utf-8") != content:
            stale.append(name)
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
if stale:
    raise SystemExit("Runtime adapter copy is stale; run tools/export-game-adapters.py:\n" + "\n".join(stale))
print(f"Runtime source snapshot {'checked' if args.check else 'exported'}: {len(manifest)} files")
