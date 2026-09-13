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

## Unified execution order — existing work and new issues

The first replan compressed prior unfinished work into six broad bullets. This follow-up restores explicit acceptance and execution slots. **New issues extend the existing backlog; they do not replace it.** The U identifiers refer to the carried-forward register below; steps 1–10 retain their detailed new-issue descriptions and existing links.

| Order | Work to finish together | Dependency / exit condition |
| --- | --- | --- |
| First | Existing-device gaps U1–U3: reliability evidence, melonDS backup activation and RA production read/cache checks | Reconcile delivered evidence first; perform only missing checks. Diagnose an active freeze before large transfers or destructive work. These are bounded checks, not a restart of completed modules. |
| Collection | Steps 1–2 plus U4: private archive inventory, missing transfers, names/Worlds, exact variants and cleanup | Audit source/destination manifests before changing content; preserve personal/save identity and the broad reference catalogue. |
| Existing play paths | U5: DS pointer comfort, Wii failure and actual controller launch/return coverage | Finish or document the specific unsupported title/configuration; an unrelated blocked title need not stop independent UI work. |
| Adventure presentation | Steps 3–4 plus U6's existing thumbnail/provenance checks | Shared media → Worlds → Home. Finish controller moment/backup maintenance as its own subsequent increment, never implicit ROM/cache pruning. |
| Achievements | Step 5 plus U3/U7 | Existing account/read evidence → global ownership/migration → real earning → additional verified matching formats. No repeated login just to satisfy an old pending note. |
| Personal collection | Steps 6–7 plus U9's derived journal/Trainer/Home views | Species artwork and useful manual Caught first; save-derived observations require the appropriate reader and provenance. |
| Save enrichment and services | U8–U9, then step 8 for the first proven title | Verify ordinary-save access/backups → read party/money/progress → expose honest Home data → optional guarded healing. New DS/GameCube readers proceed separately; healing does not wait for every platform. |
| More playable Adventures | U10, one runtime/adapter at a time | Launch/input/return first; save backup, exact resume and progress are independent later capabilities. |
| System delivery | U11's install/update/rollback foundation, then steps 9–10 | Preserve a recoverable installed build and manifests before Mobile/Desktop/Steam changes or software cleanup. Final clean-install acceptance follows the selected supported session setup. |
| Release polish | U12 and final U11 acceptance | Measure startup/idle/frame pacing, finish controller configuration and verify install/update/recovery on Flip. Already delivered themes/reduced motion are preserved. |

This is the default order, not an artificial dependency between unrelated features. Finish and deliver each bounded increment. If a required device/card interaction is unavailable, prepare its fixture/contract work, record the exact gate and advance an independent ready item. U13 sleep remains explicitly deferred, not a silent release claim or a blocker for all development. Optional ideas below do not become prerequisites.

## Detailed new-issue increments

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

## Optional later ideas — retained, not current delivery requirements

Multiple Trainer profiles, richer timeline/shiny history, local milestones, optional cloud sync, additional theme/World-atmosphere packs and restrained notifications remain on the product backlog. Reference-provider alternatives/custom species remain replaceable extensions rather than fabricated official data. Consider a custom Armada image only after mature package/session installation proves insufficient. Hardware modification remains out of scope.

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
| 4: device baseline | U1/U12 remaining evidence/performance; U13 deferred wake. Later checks supersede early “unverified” notes. |
| 5–6: Worlds/Home/Continue | U4–U6 plus steps 1–4; identity, archive completeness, actual launch and moment maintenance remain explicit. |
| 7: Trainer/Hall | U3/U7/U9 plus step 5; production reads, matching/earning and derived personal views. |
| 8–9: Pokédex/Center | U2/U6/U8/U9 plus steps 6–8; finish DS backup, add verified formats and source-aware collection/services. |
| 10–11: dedicated/default session | U1/U11 plus step 9; preserve tested session and finish installation/update recovery. U13 wake is deferred. |
| 12–13: adapters/progress | U5/U7–U10; concrete per-title launch, content matching, save readers and feature projections. |
| 14: packaging/polish | U11/U12; themes/reduced motion and initial ARM build already delivered. U13 remains explicitly deferred. |

## Delivery gate for each increment

Review actual changes and acceptance; run affected native checks, broader tests for shared input/storage/lifecycle, and rendered SDL controller scenarios for QML. Inspect 960×540/1920×1080 and Flip layouts. Preserve L1/R1, A/Y selection-versus-launch, Back/modal priority; cover offline/unknown/empty/corrupt states. Fixtures are not device proof.

Keep credentials, private reports/manifests, ROMs, BIOS, saves and unlicensed artwork out of Git. Documentation-only planning needs content/link/diff checks, not an unrelated rebuild. Stage reviewed paths, commit, fetch/integrate remote work, push normally and verify remote SHA/final status. Do not run/poll GitHub Actions. See [workflow](DEVELOPMENT_WORKFLOW.md).
