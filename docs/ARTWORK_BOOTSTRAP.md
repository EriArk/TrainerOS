# Classic artwork on Flip — #60 bootstrap

This is the private development bridge from [the #58 corpus](CLASSIC_ART_CORPUS.md)
to the real Pokédex. It is **not** the frozen generic pack format (#57), Pack
Studio (#59), an artwork downloader, or the optional animated-sprite provider.

## Delivered feature chain

- List entries have 48-unit illustrations, and the selected entry has a larger
  preview. Detail keeps the illustration, reference stats and manual journal in
  separate readable columns. The bottom rail no longer overlaps the last list
  row, picture or journal text.
- **Up on detail** opens the attached illustration panel. Left/right browses
  candidates for that exact species/form; A saves the preferred image, B cancels.
  X still changes forms on detail. Start overlays the panel; L1/R1 cancels its
  draft and changes primary page. Y and L2/R2 cannot escape through this modal.
- The panel shows the original filename, source, attribution where actually
  known, identity-review status and a small-source warning. A source/category is
  not proof of an individual artist or a redistribution license.
- Trainer's favorite-species picker uses the same provider and its own 36-unit
  profile. Art is device-wide presentation; it never writes journal marks,
  favorites, game progress or save data.
- Coverage counts **exact forms**, not species with any candidate. Missing,
  ambiguous, corrupt and unavailable images retain a neutral labeled fallback.
  Another form's picture is never silently substituted. In particular the
  ambiguous Mega Meowstic, Neutral Xerneas and wheeled Miraidon cases remain
  unresolved. Hidden-lineage/shared-appearance questions are not guessed.

## Private installation and reproducibility

The complete raw corpus is retained unchanged under the actual application's
private data directory, alongside `artwork/bootstrap/`. No originals, rendered
derivatives, private source/path/hash manifests, screenshots or game data enter
Git. The raw index still records every image's provenance, ambiguity and review
status; supplemental visual/source review evidence remains in the private PC
handoff. The runtime reads only `bootstrap-index.json`, validated small PNG
derivatives and an optional atomically written `choices.json`.

```text
<TrainerOS app data>/artwork/
  raw-corpus-20260920/       complete originals and #58 metadata
  bootstrap/
    bootstrap-index.json   temporary native reader input
    pokemon-art-targets.json
    artwork-target-profiles.json
    images/                content-addressed, pre-sized derivatives
    choices.json           optional device-wide preferred images
```

Generate a **new** private staging directory using Python/Pillow:

```sh
python tools/bootstrap-classic-art.py --corpus <raw-corpus> \
  --reference data/pokedex.json --profiles data/artwork-target-profiles.json \
  --output <new-private-bootstrap>
```

The tool verifies master hashes, confines source reads to the corpus, rejects
existing output directories and preserves originals. Confirm the generated
manifest and rendered cases before replacing an installed bootstrap; retain the
previous directory and its choices for rollback. `--art-dir` selects an isolated
bootstrap for development. Normal production uses the app-data location;
ephemeral/tests use no installed art unless explicitly given that option.

`pokemon-art-targets.json` is the authoritative private identity handoff for
#57: all 1,579 reference species/form pairs, filesystem-safe nonlocalized
`pokemon/<species-id>/<form-id>` stems, nullable fallback relations, reviewed
default selection, eligible alternatives, original filename/source/hash and
confidence. It deliberately retains unresolved targets. Shared appearance may
be accepted later only with explicit identity evidence; no fallback relation is
fabricated merely to increase coverage. `bootstrap-index.json` additionally
retains per-image framing and source provenance.

The runtime accepts only bounded, correctly sized PNG derivatives inside its
installed directory. Invalid files are excluded, an invalid preferred ID falls
back to the same target's valid default, and failed choice writes preserve the
previous selection. This is a narrow development reader, not the complete
untrusted-pack/import transaction contract promised by #57.

## Device-derived presentation profiles

[Machine-readable profiles](../data/artwork-target-profiles.json) are inputs to
the bootstrap and the next contract work. At the 960×540 logical composition
and 1920×1080 physical window:

| Profile | Maximum logical slot | Derivative | Decoded RGBA bytes |
| --- | --- | --- | --- |
| `pokedexListArt` | 48×48 | 96×96 PNG | 36,864 |
| `pokedexDetailArt` | 240×200 illustration panel; 240×164 entry slot | 480×400 PNG | 768,000 |
| `speciesPickerArt` | 36×36 | 72×72 PNG | 20,736 |

The detail profile serves the larger source/alternative review panel as well as
the compact entry and list preview, so it retains enough pixels for that largest
consumer. Preserve aspect ratio and contain the visible content inside a 4%
inset. Crop only fully transparent margins; opaque backgrounds remain intact.
Use RGBA PNG/Lanczos, zero content bleed and centered framing. Original masters
are never decoded during browsing. If the visible source bounds require
upscaling to fill the safe rectangle, surface the softness warning.

Framing remains non-destructive metadata: alpha bounds, visual center, scale,
offset, baseline policy and upscale flag per profile. `--framing` accepts a
private hash-keyed scale/x/y adjustment file within explicit no-bleed limits;
there are no per-Pokémon QML offsets. Centering is an illustration framing
choice, not a claim that every creature shares a ground plane.

Images decode asynchronously. QML's accumulated image cache is disabled for
these assets, hidden slots clear their sources, and list prefetch stays bounded.
The OS filesystem cache may still retain compressed bytes. Qt scenegraph,
window/capture buffers and the rest of TrainerOS also consume memory; process
RSS is not the artwork cache size.

## Verification and remaining boundary

The non-testing ARM64 executable is installed with a preserved binary/database
backup. The install check verified its ELF architecture and read-back SHA-256,
successful dedicated-session return, unchanged schema 7, database integrity and
all 686 Adventure registrations. No game save, sleep setting, emulator setup or
recovery session was changed. Windows checks and the 39-test ARM64 suite passed;
affected QML scenarios were repeated after the final layout correction. The
three synthetic bootstrap/framing tests also passed on Windows and Linux.
The installed production build was then driven through InputPlumber: Pokédex
list/detail/source panel, Trainer's illustrated species picker, scrolling,
cancel and physical-Home routing all completed. Gamescope captures were visually
inspected; profile edits were cancelled and the shell was returned to Home.

The 2026-09-20 native run used the existing Gamescope/XWayland session after the
Armada update (kernel 7.2.3, Qt 6.11.2), with a 1920×1080 Qt window and 960×540
shell composition. The private transfer check verified all **2,067 unique raw
original hashes** and **4,404 derivative files**. **1,437 / 1,579** exact forms
resolve; 142 retain the unresolved boundary. Derivatives occupy **189,836,961
bytes (181 MiB)**; the complete raw archive was 917,370,880 bytes.

In the native controller/render run, the sampled 480×400 decodes took
**1.92–4.48 ms**. Across 129 eight-entry controller jumps, dispatch median/p95
was **61.27 / 69.11 ms** and observed request-to-frame median/p95 was
**63.07 / 119.93 ms**. These include the existing shell/input/render workload;
they do not establish 60-fps navigation or an artwork-only timing. The PC
no-pack control also showed significant baseline dispatch cost. Deeper general
shell profiling remains separate from selecting these bounded image sizes.

Process RSS was **207,400 → 348,064 KiB** across the native scenario, which also
captures full-window frames. The initial cache-enabled trial ended at 482,168
KiB; releasing hidden image sources and disabling accumulated image caching
reduced that observed end value by about 131 MiB. These are whole-process
observations, not an assertion of a fixed memory ceiling or a leak-free endurance
test. The per-profile decoded sizes above are deterministic.

`classic_art` tests exact-form isolation, rejected paths, unavailable files,
preferred-choice persistence, write failure and controller draft cancellation.
`artwork_qml_smoke` exercises the complete reference and missing-pack fallback
through SDL input. The private `--art-smoke-test --art-dir …` run checks rendered
base/regional/tall/wide/margin/special/missing forms, alternative selection,
modal priority and rapid navigation to species 1025 on the native Flip window.
Its private report records decode time, input-dispatch/request-to-frame timing,
RSS and source coverage. Request-to-frame includes image upload and scheduling;
it is not an isolated GPU-upload benchmark or a guaranteed frame-rate figure.

The generic pack contract, desktop Pack Studio, production Settings import,
additional authorship/style research, animated sprites and living Party remain
separate roadmap work. Owner clarification, 2026-09-20: Studio is the final
delivery stage after UI and asset structure stabilize; runtime validation and
Settings do not wait for it. The 142 unresolved raw forms remain visible work rather
than fictional completed coverage. Physical comfort/readability can be reviewed
by the owner after the automated native display/controller checks.
