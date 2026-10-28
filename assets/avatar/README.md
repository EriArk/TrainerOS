# Shared Trainer avatar pack

**28 October follow-up: runtime revision 6** reduces irises to 78%, adds a
cat smile and half-lidded expression, closed arcs and cartoon squint/heart/shock
eyes. All 14 IDs remain; round/almond now use separate modular eye surfaces.
Open mouths include teeth. There are 9 mouth poses and 14 expressions; the
[new review and evidence](../../docs/AVATAR_CARTOON_20261028.md) supersede the
revision-5 appearance below. Retained approved source files are unchanged.

**28 October: runtime revision 5** adds the shared facial rig: eight mouth poses,
nine expressions and four transient reactions. The procedural source is
[`build_expressions.py`](../../tools/avatar/build_expressions.py); the
[authoring and check record](../../docs/AVATAR_EXPRESSIONS_20261028.md) distinguishes
this new work from the approved originals below. Those editable originals are
unchanged. Revision 5 needs the matching updated resolver and SDK presenter.

The corrected **standalone base body**, without wearable clothing, shoes or hair,
is [`approved/base-body/trainer-body.glb`](approved/base-body/trainer-body.glb).
Its [Blender source and verification](approved/base-body/README.md) preserve the
approved smaller head and shorts repair. This is the original corrected body;
the separate eye-ready face revision is documented alongside the eye components.

**27 October: runtime revision 4** imports these approved body, tank, hair and eye
assets into the shared resolver. The authoring files remain separate; see
[the current SDK contract](../../sdk/AVATAR.md#approved-art-pack-revision-4-27-october-2026)
for all fourteen eye IDs, palette handling and legacy-profile compatibility.

The owner-approved 26 October authoring revision is preserved separately in
[`approved/2026-10-26`](approved/2026-10-26/README.md): an original thin-strap
sports tank, editable Blender source, and a head/hair scale of 0.92. Runtime
revision 4 includes it, the corrected shorts seam, the approved
[anime hairstyles](approved/2026-10-26/anime-hair/README.md) and
[12 chibi eye variants](approved/2026-10-26/chibi-eyes/README.md).
The downloaded tank below belongs only to historical runtime revision 3; its
geometry was not used for the current tank.

TrainerOS owns this pack. Games load a resolved character through
[`avatar.local`](../../sdk/AVATAR.md); they do not redistribute a copy in each
game. The owner authorized adding the prepared models to this repository on
24 October 2026. Intermediate sculpting files are not included.

This art is separate from the GPL source-code license. Inclusion is not a blanket
GPL relicensing of third-party meshes. The pack is still undergoing visual and
redistribution review, not a final public asset library.

| Component | Source / attribution | Changes |
| --- | --- | --- |
| Body, round/almond face | Owner-supplied approved Trainer character and accepted local revisions; upstream ownership record remains to be completed | Approved head scale 0.92 and shorts seam repair; skin and 46 correctives retained; sparse packing, face patches, palette masks |
| Swept hair | [Anime Hair 19](https://sketchfab.com/3d-models/anime-hair-19-b0a497ba4aad4b9aadb37a60b7a6c24a), neutralize, CC BY 4.0 | Fitted, simplified, recolorable, head binding |
| Ponytail | [Anime Hair 17](https://sketchfab.com/3d-models/anime-hair-17-bb43ed9ad59c4216a09c89557fdf0ee4), neutralize, CC BY 4.0 | Fitted, simplified, recolorable, head binding |
| Crop / bob | Owner-supplied Hair Anime20.fbx / Hair Anime14.blend from CGTrader | Fitted, simplified, recolorable; redistribution terms not yet independently verified |
| Historical tank (revision 3 only) | [Tank Top](https://sketchfab.com/3d-models/tank-top-d6b2561de39f423f9c7a2e7944485beb), Shift4cube / Mohan M, CC BY 4.0 | Loose fitting, reduced polygons, skin/corrective binding; source garment openings retained |
| Current sports tank / modular eyes | Owner-approved original authoring | Separate fitted tank and 12 eye pairs; shared rig and palette |
| Facial rig / mouths (revision 5) | Original procedural geometry and morph authoring for TrainerOS | Eight mouth poses, eye/brow expressions; visual owner acceptance pending |
| Flip-flops | [Flip Flops](https://sketchfab.com/3d-models/flip-flops-ac0eaae976a545caa52908e977f59ae0), Remie07, CC BY 4.0 | Simplified soles, fitted and mirrored pair, rebuilt straps, foot binding |

[CC BY 4.0 terms](https://creativecommons.org/licenses/by/4.0/) apply to the
attributed components and adaptations. No endorsement by their authors is implied.
Complete the body and CGTrader source/redistribution records before a public
binary asset release; the software license does not settle those records.

`packs/trainer-avatar-1/manifest.json` contains source and output hashes, rig,
palette/material roles and action mapping. `template.glb` is the shared body and
wardrobe; the other files are face patches and color atlases. Authored runtime
outputs and per-profile caches belong in user data, never in game packages.
