# TrainerOS acceptance - issues 63 and 64

Reviewed against the GitHub issue bodies on **2026-09-19**. These are accepted targets, not delivered features. [ROADMAP.md](ROADMAP.md) owns execution order; earlier steps 1-10, U1-U13 and all #1-62 commitments remain in that plan.

## Chronological Adventures inside a World

Source: [#63](https://github.com/EriArk/TrainerOS/issues/63). P1 can exercise the list with fixtures; P3 delivers shared metadata and the real sorted-list consumer together. No save research or emulator expansion is required.

Each Pokemon World keeps its region-first identity. Its Adventure list defaults to:

1. Explicit platform chronology, oldest system first.
2. Reliable original edition release date/year within that platform, where known.
3. Explicit catalogue/edition ordering.
4. Stable display-title/ID fallback.

Platform chronology is data-driven platform/catalogue metadata, not alphabetic display-name sorting or QML conditionals. Cover the platforms actually represented; adding a platform extends metadata without editing feature QML. Unknown platform identities remain visible after known systems, with deterministic ties and no invented generation/year.

Use the exact represented edition's platform. A remake belongs to its actual runtime platform; GBA/DS hacks stay with their actual content family, regardless of hack release year or story era. Prefer stable metadata to filename inference. Existing World relationships remain unchanged; sorting never invents cross-region membership.

Search and Linked/Missing filters preserve the same relative order. Restore selection by stable identity through filtering, return and restart. Existing platform badges explain the grouping; extra section headers must not consume the four-full-row target. Multiverse keeps its own system/game ordering unless explicitly changed later.

**Acceptance:** mixed platforms, same-system release ties, remake, GBA/DS hack, unknown metadata, future-platform insertion, search/filter and persisted focus all behave deterministically. No rename, file move, history merge or additional save read is part of this change.

## Accurate League badges and neutral fallback

Source: [#64](https://github.com/EriArk/TrainerOS/issues/64). This **supersedes the final crystal visual** in #47 and earlier design notes; it does not remove the current renderer before replacement is safe.

The issue nominates [SteGriff/pokemon-badges](https://github.com/SteGriff/pokemon-badges), attributing Stephen Griffiths (2011), CC BY 3.0 Unported, and the first 40 badges from Kanto through Unova Black/White 1. At import time verify the source revision, license, actual asset coverage and modification requirements. The planning review has not imported or independently cleared these assets. Do not describe the underlying Pokemon designs as TrainerOS-owned or assume later games are covered.

Resolve exact title/build -> verified progress provider -> semantic `badgeSetId` -> ordered badge IDs. An asset provider resolves those IDs to vector/render handles and provenance. Save readers own verified earned bits/order, not SVG paths or presentation names; QML owns neither save offsets nor filesystem mappings.

Before enabling a set for a build, verify badge names/order, design compatibility, remake reuse and extra/alternate progression. Region name alone is insufficient. Do not assume eight meaningful slots for every title. Home's compact tray and Hall/Journey's larger case reuse the same identities; future World/Trainer badge views must use that provider too.

- Exact earned bits render the corresponding earned/unearned badges.
- A count-only source shows the count with neutral unresolved mounts; it cannot invent which badges were earned.
- Unknown progress stays unknown, distinct from a verified zero.
- Unsupported titles, missing sets and later generations use deliberate neutral mounts, never another generation's set or crystals presented as genuine badges.

Preserve vector aspect, source SVG/provenance and a record of modifications. Normalize padding/viewBox only where needed and cache bounded derivatives. Mounts, recesses and shadows may fit TrainerOS's material style without changing recognizable badge silhouettes/colors.

Required attribution includes Stephen Griffiths, source, CC BY 3.0 license/link and any modifications. When licensed assets first ship, attribution must already be accessible through a small Credits surface and asset metadata; it cannot wait for the full P11 Help viewer. P11 incorporates the same credits into the guide. No general art download or redistribution permission is inferred for other sources.

**Scheduling:** prepare the provider contract, UI states and fixture rendering with the P1 screen work. Use early P7 alongside P1 for source/license review and asset preparation against usable slot sizes; the badge asset work is independent of the species-art #61 pipeline. The first real Home consumer may use existing verified FireRed/Emerald badge evidence after exact-set checks, without expanding a save parser. Prepare Journey badge layouts with fixtures now; new formats/builds and real Journey projections remain late P8 work. The owner explicitly prioritizes this badge work and Pokedex art/sprites as early UI, before heavier backend/save integrations. Do not add speculative parsers merely to finish this visual change.

**Acceptance:** exact-set and ordered-ID mapping, earned/unearned/count-only/unknown states, missing-set and later-title fallback, no cross-generation substitution, preserved SVG aspect/padding, compact Home and large Hall layouts, and included credits. Retire `BadgeCrystal` in normal game-backed surfaces only after replacement/fallback coverage works; temporary development fixtures may retain it. No implementation or asset import is claimed by this document.
