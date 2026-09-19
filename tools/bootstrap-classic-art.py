#!/usr/bin/env python3
"""Private #60 device bootstrap. Not the future generic pack format (#57).

No network, no source edits, no guessing ambiguous forms. Build a NEW directory,
then atomically switch the device's installed directory after verification.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
from PIL import Image


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def inside(root, relative):
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError("Source outside corpus or missing")
    return path


def frame(image, size, adjustment=None):
    image = image.convert("RGBA")
    bounds = image.getchannel("A").getbbox()
    if not bounds:
        raise ValueError("Empty transparent image")
    visible = image.crop(bounds)
    adjustment = adjustment or {}
    scale = float(adjustment.get("scale", 1))
    dx, dy = float(adjustment.get("x", 0)), float(adjustment.get("y", 0))
    if not all(math.isfinite(v) for v in (scale, dx, dy)) or not 0.25 <= scale <= 1 or abs(dx) > .04 or abs(dy) > .04:
        raise ValueError("Framing would exceed safe bounds")
    ratio = min(size[0] * .92 / visible.width, size[1] * .92 / visible.height) * scale
    resized = visible.resize((max(1, round(visible.width * ratio)), max(1, round(visible.height * ratio))), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", tuple(size))
    position = (round((size[0] - resized.width) / 2 + dx * size[0]), round((size[1] - resized.height) / 2 + dy * size[1]))
    canvas.alpha_composite(resized, position)
    return canvas, {"alphaBounds": list(bounds), "visualCenter": [.5, .5], "scale": scale,
                    "offset": [dx, dy], "baseline": "centered illustration; no shared ground plane",
                    "upscaled": ratio > 1, "sourceContentSize": list(visible.size)}


def build(corpus, reference, profiles, output, adjustments=None):
    corpus, output = Path(corpus).resolve(), Path(output).resolve()
    if output.exists():
        raise ValueError("Output must be new; existing artwork is never overwritten")
    # Private provenance and derivatives must not enter a tracked source tree.
    for parent in (output, *output.parents):
        if (parent / ".git").exists() and not output.is_relative_to(parent / "work/research"):
            raise ValueError("Private output must be outside Git (or under ignored work)")
    index = read(corpus / "corpus-index.json")
    reference, profiles = read(reference), read(profiles)
    targets = {e["id"] + "/" + f["id"]: {"speciesId": e["id"], "formId": f["id"],
               "canonicalPath": "pokemon/" + e["id"] + "/" + f["id"], "fallback": None}
               for e in reference["entries"] for f in e["forms"]}
    images = {r["sha256"]: r for r in index["images"]}
    candidates = {key: [] for key in targets}
    for digest, record in images.items():
        matches = record.get("candidates", [])
        if len(matches) == 1 and matches[0] in targets and record["confidence"] in ("reviewed", "exact-label-provisional"):
            candidates[matches[0]].append(digest)
    for key, digest in index["associations"].items():
        if key not in targets or digest not in images:
            raise ValueError("Unknown association")
        if digest not in candidates[key]:
            candidates[key].append(digest)  # Explicit #58 reviewed association.
    output.mkdir(parents=True)
    (output / "images").mkdir()
    rendered = {}
    for digest in sorted({d for values in candidates.values() for d in values}):
        record = images[digest]
        path = inside(corpus, record["path"])
        with path.open("rb") as source:
            verified_digest = hashlib.file_digest(source, "sha256").hexdigest()
        if verified_digest != digest:
            raise ValueError("Master hash mismatch")
        files, framing, warnings = {}, {}, []
        with Image.open(path) as original:
            if original.width * original.height > 40_000_000:
                raise ValueError("Master exceeds decode limit")
            for profile, info in profiles["profiles"].items():
                result, meta = frame(original, info["size"], (adjustments or {}).get(digest))
                relative = f"images/{digest}-{profile}.png"
                result.save(output / relative, optimize=True)
                files[profile] = relative
                framing[profile] = meta
                if meta["upscaled"]:
                    warnings.append(profile + ": source below recommended content resolution")
        rendered[digest] = {"id": digest, "files": files, "source": record.get("source", ""),
                            "sourceName": record["sourceName"], "creator": record.get("creator"),
                            "confidence": record["confidence"], "artworkClass": record.get("artworkClass", ""),
                            "framing": framing, "warnings": warnings}
    for key, value in targets.items():
        chosen = index["associations"].get(key)
        source = images.get(chosen, {})
        value.update({"selected": chosen, "candidates": sorted(candidates[key]),
                      "status": "mapped" if chosen else "review-required" if candidates[key] else "missing",
                      "selectedSource": {"sha256": chosen, "source": source.get("source"),
                          "originalFilename": source.get("sourceName"), "confidence": source.get("confidence"),
                          "artworkClass": source.get("artworkClass")} if chosen else None})
    manifest = {"version": 1, "stage": "flip-bootstrap", "targets": targets, "images": rendered,
                "sourceIndexSha256": hashlib.sha256((corpus / "corpus-index.json").read_bytes()).hexdigest(),
                "summary": {"targets": len(targets), "mapped": sum(bool(v["selected"]) for v in targets.values()),
                            "derivativeImages": len(rendered), "derivativeBytes": sum(p.stat().st_size for p in (output / "images").iterdir())}}
    write(output / "bootstrap-index.json", manifest)
    write(output / "pokemon-art-targets.json", targets)
    write(output / "artwork-target-profiles.json", profiles)
    print(json.dumps(manifest["summary"]))
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("corpus", "reference", "profiles", "output"):
        parser.add_argument("--" + option, required=True)
    parser.add_argument("--framing")
    args = parser.parse_args()
    build(args.corpus, args.reference, args.profiles, args.output, read(args.framing) if args.framing else None)
