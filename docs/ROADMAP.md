# TrainerOS Roadmap

Replanned on **2026-09-13** after reading GitHub issues #1–18, including all nine new issues #10–18, and the owner's subsequent Plasma Mobile and careful system-cleanup requests. This is the execution plan; old unchecked milestones do not mean their implementation is absent. Planning does not close issues or certify untested capabilities.

## Working baseline

The native skeleton, shared controller/UI components and SQLite foundation already exist. Extend them in the confirmed order: **required boundary → persistence/provider → complete feature → device verification**. Do not restart the mock or build speculative frameworks. Visuals remain replaceable; product/input invariants stay fixed.

| Area | Implemented / evidence | Remaining boundary |
| --- | --- | --- |
| Shell and storage | C++20, Qt Quick, SDL; five peer pages, keyboard, themes/reduced motion, profile and SQLite state. Physical navigation/profile/launch confirmed by the owner. [Persistence](LOCAL_PERSISTENCE.md), [device baseline](ARMADA_DEVICE_BASELINE.md). | New routes/migrations need their own checks; earlier mock checkpoints are historical. |
| Worlds | Region-first catalogue, missing editions, platform badges, file attachment, custom/multiple Worlds and stable personal IDs. [Catalogue](COLLECTION_CATALOGUE.md). | Personal duplicate/language audit; catalogue presence is not ownership or compatibility. |
| Adventures | RetroArch and standalone melonDS/Dolphin launch/return; selected DS/GameCube/Wii titles exercised on Flip. [RetroArch](RETROARCH_ADAPTER.md), [standalone adapters](STANDALONE_ADAPTERS.md). | Per-title coverage; PokéPark 2's initial black screen unresolved. Installed emulators do not prove 3DS/Wii U/Switch integration. |
| Home / Continue | Persistent Home selection, fixed A launch, Y selector, observed history; verified mGBA GBA moments, thumbnails and exact resume. [Home/history](HOME_AND_HISTORY.md), [resume](RETROARCH_RESUME.md). | Broader state formats and media presentation; no universal exact resume. |
| Progress | English FireRed original/Rev 1 and Emerald ordinary saves supply badges/National Dex counts; colored crystals checked on Flip. [Game progress](GAME_PROGRESS.md). | No party/money reader, save editor, individual capture history, DS/GameCube or ROM-hack progress claim. |
| Trainer / Hall | Persistent profile, derived overview, manual memories; real RA sign-in/read provider and account cache implemented. [Trainer](TRAINER_OVERVIEW.md), [Hall](HALL_OF_FAME.md), [RA](RETROACHIEVEMENTS.md). | Global account ownership and same-account emulator earning are #12. Login/read support does not prove an earned unlock. |
| Pokédex | Offline 1025 species / 1579 forms, search/filter, favorites and persistent manual species-wide Seen/Caught/notes. [Pokédex](POKEDEX.md). | External art, dedicated Caught collection; today's journal does not identify caught forms or individuals. |
| Pokémon Center | Verified GBA/mGBA backup/restore with protection copy; separate opt-in melonDS save resolver. [Save services](SAVE_BACKUPS.md). | Additional formats and healing require independent proof. |
| Device/session | TrainerOS installed as default; launch/return, shell-crash survival, startup-failure recovery and Steam/Plasma transitions exercised. Battery, volume, brightness and power controls exist. [Session](SESSION_PROTOTYPE.md), [controls](DEVICE_CONTROLS.md). | Sleep disabled; wake reliability and prior I/O freeze unresolved. Mobile replacement and reversible Steam removal are new work. |

Module documents contain dated evidence and exact limits. Existing Windows/Linux/ARM results apply to those revisions, not automatically to the next change. GitHub Actions is not a delivery gate, per the owner's instruction.

## Decisions and precedence

