#!/usr/bin/env python3
"""Build the bundled factual field guide from a pinned PokéAPI CSV snapshot.

Only numeric/reference facts and names are included. No sprites, cries, flavor
text or other game assets are downloaded. Network is used only by this tool,
never by the application. Pass --source-dir to reuse an existing CSV download.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import urllib.request

REVISION = "4b82c204ddd19ecb8eda2ea044ccb59e222b721c"
FILES = ("pokemon_species", "pokemon_species_names", "pokemon", "pokemon_types", "types", "type_names",
         "pokemon_stats", "stats", "pokedexes", "pokemon_dex_numbers", "regions", "pokemon_forms", "pokemon_form_names")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("data/pokedex.json"))
    args = parser.parse_args()
    args.source_dir.mkdir(parents=True, exist_ok=True)
    provenance = {}
    for name in (*FILES, "LICENSE"):
        filename = name + (".md" if name == "LICENSE" else ".csv")
        relative = "LICENSE.md" if name == "LICENSE" else "data/v2/csv/" + filename
        url = f"https://raw.githubusercontent.com/PokeAPI/pokeapi/{REVISION}/{relative}"
        # Always verify supplied cache bytes against the pinned upstream snapshot.
        data = urllib.request.urlopen(url, timeout=30).read()
        path = args.source_dir / filename
        if path.exists() and path.read_bytes() != data:
            raise ValueError(f"Cached source does not match pinned upstream: {filename}")
        path.write_bytes(data)
        provenance[filename] = hashlib.sha256(data).hexdigest()

    def rows(name):
        with (args.source_dir / (name + ".csv")).open(encoding="utf-8", newline="") as stream:
            return list(csv.DictReader(stream))

    species = {r["id"]: r for r in rows("pokemon_species")}
    names = {r["pokemon_species_id"]: r["name"] for r in rows("pokemon_species_names") if r["local_language_id"] == "9"}
    pokemon = {r["id"]: r for r in rows("pokemon")}
    form_names = {r["pokemon_form_id"]: r["form_name"] for r in rows("pokemon_form_names") if r["local_language_id"] == "9"}
    type_names = {r["type_id"]: r["name"] for r in rows("type_names") if r["local_language_id"] == "9"}
    types = {}
    for r in sorted(rows("pokemon_types"), key=lambda r: int(r["slot"])):
        types.setdefault(r["pokemon_id"], []).append(type_names[r["type_id"]])
    stats = {}
    for r in rows("pokemon_stats"):
        stats.setdefault(r["pokemon_id"], {})[int(r["stat_id"])] = int(r["base_stat"])
    regions = {r["id"]: r["identifier"] for r in rows("regions")}
    dex_regions = {r["id"]: regions[r["region_id"]] for r in rows("pokedexes") if r["region_id"]}
    collections = {}
    for r in rows("pokemon_dex_numbers"):
        if r["pokedex_id"] in dex_regions:
            collections.setdefault(r["species_id"], set()).add(dex_regions[r["pokedex_id"]])
    forms = {}
    for form in rows("pokemon_forms"):
        p = pokemon[form["pokemon_id"]]
        label = form_names.get(form["id"]) or form["form_identifier"].replace("-", " ").title() or "Standard"
        values = stats.get(p["id"], {})
        record = {"id": form["id"], "name": label, "types": types.get(p["id"], []),
                  "heightDm": int(p["height"]) if p["height"] else None,
                  "weightHg": int(p["weight"]) if p["weight"] else None,
                  "stats": [values.get(i) for i in range(1, 7)]}
        assert record["types"], form
        order = (p["is_default"] != "1", form["is_default"] != "1", int(form["form_order"]), int(form["id"]))
        forms.setdefault(p["species_id"], []).append((order, record))
    entries = []
    for number, s in sorted(species.items(), key=lambda item: int(item[0])):
        entries.append({"id": s["identifier"], "number": int(number), "name": names[number],
                        "collections": sorted(collections.get(number, set())),
                        "forms": [f for _, f in sorted(forms[number])],
                        "family": [v["identifier"] for v in species.values() if v["evolution_chain_id"] == s["evolution_chain_id"]]})
    used = {region for entry in entries for region in entry["collections"]}
    guide = {"version": 1, "source": "PokéAPI", "revision": REVISION,
             "speciesCount": len(entries), "formCount": sum(len(e["forms"]) for e in entries),
             "collections": [{"id": name, "name": name.title()} for name in regions.values() if name in used],
             "entries": entries}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(guide, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8", newline="\n")
    Path("data/pokedex-source.json").write_text(json.dumps({"revision": REVISION, "sha256": provenance}, indent=2) + "\n", encoding="utf-8", newline="\n")
    license_path = Path("data/licenses/PokeAPI.txt")
    license_path.parent.mkdir(parents=True, exist_ok=True)
    license_path.write_bytes((args.source_dir / "LICENSE.md").read_bytes())
    print(f"{guide['speciesCount']} species, {guide['formCount']} forms, {len(used)} regional collections; {args.output.stat().st_size} bytes")


if __name__ == "__main__":
    main()
