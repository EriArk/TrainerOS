#!/usr/bin/env python3
"""Validate the small research index; never enable runtime capabilities."""
import json
import re
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
base = root / "docs/adapters"
data = json.loads((base / "registry.json").read_text())
source = (root / "src/integrations/progress/Gen3Progress.cpp").read_text()
allowed = set(re.findall(r'"([0-9a-f]{64})"', source))
seen = set()
states = {"planned", "researching", "partial", "read-verified", "write-verified", "integrated", "blocked"}
for row in data["builds"]:
    digest = row["romSha256"]
    assert digest in allowed and digest not in seen, row["id"]
    seen.add(digest)
    record = base / row["record"]
    assert record.is_file() and digest in record.read_text(encoding="utf-8"), row["id"]
    assert row["capabilities"] and {v["state"] for v in row["capabilities"].values()} <= states
    assert all(v["evidence"] and (base / v["details"]).is_file() for v in row["capabilities"].values())
    assert row["saveBytes"] > 0
assert seen == allowed, "Compiled exact builds and research index differ"
for row in data.get("runtimeProfiles", []):
    runtime_source = (root / row.get("implementationSource", "src/integrations/adventure/retroarch/RetroArchNetplay.cpp")).read_text()
    assert row["id"] in runtime_source and row["romSha256"] in runtime_source, row["id"]
    assert row["state"] in states and (base / row["record"]).is_file(), row["id"]
    assert row["evidence"] and row["remaining"]
    assert isinstance(row["persistentSaveWrites"], bool)
    assert not row["persistentSaveWrites"] or row.get("savePolicy")
print(f"Adapter knowledge checked: {len(seen)} exact builds")
subprocess.run([sys.executable, str(root / "tools/export-game-adapters.py"), "--check"], check=True)
