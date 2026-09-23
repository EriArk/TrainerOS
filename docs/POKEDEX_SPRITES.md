# Optional Pokédex companion

**Owner refinement, 2026-09-23:** the separate sprite/portrait viewer described
in the initial delivery evidence below has been removed. Normal Pokédex use
keeps the illustration and animated companion without asset-inspection controls.
Author-entry tooling and consolidated Credits move to final pack/Pack Studio
delivery; existing per-asset authors, sources and terms remain in private data.

The removal passed the full Windows 36/36 and ARM64 40/40 checks. The populated
native Flip scenario also passed: illustration/actor rendering, motion/pause,
Down retaining normal detail focus, Start/page restoration and direct Back to
entries. No provider, asset, save or personal-record migration accompanies it.
The non-testing update was installed with rollback copies; hash, database
integrity, unchanged Trainer and 686 Adventures were verified. Production
Gamescope captures after Down and Back confirm the simplified detail/list.

The existing illustration stays in both the species list and the selected entry.
An optional small sprite lives in the upper-right detail margin, leaving the
illustration, reference statistics and manual journal in place. This is a
reference-species presentation, not an observed Party member or save state.

## Controls and movement

- Up on detail retains illustration choices. Down keeps the normal detail;
  B returns directly to entries. No separate motion-strip/emotion browser,
  asset URL, attribution tray or sprite-related focus stop is added.
- Start covers and restores detail; L1/R1 retains primary navigation. Existing
  shared Y, paired triggers and illustration-panel modal priority remain intact.
- The detail sprite occasionally idles, sleeps for five seconds, or walks behind
  the right edge and immediately walks back. Each outward/inward leg takes 1.5
  seconds. A random choice occurs every 10–18 seconds; missing Sleep/Walk clips
  fall back to Idle, never invented recoloring or simulated save status.
- Only the bounded header margin hosts movement; no full-screen wandering,
  click target, focus stop, Party logic, physics or shared scene engine is added.
  Pause/reset when covered, hidden, inactive or Reduced Motion is enabled. The
  main sprite uses at most 2 logical units per source pixel, with a fit ceiling
  for oversized cells. Small and large source sprites are not all stretched to
  the same rectangle. This preserves source pixel scale, not literal metre ratios.

## Source and private import

The one-shot [import utility](../tools/import_pmd_details.py) reads a pinned
[PMDCollab SpriteCollab](https://github.com/PMDCollab/SpriteCollab) revision and
the existing TrainerOS reference. It requires Python/Pillow only during private
development import, not during normal shell use. Nothing is downloaded by the
running application. No upstream images, manifests or original sheets enter Git.

The initial mapping accepts only exact Standard roots and explicitly named
regional/Mega forms. Other special/gender/shiny identities stay unresolved;
no first-filename, fuzzy-form or ordinary-to-shiny substitution is allowed.
Each action's current upstream credit rows must resolve to known artists and
recorded terms. Official CHUNSOFT and unresolved terms are omitted from this
community-source import rather than assigned a community license.

The importer preserves per-action credits and original upstream files privately.
Recorded terms are [PMDCollab 1](https://github.com/PMDCollab/SpriteCollab/blob/master/license_history/LICENSE.PMDCollab_1.md),
[PMDCollab 2](https://github.com/PMDCollab/SpriteCollab/blob/master/license_history/LICENSE.PMDCollab_2.md),
or [CC BY-NC 4.0](https://github.com/PMDCollab/SpriteCollab/blob/master/license_history/LICENSE.CC_BY-NC_4.md),
as attached to the individual current contributions. Repository attribution is
not blanket permission for official assets. Follow the recorded terms for any
later redistribution; this delivery uses optional private local files.

Sprite strips retain original frame timing, in 1/60-second source units, and
one direction. The [PMD format](https://wiki.pmdo.pmdcollab.org/PMD_Sprite_Format)
orders frames across and directions down; SpriteBot's
[direction table](https://github.com/PMDCollab/SpriteBot/blob/master/Constants.py)
identifies right as row 2 and left as row 6. Aliased animations without a verified
resolution and missing facings remain unavailable. Normal/Happy/Worried/Pain
portraits remain separate assets; an illustration is never used as a sprite.

## Runtime boundary

`SpriteArt` reads a bounded optional `artwork/sprites/sprite-index.json` below
application data, or an explicit development `--sprite-dir`. This provisional
detail index is not the final pack-authoring format or a Settings pack manager.
The source index carries exact species/form keys, hashes, transformations,
original source URLs and separate motion/portrait metadata. Nothing writes ROMs,
saves, journal marks or personal progress.

The Qt image adapter performs decoding away from the UI thread. It verifies
canonical path containment and SHA-256 before decoding, caps file/pixel/frame
counts and uses a 4 MiB decoded-image cache. Individual PNGs are bounded to
1 MiB, 8,192 by 256 pixels and 262,144 pixels overall; frames are at most 256
pixels wide and 32 per strip. QML releases hidden image sources and disables its
accumulating image cache. This does not claim a 4 MiB total process-memory limit.
Animation uses one frame timer per visible sprite plus finite movement/nap timers.

## Acceptance and remaining work

Synthetic tests cover exact-form/shiny isolation, missing/corrupt files, hashes,
path rejection, pixel limits, per-action attribution, sheet direction/timing and
controller return. The extended artwork SDL scenario checks that the illustration
remains visible, the optional sprite loads, sleep/menu pause/walk return, normal
detail focus/Back without an inspector and paired-form fallback. A run without private assets tests
absence; it cannot certify animation. Native populated checks are recorded below.

### 2026-09-23 verification

Windows native build and all 36 checks passed, with the existing Hall rendering
check rerun separately after a parallel-load timeout. ARM64 build and all 40
checks passed. The four synthetic Python import checks passed. After extending
the motion scenario, its Windows and ARM64 checks passed again.

The populated ARM64 scenario ran through the Flip's native XCB/Gamescope path at
1920×1080, with zero QML warnings. It exercised Sleep, menu reset, WalkRight,
WalkLeft, return to the original position, Reduced Motion, controller modal
priority and portrait/form navigation. Frame capture stalls the render clock,
so the check awaits the actual finite return with a six-second deadline; the
captured native run completed in 3.75 seconds rather than assuming timer ticks
equal rendered animation time. This is injected-controller evidence, not a new
physical-button or sustained-performance acceptance claim.

The production-session visual check found a neighboring atlas frame leaking
beside a small sprite. The preview now clips to a single scaled cell rather than
the outer display area; the populated native scenario checks this boundary and
passed again with the installed illustration corpus present.

The non-testing ARM64 build was installed with binary/database rollback copies.
The installed hash, database integrity, unchanged Trainer profile, schema 7 and
686 Adventure records were verified. Gamescope screenshots from that production
session show Sprigatito detail, its portrait viewer and Cetitan detail; the larger
sprite remains larger within the header ceiling. Both reference illustrations
and manual-journal copy remain visible. The captured images were shown in chat;
private artwork and device reports are not committed.

The private import at SpriteCollab revision
`646f6c3208685f221cc6e900f6e927d58389c95d` has 3,422 images for 527 of 1,579 exact
forms; 486 forms have an Idle clip. Unmapped forms, official assets and unresolved
attribution are explicitly omitted. Credits and terms accompany each image.

Remaining: fuller form/source review, physical comfort and sustained performance
acceptance, actual Party/Playroom consumers and their P8 data gates. Generic
artwork-pack management and Pack Studio stay at the end per the owner's
2026-09-23 clarification. The minimal detail actor does not complete #52's living
Party scene or imply real individual records.
