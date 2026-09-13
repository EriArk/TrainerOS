# TrainerOS Roadmap

Rebuilt on **2026-09-13** after reading all issues **#1–41**, including the new batch **#19–41**, and the owner's requests to preserve the entire backlog, prefer Plasma Mobile and carefully clean unnecessary software. **Phases P0–P12 are the execution order.** Stable steps 1–10 and U1–U13 below preserve the earlier acceptance register; [EXPANSION_PLAN.md](EXPANSION_PLAN.md) supplies detailed acceptance for the new batch. Planning does not close issues, implement features or certify untested capabilities.

This is a dependency replan, not a reset. The [pre-expansion plan](https://github.com/EriArk/TrainerOS/blob/252f04e0772bd05966fb212ca736d303cab366ea/docs/ROADMAP.md) and the preservation map below make previous commitments traceable. Read the phase table for what comes next, then the referenced acceptance; do not execute the old numbered register as a competing queue.

## Working baseline

The native skeleton, shared controller/UI components and SQLite foundation already exist. Extend them in the confirmed order: **required boundary → persistence/provider → complete feature → device verification**. Do not restart the mock or build speculative frameworks. Visuals remain replaceable; product/input invariants stay fixed.

| Area | Implemented / evidence | Remaining boundary |
| --- | --- | --- |
| Shell and storage | C++20, Qt Quick, SDL; five peer pages, keyboard, themes/reduced motion, profile and SQLite state. Physical navigation/profile/launch confirmed by the owner. [Persistence](LOCAL_PERSISTENCE.md), [device baseline](ARMADA_DEVICE_BASELINE.md). | New routes/migrations need their own checks; earlier mock checkpoints are historical. |
| Worlds | Region-first catalogue, missing editions, platform badges, file attachment, custom/multiple Worlds and stable personal IDs. [Catalogue](COLLECTION_CATALOGUE.md). | Personal duplicate/language audit; catalogue presence is not ownership or compatibility. |
| Adventures | RetroArch and standalone melonDS/Dolphin launch/return; selected DS/GameCube/Wii titles exercised on Flip. [RetroArch](RETROARCH_ADAPTER.md), [standalone adapters](STANDALONE_ADAPTERS.md). | Per-title coverage; PokéPark 2's initial black screen unresolved. Installed emulators do not prove 3DS/Wii U/Switch integration. |
| Home / Continue | Persistent Home selection, fixed A launch, Y selector, observed history; verified mGBA GBA moments, thumbnails and exact resume. [Home/history](HOME_AND_HISTORY.md), [resume](RETROARCH_RESUME.md). | Broader state formats and media presentation; no universal exact resume. |
| Progress | English FireRed original/Rev 1 and Emerald ordinary saves supply badges/National Dex counts; colored crystals checked on Flip. [Game progress](GAME_PROGRESS.md). | No party/money reader, save editor, individual capture history, DS/GameCube or ROM-hack progress claim. |
| Trainer / Hall | Persistent profile, derived overview, manual memories; real RA sign-in/read provider and account cache implemented. [Trainer](TRAINER_OVERVIEW.md), [Hall](HALL_OF_FAME.md), [RA](RETROACHIEVEMENTS.md). | Central account management and same-account emulator earning are #12, now scoped to the active Trainer by #20. Login/read support does not prove an earned unlock. |
| Pokédex | Offline 1025 species / 1579 forms, search/filter, favorites and persistent manual species-wide Seen/Caught/notes. [Pokédex](POKEDEX.md). | External art, dedicated Caught collection; today's journal does not identify caught forms or individuals. |
| Pokémon Center | Verified GBA/mGBA backup/restore with protection copy; separate opt-in melonDS save resolver. [Save services](SAVE_BACKUPS.md). | Additional formats and healing require independent proof. |
| Device/session | TrainerOS installed as default; launch/return, shell-crash survival, startup-failure recovery and Steam/Plasma transitions exercised. Battery, volume, brightness and power controls exist. [Session](SESSION_PROTOTYPE.md), [controls](DEVICE_CONTROLS.md). | Sleep disabled; wake reliability and prior I/O freeze unresolved. Mobile replacement and reversible Steam removal are new work. |

Module documents contain dated evidence and exact limits. Existing Windows/Linux/ARM results apply to those revisions, not automatically to the next change. GitHub Actions is not a delivery gate, per the owner's instruction.

## Decisions and precedence

- **Home input stays as shipped:** Y opens/closes the Home selector; A in it selects without launching; unobstructed Home A immediately invokes its large action button. The direct-launch behavior in older [#9](https://github.com/EriArk/TrainerOS/issues/9) is superseded by the owner's later clarification. Global access remains deferred: it needs revised select-for-Home semantics and conflict-free replacements for current local Y actions, not a rollout of the old issue as written.
- **Sessions change deliberately:** [#11](https://github.com/EriArk/TrainerOS/issues/11) replaces permanent Steam retention with reversible removal. The later request prefers **TrainerOS + Plasma Mobile only**, if Mobile can replace maintenance/recovery Desktop. Retain current working sessions until replacement/restore gates pass. Preserve shared KDE dependencies and personal Steam data. See [platform migration](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).
- **Collection remains English-first; eligibility is intentionally revised by [#30](https://github.com/EriArk/TrainerOS/issues/30):** a complete collection of substantial playable Pokémon titles with credible Flip/controller routes replaces the broad historical/promotional checklist. Missing eligible editions stay grey/linkable; meaningful revisions, translations and ROM hacks remain. Per-title review must not erase substantial spin-offs. #18 still concerns redundant personal files; #30 concerns reference/navigation eligibility and never deletes personal data. All previously requested sources are reconciled in U4, including intentionally excluded items.
- **Multiverse is an explicit product extension:** [#28](https://github.com/EriArk/TrainerOS/issues/28) adds non-Pokémon system browsing inside Worlds; Pokémon Worlds stays region-first. [#31](https://github.com/EriArk/TrainerOS/issues/31) reserves Home X for Pokémon/Multiverse, with separate Home selections and Continue per Trainer/context. [#32](https://github.com/EriArk/TrainerOS/issues/32) permits a distinct dark inner Multiverse Home while preserving the shared chassis and fixed A/Y behavior. No sixth primary page or second launch/history engine.
- **Multiple Trainers promote a former future idea into planned delivery:** [#19](https://github.com/EriArk/TrainerOS/issues/19)/[#20](https://github.com/EriArk/TrainerOS/issues/20) require owner-scoped data before chooser/PIN/onboarding. Library/installations/shared media stay device-wide; history/journal/Hall/Home and RA identities are per Trainer. Shared external saves do not become separate playthroughs automatically. #12 means one account service across features, not one account across people.
- **New menus preserve deliberate input:** [#26](https://github.com/EriArk/TrainerOS/issues/26)/[#35](https://github.com/EriArk/TrainerOS/issues/35) add Power and quick controls. Existing post-login L1/R1 cancels transient panels and switches pages; it never adjusts sliders or confirms actions. Wording suggesting shoulders should be ignored is not adopted silently; see [input reconciliation](EXPANSION_PLAN.md#trainer-ownership-onboarding-and-power). Before profile unlock no normal page can be accessed.
- **Sources stay separate:** current-save observations, manual journal assertions, personal history and external account achievements have different owners. Artwork proves none of them. See [ownership contract](DATA_MODEL.md#ownership-contract--2026-09-13).
- **Sleep is outside this queue.** Do not enable suspend or reopen wake trials as a session-migration side effect. A new black screen remains a fault to diagnose, not proof of sleep.
- **System cleanup is separate from ROM cleanup:** remove verified unnecessary software/background work to reclaim space and reduce idle load, using a dependency audit and reversible batches. Do not confuse fewer session entries with fewer installed bytes or processes.

## Unified execution order — existing work and new issues

This table **replaces the previous execution queue**. It includes current gaps, all new issues and final delivery work. A phase is a group of bounded, separately committed increments, not one giant implementation. Finish each needed boundary and its first complete consumer before expanding it. Dependencies are explicit; an unavailable device/title does not block independent ready work.

| Phase | Work and source | Dependency / exit condition |
| --- | --- | --- |
| **P0 — reconcile and inspect** | U1–U3 evidence/reliability/DS backup/RA read gates; step 1 + U4 read-only archive/identity inventory; U5 current title failures; read-only haptics/RGB support probe | Reuse delivered evidence and run missing checks only. Preserve fault logs and source manifests. Investigate active storage failure before heavy I/O/mutations; no sleep tests. Record unsupported/blocked paths rather than pretending they passed. |
| **P1 — shared interface** | #21 tab/panel geometry → #23 shared pattern + #33 compact headers → #22 quiet status / #27 physical chrome; #35 quick controls and #26 Power off/restart; #40 Help viewer/current-feature articles | Existing input/device services already work. Finish shared components and their current consumers; preserve fixed A/Y/shoulders, themes/reduced motion and actual brightness/volume results. Do not expose Switch Player before P2. No schema or new emulator dependency. |
| **P2 — Trainer ownership** | #2 audit; #20 device/Trainer ownership migration and account/save-lineage boundaries → #19 first-run creation + #20 chooser/PIN → #26 Switch Player; step 5's #12 central account management | Lossless single-owner migration first, then controller flows. Shared library is not copied per Trainer. Stop old-owner callbacks/credentials and refuse unsafe switching during active games/writes. #12 earning remains P6; no premature new-achievement claim. |
| **P3 — library identity and collection** | #28 domain/system model → #30 per-title curation; U4 complete archive/title/hack reconciliation; step 1 manifest refresh → step 2 / #18 relink/quarantine | Separate Pokémon/Multiverse membership from runtime and owner history. Preserve all private files and references during catalogue changes. Verified duplicate cleanup uses refreshed references for every Trainer; eligible missing editions remain. Full collection concerns playable eligible titles, not museum filler. |
| **P4 — library and Home experiences** | Step 3 / #17 shared media → #16 Worlds and complete #28 Multiverse browser; #31 context-scoped Home/Continue → step 4 / #15 backgrounds and #32 Multiverse Home; U6 controller moment/backup maintenance | Trainer/domain identities precede projections; static media precedes video. One launch/history pipeline, per-context selections, correct return route, honest metrics and media provenance. Deliver saved-artifact maintenance separately, never as implicit ROM/cache pruning. |
| **P5 — media and feedback** | #29 delayed local video; #36 selectable music/UI sound packs; #37 charger haptics and #38 Armada RGB as separate capabilities | Video needs #17/selected-game panel and real codec/performance proof. Audio arbitration precedes #25 jingle. Hardware controls need actual support, debounce/state round-trip and no gameplay/input interference. A blocked RGB path cannot hold back audio or later features. |
| **P6 — achievements** | U3 read/cache reconciliation → #24 earned-state UI; remaining step 5 / #12 one real emulator earning path; U7 next verified matching formats; #25 deduplicated banner/jingle | Active-Trainer account isolation from P2; current complete/cached/unknown states distinguished. Verify actual earning under that account. Return-time notification first; live overlay conditional. #24 may ship earlier after read/ownership proof; broader hashes are separate increments. |
| **P7 — personal Pokédex** | Step 6 / #13 species artwork → step 7 / #14 manual Caught; U9 manual journal/Trainer/Home projections | Species/form identity is separate from game media; useful manual Caught does not wait on artwork/network or individual-save parsing. Two-Trainer assertions stay isolated; species marks do not become fictional forms/individuals. |
| **P8 — save enrichment and Center** | U8 verified ordinary-save formats/readers → U9 source-aware Home/Trainer/World/collection views → step 8 / #10 protected paid healing for one proven build | U2 gates DS save access, not independent Gen III reading. Read party/money and expose honest Home data before writing. Individual/catch history requires evidence beyond totals; protected writes need checksum/race/rollback and in-game validation. DS/GameCube/hacks remain separate providers. |
| **P9 — additional playable paths** | U5 unresolved title/input coverage; U10 Azahar/3DS then Cemu/Wii U by actual viability; incremental U6/U7/U8 resume/matching/save capabilities; later representative classics selection per system | Finish each runtime launch/input/save/return path independently. Multiverse examples do not mandate unsupported systems. Switch/PC/Pico/Android remain conditional; no universal coverage claim. A proven high-priority runtime may move earlier after its actual prerequisites. Assemble each system's small classics selection as its playable route is validated, using the owner's authorized ROM disk. |
| **P10 — system delivery and cleanup** | U11 reproducible install/update/rollback foundation → step 9 / #11 verified Mobile/Steam consolidation → #39 Settings restore front end → step 10 software/background cleanup; complete U11 packaging acceptance | Minimal durable rollback is maintained from P0 onward, not deferred until this phase. Preserve shared KDE/planned emulator dependencies, personal content, maintenance launcher/SSH recovery and versioned manifests. Prove physical remove/restore/reboot, then measure real space/load changes. |
| **P11 — complete offline guide** | #40 full current-feature, game/BIOS/media, account/save and maintenance/recovery coverage | Viewer starts in P1 and articles evolve with features; now audit every topic, runtime path/capability slot, internal link and controller recovery route. Ship version-matched facts, not documentation for unimplemented capabilities. |
| **P12 — startup and whole-product acceptance** | #41 userspace/session boot identity; #34 every feature screen/state; U12 measured performance/remapping/calibration and final U11 release/recovery verification | Profile routes and installer/session recovery must already work. Boot branding cannot delay readiness or hide failure. Physical startup/rollback, readable full-screen controller flows, themes/reduced motion and performance evidence close delivery. Artwork preparation and per-feature visual checks happen earlier. |

**Next concrete work:** P0's bounded evidence/inventory checks, then P1's shared content rectangle and headings. The earlier collection dry run is still an immediate read-only task; it is no longer permission to jump directly into mutation before Trainer/domain ownership is reconciled. System audits and minimal recovery repairs may proceed when needed, without waiting for unrelated artwork/healing. Sleep U13 remains explicitly deferred; optional future ideas remain separate.

## Preservation map — earlier work still has a destination

| Previous register | New execution location; retained obligation |
| --- | --- |
| Steps 1–2, U4 | P0 inventory → P2 ownership → P3 classification/source reconciliation/relink/quarantine; all archive, version, hash and protected-data gates retained. #30 changes reference eligibility explicitly, not file ownership. |
| Steps 3–4, U6 | P4 shared media/Worlds/Home and controller moment/backup maintenance; P5 optional local video. Exact-resume provenance and future adapter capabilities stay intact. |
| Step 5, U3/U7 | P0 read/cache evidence → P2 central active-Trainer account → P6 earning/state/notification/matching. No login-as-earning shortcut. |
| Steps 6–7, U9 | P7 artwork/manual Caught → P8 proven observations and derived views; individual/form/timeline uncertainty remains. |
| Step 8, U2/U8 | P0 DS backup gate → P8 independent verified readers/protected healing; other formats remain per-provider work. |
| Steps 9–10, U1/U11 | P0 health/durability; P10 reproducible update/Mobile/Steam/software cleanup; P12 final startup/recovery. Manifests, data protection and measurements retained. |
| U5/U10 | P0 existing input/title triage → P9 complete next adapters, with earlier execution allowed for a ready dependency. PokéPark 2/DS pointer gaps remain explicit. |
| U12 | P1 shared controls, per-feature handheld checks, P12 remap/calibration/performance and full visual acceptance. Existing themes/reduced motion are not rebuilt. |
| U13 | Deferred sleep/hinge/wake and sleep-aware duration accounting; no active-phase substitution. |
| Former optional ideas | Retained below; multiple Trainers and verified RA notifications are promoted, not deleted. Richer history/cloud/themes/custom reference/image options keep their own conditions. |

The detailed old register follows with stable headings for existing links. Explicit amendments for #20/#30/#31 are marked at their point of use; all other carried-forward U acceptance remains unchanged. The issue reconciliation table covers **every issue #1–41** and owner follow-ups.

## Detailed new-issue increments

Stable acceptance register from issues #10–18; the phase table above now controls execution. New-batch criteria are in [EXPANSION_PLAN.md](EXPANSION_PLAN.md).

### 1. Audit the personal collection and identity ownership

Issues: [#18 audit](https://github.com/EriArk/TrainerOS/issues/18), remaining [#2 ownership review](https://github.com/EriArk/TrainerOS/issues/2).

**P0 read-only increment: inventory and private dry-run manifest.** Change no ROM, save, database record or catalogue row. Refresh the manifest after P2 ownership and P3 domain/eligibility decisions before authorizing its mutation phase.

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

**Done when:** proven redundant files leave the active collection, references resolve, protected data is unchanged and a private recovery/final-space report exists. Eligible reference catalogue editions remain available to link. **#30 amendment:** reference eligibility is separately reviewed in P3; excluded owned records keep their IDs/history and maintenance access, and exclusion never authorizes deletion of their original files.

### 3. Shared Adventure media, then Worlds

Issues: [#17 provider](https://github.com/EriArk/TrainerOS/issues/17) → [#16 Worlds](https://github.com/EriArk/TrainerOS/issues/16). Requires stable Adventure/catalogue/variant identity, not resolution of every ambiguous cleanup candidate.

Implement a small semantic result: identity/variant, kind, provider-owned local handle, dimensions, source/revision, availability and attribution. Support explicit local packs, assigned art and existing owned screenshots; an explicitly selected EmulationStation-style import is optional, not a Batocera dependency. No scraper paths or filename guessing in QML.

Use background validation/scaling, bounded persistent thumbnails and atomic manifests. Personal overrides win; hacks never inherit unrelated base-game art by name. Corrupt/missing/removable sources fall back without blocking input. Cache clearing preserves originals; imports never copy ROMs/saves/BIOS.

Then replace Worlds' selected Adventure Record emblem: **exact wheel → exact marquee → exact boxart → explicitly compatible title media → original title/placeholder**. Retain title text and availability, including catalogue-only entries. Fit wide logos/portrait covers without overlap or stretching. Keep list focus unchanged; do not redesign every page.

**Done when:** zero-art/imported-art, variant/override/cache/corrupt-source cases pass; hundreds of records and rapid controller selection remain responsive; 960×540 / 1920×1080 layouts and Flip presentation are inspected. #17's Home consumer remains open until step 4.

### 4. Home's faint Adventure background

Issue: [#15](https://github.com/EriArk/TrainerOS/issues/15). Requires #17's media boundary and existing history/ResumePoint ownership.

Resolve newest valid return image for Home's selected Adventure, then its selected moment image when that moment is active, then newest valid return image in the active context, then the ordinary background. Test the active-moment precedence explicitly. **#20/#31 amendment:** image selection and fallback are scoped to the active Trainer and Pokémon/Multiverse context; no cross-owner or cross-domain global fallback.

Layer the scaled/subdued screenshot **under existing ornaments/patterns and all content/focus**. Preserve the light recessed workspace, theme, crystal tray and fixed A/Y controls. Tune opacity/softening on Flip. Reuse screenshot ownership: no parallel database, independent persisted wallpaper choice, uploads or default diagnostic inclusion. Revision changes invalidate cache.

**Done when:** selection/restart/source replacement chooses deterministically; bright/dark/corrupt/missing fixtures remain readable; original decoration stays visible and page switching is smooth. Continue may share the semantic media result only while retaining exact-state provenance.

### 5. One RetroAchievements account, then verified earning

Issue: [#12](https://github.com/EriArk/TrainerOS/issues/12). Depends on the current RA provider and shared settings/keyboard, not artwork. **#20 amendment:** one account means one canonical account per active Trainer across features, not one shared credential for all people; P2 owner migration precedes this account surface. Management is P2, earning P6.

Deliver two increments:

1. Global service owns the active Trainer's canonical account identity/token, login/logout/validation and mode; storage/cache/response identities include the Trainer owner. Add **Settings → Accounts → RetroAchievements**; Hall shows status and opens that same surface. Migrate the owner-only account file without unnecessary login; preserve malformed originals. All consumers use one source. Reject previous-account responses and clear secret drafts on submission/cancel/page change/failure/exit.
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

1. **Mobile acceptance:** packages/session entries exist on Flip; [session evidence](SESSION_PROTOTYPE.md) already records Armada's Desktop choice as Mobile. Reuse proven transitions and verify the missing landscape/scale, input/text entry, network/files/terminal/package tools and privileged recovery-launcher gates. Preserve the working recovery route while checking reboot/failure behavior for the final consolidated setup. Package presence alone is not acceptance; neither should an existing successful Mobile transition be reset to “never tested”.
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

## Existing work carried forward

This register reconciles the former milestones 4–14 with current module documents. **Open** means a specific implementation or acceptance remains; **evidence review** means an old pending statement must be checked against the delivered build before repeating work. No item is declared complete merely because its first adapter/provider exists. The unified order above gives each commitment a place alongside the new issues.

### U1. Device reliability and durable installation — open investigation

Source: [device baseline](ARMADA_DEVICE_BASELINE.md), [session recovery](SESSION_PROTOTYPE.md), former milestones 4/10/11/14.

The earlier simultaneous internal-storage/microSD I/O stall has no proven root cause. Existing boot, crash survival and fallback tests remain valid evidence; they do not diagnose that stall. Before heavy transfer/cleanup, check the current installation's storage/journal health and preserve relevant fault evidence. If the failure recurs, distinguish I/O blockage, compositor failure, display-off and sleep without forcing a reproduction that risks user saves. Fix and verify a demonstrated cause, or record the remaining unknown and the conditions observed; do not mark an intermittent fault solved after a successful reboot. Durable installation and readable recovery must be retained through every update. Sleep work is U13, not an explanation by default.

### U2. Finish the existing DS save-backup increment — device gate open

Source: [DS cartridge saves](SAVE_BACKUPS.md#ds-cartridge-saves), former milestone 9.

The melonDS resolver and fixtures exist, but the module explicitly leaves real profile activation/save-path confirmation pending. Inspect the actual melonDS configuration, opt in only when supported, and prove the cartridge's ordinary save is the file selected by the service. Create a copy, restore with a verified protection copy, undo, and independently check bytes and in-game load; include externally running-emulator refusal and Home's selected-moment reset. Preserve the original save throughout. This closes a partly implemented module before expanding backup coverage; it is not DS exact-state resume.

### U3. Reconcile RetroAchievements production read/cache acceptance — evidence review

Source: [RA acceptance](RETROACHIEVEMENTS.md#acceptance), former milestone 7.

The provider, sign-in and account cache are implemented, while that document retains an earlier deployment/read-rendering gate. Match the installed revision to the evidence and run only missing checks: recognized owned Adventure → real set/unlocks in Hall → refresh → offline/restart cache, with correct account/source labels. Preserve the existing valid login and local memories. Record the result in the module document. Earning remains step 5's separate real-game check; an account login or downloaded complete save does not close it.

### U4. Complete archive/catalogue reconciliation and presentation metadata — open audit

Source: [collection catalogue](COLLECTION_CATALOGUE.md), [transfer incident](ARMADA_DEVICE_BASELINE.md), former milestone 5 and the owner's collection/archive requests.

Reconcile all previously selected source archives, the separate crossover transfer and the current destination/private manifests. A historic 686-record count is not proof that every requested file or later archive arrived. Record verified present, missing, duplicate, intentionally excluded and needs-review entries; finish only the missing authorized imports when storage is healthy. Verify hashes, not just copied filenames.

Review display titles, edition/revision/language labels, catalogue matches, primary/additional Worlds and ROM-hack versions without changing identity by name. Unknown hack geography stays unknown/Fan Worlds. Add sourced catalogue omissions only with a plausible Flip runtime, preserving grey linkable records and excluding closed online-only content. Pair this with steps 1–2 so cleanup does not reimport removed duplicates or silently omit meaningful versions. Acceptance includes a coherent private source/destination report and controller search/link/launch checks for affected records.

### U5. Complete current launch/controller compatibility checks — open per title

Source: [standalone adapters](STANDALONE_ADAPTERS.md#acceptance-and-current-boundaries), former milestones 5/12.

Diamond/Colosseum and PokéPark launch checks do not prove long-play or all-input compatibility. Check DS right-stick pointer/R2 comfort and required touch interactions on Flip. Investigate PokéPark 2's recorded initial black screen and validate the actual Wii controller profile, or preserve a specific unsupported-state explanation. Maintain a small title/runtime/configuration result matrix covering launch, usable input, save creation and clean controller return. Retain tested behavior for existing RetroArch/melonDS/Dolphin paths; no full-collection compatibility claim from one successful title.

### U6. Continue and saved-moment maintenance — open extension

Source: [Home/history](HOME_AND_HISTORY.md), [resume limits](RETROARCH_RESUME.md), [backup shelf limits](SAVE_BACKUPS.md), former milestones 6/9/12.

Preserve current GBA exact resume, source/revision-bound thumbnails, cancellation and explicit normal-launch fallback. First add useful controller maintenance for growing moment/backup shelves: inspect identity/size, handle missing or stale references, and explicitly archive/remove selected TrainerOS-managed artifacts with a recovery path. Full shelves and failed-preparation leftovers must not require guessing which directories are disposable. Distinguish metadata removal, generated cache and external save/state deletion; retain ordinary saves and unrelated history. ROM cleanup and generic software cleanup cannot perform this work implicitly.

Add another core/adapter's saved-moment or backup capability only after its format, ownership, screenshot association and launch-time validation are proven. The installed melonDS CLI does not currently establish an exact-resume entry path; do not promise one. A one-off removal of empty test-history cards is not a shipped global filter hiding playable Adventures without states. Global Continue access from #9 remains deferred until its local shortcuts and select-for-Home behavior are redesigned.

### U7. Broader RetroAchievements content matching — open capability work

Source: [verified matching](RETROACHIEVEMENTS.md#verified-matching-boundary), former milestone 7.

Current whole-file cartridge matching covers GB/GBC/GBA/Pokémon Mini; DS, disc/container and other schemes are not equivalent MD5 cases. After account ownership is unified, choose the next desired format that RA actually supports, verify its authoritative hash rules and test recognized/unrecognized/changed content before publishing a link. Already-patched hacks and soft patches need explicit treatment; titles or artwork are not match evidence. Keep unknown dates/modes honest and cache identities isolated. Unsupported platforms remain unavailable without blocking local Hall history.

### U8. Expand ordinary-save access and progress readers — open per provider

Source: [save services](SAVE_BACKUPS.md), [game progress](GAME_PROGRESS.md), former milestones 9/12/13.

For DS, finish U2 before building on that resolver; independent Gen III reading need not wait on a blocked DS device check. Expand protected ordinary-save access one format at a time, including other desired RetroArch cores (with RTC distinctions where relevant) and Dolphin memory-card/per-game formats only after independent verification. No catch-all copy of guessed save directories.

The first automatic reader supports only exact English FireRed/Emerald builds. Continue the read-only Gen III module with verified party/HP/PP/status and currency before step 8 healing; expose supported party data on Home rather than making it useful only inside a paid service. Add location or other desired progress fields only where their meaning is established. Diamond/DS and Colosseum/GameCube reading are separate next providers after reliable save resolution and format proof; an imported save is not evidence that its parser exists. ROM hacks need exact tested profiles and retain unknown fields otherwise. Validate zero/unknown, save rollback/replacement, checksum failure and refresh after return against in-game values, without modifying source bytes.

### U9. Connect proven progress to personal views — open after source prerequisites

Source: [Trainer overview](TRAINER_OVERVIEW.md), [Pokédex journal](POKEDEX.md), [Hall archive](HALL_OF_FAME.md), [ownership contract](DATA_MODEL.md#ownership-contract--2026-09-13), former milestones 7/8/13.

Finish useful manual Caught in step 7 before save-derived collection history. Then import only actual species/form observations supplied by U8's verified reader, with source/save-lineage/revision and rollback/dedup semantics. Current badge/caught totals are insufficient to identify an individual Pokémon or its catch event. Derived Home/Trainer/World summaries must distinguish manual collection, current-save progress, actual observed visits/playtime and explicit completion memories; represented library Worlds are not visited regions.

Recently observed/caught activity, teams, game-save playtime and Hall-of-Fame suggestions can follow when the source proves their fields. Keep observation time distinct from catch/victory time; a suggestion does not automatically complete an Adventure or award an RA achievement. Reuse reference/artwork and existing journal/archive editing, preserving manual records through import, rollback and restart. Richer individual/shiny timelines remain conditional on provenance rather than a prerequisite for useful manual views.

### U10. Additional Adventure adapters — open, one complete runtime path at a time

Source: [standalone adapters](STANDALONE_ADAPTERS.md), [catalogue runtime limits](COLLECTION_CATALOGUE.md), former milestone 12.

After current launch gaps, the default next adapter is the installed Azahar/3DS path, then Cemu/Wii U, subject to actual supported titles and runtime checks. Verify installed version, launch arguments, content requirements, physical controls, display, process exit/recovery and return before enabling any capability. Switch, PC/Pico and Android/Waydroid catalogue entries need an independently viable runtime/input route; do not enable them merely because the reference includes them. Do not initialize every runtime or build every adapter at once. For each supported integration, ordinary launch can ship before separately proven save backup, exact resume, achievements or progress.

### U11. Reproducible installation, updates and recovery — open deliverable

Source: [device build/installation](ARMADA_DEVICE_BASELINE.md#native-build-and-installation), [session install/rollback](SESSION_PROTOTYPE.md), former milestones 10/11/14.

Turn the development deployment into a repeatable ARM64 application/session install with a documented runtime/version policy and no dependency on private workstation scripts. Implement versioned migration, durable atomic installation, interrupted-update recovery, previous-release rollback and preservation of the personal database/external content. Verify installed bytes after reboot; a write/read through cache alone previously failed to prove durability. Prepare the minimal rollback foundation before steps 9–10 and preserve their managed manifests across supported Armada updates.

Final acceptance is clean install/reinstall, upgrade, interrupted upgrade, rollback, reboot into TrainerOS and maintenance recovery without development tools or losing personal records/saves. Retain normal app mode for diagnosis. Native ARM compilation and a session launcher already exist; they do not close the complete installation/update deliverable.

### U12. Controller configuration and measured handheld polish — open extension

Source: [controller/navigation](UX_NAVIGATION.md), [design](DESIGN_LANGUAGE.md), [device diagnostics](DEVICE_DIAGNOSTICS.md), former milestones 4/11/14.

Measure cold start/first frame, normal idle CPU/RAM, launch/return latency and frame pacing under realistic library/artwork load. Finish a controller-remapping/calibration surface on the central input service, preserving reserved global actions and deterministic focus; test repeat/dead zones and hotplug/foreground recovery with physical input. Check font size, viewport, filter/keyboard/modal usability and all themes on the actual 5.5-inch landscape screen. Decide optional page wrapping through explicit UX validation rather than silently changing it. The existing palette themes and reduced-motion option are delivered and get regression checks, not rebuilt. Startup appearance and atmosphere polish follow measured functional needs; no unsupported claim of improved battery life from a short idle test.

### U13. Sleep/hinge/wake — explicitly deferred

Source: [device investigation](ARMADA_DEVICE_BASELINE.md), [play-session time semantics](HOME_AND_HISTORY.md#recording-and-storage), former milestones 4/11/14.

Keep the current no-sleep policy. When the owner resumes this work, validate the actual native/fake suspend and lid/power paths on the installed Armada build, input/display recovery, active Adventure return, battery/AC behavior and sleep-aware recorded-duration semantics. This task has not been removed or completed; it is deliberately outside the active execution order. Session/software cleanup must preserve that boundary.

## Later collection deliverable — representative classics

The owner's **2026-09-13 follow-up** authorizes freely using the server-attached ROM disk for development tests and requests a later **small selection of iconic games for each system**. This is a planned collection task, not an immediate bulk transfer or a replacement for the complete eligible Pokémon collection. Test-content permission is recorded in [AGENTS.md](../AGENTS.md#authorized-rom-source-for-testing--2026-09-13).

After P4 establishes the library contexts, assemble the selections incrementally alongside P9's verified adapters. Start with roughly **3–5 worthwhile titles per system**, adjusting for available content and usable Flip controls/performance rather than filling a quota. Review each target system explicitly; unavailable files or unsupported runtimes remain recorded gaps, not silently omitted systems or fake enabled cards. Use the existing ROM disk, preserve source originals, keep meaningful editions distinct and verify copied content. Curate readable titles and appropriate media; Pokémon titles retain their World relationships, while general classics go to Multiverse.

**Done when:** each supported target system has a small coherent selection with verified file identity, working launch/controller input, save creation where applicable and clean return on Flip; a private manifest records the selected editions and unresolved system/content gaps. Test saves stay separate from personal playthroughs. This task neither replaces U4's archive-completeness work nor authorizes committing ROMs or private media.

## Optional later ideas — retained, not current delivery requirements

- **Promoted, not removed:** multiple Trainer profiles are now P2 / #19–20; the concrete verified RA notification/jingle subset is P6 / #25. Other restrained notification ideas remain optional.
- **Retained personal-history ideas:** richer individual/shiny timelines, teams, local milestones and observation-derived activity; only trustworthy fields can be shown. U8/U9 remain their source/provenance gates.
- **Retained optional services/content:** cloud sync, additional theme/World-atmosphere packs, reference-provider alternatives/custom species. None blocks current offline delivery or fabricates official data.
- **Retained platform option:** a reproducible custom/preconfigured Armada image only after mature package/session installation proves insufficient. #41 first uses supported stock-Armada layers; earlier boot ownership is a possible later benefit, not a new fork requirement. Hardware modification remains out of scope.
- **Still deferred from earlier work:** #9 global Continue with revised select-for-Home/context rules; U13 sleep/hinge/wake; optional page wrapping decided during U12 physical UX testing.
- **New conditional extensions preserved:** live in-game RA overlay only after an actual safe event/presentation path; Home video only after separate UX/performance acceptance; per-Trainer save namespaces adapter by adapter; simple context audio moods/optional boot sound; later controller color picker, reactive RGB or measured battery-saver policy only as separately chosen features. These are not prerequisites for the initial issues.

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
| [#10](https://github.com/EriArk/TrainerOS/issues/10) | P8 / step 8; verified party/money, guarded healing, protection and in-game validation. |
| [#11](https://github.com/EriArk/TrainerOS/issues/11) | P10 / step 9; Mobile maintenance trial and reversible Steam removal/restore. |
| [#12](https://github.com/EriArk/TrainerOS/issues/12) | P2 management + P6 earning / step 5; canonical active-Trainer account, not one shared credential. |
| [#13](https://github.com/EriArk/TrainerOS/issues/13) | P7 / step 6; separate species/form art packs, offline/corrupt-source handling. |
| [#14](https://github.com/EriArk/TrainerOS/issues/14) | P7 / step 7 + P8/U9; manual species-wide Caught first, proven enrichment later. |
| [#15](https://github.com/EriArk/TrainerOS/issues/15) | P4 / step 4; shared-media Home background now owner/context scoped. |
| [#16](https://github.com/EriArk/TrainerOS/issues/16) | P4 / step 3; recognizable Worlds game media. |
| [#17](https://github.com/EriArk/TrainerOS/issues/17) | P4 / step 3; semantic media identity/provider before consumers; P5 video extension. |
| [#18](https://github.com/EriArk/TrainerOS/issues/18) | P0 audit + P3 steps 1–2; source reconciliation, relink/quarantine and measured cleanup. |
| [#19](https://github.com/EriArk/TrainerOS/issues/19) | P2; first-run controller registration after owner migration; shared creation transaction. |
| [#20](https://github.com/EriArk/TrainerOS/issues/20) | P2; multi-Trainer migration, chooser/PIN, scoped history/accounts/save lineage. |
| [#21](https://github.com/EriArk/TrainerOS/issues/21) | P1; shared tab/panel/content geometry. |
| [#22](https://github.com/EriArk/TrainerOS/issues/22) | P1; quiet healthy status, preserved useful faults and diagnostics. |
| [#23](https://github.com/EriArk/TrainerOS/issues/23) | P1; shared cached pattern; later screenshots remain beneath it. |
| [#24](https://github.com/EriArk/TrainerOS/issues/24) | P6, or earlier after ownership/read proof; distinguish earned/locked/unknown states. |
| [#25](https://github.com/EriArk/TrainerOS/issues/25) | P6; verified deduplicated return-time unlock banner/jingle; live overlay conditional. |
| [#26](https://github.com/EriArk/TrainerOS/issues/26) | P1 existing Power actions + P2 Switch Player; durable gates and fresh confirmation. |
| [#27](https://github.com/EriArk/TrainerOS/issues/27) | P1 shared physical chrome; new service consumers follow the same components. |
| [#28](https://github.com/EriArk/TrainerOS/issues/28) | P3 domain foundation → P4 complete Multiverse system/game browser within Worlds. |
| [#29](https://github.com/EriArk/TrainerOS/issues/29) | P5; explicit local ScreenScraper video, idle delay and one bounded player. |
| [#30](https://github.com/EriArk/TrainerOS/issues/30) | P3; explicit curated-eligibility change, preserved personal records and eligible checklist. |
| [#31](https://github.com/EriArk/TrainerOS/issues/31) | P4; Home X contexts, independent selections/scoped Continue and shared history. |
| [#32](https://github.com/EriArk/TrainerOS/issues/32) | P4; distinct Multiverse Home composition, honest general metrics, common controls. |
| [#33](https://github.com/EriArk/TrainerOS/issues/33) | P1 compact shared headings; explicit Home/onboarding/banner exceptions. |
| [#34](https://github.com/EriArk/TrainerOS/issues/34) | Every delivered screen, then P12 complete physical content/state audit. |
| [#35](https://github.com/EriArk/TrainerOS/issues/35) | P1 quick Start volume/brightness, reusing actual device services. |
| [#36](https://github.com/EriArk/TrainerOS/issues/36) | P5 selectable licensed music/UI audio, lifecycle and jingle arbitration. |
| [#37](https://github.com/EriArk/TrainerOS/issues/37) | P0 support probe → P5 optional debounced charger haptics; no sleep resumption. |
| [#38](https://github.com/EriArk/TrainerOS/issues/38) | P0 support probe → P5 Armada capability-driven RGB; Flip support unproven. |
| [#39](https://github.com/EriArk/TrainerOS/issues/39) | P10 Settings front end to #11 helper; optional Steam restore/install, no second installer. |
| [#40](https://github.com/EriArk/TrainerOS/issues/40) | P1 viewer/current guide → incremental articles → P11 complete offline/runtime-fact audit. |
| [#41](https://github.com/EriArk/TrainerOS/issues/41) | P12 supported-layer startup branding with profile routing, update safety and recovery. |
| Owner follow-ups, 2026-09-13 | P10 / steps 9–10: Mobile-only maintenance if viable; careful software/background cleanup with measured results. Preserve all old work and future ideas through this replan. No new GitHub issue is implied. |
| Owner test-content and classics follow-up, 2026-09-13 | Standing permission to use the server's ROM disk for tests; small iconic-games selection per system after P4, alongside P9's validated adapters. Source files and personal saves remain protected. |

## Historical milestone map

Older module notes use milestones **0–14** from the [previous plan](https://github.com/EriArk/TrainerOS/blob/7c7fd786f1eab44427d0abd5f709c1c4f95012b6/docs/ROADMAP.md), not the new queue numbers. Checkpoints remain in Git history and module evidence documents.

| Former milestone | Current location / status |
| --- | --- |
| 0–3: skeleton/controller/mock/backend | Working baseline; extend only what the active feature needs. |
| 4: device baseline | U1/U12 remaining evidence/performance; U13 deferred wake. Later checks supersede early “unverified” notes. |
| 5–6: Worlds/Home/Continue | U4–U6 plus steps 1–4; identity, archive completeness, actual launch and moment maintenance remain explicit. |
| 7: Trainer/Hall | U3/U7/U9 plus step 5; production reads, matching/earning and derived personal views. |
| 8–9: Pokédex/Center | U2/U6/U8/U9 plus steps 6–8; finish DS backup, add verified formats and source-aware collection/services. |
| 10–11: dedicated/default session | U1/U11 plus step 9; preserve tested session and finish installation/update recovery. U13 wake is deferred. |
| 12–13: adapters/progress | U5/U7–U10; concrete per-title launch, content matching, save readers and feature projections. |
| 14: packaging/polish | U11/U12; themes/reduced motion and initial ARM build already delivered. U13 remains explicitly deferred. |

## Delivery gate for each increment

Review actual changes and acceptance; run affected native checks, broader tests for shared input/storage/lifecycle, and rendered SDL controller scenarios for QML. Inspect 960×540/1920×1080 and Flip layouts. Preserve L1/R1, A/Y selection-versus-launch, planned Home X context switching, Back/modal priority; cover offline/unknown/empty/corrupt states. Fixtures are not device proof. Update the matching offline Help article/capability facts with each delivered feature; complete the #34 screen/state matrix as screens land.

Keep credentials, private reports/manifests, ROMs, BIOS, saves and unlicensed artwork out of Git. Documentation-only planning needs content/link/diff checks, not an unrelated rebuild. Stage reviewed paths, commit, fetch/integrate remote work, push normally and verify remote SHA/final status. Do not run/poll GitHub Actions. See [workflow](DEVELOPMENT_WORKFLOW.md).