- **Home input stays as shipped:** Y opens/closes the Home selector; A in it selects without launching; unobstructed Home A immediately invokes its large action button. The direct-launch behavior in older [#9](https://github.com/EriArk/TrainerOS/issues/9) is superseded by the owner's later clarification. Global access remains deferred: it needs revised select-for-Home semantics and conflict-free replacements for current local Y actions, not a rollout of the old issue as written.
- **Sessions change deliberately:** [#11](https://github.com/EriArk/TrainerOS/issues/11) replaces permanent Steam retention with reversible removal. The later request prefers **TrainerOS + Plasma Mobile only**, if Mobile can replace maintenance/recovery Desktop. Retain current working sessions until replacement/restore gates pass. Preserve shared KDE dependencies and personal Steam data. See [platform migration](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).
- **Collection is English-first; catalogue stays broad:** #18 cleans personal files, not the reference checklist. Missing eligible editions stay grey/linkable. Preserve meaningful revisions, English translations and ROM hacks. Closed online-only titles remain excluded; a catalogue entry does not promise Flip compatibility.
- **Sources stay separate:** current-save observations, manual journal assertions, personal history and external account achievements have different owners. Artwork proves none of them. See [ownership contract](DATA_MODEL.md#ownership-contract--2026-09-13).
- **Sleep is outside this queue.** Do not enable suspend or reopen wake trials as a session-migration side effect. A new black screen remains a fault to diagnose, not proof of sleep.
- **System cleanup is separate from ROM cleanup:** remove verified unnecessary software/background work to reclaim space and reduce idle load, using a dependency audit and reversible batches. Do not confuse fewer session entries with fewer installed bytes or processes.

## Next execution queue

Numbers express default priority, not artificial dependencies between independent modules. Finish and deliver each bounded increment. If a card/device/account interaction is unavailable, prepare fixtures/contracts and advance an independent ready step, recording the outstanding gate.

### 1. Audit the personal collection and identity ownership

Issues: [#18 audit](https://github.com/EriArk/TrainerOS/issues/18), remaining [#2 ownership review](https://github.com/EriArk/TrainerOS/issues/2).

**Next concrete increment: a read-only inventory and private dry-run manifest.** Change no ROM, save, database record or catalogue row.

- Record path, size, SHA-256, platform, release/language/revision evidence, Adventure/catalogue links, classification, reason and retained equivalent. Classify exact duplicate, proven redundant release, non-English, or needs review.
- Inspect Home/history, ResumePoint, save-backup, RA and media references before proposing relinks. Hash equality is content evidence, not proof of one playthrough/history identity.
- Audit types/storage against the ownership contract: installation-wide configuration versus genuine per-Adventure overrides; composed progress versus writable identity. Change only a boundary actually needed by the next feature.
- Prefer metadata/header/database evidence over filename tags. Preserve ambiguous languages, translations, distinct hacks/revisions and language-neutral titles. No content conversion in this audit.

**Done when:** every candidate has a consistent reason and known replacement where applicable, ambiguities/dependent records are enumerated, and repeated audit leaves source data unchanged. Private paths/hashes/manifests stay outside Git. Fixtures can prepare tooling; actual counts need the collection.

### 2. Relink and quarantine verified cleanup candidates

Issue: [#18 cleanup](https://github.com/EriArk/TrainerOS/issues/18). Requires step 1's consistent manifest and proven identity/relink rules.

- Revalidate source identity before action. Keep a canonical exact copy; relink safely before moving redundant files to same-storage quarantine. Preserve distinct Adventure identities with different histories; never merge histories merely to remove duplicate ROM files.
- Never delete saves, states, screenshots, backups, play history or Hall memories. Save relinking needs adapter-specific proof. Do not break a working saved moment just to normalize a ROM path.
- Re-scan Worlds/search/counts, Home, RA/media links; exercise representative launches on affected platforms and Continue/backup for relinked Adventures.
- Report before/after active files, categories, exemptions, relinked/unresolved records and bytes. Distinguish quarantined bytes from actual free space: same-disk quarantine reclaims none. Permanent deletion follows verification and never includes ambiguous candidates.

**Done when:** proven redundant files leave the active collection, references resolve, protected data is unchanged and a private recovery/final-space report exists. Reference catalogue editions remain available to link.

### 3. Shared Adventure media, then Worlds

Issues: [#17 provider](https://github.com/EriArk/TrainerOS/issues/17) → [#16 Worlds](https://github.com/EriArk/TrainerOS/issues/16). Requires stable Adventure/catalogue/variant identity, not resolution of every ambiguous cleanup candidate.

Implement a small semantic result: identity/variant, kind, provider-owned local handle, dimensions, source/revision, availability and attribution. Support explicit local packs, assigned art and existing owned screenshots; an explicitly selected EmulationStation-style import is optional, not a Batocera dependency. No scraper paths or filename guessing in QML.

Use background validation/scaling, bounded persistent thumbnails and atomic manifests. Personal overrides win; hacks never inherit unrelated base-game art by name. Corrupt/missing/removable sources fall back without blocking input. Cache clearing preserves originals; imports never copy ROMs/saves/BIOS.

Then replace Worlds' selected Adventure Record emblem: **exact wheel → exact marquee → exact boxart → explicitly compatible title media → original title/placeholder**. Retain title text and availability, including catalogue-only entries. Fit wide logos/portrait covers without overlap or stretching. Keep list focus unchanged; do not redesign every page.

**Done when:** zero-art/imported-art, variant/override/cache/corrupt-source cases pass; hundreds of records and rapid controller selection remain responsive; 960×540 / 1920×1080 layouts and Flip presentation are inspected. #17's Home consumer remains open until step 4.

### 4. Home's faint Adventure background

Issue: [#15](https://github.com/EriArk/TrainerOS/issues/15). Requires #17's media boundary and existing history/ResumePoint ownership.

Resolve newest valid return image for Home's selected Adventure, then its selected moment image when that moment is active, then newest valid return image overall, then the ordinary background. Test the active-moment precedence explicitly.

Layer the scaled/subdued screenshot **under existing ornaments/patterns and all content/focus**. Preserve the light recessed workspace, theme, crystal tray and fixed A/Y controls. Tune opacity/softening on Flip. Reuse screenshot ownership: no parallel database, independent persisted wallpaper choice, uploads or default diagnostic inclusion. Revision changes invalidate cache.

**Done when:** selection/restart/source replacement chooses deterministically; bright/dark/corrupt/missing fixtures remain readable; original decoration stays visible and page switching is smooth. Continue may share the semantic media result only while retaining exact-state provenance.

### 5. One RetroAchievements account, then verified earning

Issue: [#12](https://github.com/EriArk/TrainerOS/issues/12). Depends on the current RA provider and shared settings/keyboard, not artwork.

Deliver two increments:

1. Global service owns canonical identity/token, login/logout/validation and mode. Add **Settings → Accounts → RetroAchievements**; Hall shows status and opens that same surface. Migrate the owner-only account file without unnecessary login; preserve malformed originals. All consumers use one source. Reject previous-account responses and clear secret drafts on submission/cancel/page change/failure/exit.
2. One supported emulator gets configure/verify/clear-managed-account integration. Separate history reading from earning and show unavailable/mismatched-account states. Make Standard/Hardcore explicit before launch; disable exact state resume when forbidden. Never silently change mode to make Continue work.

**Done when:** migration, failed login, sign-out, A/B cache isolation and controller flows pass; a real new achievement is earned on Flip under the same account and read back in Hall. That final gate needs suitable gameplay; login/cache alone is not closure. Local Hall memories remain intact. No plaintext password persistence or credentials in QML/logs.

### 6. Pokédex artwork packs

Issue: [#13](https://github.com/EriArk/TrainerOS/issues/13). Uses stable species/form IDs. Reuse useful cache mechanics from #17, but retain a **separate PokedexArtworkProvider**: species and game editions have different identities.

- Verify source terms/endpoints before enabling remote packs; user-installed PokéAPI-hosted artwork is the first candidate, not a redistribution promise. A code/data license is not artwork redistribution permission. Keep downloaded/user art outside Git and ROM/save directories, with source metadata.
- Controller install/update/verify/remove lives in Settings. Use bounded versioned HTTPS manifests, image validation, cancellation and atomic activation; partial/corrupt updates keep the old working pack.
- Exact form → known species fallback → configured compatible representation → original placeholder. Make fallback honest; unknown/custom species remain usable.
- Use sized images in Pokédex browsing/detail and Trainer's favorite picker, without decoding every full-resolution offscreen image.

**Done when:** form mapping/fallback, malformed/oversized data, failed update, cancellation/restart and removal pass; art survives reboot/offline use and full-dataset scrolling remains smooth on Flip.

### 7. Caught as a personal collection

Issue: [#14](https://github.com/EriArk/TrainerOS/issues/14). V1 depends on the persistent journal. #13 enriches it; missing art is never a blocker.

Add reachable local **All / Caught / Favorites / Journal** navigation with page-specific controls, preserving L1/R1, filters/detail focus and existing records. V1 projects manual **species-wide** Caught marks into one row per marked species with name/number/types, manual source, notes and unknown origin/date.

Today's journal does not record the caught form. Do not duplicate a species mark over all forms or invent a form/individual. Form-specific assertions require an explicit later model/editor/provider. Reuse reference/detail data; clearing a manual mark changes only that assertion. Seen-only/unknown are not Caught. Offer provenance filters only when supported; unknown metadata stays visible/selectable.

Before automatic ingestion, document a small individual-record boundary: stable record/species/form IDs, Adventure/save lineage where proven, provider/artifact revision, observed-at distinct from real catch time, optional nickname/shiny/gender/level. Do not build unused infrastructure for every future field.

**V1 done when:** manual changes appear immediately and survive offline restart; empty/missing-art/large-list/controller flows work. **Enrichment remains open:** repeated reads and rollback/re-import must not duplicate or erase history; a caught total cannot become fictional individuals. Home/Trainer summaries are later derived views; Recently Caught needs trustworthy time/source labeling.

### 8. Verified party reading before paid healing

Issue: [#10](https://github.com/EriArk/TrainerOS/issues/10). Requires exact-build save resolution, protection backups and a **new independently verified party/money reader and writer**. The shipped badge reader is not a healing capability.

Start with one verified Gen III title/build. First prove immutable party condition, allowed HP/PP/status fields and currency representation. Define a deterministic fee outside QML; show exact work, fee and before/after balance in **Start → Pokémon Center → Services → Heal party**. This is a TrainerOS service, not canonical in-game Center behavior.

The writer must refuse a live/uncertain emulator writer; validate stable source; create/verify a protection backup; prepare and independently validate modified bytes/checksums; prove only allowed fields changed; recheck source revision immediately before atomic replacement; verify committed bytes and retain recovery. Unsupported/corrupt/changed sources, insufficient money and backup failures leave the original unchanged. No changes to membership, training stats, items, moves, story or Dex beyond verified healing fields.

**Done when:** original fixtures cover healthy/no-op, mixed HP/PP/status, insufficient/max money, races, corrupt sources, running emulator, backup/replacement/verification failures and recovery. Independently verify a user-owned save in-game on Flip for healing, exact charge and retained unrelated progress. Activity is explicitly TrainerOS history. DS/later/hack support needs separate proof.

### 9. Consolidate sessions with proven recovery

Issue: [#11](https://github.com/EriArk/TrainerOS/issues/11), extended by **Plasma Mobile only if viable**. Independent platform work; it does not require healing/artwork. Reinspect the actual Armada build and recovery access before implementation.

1. **Mobile trial:** packages/session entries already exist on Flip (read-only check 2026-09-13). Validate landscape/scale, usable input/text entry, network/files/terminal/package tools, privileged recovery launcher, return to TrainerOS, reboot and shell-failure recovery. Keep working Desktop during the trial; package presence is not acceptance.
2. **Maintenance replacement:** after success, choose Mobile as maintenance and retire ordinary Desktop session/components only as supported by Armada's image/update model. Preserve shared KDE/KWin/Qt dependencies and SSH rollback. If Desktop-only packages cannot be removed safely from the immutable base, record the limit; hiding a session does not reclaim package space.
3. **Steam removal/restore:** inspect a bounded versioned manifest of managed package refs, session entries, services and configuration. Build a narrow root-owned restore helper, visible **Restore Steam Gaming Mode** application entry in the selected Plasma environment, and CLI recovery. Prove idempotence, interrupted recovery and refusal of distro drift in fixtures. Preserve personal Steam libraries, compatdata and saves.
4. **Physical round trip:** remove Steam → reboot/use TrainerOS and maintenance → restore Steam → boot Steam → remove again. Verify direct RetroArch/melonDS/Dolphin launch/return and fallback throughout. Change mode UI/supervisor assumptions only with the corresponding platform capability.

**Done when:** TrainerOS and verified Plasma Mobile are the normal choices, Desktop retirement is honestly documented, and Steam removal/restore and recovery are physically proven. Failed gates retain the last working setup with the exact limitation reported. No blind host package deletion, distro fork or sleep-policy change. Details: [platform consolidation](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).

### 10. Remove unnecessary software and background work

Source: the owner's follow-up request on 2026-09-13. The read-only audit can start independently; final removals must respect step 9's selected maintenance/recovery setup and the emulator coverage plan.

1. Inventory host packages, Flatpak apps/shared runtimes, AppImages, development containers/caches, session autostarts and system/user services. Record ownership, purpose, dependencies, installed size and actual activity. Distinguish optional UI apps, duplicate tools, temporary development material and required platform/runtime components; age or an unfamiliar name is not evidence of disuse.
2. Classify each candidate as keep, remove, disable-autostart, or needs review, with a specific reason and rollback method. Preserve current/planned emulator runtimes, controller bridges, Qt/KDE dependencies and essential input, graphics, audio, networking, authentication and recovery services. A planned adapter must not lose its already installed runtime just because the shell does not expose it yet.
3. Capture bounded manifests/configuration for small reversible batches. Disable unnecessary background/autostart entries first where appropriate, verify normal operation, then remove confirmed unneeded packages/apps through the supported installation mechanism. Do not blindly autoremove dependencies or prune every Flatpak runtime/container. Respect Fedora bootc's immutable image/update behavior and retain a verified system rollback.
4. Preserve games, BIOS, saves, states, screenshots, private media, TrainerOS/Steam personal history and unrelated configuration. Clearing generated caches is separate from deleting user originals. Report packages that remain because of shared dependencies or image constraints rather than claiming they were uninstalled.
5. Compare **actual free space, idle RAM/CPU, active services/processes and startup behavior** before and after under comparable conditions. Quarantined files and hidden session entries do not count as reclaimed disk space; disabled services do not guarantee a measurable memory saving.

**Done when:** each removed/disabled item has recorded rationale and a working rollback; reboot, TrainerOS, the chosen Plasma maintenance shell, SSH recovery, networking/audio/controller input and representative emulator launch/return still work. Report measured space/load changes and unresolved candidates. This is targeted maintenance, not a distro rebuild, blanket service shutdown or sleep-policy change.

## Remaining work outside the new issue queue

- **Adapter coverage:** validate desired titles, DS pointer comfort and the unresolved Wii title; extend 3DS/Wii U/Switch only with a supported runtime. Add backup/exact resume one adapter/format at a time. Installed software does not establish compatibility.
- **Further progress:** separate verified DS/GameCube/other readers; party/location, per-species observations and individual Caught history only with source evidence. No universal ROM-hack parser, reconstructed story history or fake automatic Hall entries.
- **Install/update delivery:** reproducible ARM64 package, dependency/version policy, installer/migrations, durable atomic update and rollback across reboot. Development deployment is not a finished end-user installer; step 9 manifests must survive supported Armada updates.
- **Device polish:** measure startup/idle/frame pacing and long-list/media behavior on the actual 5.5-inch landscape screen; preserve themes/reduced motion and add controller remapping on the shared input foundation. Sleep/hinge/wake remains separate and unverified.
- **Later optional ideas:** multiple Trainer profiles, richer timeline/shiny history, local milestones, optional cloud sync, atmosphere/theme packs and restrained notifications. None becomes a normal-play prerequisite.
- **Custom Armada image:** consider only when mature package/session installation proves insufficient. Hardware modification remains out of scope.

## Issue reconciliation

This review changes the plan, not GitHub issue states. Close/split only after checking remaining acceptance against delivered evidence.

| Issue | Treatment |
| --- | --- |
| [#1](https://github.com/EriArk/TrainerOS/issues/1) native mock | Foundation implemented; reconcile old acceptance, do not rebuild it. |
| [#2](https://github.com/EriArk/TrainerOS/issues/2) ownership | Contract clarified in DATA_MODEL; code/storage audit remains step 1. No integration-profile migration claimed. |
| [#3](https://github.com/EriArk/TrainerOS/issues/3) early device smoke | Dated hardware evidence exists; performance/wake limits remain explicit. |
| [#4](https://github.com/EriArk/TrainerOS/issues/4) extensible Worlds | Stable IDs/custom/multiple Worlds implemented; preserve through cleanup. |
| [#5](https://github.com/EriArk/TrainerOS/issues/5) lifecycle | Checkpoint/process/recovery boundary exists; new adapters still need proof. |
| [#6](https://github.com/EriArk/TrainerOS/issues/6) ResumePoint | Provenance contract and first GBA provider exist; preserve through media/relinking. |
| [#7](https://github.com/EriArk/TrainerOS/issues/7) GitHub CI | Superseded by no-Actions instruction; native/Linux/ARM verification and commit/push stay required. |
| [#8](https://github.com/EriArk/TrainerOS/issues/8) original tracker | Historical foundation tracker; this roadmap is the current queue. |
| [#9](https://github.com/EriArk/TrainerOS/issues/9) global Continue | Direct launch superseded; global access deferred pending select-for-Home semantics and local shortcut remapping. |
| #10 / #11 / #12 | Steps 8 / 9 / 5; save writes, system migration and earning have separate physical gates. |
| #13 / #14 | Steps 6 / 7; species art and personal collection stay separate from game media/save totals. |
| #15 / #16 / #17 | Shared provider first (3), then Worlds (3) and Home (4). |
| #18 | Audit (1) precedes relink/quarantine/verified cleanup (2). |
| Owner follow-ups, 2026-09-13 | Mobile-only maintenance trial/replacement (9); dependency-aware software/background cleanup with measured results (10). No new GitHub issue is implied. |

## Historical milestone map

Older module notes use milestones **0–14** from the [previous plan](https://github.com/EriArk/TrainerOS/blob/7c7fd786f1eab44427d0abd5f709c1c4f95012b6/docs/ROADMAP.md), not the new queue numbers. Checkpoints remain in Git history and module evidence documents.

| Former milestone | Current location / status |
| --- | --- |
| 0–3: skeleton/controller/mock/backend | Working baseline; extend only what the active feature needs. |
| 4: device baseline | ARMADA_DEVICE_BASELINE and later session/device checks supersede early “unverified” notes. |
| 5–6: Worlds/Home/Continue | Baseline; cleanup/media/per-adapter follow-ups above. |
| 7: Trainer/Hall | Local modules and real RA reader exist; account/earning is step 5. |
| 8–9: Pokédex/Center | Offline/manual Dex and GBA backups exist; steps 6–8 extend them. |
| 10–11: dedicated/default session | Installed/selected; consolidation is step 9, wake remains open. |
| 12–13: adapters/progress | First additional launch adapters and bounded Gen III reader exist; expand by proven capability. |
| 14: packaging/polish | Open; themes/reduced motion and initial ARM build already delivered. |

## Delivery gate for each increment

Review actual changes and acceptance; run affected native checks, broader tests for shared input/storage/lifecycle, and rendered SDL controller scenarios for QML. Inspect 960×540/1920×1080 and Flip layouts. Preserve L1/R1, A/Y selection-versus-launch, Back/modal priority; cover offline/unknown/empty/corrupt states. Fixtures are not device proof.

Keep credentials, private reports/manifests, ROMs, BIOS, saves and unlicensed artwork out of Git. Documentation-only planning needs content/link/diff checks, not an unrelated rebuild. Stage reviewed paths, commit, fetch/integrate remote work, push normally and verify remote SHA/final status. Do not run/poll GitHub Actions. See [workflow](DEVELOPMENT_WORKFLOW.md).
