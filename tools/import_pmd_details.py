"""One-shot private PMDCollab detail import; not a pack manager or downloader service.

Requires Pillow. Keeps immutable upstream files/credits beside bounded previews.
Only exact Standard roots and explicitly named regional/Mega forms are mapped.
Official/unknown-credit work is reported rather than assigned a community license.
Output belongs outside Git. Reuse the same revision/cache to resume downloads.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor
from PIL import Image

REPOSITORY = "https://github.com/PMDCollab/SpriteCollab"
LICENSES = {"PMDCollab_1": "Attribution required (PMDCollab 1)",
            "PMDCollab_2": "Attribution / non-profit (PMDCollab 2)",
            "CC_BY-NC_4": "CC BY-NC 4.0"}
REGIONS = {"Alolan Form": "Alola", "Galarian Form": "Galar", "Hisuian Form": "Hisui",
           "Paldean Form": "Paldea", "Mega Evolution": "Mega", "Mega X": "Mega_X", "Mega Y": "Mega_Y"}


def mappings(reference, tracker):
    result, missing = [], []
    for entry in reference["entries"]:
        species = f'{entry["number"]:04}'
        node = tracker.get(species, {})
        for form in entry["forms"]:
            target = entry["id"] + "/" + form["id"]
            source = None
            if form["name"] == "Standard" and form["id"] == str(entry["number"]) and node.get("canon"):
                source = (species, node)
            elif form["name"] in REGIONS:
                matches = [(species + "/" + key, child) for key, child in node.get("subgroups", {}).items()
                           if child.get("name") == REGIONS[form["name"]] and child.get("canon")]
                if len(matches) == 1:
                    source = matches[0]
            if source:
                result.append((target, *source))
            else:
                missing.append({"target": target, "reason": "Exact source form requires review"})
    return result, missing


def credited(text, action, names):
    authors, licenses = set(), set()
    for row in csv.reader(io.StringIO(text), delimiter="\t"):
        if len(row) < 5 or row[2] != "CUR" or action not in row[4].split(","):
            continue
        if row[1] == "CHUNSOFT" or row[3] not in LICENSES or row[1] not in names:
            raise ValueError("Official or unresolved attribution/terms")
        authors.add(names[row[1]]); licenses.add(row[3])
    if not authors:
        raise ValueError("No current per-action credits")
    credit = ", ".join(sorted(authors))
    if len(credit) > 200:
        raise ValueError("Extended credit display requires review")
    return credit, " / ".join(LICENSES[x] for x in sorted(licenses))


def animation_strip(xml, png, action="Idle", direction=0):
    tree = ET.fromstring(xml)
    animations = {a.findtext("Name"): a for a in tree.findall("./Anims/Anim")}
    node = animations.get(action)
    if node is None or node.find("CopyOf") is not None:
        raise ValueError("Animation alias requires review")
    w, h = int(node.findtext("FrameWidth")), int(node.findtext("FrameHeight"))
    image = Image.open(io.BytesIO(png))
    if image.width * image.height > 2 * 1024 * 1024:
        raise ValueError("Source sheet exceeds decode budget")
    if not (0 < w <= 256 and 0 < h <= 256 and image.width % w == 0 and image.height % h == 0):
        raise ValueError("Invalid sprite grid")
    # PMD frames run across; SpriteBot directions: Down, DownRight, Right, UpRight,
    # Up, UpLeft, Left, DownLeft. Preserve original cell bounds and timing.
    durations = [int(x.text) for x in node.findall("./Durations/Duration")]
    if not durations:
        raise ValueError("Source frame timing is missing")
    frames = image.width // w
    if frames != len(durations) or frames > 32 or any(not 1 <= d <= 300 for d in durations):
        raise ValueError("Unsupported animation timing")
    if image.width * h > 262144 or image.width > 8192:
        raise ValueError("Animation strip exceeds runtime pixel budget")
    if direction >= image.height // h:
        raise ValueError("Requested facing unavailable")
    image.load()
    return image.convert("RGBA").crop((0, direction * h, image.width, (direction + 1) * h)), w, [max(16, round(d * 1000 / 60)) for d in durations]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path, default=Path("data/pokedex.json"))
    args = parser.parse_args()
    if not re.fullmatch(r"[a-f0-9]{40}", args.revision):
        parser.error("Use an immutable upstream commit SHA")
    root = args.output.resolve(); cache = root / "upstream" / args.revision
    (root / "images").mkdir(parents=True, exist_ok=True)

    def fetch(path):
        local = cache / path
        if local.is_file():
            return local.read_bytes()
        url = f"https://raw.githubusercontent.com/PMDCollab/SpriteCollab/{args.revision}/{path}"
        for attempt in range(3):
            try:
                request = urllib.request.Request(url, headers={"User-Agent": "TrainerOS-private-detail-import"})
                with urllib.request.urlopen(request, timeout=30) as response:
                    data = response.read(16 * 1024 * 1024 + 1)
                if len(data) > 16 * 1024 * 1024:
                    raise ValueError("Upstream file too large")
                local.parent.mkdir(parents=True, exist_ok=True)
                temporary = local.with_suffix(local.suffix + ".part")
                temporary.write_bytes(data); temporary.replace(local)
                return data
            except urllib.error.HTTPError as error:
                if error.code == 404 or attempt == 2:
                    raise
                time.sleep(2 ** attempt)

    tracker = json.loads(fetch("tracker.json"))
    names = {}
    for row in csv.reader(io.StringIO(fetch("credit_names.txt").decode("utf-8-sig")), delimiter="\t"):
        if len(row) >= 2 and row[0].strip():
            names[row[1]] = row[0]
    for license_id in LICENSES:
        fetch(f"license_history/LICENSE.{license_id}.md")
    reference = json.loads(args.reference.read_text(encoding="utf-8"))
    jobs, missing = mappings(reference, tracker)

    def import_target(job):
        target, source, node = job
        choices, assets, errors = [], {}, []
        for kind, action, direction in [("sprite", "Idle", 0), ("sprite", "Sleep", 0), ("sprite", "Walk", 2), ("sprite", "Walk", 6),
                                       ("portrait", "Normal", 0), ("portrait", "Happy", 0), ("portrait", "Worried", 0), ("portrait", "Pain", 0)]:
            if action not in node.get(kind + "_files", {}):
                continue
            # Avoid fetching official work merely to discover the same license boundary.
            if node.get(kind + "_credit", {}).get("primary") == "CHUNSOFT":
                errors.append({"target": target, "kind": kind, "reason": "Official asset - not a community grant"})
                continue
            folder = f"{kind}/{source}"
            try:
                credit, license_label = credited(fetch(folder + "/credits.txt").decode("utf-8-sig"), action, names)
                original = folder + (f"/{action}-Anim.png" if kind == "sprite" else f"/{action}.png")
                raw = fetch(original)
                if kind == "sprite":
                    preview, frame_width, durations = animation_strip(fetch(folder + "/AnimData.xml"), raw, action, direction)
                else:
                    preview = Image.open(io.BytesIO(raw))
                    if preview.size != (40, 40):
                        raise ValueError("Unexpected portrait dimensions")
                    preview.load()
                    preview = preview.convert("RGBA")
                    frame_width, durations = 40, [120]
                if preview.getbbox() is None:
                    raise ValueError("Empty image")
                output = io.BytesIO(); preview.save(output, format="PNG")
                data = output.getvalue(); digest = hashlib.sha256(data).hexdigest()
                (root / "images" / (digest + ".png")).write_bytes(data)
                # Attribution is scoped to a target/action, even if pixels happen to match.
                record = {"file": f"images/{digest}.png", "credit": credit, "license": license_label,
                    "source": f"{REPOSITORY}/blob/{args.revision}/{original}",
                    "originalSha256": hashlib.sha256(raw).hexdigest(),
                    "kind": kind, "action": ("WalkRight" if direction == 2 else "WalkLeft") if action == "Walk" else action, "frames": len(durations), "frameWidth": frame_width, "durations": durations,
                    "transform": f"{action} strip, source direction {direction}, original timing" if kind == "sprite" else "Unscaled RGBA portrait"}
                if digest in assets and assets[digest] != record:
                    raise ValueError("Shared pixels / action identity needs review")
                assets[digest] = record
                facing = "Walk right" if direction == 2 else "Walk left"
                choices.append({"asset": digest, "label": "Full-body sprite · " + (facing if action == "Walk" else action) if kind == "sprite" else "Portrait · " + action})
            except (OSError, ValueError, ET.ParseError) as error:
                errors.append({"target": target, "kind": kind, "action": action, "reason": str(error)})
        return target, choices, assets, errors

    targets, assets = {}, {}
    with ThreadPoolExecutor(max_workers=4) as pool:
        for index, (target, choices, images, errors) in enumerate(pool.map(import_target, jobs)):
            if choices:
                # Identical bytes with different provenance are not silently reattributed.
                for key, value in images.items():
                    if key in assets and assets[key] != value:
                        choices = [row for row in choices if row["asset"] != key]
                        missing.append({"target": target, "reason": "Shared pixels / credit identity needs review"})
                    else:
                        assets[key] = value
                if choices:
                    targets[target] = choices
            missing.extend(errors)
            if index % 50 == 0:
                print(f"Reviewed {index + 1}/{len(jobs)} exact targets; {len(targets)} have assets", flush=True)
    manifest = {"version": 1, "kind": "pmd-detail-preview", "revision": args.revision,
                "targets": targets, "assets": assets}
    staged = root / "sprite-index.json.new"
    staged.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    staged.replace(root / "sprite-index.json")
    (root / "import-report.json").write_text(json.dumps({"forms": reference["formCount"],
        "availableForms": len(targets), "assets": len(assets), "unresolved": missing}, indent=2), encoding="utf-8")
    print(f"Complete: {len(targets)}/{reference['formCount']} forms, {len(assets)} detail assets", flush=True)


if __name__ == "__main__":
    main()
