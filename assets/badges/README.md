# Pokemon League badge artwork

SVG recreations by **Stephen Griffiths, 2011**, licensed under
[Creative Commons Attribution 3.0 Unported](https://creativecommons.org/licenses/by/3.0/).
Source: [SteGriff/pokemon-badges](https://github.com/SteGriff/pokemon-badges),
revision `89fde1733f2f819e7d3fe8036e68a8ade4c49579`.
The source author references [Bulbapedia](https://bulbapedia.bulbagarden.net/wiki/Badge).
Pokemon designs remain the property of their respective owners; the license covers
the author's SVG recreations and does not make the underlying designs TrainerOS-owned.

This delivery includes the Kanto and Hoenn vector layers and sixteen rendered
derivatives, not all forty badges in the upstream repository. `manifest.json`
records source file hashes, revision, crop viewBoxes, derivative hashes, semantic
IDs and attribution. Original source SVGs are available at the pinned revision;
embedded raster reference images, editor metadata/background layers are not bundled.
The extracted vector sheets preserve the paths, gradients, silhouettes and colors.

Modifications for TrainerOS: isolate vector layers, compute non-distorting square
crops with padding, and render transparent 192 x 192 PNGs. Runtime uses these bounded,
cached derivatives rather than requiring an SVG plugin on ArmadaOS. The retained
vector sheets permit future resolution changes. No game graphics are imported.
In-app attribution is available with physical controls at Start -> Settings -> Credits.

Reproduce with Python/Pillow and librsvg's `rsvg-convert`:

```sh
python tools/prepare-badge-assets.py /path/to/pinned/source assets/badges
```

The importer refuses a different Git revision and never copies embedded raster
references. It is a manual development tool, not an app downloader. The source
checkout and comparison game graphics belong outside Git.

## Mapping boundary

Only the existing hash-verified English FireRed/original and Rev 1 providers use
`kanto-frlg`; the existing English Emerald provider uses `hoenn-rse`. This does
not enable LeafGreen, Ruby/Sapphire, other languages, hacks or later generations.
An unrecognized semantic set cannot select artwork by region-name fallback.

The existing save reader's ascending `FLAG_BADGE01_GET` through `FLAG_BADGE08_GET`
order was checked against the corresponding gym scripts in
[pret/pokefirered](https://github.com/pret/pokefirered/tree/master/data/maps) and
[pret/pokeemerald](https://github.com/pret/pokeemerald/tree/master/data/maps):

| Set | Ordered identities |
| --- | --- |
| kanto-frlg | Boulder, Cascade, Thunder, Rainbow, Soul, Marsh, Volcano, Earth |
| hoenn-rse | Stone, Knuckle, Dynamo, Heat, Balance, Feather, Mind, Rain |

These are stylized vector recreations, not pixel-identical game art. Initial
reference-sheet inspection is not a completed in-game visual acceptance. The owner
requested a later comparison of shapes, colors, order and earned state against
actual FireRed/Emerald game screens; that acceptance remains open.
