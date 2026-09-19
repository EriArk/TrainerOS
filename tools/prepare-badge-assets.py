"""Reproduce the bundled badge derivatives from the pinned SteGriff checkout.

Development only: Python + Pillow + rsvg-convert. No runtime downloader.
Embedded reference bitmaps and editor/background layers are deliberately excluded.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from PIL import Image

REVISION = "89fde1733f2f819e7d3fe8036e68a8ade4c49579"
SETS = {
    "kanto-frlg": ("Kanto", "Boulder Cascade Thunder Rainbow Soul Marsh Volcano Earth".split()),
    "hoenn-rse": ("Hoenn", "Stone Knuckle Dynamo Heat Balance Feather Mind Rain".split()),
}
SVG = "{http://www.w3.org/2000/svg}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--renderer", default="rsvg-convert")
    args = parser.parse_args()
    revision = subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "HEAD"], text=True).strip()
    if revision != REVISION:
        raise SystemExit("Review the source revision before regenerating badge assets")
    dirty = subprocess.check_output(["git", "-C", str(args.source), "status", "--porcelain", "--untracked-files=no", "--", "svg/Kanto.svg", "svg/Hoenn.svg"], text=True)
    if dirty.strip():
        raise SystemExit("Source vector sheets differ from the pinned revision")
    args.output.mkdir(parents=True, exist_ok=True)
    ET.register_namespace("", SVG[1:-1])
    manifest = {"author": "Stephen Griffiths, 2011", "license": "CC BY 3.0 Unported",
                "licenseUrl": "https://creativecommons.org/licenses/by/3.0/",
                "source": "https://github.com/SteGriff/pokemon-badges", "revision": REVISION,
                "modifications": "Vector layer extracted; embedded reference images/editor layers removed; square padded crops rendered to 192px PNG. Badge paths and colors unchanged.",
                "sets": {}}
    with tempfile.TemporaryDirectory() as temp:
        for set_id, (region, names) in SETS.items():
            raw = (args.source / "svg" / (region + ".svg")).read_bytes()
            original = ET.fromstring(raw)
            root = ET.Element(SVG + "svg", {"width": "744.09448819", "height": "1052.3622047",
                                           "viewBox": "0 0 744.09448819 1052.3622047"})
            root.extend(e for e in original if e.tag == SVG + "defs" or e.get("id") == "layer1")
            assert not list(root.iter(SVG + "image"))
            vector = args.output / (region.lower() + "-vectors.svg")
            ET.ElementTree(root).write(vector, encoding="utf-8", xml_declaration=True)
            sheet = Path(temp) / "sheet.png"
            subprocess.run([args.renderer, "-o", str(sheet), str(vector)], check=True)
            alpha = Image.open(sheet).convert("RGBA").getchannel("A")
            occupied = [alpha.crop((x, 0, x + 1, alpha.height)).getbbox() is not None for x in range(alpha.width)]
            runs, start = [], None
            for x, present in enumerate(occupied + [False]):
                if present and start is None:
                    start = x
                elif not present and start is not None:
                    runs.append((start, x)); start = None
            assert len(runs) == len(names), (region, runs)
            entries = []
            for (left, right), name in zip(runs, names):
                bounds = alpha.crop((left, 0, right, alpha.height)).getbbox()
                top, bottom = bounds[1], bounds[3]
                side = max(right - left, bottom - top) * 1.14
                box = [(left + right - side) / 2, (top + bottom - side) / 2, side, side]
                root.set("viewBox", " ".join(f"{v:.5f}" for v in box))
                root.set("width", "192"); root.set("height", "192")
                crop = Path(temp) / "crop.svg"
                ET.ElementTree(root).write(crop, encoding="utf-8", xml_declaration=True)
                filename = region.lower() + "-" + name.lower() + ".png"
                output = args.output / filename
                subprocess.run([args.renderer, "-o", str(output), str(crop)], check=True)
                entries.append({"id": name.lower(), "name": name + " Badge", "image": filename,
                                "viewBox": box, "sha256": hashlib.sha256(output.read_bytes()).hexdigest()})
            manifest["sets"][set_id] = {"sourceFile": "svg/" + region + ".svg",
                "sourceSha256": hashlib.sha256(raw).hexdigest(), "vector": vector.name, "badges": entries}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
