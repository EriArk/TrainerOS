#!/usr/bin/env python3
"""Record a bounded core build; this is not a runtime admission manifest."""
import hashlib
import json
import platform
import subprocess
import sys
from pathlib import Path

PIN = "906e9ebb27da8c6a715cd7abab4abfe8a8d29427"


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: source_manifest.py PATCHED_SOURCE NEW_MANIFEST")
    source, output = (Path(arg).resolve() for arg in sys.argv[1:])
    recipe = Path(__file__).resolve().parent
    if output.exists():
        raise SystemExit("Retain the existing manifest; choose a new output path")
    head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        raise SystemExit("Unreviewed source revision")
    patch = recipe / "traineros-detmp.patch"
    subprocess.run(["git", "-C", str(source), "apply", "--reverse", "--check", str(patch)], check=True)
    changed = [line[6:] for line in patch.read_text().splitlines() if line.startswith("+++ b/")]
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = {
        "purpose": "Native DS network frontend preparation; not an installed GameParty capability",
        "upstream": PIN,
        "referenceCore": "85fdb7596011a3d0e679a6f1f88c312e77ff4cc5",
        "referenceChange": "88a65f9445bc2537ff6f5a78f27694df5a254dcf",
        "buildHost": {"system": platform.system(), "machine": platform.machine()},
        "patchSha256": digest(patch),
        "patchedFiles": {name: digest(source / name) for name in changed},
        "recipeFiles": {p.name: digest(p) for p in sorted(recipe.iterdir()) if p.is_file()},
        "runtimeReady": False,
        "remaining": ["installed-personal-save-return", "offline-nearby-field-acceptance",
                      "game-party-frontend", "separate-internet", "performance-and-control-acceptance"],
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Core preparation provenance recorded; no runtime enabled")


if __name__ == "__main__":
    main()
