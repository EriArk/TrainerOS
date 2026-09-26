# TrainerOS acceptance reconciliation — issues #69–90

Reviewed **2026-09-26** against all 88 GitHub issues, including issue bodies and
comments. This register preserves acceptance; [ROADMAP](ROADMAP.md#unified-execution-order--existing-work-and-new-issues)
is the single execution queue. Everything newly accepted here is **planned**,
not a claim about the installed build. Earlier registers remain applicable except
for the explicit replacements below. Issues #66/#67 are not issue records in the
reviewed inventory; no requirements are inferred for them.

## Decisions that replace earlier instructions

| Earlier direction | Accepted replacement and retained obligations |
| --- | --- |
| #11 remove Steam; #39 install/restore Steam UI | Both issues were closed as **superseded**, not delivered. Their closure comments retain Steam Gaming Mode. #69 adds installed Steam games to Multiverse. Preserve Steam data, non-Steam independence and Plasma recovery; do not build a removal/restore feature. |
| #26 Switch Player under Power | Updated comment and #86 move Switch Trainer to the first level of Start. Confirmed shutdown/restart, write draining and held-button guards remain. |
| #28 Multiverse A chooses for Home; Pokémon requires details | Updated #28 comment and #87 make short A launch playable games in both wheels. Setup is the fallback for missing/unconfigured games. Y selection remains non-launching. |
| #31 Home X changes domain | Updated comment and #84 use L2/R2. Independent per-Trainer selections/history remain. #90 later extends the two domains to a useful set of franchise packs. |
| #43 only pairs; fixed Pokédex/Hall primary names | Updated comment and #83–85 keep five primaries but target **Home / Worlds / Pokémon / Trainer / Journey**, with cyclic secondary faces. |
| #47 live progression and archive under Hall | Updated comment and #85 separate Journey / Hall / RA faces. #47 still owns live-versus-historical semantics; #48 still owns independent external RA truth. |
| All control hints only in the footer | #88 moves persistent L1/R1 and L2/R2 indication into the header/chassis. Contextual action hints remain at the bottom with no content duplicates and no whole-row shrink-to-fit. |
| Image only if packages prove insufficient | #70 explicitly chooses a reproducible Armada-based **image** as the consumer distribution boundary. Native package/session deployment remains the development building block. No new low-level OS/hardware rewrite is implied. |
| Sleep permanently outside the active queue | #79 schedules a later dedicated reliability phase. Suspend stays disabled until its physical gates pass; other system work must not enable it incidentally. |
| Pokémon/Multiverse as a permanent binary | #90 accepts franchise packs, with Pokémon first and generic Multiverse fallback. This changes presentation ownership, not ROM identity or the exact-save safety contract. |

The owner’s earlier explicit amendments also remain: real confirmed #68 purchases;
English Emerald first; no manual Dex Seen/Caught/notes editor (legacy rows inert);
ScreenScraper live access paused; no emulator savestate workflow; generic **artwork**
packs, Pack Studio and consolidated Credits last. Hall memories remain supported.

## Navigation and ordinary use — #83–88

| Section / surface | Target behavior |
| --- | --- |
| Home | L2/R2 changes Pokémon / Multiverse, later available pack contexts. Restore independent current selection, history and useful navigation; X no longer changes domain. Fixed Home A launches; Y selects without launching within that domain. |
| Worlds | L2/R2 retains Pokémon region-first / generic Multiverse system-first browsing. Short A launches a playable wheel selection or opens setup. No mandatory detail screen. Hold A retains contextual management without accidental launch or an artificial long delay on short A. Return restores wheel, system/World, selection and filters. |
| Pokémon | Wrapped L2/R2 faces: Pokédex, Party, Boxes, Center, Playroom, Shops & Traders. Party/Boxes are peers; remove X-to-Storage and nested Activities as their normal entry route. Center holds Heal, Backup/Recovery and later Link Counter. Practice is a Playroom action, not a mandatory extra face. |
| Journey | Wrapped L2/R2 faces: live Journey, historical Hall of Fame, external RetroAchievements. Each remembers local route/focus; one selected Adventure, no second Hall game picker. |
| Trainer | Remains a primary section. Do not invent empty secondary faces. |
| Start | Quick Volume/Brightness, Switch Trainer, Settings, System modes, Power. No Pokémon Center/content destination. Steam and Desktop/Maintenance are deliberate system-mode choices. Power contains Restart/Power off/Cancel, and Sleep only after #79 passes. |

Use a compact face-position treatment in the existing header/chassis, not a
large second tab bar. L1/R1 stays primary navigation. B only backs out, closes or
cancels; it never opens Filters or flips a face. At a root with nowhere to return
it can be inert. Pokédex Up reaches filters. Contextual footer hints show only
available actions, short consistent labels and readable fixed sizing.

Face visibility follows **exact-build** capabilities: an unsupported feature
need not occupy an empty permanent face; a supported feature with a missing or
invalid save can retain an honest unavailable face. Reference Dex remains useful
offline. A visible capability is not permission to write.

Keyboards, destructive confirmations, library edits, guarded game exit and active
transactions retain input/context priority. Changing face must never retarget a
pending purchase/heal. Preserve each face's useful focus, box and filter state;
normalize legacy saved routes without deleting personal records. Y remains shared
where applicable; Worlds filtering and other modal-local actions retain priority.

Acceptance: physical Flip loops in both directions through all available faces,
restart/state restoration, two Trainers, capability changes, modal cancellation,
short/held A, missing-game setup, real launch/return and Start opened/closed from
every primary. Inspect installed screenshots and native-size footer density.

Sources: [#83](https://github.com/EriArk/TrainerOS/issues/83),
[#84](https://github.com/EriArk/TrainerOS/issues/84),
[#85](https://github.com/EriArk/TrainerOS/issues/85),
[#86](https://github.com/EriArk/TrainerOS/issues/86),
[#87](https://github.com/EriArk/TrainerOS/issues/87),
[#88](https://github.com/EriArk/TrainerOS/issues/88).

## Exact integrations and franchise experiences — #74–76, #82, #89–90

**#89 is a bounded architecture audit, not an authorized rewrite.** Map current
launch adapters, exact-save readers/mutators, feature coupling and host transaction
ownership. Answer whether a thin descriptor composing existing narrow interfaces
is enough, or whether actual duplication warrants a change. Keep #42 as the one
save framework. Compare compiled in-tree providers, versioned manifests/native
code, declarative rules with hooks, process helpers and later sandboxable formats;
record versioning, errors/unknown, extension and private-fixture testing strategy.
Do not choose a dynamic ABI or external executable plugin system prematurely.

Deliver a short note with the current-state map, concrete coupling findings,
recommended conceptual interfaces, portability decision, Emerald migration impact
and a host-versus-adapter responsibility table. Exact-build parsing/checksums and
bounded semantic transforms can be portable; resolvers, filesystem authority,
live-writer checks, backups, atomic replacement, ownership, persistence, network,
processes and UI stay host-owned. Do not depend on ELIGEN or another project.
Any justified implementation is a separately reviewed follow-up slice; creating
or posting new GitHub issues is not part of this planning change.

**#74 capability presentation** belongs in Adventure Properties and entry-point
availability. Human labels distinguish unsupported exact builds, unavailable
sources and incomplete configuration. Cover launch/return, personal saves,
progress/Dex, Party/Storage, Center/Shops/Journey/RA and protected writes where
actually verified. Effective capabilities reflect policy without overwriting
provider evidence. Relinking/build/source/runtime changes refresh them; generic
Multiverse launch/media-only titles remain useful.

**#75 read-only integration** is a persistent device-wide policy enforced below
QML at the common mutation boundary. Reads and copy-only backups continue;
TrainerOS healing, purchases, moves/release, import/trade and restore are blocked.
No recovery exception is chosen by this plan. Ordinary in-game saving is unaffected.
Tests enumerate every registered writer and prove central rejection, including
future writers denied by default. Keep ordinary screens quiet, with concise
actionable availability rather than repeated warnings.

**#76 save ownership** extends the proven mGBA namespace one officially supported
runtime at a time: melonDS, Dolphin and relevant RetroArch/other routes. Verify
actual directories/configuration, external launch behavior, backup/restore and
legacy policy separately. Personal / Shared / Unsupported are distinct. Bind the
owner at entry/launch, block switching during a game/write, never fall back to a
legacy save for a new Trainer. Physical two-Trainer launch → save → exit → relaunch
and return to the original lineage are required before advertising Personal.

**#82 second full Pokémon vertical** follows completed meaningful Emerald
consumers and #89. FireRed is preferred, subject to exact-build evidence and the
recorded existing-reader fault. Cover identity, validation, owner save routing,
summary/badges, species Dex, Party/Storage, Journey/Champion where provable and at
least one independently verified protected write if feasible. Reuse consumers
and transaction services; no Emerald-shaped fiction or title checks in QML.
Use private save copies, physical in-game readback and Emerald regressions.

**#90 franchise packs** are game-family experiences, not artwork packs, themes or
user-facing genre modes. Pokémon is the first pack; generic Multiverse retains
unclaimed games. A pack may own Home, library grouping, secondary faces,
terminology, reusable semantic/presentation components and exact-title overrides.
Adapters remain presentation-agnostic; a pack resolver maps verified capabilities
to faces/widgets/actions without filename/title conditionals in QML.

Define a small versioned registry, per-Trainer per-pack current choices and useful
navigation, cyclic Home contexts, scoped Y, grouping and migration. Keep stable
Adventure IDs, history, media, ownership and save lineage when an explicitly
matched game moves from Multiverse to a pack; never duplicate ROM/library rows.
Ambiguous classification needs review. Do not expose a long catalogue of empty
packs. Shell grammar stays stable and a pack's current choice cannot overwrite
another's. Exact controls beyond the initial #84 grammar get physical validation.

First prove the domain model with existing Pokémon/Multiverse consumers; then
deliver **one non-Pokémon franchise with multiple games, at least two sharing
components and one title-specific override**, while fallback remains functional.
Choose the franchise from adapter readiness, not an arbitrary promise of FF/NFS/
Metal Slug/Sonic support. This requirement is retained even though deep format
research and externally authored adapter loading are not prerequisites for the
current Emerald shop batch. Pack presentation stays separate from executable
adapter code and from final art-pack authoring.

Sources: [#74](https://github.com/EriArk/TrainerOS/issues/74),
[#75](https://github.com/EriArk/TrainerOS/issues/75),
[#76](https://github.com/EriArk/TrainerOS/issues/76),
[#82](https://github.com/EriArk/TrainerOS/issues/82),
[#89](https://github.com/EriArk/TrainerOS/issues/89),
[#90](https://github.com/EriArk/TrainerOS/issues/90).

## Steam, device support and readiness — #69, #77–81

| Issue | Required delivery / proof |
| --- | --- |
| [#69 Steam](https://github.com/EriArk/TrainerOS/issues/69) | Bounded installed-library/app-manifest discovery; stable app/library identity through relocation, uninstall and reinstall; safe local media mapping; supported Steam launch/return with original TrainerOS context. Preserve unavailable history and keep Steam's history separate. No duplicate ROM-tree copies, credential UI or arbitrary QML commands. Steam retains store/downloads/Proton/input management and Gaming Mode; non-Steam launches stay independent. Physical Armada validation is mandatory; installed does not mean a title runs on ARM64. Apply #87 immediate wheel launch. |
| [#77 input](https://github.com/EriArk/TrainerOS/issues/77) | Profile matching, semantic face buttons/shoulders/Home, axes/triggers, dead zones/repeat/quirks; live test, minimal remap/calibration/reset UI. Overrides separate from shipped defaults; reserve recovery, reject malformed mappings, handle disconnect/reconnect without stale actions. Show physical and semantic observations in diagnostics. Prove printed Flip controls and restart. Do not clone Steam Input complexity. |
| [#78 device profiles](https://github.com/EriArk/TrainerOS/issues/78) | Explicit identity/display/input/Home/power/battery/lid/session/recovery/storage/runtime-package/image-channel contract. No game-specific emulator logic or model conditionals in feature QML. Official / experimental / unknown support, independently unavailable capabilities, repeatable baseline → image → display/input → recovery → Home → storage → runtimes → sleep → OTA → acceptance checklist. Flip reference first; additional hardware needs physical evidence, not similarity. |
| [#79 lid/sleep/wake](https://github.com/EriArk/TrainerOS/issues/79) | Separate shell, running game, backup/mutation, modal/onboarding, lid and explicit sleep, wake and partial failure. Drain/block writes; define suspended-time accounting. Repeated display/foreground/input/Home/audio/brightness recovery and neutral-before-action; per-runtime game policy, failed-wake recovery and measured long-run battery/thermal behavior. Keep sleep disabled until proven. |
| [#80 readiness](https://github.com/EriArk/TrainerOS/issues/80) | Ready / runtime missing / firmware missing / incomplete / unsupported / needs verification from actual checks, not a second QML compatibility table. Logical firmware identity/validation, recheck off the UI thread, affected games, reviewed setup and later Help links. Reuse in Properties/onboarding. Package presence does not prove a playable route; no general package manager or bundled proprietary firmware. |
| [#81 health/support](https://github.com/EriArk/TrainerOS/issues/81) | Concise image/device/session/input/database-integrity/storage/library/readiness/failure health; explicit offline controller export of allowlisted, size/time-bounded logs and manifests. No personal DB rows, save/ROM/BIOS bytes, secrets, PIN/reset data or personal screenshots. Redact paths/identities and credential-bearing URLs; privacy regression tests. No library mutations or arbitrary root-command collection. Extend the existing diagnostics. |

## Portable accounts and system release — #70–73

| Issue | Required delivery / proof |
| --- | --- |
| [#73 Trainer backup](https://github.com/EriArk/TrainerOS/issues/73) | Atomic export with versioned manifest/integrity: identity/preferences/selections/history/Hall/favorites, managed personal saves/protection and bounded personal exit media. Portable bindings only under explicit secret policy; no plaintext external credentials or copied shared ROM/runtime/BIOS/system/media library. Handle fresh/existing identity, ID collisions, absent games/providers and schema versions; no claiming another Trainer's shared save. Physically restore onto a clean compatible image and relink stable games without losing unavailable history. |
| [#72 first boot](https://github.com/EriArk/TrainerOS/issues/72) | Compose existing #19 registration with device/network-or-offline/input/display/storage/library/readiness checks, optional RA/#65 and control tour. Derive completion from durable state, not one fragile done flag. Controller-only, safely resumable after interruption/reboot, no duplicate Trainers or forced destructive onboarding on upgrades. Missing games/network do not prevent usable Home. Build the flow before #70 clean-flash acceptance. |
| [#70 image](https://github.com/EriArk/TrainerOS/issues/70) | Repository-controlled reproducible Armada base/layer definitions, pinned or recorded inputs, release manifest/hashes and Flip device profile. Include app/session/supervisor/input/dependencies/validated runtimes/default paths/recovery/build identity. No ROMs, BIOS, private saves/artwork or secrets. Fresh flash reaches setup without shell surgery; physical failed-start/maintenance recovery before release. Additional devices reuse explicit profiles. |
| [#71 OTA](https://github.com/EriArk/TrainerOS/issues/71) | Asynchronous discovery/download, version/notes/controller flow, space/power/trusted artifact checks, staged safest-supported atomic activation, explicit installation and honest phases. Interrupted/corrupt downloads leave running system intact. Track known-good image and compatible personal state; back up before irreversible DB migration. Never run an old binary on a newer incompatible schema. Physically update → reboot/use → rollback → update again, preserving profiles/saves/history/library. |

Image definition precedes OTA implementation. Portable account recovery,
device/runtime reports and onboarding precede clean-flash release acceptance.
OTA-specific health fields bind when OTA exists; they do not delay the initial
support exporter. Sleep and any unproven runtime remain explicitly unavailable
rather than silently included in official support. #41 branding uses the final
supported userspace/session layers and must never mask failure or delay readiness.

## Preservation and closure rules

- Earlier steps 1–10, U1–U13 and P0–P12 remain indexed in ROADMAP. The earlier
  #1–64 registers, #65 integration and #68 complete trader scope are preserved.
- Keep collection/hacks/variants, Batocera folders/XML/media, controller editing,
  English catalogue curation, artwork/form gaps, badge comparison, DS backup,
  existing runtime input gaps and representative server-ROM selection.
- Keep #44/#53 box management, #45 two-device durable Link Counter, #55 disposable
  read-only practice, #46 Dex, #47 Champion records and #48 RA truth separation.
  A missing second device blocks trade verification, not unrelated feature work.
- Keep P5 audio/haptics/RGB/gyro-or-gentle-motion, P11 full Help, P12 startup and
  measured performance; speculative hardware modes remain optional.
- Art-pack installation/selection/validation/update/removal, all #57–61 Studio
  requirements, source/author/terms data and consolidated Credits remain final.
- Closed-as-superseded issues are not implementation evidence. Neither this
  register nor a roadmap checkbox can claim a write, device, image or OTA gate
  passed without its actual verification record.
