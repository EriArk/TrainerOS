# Adapter, trust and asset acceptance — #91–97

Reviewed 2026-09-27, including comments added to #51, #57, #61 and #92,
and the owner's Pack Studio clarification. [ROADMAP](ROADMAP.md#unified-execution-order--existing-work-and-new-issues)
is the execution queue. This register adds acceptance without replacing earlier
unfinished work. Except for the bounded #91 delivery, these are **planned**
capabilities, not installed behavior.

## #91 — durable knowledge and reusable implementation

[Issue #91](https://github.com/EriArk/TrainerOS/issues/91): consult and update the
[knowledge base](adapters/README.md) throughout exact-game work. Keep identities,
sources, dated findings, failures, unknowns and per-capability evidence distinct.
Synthetic, private-copy, allowed-byte-delta, device, normal-game and rollback
proof are different evidence levels. Unknown builds never inherit write support.

Delivered: three exact Gen III records/profiles and a separately buildable source
copy, tables and example. Export/check tools detect drift. The owner's reusable
copy requirement remains mandatory for future adapters. These source snapshots
are not the installable runtime format in #92.

## #92 — installable runtime adapters

[Issue #92](https://github.com/EriArk/TrainerOS/issues/92) builds on #89/#91.
Version stable module identity, API/schema, exact game builds, save formats,
semantic capabilities, reference data, readers, candidate transforms, validation,
extensions and test/source/license metadata. Keep research records, executable
packages, #90 franchise presentation packs and artwork packs separate.

Compare in-tree/native modules, manifest/native combinations, isolated helper
protocols, WASM and declarative rules with bounded hooks before choosing a
format. Evaluate ARM64/x86-64, crashes, performance, isolation, versioning,
testing, size, reproducibility, community safety and external reuse. No public
store or second semantic framework is required.

Define install/update/rollback/disable/remove, API/host incompatibility, conflicts
over an exact build, downgrade, corruption and package provenance/signatures.
Unknown versions fail closed; updates never silently widen verified write powers.
The host retains file resolution, owners, runtime exclusion, backups, revision
checks, atomic commit/readback/recovery, network transport, UI and processes.

Do not freeze v1 after Emerald/FireRed. Proof requires Emerald, another state-rich
game and a structurally different non-RPG. Schedule the package work after that
cross-game evidence; no speculative loader during current Emerald work.
#95 extraction will be an optional capability of this same contract.

## #93–94 — lineage and verified sessions before trusted peer use

[#93](https://github.com/EriArk/TrainerOS/issues/93) gives Trainers a stable
signing identity separate from their PIN and a verifiable append-only save
lineage. Bind owner, Adventure/build, runtime namespace, save/parent hashes,
source revision, operation, relevant time and adapter/version/capability.
Record ordinary gameplay, protected edits, restores and peer transfers explicitly.
Failed or rolled-back edits must not create a valid successor.

Distinguish imported/unknown history, locally managed continuity and stronger
verified provenance. Unexpected external changes preserve both user data and
earlier history, lose verified continuity and remain playable locally. Never
call an old imported save cheating or sign it as having a known past. Entity
history is TrainerOS metadata; do not rewrite native OT/origin fields.
Peers exchange bounded identity/lineage/transaction proofs, not arbitrary private
save contents. Compose with #76 namespaces, #73 backup/restore, #71 rollback,
adapter updates, relinking and separate playthroughs. Restore must preserve key
identity and handle duplicate/conflicting branches rather than forging continuity.

[#94](https://github.com/EriArk/TrainerOS/issues/94) defines what observable
session evidence permits a verified successor: exact content/runtime/version,
controlled config/save namespace, cheat state, source lineage, launch/return,
and enforceable debugger/config/external-write conditions. Runtime adapters
report bounded evidence; QML and parsers do not implement anti-cheat policy.
Legal-looking values are not proof of fair play. Loss of a condition downgrades
trust without terminating gameplay or destroying saves.

Investigate hardware key storage and document software-only/root-control limits.
Local signatures cannot prove resistance to a fully controlled host. Stronger
boot/device/server attestation and revocation are future research, not an offline
dependency. Define policies per feature: informed casual exchanges may accept
unknown history; verified or competitive uses require their proven conditions.
No ordinary single-player feature gains a compulsory verified-session gate.

Place these prerequisites before trust-sensitive #45 Link operations. A missing
second device or trust gate does not block Emerald/Journey or other independent
queue rows. Preserve exact-pair writers and durable two-device recovery proof.

## #95–97 — final ROM-native asset stage

[#95](https://github.com/EriArk/TrainerOS/issues/95): exact-game adapters may
extract actual supported asset kinds from bounded, identified user content.
Offsets, compression, palettes, frame layouts and entity/form mappings belong
to the adapter. Compare data/code needs without a second extractor plugin format.
The host controls access, exact identity, permissions, storage and errors;
extractors return semantic results, not arbitrary filesystem effects.

[#96](https://github.com/EriArk/TrainerOS/issues/96): a host-owned semantic
provider/cache separates these outputs from QML. Cache identity includes exact
content and extractor version. It is bounded, accounted, invalidated, deletable
and reproducible; it is never gameplay truth. Handle missing ROMs, unsupported
builds/kinds/forms, failed extraction and rebuilds. Never pass ROM offsets to UI.

Keep derived assets local. Exclude their bytes from Git, images, cloud uploads,
support bundles, portable Trainer backups and distributable artwork packs.
Back up rebuild metadata instead. Explicit content export is a separate decision.
No extractor may invent walk/sleep/emotion frames and claim they came from ROM;
presentation-owned bob/hop motion remains distinct from native animation.

[#97](https://github.com/EriArk/TrainerOS/issues/97): start only after current
UI/animation consumers, artwork packs, adapter boundaries/knowledge, package
direction and several materially different adapters are ready. Audit actual
coverage, useful screen slots, animation gaps, performance/storage and packaging.
Adopt incrementally: Dex icons/front images, Party/Boxes, optional Home, then
evaluate Playroom. Keep richer external packs first-class. Reconcile provenance,
Credits, exports and image contents; repeat affected image/OTA/Help/visual checks.
A clean release remains functional with neutral original placeholders and no
bundled commercial graphics. Existing PMDCollab/artwork work is not replaced,
removed, delayed or rewritten now; #51/#57/#61 comments explicitly preserve it.

## Owner clarification — Pack Studio and avoiding redundant extraction

The final #59 Windows/Linux constructor covers **both artwork and sprite packs**.
It is an authoring/import tool, not a bundled collection of the present images.
Use the current ClassicArt/bootstrap and PMDCollab/SpriteArt sets as private
reference examples for import, mapping, preview and round-trip validation.
Preserve their separate semantic identities through a shared authoring workflow.

Allow users to add their own illustrations, sprite sheets/frames and reaction
portraits, map species/forms/variants and actions/directions, configure animation
timing/anchors/scale, preview actual consumers and export validated packs.
Preserve originals, non-destructive framing, contributor/source/terms metadata,
partial coverage and explicit unavailable animations. Use the shared runtime
validator and device-proven targets. Do not freeze formats or build Studio now.

**Pack presence is checked before extraction, not just before display.** An
installed compatible active pack supplies its artwork/sprite family; do not
start, queue or rebuild ROM extraction for that family behind it. An art-only
pack and a sprite-only pack can be handled independently. Missing entries in
that pack use a configured compatible fallback pack or neutral placeholder;
they do not trigger an unsolicited ROM extraction pass while the pack is active.
Unsupported/unrelated packs do not suppress usable assets for other games.

Without a compatible active pack for a family, use available local ROM-native
cache or extract supported assets on demand, then neutral placeholders if absent.
Installing a pack suppresses redundant pending extraction; selecting/removing
one re-evaluates source priority. Keep existing cache reusable rather than
deleting it merely because a pack was installed. No extraction on the render
thread or resident acquisition job is implied.

This owner refinement supersedes an interpretation of #96/#97 that would always
extract ROM assets first and merely hide them under a pack. At the final stage,
test pack-present/partial/absent, art-only/sprite-only, selection/removal, cancelled
jobs, warm-cache reuse, missing ROM and failed extraction, alongside #57's
validation/update/rollback and #59's authoring/export/reimport acceptance.
