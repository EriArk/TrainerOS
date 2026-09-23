# TrainerOS Roadmap

**Owner scheduling amendment, 2026-09-23:** generic artwork-pack installation,
selection, verification/update/removal and Pack Studio all move to the final
artwork delivery stage. Existing private illustrations remain in both the Dex
list and selected entry. Early #51 sprites/portraits use a separate bounded,
read-only local provider; its necessary validation is not a generic pack manager.
The next order is sprites/portraits, P2 ownership, then P3/P4 library binding.
All earlier acceptance remains; ROADMAP is the execution queue.

Reconciled on **2026-09-19** for **#1–64**, including the updated #9/#13/#15/#17/#28/#31/#40 and #49's explicit supersession of state-based resume. **Phases P0–P12 own the dependency queue**, not a serial list of 64 issues. Stable steps 1–10 and U1–U13 preserve prior commitments with explicit amendments. [Earlier acceptance](EXPANSION_PLAN.md), [#42–62 acceptance](EXPANSION_42_62.md) and [#63–64 acceptance](EXPANSION_63_64.md) specify each slice. #62 changes documentation only, not feature availability or GitHub issue states.

This is a dependency replan, not a reset. The [pre-expansion plan](https://github.com/EriArk/TrainerOS/blob/252f04e0772bd05966fb212ca736d303cab366ea/docs/ROADMAP.md) and the preservation map below make previous commitments traceable. Read the phase table for what comes next, then the referenced acceptance; do not execute the old numbered register as a competing queue.

## Delivered increments under this plan

- **2026-09-23 - P4 cartridge runtime group:** installed missing AArch64 cores and connected 42 existing SNES, Mega Drive, 32X, NGPC and PC Engine registrations without replacing owner metadata or history. The collection now has 80 configured records and 53 remaining setup gaps. Windows 39/39 and ARM64 43/43 passed; actual Flip launch/input/guarded return and clean exit pictures verified for one title per system. [Evidence, ordinary-save limits and remaining runtimes](MULTIVERSE_RUNTIMES.md). Disc/arcade routes remain next within P4, followed by media and World/Home presentation.

- **2026-09-23 - P3/P4 persistent Multiverse binding:** schema 11 preserves existing Pokemon records and registers 133 real games across 22 populated systems. Independent per-Trainer Home choices, selection-only Y, fixed A, shared history and clean exit pictures now work with existing adapters. 38 registrations resolve to installed configurations; 95 retain setup gaps. Windows checks and ARM64 43/43 passed; actual Advance Wars launch/return and installed screenshots verified. [Evidence and remaining runtime/media work](MULTIVERSE_BINDING.md).

- **2026-09-23 — P3/P4 collection file preparation:** copied 133 games across 22 systems plus available BIOS to Flip's microSD; all 189 files passed SHA-256 verification. Small systems receive 8–12 titles; source shortages and runtime gaps remain explicit. [Selection, layout and acceptance](MULTIVERSE_COLLECTION.md). Real Multiverse binding and emulator/controller setup remain next.

- **2026-09-23 — P3/#63 chronology:** data-driven platform/edition ordering in the real collection, preserving filters and stable selection. 96 sourced edition years; unverified dates remain unknown. [Sources and limits](WORLD_CHRONOLOGY.md). Production Multiverse no longer shows six empty fixture systems; real binding remains next.

- **2026-09-23 — P2 local ownership foundation:** schema 8 preserves the current Trainer and shared library, assigns existing journal/favorites/history/Hall/navigation to that owner, and scopes the live repositories. Atomic first-profile adoption preserves unnamed records. Switch Player/PIN and per-Trainer RA activation remain gated on the next binding increment. [Boundary, migration and acceptance](TRAINER_OWNERSHIP.md).

- **2026-09-23 — owner UI refinement:** removed the separate sprite/portrait inspector and its Down hint; the illustrated detail and animated companion remain. Author-entry tooling and consolidated Credits move to final row 14 alongside Pack Studio, while underlying attribution/source/terms remain intact. This supersedes the inspector portion of the initial #51 delivery below.

- **2026-09-23 — early P7/#51 optional sprites/portraits:** preserved list/detail illustrations, added a bounded detail companion with idle/sleep/walk-away-and-return, and a controller source/emotion viewer. Private credited assets cover 527 exact forms (486 Idle clips); no substitute forms or Party/save claims. Windows/ARM64 checks and populated native Flip motion passed; production installation and real-session screenshots verified. Generic pack management remains final row 14. [Evidence and remaining gates](POKEDEX_SPRITES.md).

- **2026-09-20 — early P7/#60 artwork bootstrap:** the complete #58 corpus is on Flip, with hash-verified originals and 4,404 bounded derivatives. Real Pokédex list/detail and Trainer species-picker art, controller alternative selection/source credits, honest 1,437/1,579 form coverage and corrected lower-rail layout are delivered. Native 1080p SDL rendering, decode/scroll/RSS observations and private canonical mapping/profile exports feed #57. This is not the generic pack format, Studio, a sprite provider or resolution of the 142 ambiguous/missing forms. [Evidence and limits](ARTWORK_BOOTSTRAP.md).

- **2026-09-19 - early P7/#64 badges:** sixteen credited Kanto/Hoenn vector-derived assets replace Home crystals for existing verified FireRed/Emerald observations. Shared compact/large tray, neutral unknown/missing-art states and controller Credits are implemented; no new save parser. [Acceptance](EXPANSION_63_64.md#first-badge-delivery---2026-09-19). Actual in-game appearance comparison remains open. Journey now reuses the verified Home observations; new semantic Journey/Champion data remains P8.

- **2026-09-19 - P1 Hall/RA pair:** L2/R2 replaces the inner section buttons, preserving each face's record/detail/action through switching and restart. B stays within its face; shared Y and modal priority remain intact. Provider completion and unavailable records preserve a usable view. Live Journey/Champion and exact selected-Adventure RA projection remain later provider work. [Hall routing](HALL_OF_FAME.md).
- **2026-09-19 — P1 shared #9 selection and first #43 pair:** Y selects from Home/Pokédex/Center/Trainer/Hall/RA without launching, with modal priority and Select taking former local Y shortcuts. L2/R2 pairs Pokédex with the protected-save Center; Start reaches that same selected-game shelf. Stale asynchronous reads cannot populate another Adventure's shelf, and a missing explicit choice never falls back to another game. P2 ownership and later current-save/RA/Multiverse consumers remain open. [Contract and acceptance](SHARED_ADVENTURE.md).

- **2026-09-19 — #49 durable exit media and ordinary Home selection:** confirmed clean process exits publish a scoped, integrity-checked picture together with session completion in schema 7. Home and Choose Adventure share that picture; older valid media survives cancellation, failed capture/write and later uncaptured sessions. Production selection no longer exposes state slots; old Home state targets are ignored without losing the Adventure choice, and the verified mGBA ordinary launch disables automatic states without changing save paths. Multiple-owner activation, legacy-artifact retirement/resolver decoupling, other adapters and reboot/access proof remain open. [Acceptance and limits](ADVENTURE_EXIT.md#durable-exit-media-and-ordinary-home-selection).

- **2026-09-19 — #49 Home integration:** installed opt-in RetroArch exit transport, physical neutral gating, clean capture, owned-window close and independent crash/stall restoration. The owner confirmed the working exit question. Home outside a game returns to the Home page; Start/L1/R1 retain their roles. Fixed SDL source selection after granting read-only physical-pad access. Windows 33/33 and ARM64 36/36 passed; non-testing ARM build installed with backup and unchanged database integrity. The follow-up presents a compact question over the captured game with minimal copy. Standalone exit adapters, reboot/access proof, durable media and legacy-state migration remain open. [Evidence](ADVENTURE_EXIT.md#home-transport-increment-2026-09-19).

- **2026-09-19 — #49 Qt presentation:** separate exit window, fixed Switch-style A/B actions and a generation-scoped input gate requiring isolation, window focus and released controls. All save policies now ask before closing; verified autosave uses an exit question per the owner's clarification. Home/Guide replaces Start+Select in the target design, pending physical routing/interception validation. The presenter remains unavailable for normal launches until the platform lease/watchdog and graceful-close connection pass their gate. Media and migration commitments remain open. [Details](ADVENTURE_EXIT.md#qt-presentation-increment-2026-09-19).

- **2026-09-19 — #49 foundation and Flip feasibility:** native exit protocol with real child-process tests; actual clean Emerald title capture, temporary overlay focus handoff, physically observed InputPlumber A/B isolation/cancellation and owned-window close. [Evidence and limits](ADVENTURE_EXIT.md#flip-feasibility-follow-up-2026-09-19). The owner requested Switch-style shell A/B (right confirm, bottom back), now centralized with matching diagnostics/SDL scenarios. Production Qt confirmation, crash-safe input release, media persistence and state retirement remain open; the probe does not deliver the full #49 feature.

- **2026-09-19 — #62 documentation:** product/navigation/domain/architecture and all acceptance registers reconciled; legacy state semantics marked superseded, tests/device evidence retained. This is not delivery of #42–61 code.

- **2026-09-13 — #21:** shared panel/tab geometry and safe content inset completed; active tabs overlap the raised panel with contact depth. Windows and native ARM64 rendered SDL/persistence checks passed (6/6 each); production installed and controller-driven Gamescope captures inspected on Flip. [Details](DESIGN_LANGUAGE.md#shared-chassis-geometry--delivered-2026-09-13). P1's remaining items and all P0/U investigations retain their own gates.

- **2026-09-20 - P1/#26/#35:** Start quick volume/brightness/mute with an X shortcut, coalesced pending adjustments and external refresh; mounted Power defaults to Cancel and uses the durable confirmed exit path. Dedicated-session physical volume-key handling stays active during Adventures. Switch Player remains explicitly unavailable pending P2. [Behavior and verification](DEVICE_CONTROLS.md).
- **Platform follow-up from that check:** the existing built-in pad read ACL was absent despite its startup drop-in. Access was restored with the existing narrow grant hook and the exit helper/watchdog started again. Preserve a boot/device-re-enumeration investigation and physical Home/A/B regression in the platform acceptance backlog; do not treat this repair as proven persistence.

- **2026-09-23 — P2 Trainer entry and family PIN recovery:** schema 10, optional 4–6 digit PINs, protected startup and switching, six-digit parent-defined family code, explicit reset preserving records. Personal repositories and RA binding wait for entry. [Behavior, storage and limits](TRAINER_ACCESS.md). Deletion and remaining context/domain audit stay next; later phases are unchanged.
- **Platform follow-up from PIN delivery:** after repeated session transitions, SDDM rejected both TrainerOS and Plasma Mobile before the shell service started (exit 5). A normal reboot restored default TrainerOS entry. Preserve repeated-transition diagnosis in P10/P12; this does not expand the PIN feature or prove the cause fixed. [Evidence](TRAINER_ACCESS.md#verification--2026-09-23).
- **Platform follow-up from owner-save delivery:** after the maintenance/session transition, the hidden built-in pad had mode 000 without its named read ACL; the existing root grant hook restored the exit helper. P10/P12 must verify/recover that access after source recreation as well as service startup, without restarting InputPlumber during play. Physical Home acceptance after reboot remains required; this increment does not claim a permanent transition fix.

## Working baseline

The **2026-09-13 combined visual pass** implements #22 quiet status, #23 shared pattern and #33 headings, four visible World game rows and larger region cards. Follow-up #21 raises the lip to the inactive bevels without shortening the active tab; the enlarged title grows from a continuous upper/side/lower chassis around one beveled screen aperture. Existing #27 chrome/#34 content receive mounted controls, grouped Start and consistent service trays. [Visual review](VISUAL_REVIEW.md). The #26 Power UI is now delivered (Switch Player still requires P2); #24 earned states and Multiverse/onboarding/media and their final #27/#34 review remain in their original phases; all U gates are preserved.

**Added for a future P5 increment (owner, 2026-09-13):** probe Flip gyroscope availability under ArmadaOS; if usable, build gentle tilt-driven background parallax, otherwise a calm autonomous motion alternative. Keep controls/text stationary, honor Reduced Motion, pause hidden/in-game, and verify frame pacing/power/comfort on the device. [Acceptance](DESIGN_LANGUAGE.md#future-background-motion--owner-request-2026-09-13). No sensor support is claimed yet.

The native skeleton, shared controller/UI components and SQLite foundation already exist. The latest owner clarification prioritizes completing the whole handheld UI on small real/fake boundaries, then binding functional modules in dependency order. Early artwork belongs with UI; new save formats and heavy research belong near the end. Existing real features remain operational. Do not restart the mock or build speculative frameworks. Visuals remain replaceable; product/input invariants stay fixed.

| Area | Implemented / evidence | Remaining boundary |
| --- | --- | --- |
| Shell and storage | C++20, Qt Quick, SDL; five peer pages, keyboard, themes/reduced motion, profile and SQLite state. Physical navigation/profile/launch confirmed by the owner. [Persistence](LOCAL_PERSISTENCE.md), [device baseline](ARMADA_DEVICE_BASELINE.md). | New routes/migrations need their own checks; earlier mock checkpoints are historical. |
| Worlds | Region-first catalogue, missing editions, platform badges, file attachment, custom/multiple Worlds and stable personal IDs. [Catalogue](COLLECTION_CATALOGUE.md). | Personal duplicate/language audit; catalogue presence is not ownership or compatibility. |
| Adventures | RetroArch and standalone melonDS/Dolphin launch/return; selected DS/GameCube/Wii titles exercised on Flip. [RetroArch](RETROARCH_ADAPTER.md), [standalone adapters](STANDALONE_ADAPTERS.md). | Per-title coverage; PokéPark 2's initial black screen unresolved. Installed emulators do not prove 3DS/Wii U/Switch integration. |
| Home / Continue | Persistent choice, fixed A ordinary launch, shared Y selector, observed history and durable clean exit pictures. [Shared routing](SHARED_ADVENTURE.md), [Home/history](HOME_AND_HISTORY.md). | P2 per-Trainer/domain ownership and further current-save consumers remain open; historical state resume is retired in production. |
| Progress | English FireRed original/Rev 1 and Emerald ordinary saves supply badges/National Dex counts; exact-set badge artwork replaces the earlier crystal presentation. [Game progress](GAME_PROGRESS.md). | No party/money reader, save editor, individual capture history, DS/GameCube or ROM-hack progress claim. |
| Trainer / Hall | Persistent profile, derived overview, manual memories; real RA sign-in/read provider and account cache implemented. [Trainer](TRAINER_OVERVIEW.md), [Hall](HALL_OF_FAME.md), [RA](RETROACHIEVEMENTS.md). | Central account management and same-account emulator earning are #12, now scoped to the active Trainer by #20. Login/read support does not prove an earned unlock. |
| Pokédex | Offline 1025 species / 1579 forms, search/filter, favorites and persistent manual species-wide Seen/Caught/notes. [Pokédex](POKEDEX.md). | Bootstrap illustrations are delivered; generic runtime pack management and separate sprites remain early P7. Dedicated Caught collection and observed individuals retain P7/P8 gates. |
| Pokemon Center | Paired Pokedex/Center route follows the shared choice; verified GBA/mGBA protected backup/restore and opt-in melonDS resolver. [Save services](SAVE_BACKUPS.md). | Party/Storage presentation is delivered; real individuals, additional formats and healing retain P8 gates. |
| Device/session | TrainerOS installed as default; launch/return, shell-crash survival, startup-failure recovery and Steam/Plasma transitions exercised. Battery, volume, brightness and power controls exist. [Session](SESSION_PROTOTYPE.md), [controls](DEVICE_CONTROLS.md). | Sleep disabled; wake reliability and prior I/O freeze unresolved. Mobile replacement and reversible Steam removal are new work. |

Module documents contain dated evidence and exact limits. Existing Windows/Linux/ARM results apply to those revisions, not automatically to the next change. GitHub Actions is not a delivery gate, per the owner's instruction.

## Decisions and precedence

- **Latest #9 supersedes deferred shared Y:** one per-Trainer current Pokémon Adventure serves Home/Dex/Center/Hall/RA and relevant Trainer projections. Shared drawer A selects without launching; fixed Home A launches normally. Worlds keeps local Y, modals retain priority, no duplicate persistent context capsule. Resolve legacy local shortcuts during implementation.
- **#49 replaces state-resume development:** ordinary saves/autosaves only; capture gameplay before save confirmation while the process is alive, cancel back to it or exit gracefully, verified autosave also asks exit permission (owner clarification, 2026-09-19). Home/Guide is the target request after its input-routing gate. #6/#8 state milestones are historical, not a reason to expand legacy code. Preserve user saves/history and retire only owned obsolete artifacts safely.
- **#43 pairs existing pages:** L1/R1 remains five primaries; L2/R2 is Worlds/Multiverse, Pokédex/Center, Hall/Journey/RA. Home X remains #31; preserve per-face routes and launch return.
- **#42/#50 exact builds before projections/writes:** evidence-bound reads first, independent guarded writes later. #46 current-save Dex and #47 Journey/Champion retain manual #14/Hall history; #48 RA remains external. Center Party is practical; Playroom/practice are separate, with no save mutation by play.
- #61 runtime prerequisites remain #58 raw corpus -> #60 real Flip bootstrap/canonical map and measured profiles -> #57 required contract/shared validation. **Owner clarification, 2026-09-20: #59 Qt desktop Pack Studio moves to the very end**, after handheld UI and asset structure stabilize; it no longer blocks Settings polish. Keep early work limited to necessary runtime loading/validation; do not prematurely freeze the final authoring format. ROADMAP owns execution order. No resident downloader; early artwork remains independent from save writers. Classic illustrations remain primary list art; #51 sprites serve detail/scenes.
- **Sessions change deliberately:** [#11](https://github.com/EriArk/TrainerOS/issues/11) replaces permanent Steam retention with reversible removal. The later request prefers **TrainerOS + Plasma Mobile only**, if Mobile can replace maintenance/recovery Desktop. Retain current working sessions until replacement/restore gates pass. Preserve shared KDE dependencies and personal Steam data. See [platform migration](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).
- **Collection remains English-first; eligibility is intentionally revised by [#30](https://github.com/EriArk/TrainerOS/issues/30):** a complete collection of substantial playable Pokémon titles with credible Flip/controller routes replaces the broad historical/promotional checklist. Missing eligible editions stay grey/linkable; meaningful revisions, translations and ROM hacks remain. Per-title review must not erase substantial spin-offs. #18 still concerns redundant personal files; #30 concerns reference/navigation eligibility and never deletes personal data. All previously requested sources are reconciled in U4, including intentionally excluded items.
- **Multiverse is an explicit product extension:** [#28](https://github.com/EriArk/TrainerOS/issues/28) adds non-Pokémon system browsing inside Worlds; Pokémon Worlds stays region-first. [#31](https://github.com/EriArk/TrainerOS/issues/31) reserves Home X for Pokémon/Multiverse, with separate Home selections and Continue per Trainer/context. [#32](https://github.com/EriArk/TrainerOS/issues/32) permits a distinct dark inner Multiverse Home while preserving the shared chassis and fixed A/Y behavior. No sixth primary page or second launch/history engine.
- **Multiple Trainers promote a former future idea into planned delivery:** [#19](https://github.com/EriArk/TrainerOS/issues/19)/[#20](https://github.com/EriArk/TrainerOS/issues/20) require owner-scoped data before chooser/PIN/onboarding. Library/installations/shared media stay device-wide; history/journal/Hall/Home and RA identities are per Trainer. Shared external saves do not become separate playthroughs automatically. #12 means one account service across features, not one account across people.
- **New menus preserve deliberate input:** [#26](https://github.com/EriArk/TrainerOS/issues/26)/[#35](https://github.com/EriArk/TrainerOS/issues/35) add Power and quick controls. Existing post-login L1/R1 cancels transient panels and switches pages; it never adjusts sliders or confirms actions. Wording suggesting shoulders should be ignored is not adopted silently; see [input reconciliation](EXPANSION_PLAN.md#trainer-ownership-onboarding-and-power). Before profile unlock no normal page can be accessed.
- **Sources stay separate:** current-save observations, manual journal assertions, personal history and external account achievements have different owners. Artwork proves none of them. See [ownership contract](DATA_MODEL.md#ownership-contract--2026-09-13).
- **Sleep is outside this queue.** Do not enable suspend or reopen wake trials as a session-migration side effect. A new black screen remains a fault to diagnose, not proof of sleep.
- **System cleanup is separate from ROM cleanup:** remove verified unnecessary software/background work to reclaim space and reduce idle load, using a dependency audit and reversible batches. Do not confuse fewer session entries with fewer installed bytes or processes.

## Unified execution order — existing work and new issues

This table is the **single execution queue**, updated for the owner's 2026-09-19 UI-first and 2026-09-20 Studio-last clarifications. **Read rows top to bottom, not by phase number.** P0-P12 remain stable work-area identifiers so earlier links and evidence keep their meaning; their numerical order is no longer the schedule. Acceptance registers below preserve scope, not a second queue.

Complete coherent increments within each row. A UI increment includes reusable components, the entire controller flow, explicit states and small data contracts; it does not claim the future service works. Use existing real services wherever available. Future data uses clearly labelled development fixtures, never fabricated personal progress in production. Unsupported writes, exchanges and battles stay unavailable in the normal app.

| Order / stable phase | Work and source | Dependency / completion boundary |
| --- | --- | --- |
| **1. P0 - targeted baseline** | Keep completed native/input/storage/lifecycle work; inspect relevant health/evidence and #62 reconciliation; retain U1-U5 investigations and collection inventory | Only checks required by the next increment. No exhaustive ROM hashing, RGB probing or save research as a prerequisite for local UI. A demonstrated storage fault blocks heavy device writes. Preserve recovery and existing save protections; no sleep tests. |
| **2. P1 - whole handheld UI and controls** | Finish Start/Settings/Power/quick controls (#26/#35); Trainer chooser/PIN/onboarding/account flows; Worlds/Multiverse and both Home contexts; Dex/Center Party/Storage, Journey/RA, Playroom/Practice/Link Counter presentations; media/device settings and all loading/empty/error/unsupported states | Reuse delivered #21/#22/#23/#27/#33, shared #9 selection, Dex/Center and Hall/RA pairs, and #49 ordinary exit. Prove controller flow/focus/Back and handheld layout per slice. Minimal presentation contracts and fake providers only where real data is absent; no new parser, battle engine or transaction research needed to finish these screens. Functional ownership remains P2. Full Help stays P11; runtime artwork management obeys P7 safety gates; desktop Studio waits for final row 14. |
| **3. P7 - early Pokedex art, sprites and badges** | #13/#61: retain completed #58 corpus and #60 Flip bootstrap. #57 generic runtime pack management and #59 Studio move to final row 14. #51 sprite/portrait assets and bounded provider; #64 accurate League badge sets, neutral fallback and Credits. #14 manual Caught projection can bind after P2 | Start alongside the relevant P1 screen once its slots are usable; do not wait for all P1 screens or P2-P8 integrations. Preserve art bootstrap prerequisites and real Flip geometry proof; do not freeze the final authoring format during early UI work. Classic illustrations remain primary list art; optional sprites serve detail/scenes. Badges can consume existing verified FireRed/Emerald bits after exact-set checks, without a new save reader. Source/license/credits checks occur before asset delivery. Artwork preparation does not certify save progress. |
| **4. P2 - Trainer/backend ownership** | #2 audit and #20 lossless migration -> bind prepared #19 onboarding/chooser/PIN -> #26 Switch Player and #12 active-Trainer account; shared #9 context persistence; first verified per-Trainer RetroArch ordinary-save route (owner follow-up 2026-09-23) | Shared library/installations/media stay device-wide; journal/history/Hall/Home/RA belong to a Trainer. Preserve legacy identity and external save lineage, scope/invalidate async results, block unsafe switching. Verify rollback and restart before enabling multiple owners. UI prototypes do not grant access across profiles. |
| **5. P3 - library and chronology** | #28 domain/system metadata, #30 eligible catalogue, U4 archive/title/hack completeness; #63 oldest-platform-first World lists; steps 1-2/#18 audit and safe relink/quarantine | Real ownership mutations follow P2. Preserve private content, variants, missing/linkable editions and separate histories. Chronology is data-driven, retains stable selection/search order, and does not require save research. Read-only sorting/metadata can land earlier when it touches no pending ownership migration. Destructive cleanup is not a prerequisite for Worlds UI. |
| **6. P4 - bind Worlds and Home** | #17 static Adventure media -> #16 Worlds/paired Multiverse; #56 environmental/split cards; #31 per-context Home -> #15 exit backgrounds/#32 Multiverse Home; install missing viable emulators and required BIOS, import four server-disk ROMs per supported system; U6 ordinary-backup maintenance | P1 UI and P2/P3 identities before real context mutations. Reuse one launch/history engine, fixed A and selection-only Y. Existing clean exit images replace state thumbnails. Static/fallback media works before optional video or a complete art collection; environmental presentation can be prepared in P1. Deliver the initial playable Multiverse collection in this same P4 increment; record unavailable files/runtimes explicitly. P9 retains unresolved runtime paths and broader verification, not the initial collection as a deferred prerequisite. |
| **7. P5 - motion, media and feedback** | #29 video, #36 audio packs; #37 haptics/#38 RGB and gyro/background motion capability checks; #52 bounded party animation using early #51 assets | Stable UI slots and lifecycle pause/Reduced Motion first. Probe only the capability being added. Fixture scenes are allowed early; production living Party/Playroom waits for P8 exact records. Unsupported hardware never blocks unrelated work. Preserve codec, ownership, credits and measured Flip performance gates. |
| **8. P9 - finish playable runtime paths** | U5 current input/title gaps; U10 Azahar then Cemu by viability; per-runtime #49 exit coverage; remaining gaps in the P4 representative classics collection | Verify ordinary launch/input/in-game save/load/exit/return per runtime. This does not wait for optional P5 effects or new progress parsers. Matching/read research remains late P6/P8. Other systems stay conditional; protect source ROMs and personal saves. Existing supported routes are not rebuilt. |
| **9. P10 - system delivery and cleanup** | U11 reproducible install/update/rollback -> #11 verified Mobile/Steam consolidation -> #39 restore UI -> software/background cleanup; complete eligible #18 cleanup after relink/consumer proof | Minimum recovery exists throughout development; final migration follows known runtime/package dependencies. Preserve KDE dependencies, maintenance/SSH, personal content and manifests. Physically verify restore/reboot, then measure space/load. Quarantine alone is not reclaimed space. |
| **10. P6 - deeper achievements integration** | U3/U7 matching/cache limits; #24/#48 bind prepared earned/current-Adventure views; #12 actual earning; #25 deduplicated unlock banner/jingle | Keep existing RA login/read/cache working throughout. Account isolation and exact-title mapping precede new earning claims. Ordinary read-only UI can bind earlier after P2; heavier format/matching investigations come here. Real earning proof is separate from login; return-time notifications first, live overlay conditional. |
| **11. P8 - late save research and functional Center** | #50/#42 exact-title samples and readers/U8 -> #46/#47 current Dex/Journey/Champion/U9 -> #44/#53 Party/Storage -> guarded #10 healing/writes. Bind #52/#54 real Party/Playroom; then #55 exact-rules practice spike and #45 Link Counter | Start with one exact build and complete its consumer before another parser. Preserve working GBA observations/backups now. U2 gates DS save operations; no all-platform research ahead of UI. Each field/build needs fixtures and physical proof. #49 protection precedes writers. Read-only practice needs verified Party/rules/ARM64 feasibility, not a save writer; it can proceed independently if healing is blocked. Link needs compatible writers, durable two-device recovery and a second physical device. Conditional failures do not block ordinary Center or final docs for delivered capabilities. |
| **12. P11 - complete offline guide** | #40 viewer and all current-feature, content/BIOS/media/account/save/maintenance/recovery articles | Entire product Help implementation near the end after actual flows stabilize. Engineering docs, capability facts, recovery notes and required asset Credits remain current earlier. Audit controller routes, internal links and version-matched facts; planned functions are not documented as available. |
| **13. P12 - startup and whole-product acceptance** | #41 supported userspace/session boot identity; #34 full screen/state audit; U12 measured performance/remapping/calibration; final U11 packaging/recovery | Final profile/session/installer routes first. Branding cannot hide failure or delay readiness. Per-feature controller, layout and regression checks run throughout, not only here. Physically validate startup/rollback, themes/reduced motion and performance. |
| **14. P7 - final artwork packs and Pack Studio** | #57 generic pack validation and controller install/select/verify/update/remove, then #59 Windows/Linux Qt desktop authoring tool; stabilize the authoring contract against completed handheld consumers; shared validator, mapping, transforms, editable author/source/terms fields, consolidated runtime Credits and export/reimport | Last planned delivery stage, after handheld UI and asset structure settle. Reuse #60 measured profiles and runtime validation; verify exports in TrainerOS and update matching Help/packaging. All Studio acceptance remains. Conditional/deferred research and hardware tasks remain preserved; resolve any effect on the asset contract without inventing support. |

**Next visible increment:** P4 runtime completion for the registered Multiverse collection, followed by static game media and the accepted World/Home presentation. [Schema-11 domain binding](MULTIVERSE_BINDING.md) preserves eligible Pokemon catalogue entries, variants and histories and adds independent per-Trainer choices. Per the owner's 2026-09-23 clarification, include every platform with a credible Flip/Armada runtime route and hide systems without available ROMs; current search/filter must not hide an otherwise populated system. Verify the complete runtime inventory, then missing emulators/BIOS and four server ROMs per supported system with the existing P4 gates. [World chronology](WORLD_CHRONOLOGY.md) is delivered separately; #30/U4 collection curation and all later work remain in the queue. The first [per-Trainer GBA/mGBA ordinary-save route](TRAINER_SAVES.md) is delivered; additional emulator routes require their own proof. New save parsers/editing remain late P8. [Trainer removal and consumer audit](TRAINER_REMOVAL.md) are delivered; retain [profiles and guarded Switch Player](TRAINER_OWNERSHIP.md) and [PIN/startup/family-code recovery](TRAINER_ACCESS.md). Real creation, selection, owner-scoped journals/navigation/history and separate TrainerOS RA account/cache directories are delivered; new Trainers have separate GBA/mGBA saves, while other save routes remain shared. The current consumer audit is recorded; domain identities and independent Multiverse selections are delivered; remaining runtime routes and media stay P4. Early #51 sprite/detail presentation is delivered with partial exact-form coverage; remaining source review and physical acceptance stay recorded in [the sprite boundary](POKEDEX_SPRITES.md). Generic artwork-pack management joins Studio at final row 14, per the owner's 2026-09-23 clarification. The [P1 acceptance matrix](P1_UI_AUDIT.md#acceptance-reconciliation---2026-09-20) preserves physical and future-provider gates. [Two-pane Settings and top-mounted Start controls](MEDIA_SETTINGS_UI.md) expose real volume/brightness sliders, shared theme/motion preferences and compact capability fallbacks; real playback, pack installation, haptics, lighting and parallax retain P5/P7 gates. Center Playroom/Practice/Link Counter now have isolated controller screen rehearsals and production unavailable states ([boundary](CENTER_ACTIVITIES_UI.md)); sprites/motion, battle rules and actual two-device transactions retain their P5/P8 gates. Journey overview, Champion availability/sample-detail routes and clearer RA earned states are delivered ([boundary](JOURNEY_UI.md)); new semantic snapshots, Champion persistence and selected-Adventure RA projection retain their P8/P6 gates. Center Party/Storage and read-only detail are delivered as an isolated presentation with the existing real backup/restore shelf ([boundary](CENTER_PARTY_UI.md)); real individual records and semantic writes remain P8. Worlds/Multiverse and both Home/selector presentations now consume persistent registrations ([binding](MULTIVERSE_BINDING.md)); 133 copied games are registered, with runtime completion and media still P4. Generic on-device creation of a new Multiverse registration remains pending; editing/relinking imported records is available. The current P1 presentation audit is reconciled; new consumers extend its checks, and physical/device capabilities keep their own phase gates. Trainer/account entry, real registration/chooser and owner migration are delivered ([boundary](TRAINER_SETUP_UI.md)); real PIN/startup/family-code recovery and active-profile removal and the first GBA/mGBA save route are delivered; P3 library/domain identity is delivered; catalogue curation remains open. Start quick controls and confirmed Power are delivered; further Settings capabilities follow their own feature gates. Keep the completed [real Flip artwork bootstrap](ARTWORK_BOOTSTRAP.md). Limit early #57 work to runtime loading/validation needed by an actual consumer, preserving exact identities, provenance, explicit missing/fallback rules and bounded derivatives. Install/update still requires staged validation, cancellation and atomic activation. Do not start Studio or freeze its final authoring format now. The 142 unresolved forms remain explicit review work, not invented defaults. #51 sprites remain a separate optional detail/scene provider; production living Party stays late P8. #64 Home and the Journey badge case reuse existing verified observations; later in-game comparison and full semantic Journey/Champion snapshots remain open. All unfinished P1 UI flows below retain their place; no new save research is introduced.

**P1 presentation checklist (reconciled, with explicit remaining gates in [the audit](P1_UI_AUDIT.md)):** (a) shared Start/Settings/Power and quick controls; (b) Trainer registration/chooser/PIN/account screens; (c) Worlds/Multiverse, both Homes and selectors; (d) Dex/Center Party/Storage and Journey/RA; (e) Playroom/Practice/Link Counter screen flows; (f) media/device settings, capability fallbacks and consistent focus/layout states. These are sub-slices of row 2, not a competing queue. Existing working pages are extended, not replaced by fake data. Finish and check each chosen UI flow before opening another. A fixture-complete screen remains marked as such until its functional phase lands.

**Observed follow-up, 2026-09-20:** during the media-settings delivery check, the installed Home reported FireRed's ordinary save could not be verified and withheld badges/Caught. Cause and onset are unverified. Preserve the external save; inspect the existing reader/source boundary before claiming current FireRed progress works. This settings increment changed no save reader or external save. Keep this existing-feature triage separate from new P8 format research and the P1 UI audit.

**Preserved #49 baseline:** native exit protocol, compact question, installed Home transport, durable correctly framed Home/Y pictures, state-independent save resolution and bounded legacy retirement are delivered; see [exit evidence](ADVENTURE_EXIT.md#ordinary-save-decoupling-and-legacy-retirement). The owner's explicit permission to delete old Flip test states does not authorize deleting personal/unclassified saves. Remaining adapter and physical reboot/access checks apply to their own routes, not every UI change.

**Scheduling decisions - owner, 2026-09-19:** all handheld UI first; Pokedex artwork/sprites and real badge presentation early; new save work and heavy format/battle/transport investigations near the end. Small interface contracts, existing readers and protection copies remain available throughout. UI fixtures cannot fake achievements, healing or transfer success. #40 viewer and articles stay P11, with every accepted topic preserved. Sleep stays U13, deferred. If a device/art-source gate is blocked, finish another ready UI slice without bypassing that gate.

## Preservation map — earlier work still has a destination

| Previous register | New execution location; retained obligation |
| --- | --- |
| Steps 1–2, U4 | P0 inventory → P2 ownership → P3 classification/source reconciliation/relink/quarantine; all archive, version, hash and protected-data gates retained. #30 changes reference eligibility explicitly, not file ownership. |
| Steps 3–4, U6 | P1 #49 migration and #9 context → P4 exit media/Worlds/Home and ordinary-backup maintenance; P5 video. State expansion is explicitly superseded, historical provenance/data protected. |
| Step 5, U3/U7 | P0 read/cache evidence → P2 central active-Trainer account → P6 earning/state/notification/matching. No login-as-earning shortcut. |
| Steps 6–7, U9 | P7 runtime #58 -> #60 -> #57 art track/manual history; #59 Studio moves to final row 14; P8 exact reads feed primary #46 Dex/#47 Journey. Manual-first gating is superseded; records and uncertainty remain. |
| Step 8, U2/U8 | P8 DS backup gate immediately before DS save work, plus independent verified readers/protected healing; other formats remain per-provider work. |
| Steps 9–10, U1/U11 | P0 health/durability; P10 reproducible update/Mobile/Steam/software cleanup; P12 final startup/recovery. Manifests, data protection and measurements retained. |
| U5/U10 | P0 existing input/title triage → P9 complete next adapters, with earlier execution allowed for a ready dependency. PokéPark 2/DS pointer gaps remain explicit. |
| U12 | P1 shared controls, per-feature handheld checks, P12 remap/calibration/performance and full visual acceptance. Existing themes/reduced motion are not rebuilt. |
| U13 | Deferred sleep/hinge/wake and sleep-aware duration accounting; no active-phase substitution. |
| Former optional ideas | Retained below; multiple Trainers and verified RA notifications are promoted, not deleted. Richer history/cloud/themes/custom reference/image options keep their own conditions. |

The detailed old register follows with stable headings for existing links. Explicit amendments for #9/#42–64 are marked where old acceptance is superseded; identifiers and unaffected obligations are preserved. The issue reconciliation tables cover **every issue #1–64** and owner follow-ups.

## Detailed new-issue increments

Stable acceptance register from issues #10–18; the phase table above now controls execution. New-batch criteria are in [EXPANSION_PLAN.md](EXPANSION_PLAN.md).

### 1. Audit the personal collection and identity ownership

Issues: [#18 audit](https://github.com/EriArk/TrainerOS/issues/18), remaining [#2 ownership review](https://github.com/EriArk/TrainerOS/issues/2).

**P0 read-only increment: inventory and private dry-run manifest.** Change no ROM, save, database record or catalogue row. Refresh the manifest after P2 ownership and P3 domain/eligibility decisions before authorizing its mutation phase.

- Record path, size, SHA-256, platform, release/language/revision evidence, Adventure/catalogue links, classification, reason and retained equivalent. Classify exact duplicate, proven redundant release, non-English, or needs review.
- Inspect Home/history, legacy ResumePoint migration references, save-backup, RA and media references before proposing relinks. Hash equality is content evidence, not proof of one playthrough/history identity.
- Audit types/storage against the ownership contract: installation-wide configuration versus genuine per-Adventure overrides; composed progress versus writable identity. Change only a boundary actually needed by the next feature.
- Prefer metadata/header/database evidence over filename tags. Preserve ambiguous languages, translations, distinct hacks/revisions and language-neutral titles. No content conversion in this audit.

**Done when:** every candidate has a consistent reason and known replacement where applicable, ambiguities/dependent records are enumerated, and repeated audit leaves source data unchanged. Private paths/hashes/manifests stay outside Git. Fixtures can prepare tooling; actual counts need the collection.

### 2. Relink and quarantine verified cleanup candidates

Issue: [#18 cleanup](https://github.com/EriArk/TrainerOS/issues/18). Requires step 1's consistent manifest and proven identity/relink rules.

- Revalidate source identity before action. Keep a canonical exact copy; relink safely before moving redundant files to same-storage quarantine. Preserve distinct Adventure identities with different histories; never merge histories merely to remove duplicate ROM files.
- Never delete saves, states, screenshots, backups, play history or Hall memories. Save relinking needs adapter-specific proof. Do not delete or invalidate legacy artifacts through ROM cleanup; retire them only through #49's reviewed migration.
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

Issue: [#15](https://github.com/EriArk/TrainerOS/issues/15). Requires #17's media boundary and history/clean exit-image ownership after #49.

Resolve the newest valid #49 clean exit image for the selected Adventure, then the newest eligible clean exit image in the active Trainer/domain context, then the ordinary background. Preserve provenance; no selected-state image priority, state thumbnails or cross-owner/domain fallback. An explicit Adventure choice is not overwritten by image fallback.

Layer the scaled/subdued screenshot **under existing ornaments/patterns and all content/focus**. Preserve the light recessed workspace, theme, badge tray and fixed A/Y controls. Tune opacity/softening on Flip. Reuse screenshot ownership: no parallel database, independent persisted wallpaper choice, uploads or default diagnostic inclusion. Revision changes invalidate cache.

**Done when:** selection/restart/source replacement chooses deterministically; bright/dark/corrupt/missing fixtures remain readable; original decoration stays visible and page switching is smooth. Choose Adventure shares the same exit-media handle and provenance; an image is not a restore point.

### 5. One RetroAchievements account, then verified earning

Issue: [#12](https://github.com/EriArk/TrainerOS/issues/12). Depends on the current RA provider and shared settings/keyboard, not artwork. **#20 amendment:** one account means one canonical account per active Trainer across features, not one shared credential for all people; P2 owner migration precedes this account surface. Management is P2, earning P6.

Deliver two increments:

1. Global service owns the active Trainer's canonical account identity/token, login/logout/validation and mode; storage/cache/response identities include the Trainer owner. Add **Settings → Accounts → RetroAchievements**; Hall shows status and opens that same surface. Migrate the owner-only account file without unnecessary login; preserve malformed originals. All consumers use one source. Reject previous-account responses and clear secret drafts on submission/cancel/page change/failure/exit.
2. One supported emulator gets configure/verify/clear-managed-account integration. Separate history reading from earning and show unavailable/mismatched-account states. Make Standard/Hardcore explicit before launch and never silently change mode. #49 removes normal state resume in both modes; its exit flow must not undermine verified earning rules.

**Done when:** migration, failed login, sign-out, A/B cache isolation and controller flows pass; a real new achievement is earned on Flip under the same account and read back in Hall. That final gate needs suitable gameplay; login/cache alone is not closure. Local Hall memories remain intact. No plaintext password persistence or credentials in QML/logs.

### 6. Pokédex artwork packs

Updated #13 and #57–61 intentionally replace the earlier generic-downloader-first proposal. Keep a separate species/form `PokedexArtworkProvider`; classic illustrations are primary list art and #51 sprites serve detail/living scenes.

Execute **#58 raw corpus -> #60 real Flip bootstrap and canonical profiles -> #57 required versioned runtime contract/shared validation**. Controller Settings does not wait for Studio. Owner clarification, 2026-09-20: **#59 Qt desktop Pack Studio is the final delivery stage**, after UI and asset structure stabilize. Finalize its authoring format there. Full [artwork acceptance](EXPANSION_42_62.md#artwork-sequence) retains offline use, partial packs, exact-form fallback, malformed/oversized-source protection, cancellation/atomic activation, authorship and original-file safety. No resident downloader or invented profiles.

**Done when:** all stage artifacts and physical list/detail/picker profiles are verified; Studio consumes those same targets, exported packs reimport, controller install/update/verify/remove and credits work offline with honest coverage. Raw copyrighted art/private manifests stay outside Git. A blocked Flip gate cannot be bypassed by inventing profiles.

### 7. Caught as a personal collection

Issue: [#14](https://github.com/EriArk/TrainerOS/issues/14). V1 depends on the persistent journal. #46 now makes verified current-save Seen/Caught the primary progression view; this manual collection remains a separately sourced secondary view, not a prerequisite blocking save-derived browsing. #13 enriches it; missing art is never a blocker.

Add reachable local **All / Caught / Favorites / Journal** navigation with page-specific controls, preserving L1/R1, filters/detail focus and existing records. V1 projects manual **species-wide** Caught marks into one row per marked species with name/number/types, manual source, notes and unknown origin/date.

Today's journal does not record the caught form. Do not duplicate a species mark over all forms or invent a form/individual. Form-specific assertions require an explicit later model/editor/provider. Reuse reference/detail data; clearing a manual mark changes only that assertion. Seen-only/unknown are not Caught. Offer provenance filters only when supported; unknown metadata stays visible/selectable.

Before automatic ingestion, document a small individual-record boundary: stable record/species/form IDs, Adventure/save lineage where proven, provider/artifact revision, observed-at distinct from real catch time, optional nickname/shiny/gender/level. Do not build unused infrastructure for every future field.

**V1 done when:** manual changes appear immediately and survive offline restart; empty/missing-art/large-list/controller flows work. **Enrichment remains open:** repeated reads and rollback/re-import must not duplicate or erase history; a caught total cannot become fictional individuals. Home/Trainer summaries are later derived views; Recently Caught needs trustworthy time/source labeling.

### 8. Verified party reading before paid healing

Issue: [#10](https://github.com/EriArk/TrainerOS/issues/10). Requires exact-build save resolution, protection backups and a **new independently verified party/money reader and writer**. The shipped badge reader is not a healing capability.

Start with one verified Gen III title/build. First prove immutable party condition, allowed HP/PP/status fields and currency representation. Define a deterministic fee outside QML; show exact work, fee and before/after balance in **Pokédex ⇄ Pokémon Center → Services → Heal party** (planned #43/#44 route; Start is the legacy backup entry). This is a TrainerOS service, not canonical in-game Center behavior.

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

The melonDS resolver and fixtures exist, but the module explicitly leaves real profile activation/save-path confirmation pending. Inspect the actual melonDS configuration, opt in only when supported, and prove the cartridge's ordinary save is the file selected by the service. Create a copy, restore with a verified protection copy, undo, and independently check bytes and in-game load; include externally running-emulator refusal and refreshed ordinary-save context/projections. Preserve the original save throughout. This closes a partly implemented ordinary-backup module. The selected-moment-reset check is historical and replaced by #49 migration plus refreshed ordinary-save context/projections; no DS state-resume work remains planned.

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

**Explicitly amended by #49/#9:** the ID/heading remains for historical links; preserving/expanding exact-state resume is superseded, not an open delivery gate. [GBA validation](RETROARCH_RESUME.md) and [old provenance](RESUME_POINTS.md) remain evidence of legacy code only.

P1 now delivers normal-launch/save-confirmed exit, clean capture, legacy dependency removal and safe retirement of demonstrably TrainerOS-owned state artifacts. Preserve ordinary saves, independent screenshots, all valid play history and recovery; distinguish metadata/cache deletion from external files. ROM/software cleanup cannot perform this migration implicitly. New core/adapter state providers are cancelled as target work, while ordinary-save backup/resolver expansion remains U8/P9.

P4 retains useful controller maintenance for growing ordinary-backup shelves: inspect identity/size, detect missing/stale references, explicitly archive/remove managed copies with recovery and handle full shelves/failed-operation leftovers. No blind directory pruning. Updated #9 shared Y/context is active planned work in P1/P2/P4, not deferred; Worlds/modal exceptions and independent Trainer/domain selections are mandatory. Choosing never launches; games without state artifacts are not hidden for that reason.

### U7. Broader RetroAchievements content matching — open capability work

Source: [verified matching](RETROACHIEVEMENTS.md#verified-matching-boundary), former milestone 7.

Current whole-file cartridge matching covers GB/GBC/GBA/Pokémon Mini; DS, disc/container and other schemes are not equivalent MD5 cases. After account ownership is unified, choose the next desired format that RA actually supports, verify its authoritative hash rules and test recognized/unrecognized/changed content before publishing a link. Already-patched hacks and soft patches need explicit treatment; titles or artwork are not match evidence. Keep unknown dates/modes honest and cache identities isolated. Unsupported platforms remain unavailable without blocking local Hall history.

### U8. Expand ordinary-save access and progress readers — open per provider

Source: [save services](SAVE_BACKUPS.md), [game progress](GAME_PROGRESS.md), former milestones 9/12/13.

For DS, finish U2 before building on that resolver; independent Gen III reading need not wait on a blocked DS device check. Expand protected ordinary-save access one format at a time, including other desired RetroArch cores (with RTC distinctions where relevant) and Dolphin memory-card/per-game formats only after independent verification. No catch-all copy of guessed save directories.

The first automatic reader supports only exact English FireRed/Emerald builds. Continue the read-only Gen III module with verified party/HP/PP/status and currency before step 8 healing; expose supported party data on Home rather than making it useful only inside a paid service. Add location or other desired progress fields only where their meaning is established. Diamond/DS and Colosseum/GameCube reading are separate next providers after reliable save resolution and format proof; an imported save is not evidence that its parser exists. ROM hacks need exact tested profiles and retain unknown fields otherwise. Validate zero/unknown, save rollback/replacement, checksum failure and refresh after return against in-game values, without modifying source bytes.

### U9. Connect proven progress to personal views — open after source prerequisites

Source: [Trainer overview](TRAINER_OVERVIEW.md), [Pokédex journal](POKEDEX.md), [Hall archive](HALL_OF_FAME.md), [ownership contract](DATA_MODEL.md#ownership-contract--2026-09-13), former milestones 7/8/13.

**#46 amendment:** current-save Dex can follow an exact verified reader independently of step 7; retain manual Caught/journal as secondary history. Import only actual species/form observations supplied by U8's verified reader, with source/save-lineage/revision and rollback/dedup semantics. Current badge/caught totals are insufficient to identify an individual Pokémon or its catch event. Derived Home/Trainer/World summaries must distinguish manual collection, current-save progress, actual observed visits/playtime and explicit completion memories; represented library Worlds are not visited regions.

Recently observed/caught activity, teams, game-save playtime and Hall-of-Fame suggestions can follow when the source proves their fields. Keep observation time distinct from catch/victory time; a suggestion does not automatically complete an Adventure or award an RA achievement. Reuse reference/artwork and existing journal/archive editing, preserving manual records through import, rollback and restart. Richer individual/shiny timelines remain conditional on provenance rather than a prerequisite for useful manual views.

### U10. Additional Adventure adapters — open, one complete runtime path at a time

Source: [standalone adapters](STANDALONE_ADAPTERS.md), [catalogue runtime limits](COLLECTION_CATALOGUE.md), former milestone 12.

After current launch gaps, the default next adapter is the installed Azahar/3DS path, then Cemu/Wii U, subject to actual supported titles and runtime checks. Verify installed version, launch arguments, content requirements, physical controls, display, process exit/recovery and return before enabling any capability. Switch, PC/Pico and Android/Waydroid catalogue entries need an independently viable runtime/input route; do not enable them merely because the reference includes them. Do not initialize every runtime or build every adapter at once. For each supported integration, ordinary launch can ship before separately proven ordinary-save backup, exit capture/policy, achievements or progress.

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

**Owner clarification, 2026-09-23:** prepare the actual server-disk copies now:
approximately four games per system, with more for small titles. The initial
[collection preparation](MULTIVERSE_COLLECTION.md) covers 133 games across 22
systems, preserving source originals and existing Pokemon content. This advances
file preparation only; P3/P4 domain binding, missing viable emulator setup and
launch/input/save/return acceptance remain next. Small systems may receive 8–12
titles; source shortages remain explicit rather than invented entries. All earlier
collection, runtime and save-protection acceptance below remains in force.

The owner's **2026-09-13 follow-up** authorizes freely using the server-attached ROM disk for development tests and requests a **small selection of iconic games for each system**. Its original later scheduling is superseded for file preparation by the 2026-09-23 request above; the selection does not replace the complete eligible Pokémon collection. Test-content permission is recorded in [AGENTS.md](../AGENTS.md#authorized-rom-source-for-testing--2026-09-13).

**Owner clarification, 2026-09-20:** when P4 binds the real Multiverse library, deliver the playable setup in the same increment: inventory installed runtimes, install and configure all missing emulators with viable ArmadaOS/Flip routes, copy their required BIOS/firmware from the server ROM disk, and import **four ROMs per supported system** from that same disk. Prefer a small iconic selection. This supersedes the earlier 3–5-title target and postponement of the initial collection until P9; the current P1 presentation-only slice does not trigger bulk installation or copying. P9 retains unresolved runtime gaps and wider adapter coverage.

Review each target system explicitly; unavailable files or unsupported runtimes remain recorded gaps, not silently omitted systems or fake enabled cards. Preserve source originals, keep meaningful editions distinct and verify copied content. Curate readable titles and appropriate media; Pokémon titles retain their World relationships, while general classics go to Multiverse. Keep emulator-compatible per-system ROM directories and runtime-specific BIOS placement behind the platform adapters; TrainerOS references the same files instead of creating a second library copy. Record source, destination, verification and any missing BIOS privately, outside Git.

**Done when:** each supported target system has a small coherent selection of four titles, with required emulator/BIOS configuration, verified file identity, working launch/controller input, save creation where applicable and clean return on Flip; a private manifest records the selected editions and unresolved system/content gaps. Test saves stay separate from personal playthroughs. This task neither replaces U4's archive-completeness work nor authorizes committing ROMs or private media.

## Optional later ideas — retained, not current delivery requirements

- **Promoted, not removed:** multiple Trainer profiles are now P2 / #19–20; the concrete verified RA notification/jingle subset is P6 / #25. Other restrained notification ideas remain optional.
- **Retained personal-history ideas:** richer individual/shiny timelines, teams, local milestones and observation-derived activity; only trustworthy fields can be shown. U8/U9 remain their source/provenance gates.
- **Retained optional services/content:** cloud sync, additional theme/World-atmosphere packs, reference-provider alternatives/custom species. None blocks current offline delivery or fabricates official data.
- **Retained platform option:** a reproducible custom/preconfigured Armada image only after mature package/session installation proves insufficient. #41 first uses supported stock-Armada layers; earlier boot ownership is a possible later benefit, not a new fork requirement. Hardware modification remains out of scope.
- **Promoted from deferral:** updated #9 shared Y/context is now P1/P2/P4. **Still deferred:** U13 sleep/hinge/wake and optional page wrapping decided during U12 physical UX testing. #49 cancels state-resume expansion rather than postponing it.
- **New conditional extensions preserved:** live in-game RA overlay only after an actual safe event/presentation path; Home video only after separate UX/performance acceptance; additional per-Trainer save namespaces adapter by adapter after the first P2 RetroArch route; simple context audio moods/optional boot sound; later controller color picker, reactive RGB or measured battery-saver policy only as separately chosen features. These are not prerequisites for the initial issues.

## Issue reconciliation

This review changes the plan, not GitHub issue states. Close/split only after checking remaining acceptance against delivered evidence.

| Issue | Treatment |
| --- | --- |
| [#1](https://github.com/EriArk/TrainerOS/issues/1) native mock | Foundation implemented; reconcile old acceptance, do not rebuild it. |
| [#2](https://github.com/EriArk/TrainerOS/issues/2) ownership | Contract clarified in DATA_MODEL; code/storage audit remains step 1. No integration-profile migration claimed. |
| [#3](https://github.com/EriArk/TrainerOS/issues/3) early device smoke | Dated hardware evidence exists; performance/wake limits remain explicit. |
| [#4](https://github.com/EriArk/TrainerOS/issues/4) extensible Worlds | Stable IDs/custom/multiple Worlds implemented; preserve through cleanup. |
| [#5](https://github.com/EriArk/TrainerOS/issues/5) lifecycle | Checkpoint/process/recovery boundary exists; new adapters still need proof. |
| [#6](https://github.com/EriArk/TrainerOS/issues/6) ResumePoint | Closed/superseded by #49; retain historical provenance/device evidence, migrate legacy data safely, no expansion. |
| [#7](https://github.com/EriArk/TrainerOS/issues/7) GitHub CI | Superseded by no-Actions instruction; native/Linux/ARM verification and commit/push stay required. |
| [#8](https://github.com/EriArk/TrainerOS/issues/8) original tracker | Historical foundation tracker; old Phase D state work superseded by #49; this roadmap is current. |
| [#9](https://github.com/EriArk/TrainerOS/issues/9) global Continue | P1/P2/P4 shared context/Y, Worlds/modal exceptions, selection without launch; updated issue resolves the earlier deferral. |
| [#10](https://github.com/EriArk/TrainerOS/issues/10) | P8 / step 8; verified party/money, guarded healing, protection and in-game validation. |
| [#11](https://github.com/EriArk/TrainerOS/issues/11) | P10 / step 9; Mobile maintenance trial and reversible Steam removal/restore. |
| [#12](https://github.com/EriArk/TrainerOS/issues/12) | P2 management + P6 earning / step 5; canonical active-Trainer account, not one shared credential. |
| [#13](https://github.com/EriArk/TrainerOS/issues/13) | P7 / step 6; classic illustration packs with #61 ordered bootstrap/device/contract/Studio/Settings gates. |
| [#14](https://github.com/EriArk/TrainerOS/issues/14) | P7 / step 7 + P8/U9; manual species-wide collection preserved as secondary; #46 current-save primary follows exact reads independently. |
| [#15](https://github.com/EriArk/TrainerOS/issues/15) | P4 / step 4; shared-media Home background now owner/context scoped. |
| [#16](https://github.com/EriArk/TrainerOS/issues/16) | P4 / step 3; recognizable Worlds game media. |
| [#17](https://github.com/EriArk/TrainerOS/issues/17) | P4 / step 3; semantic media identity/provider before consumers; P5 video extension. |
| [#18](https://github.com/EriArk/TrainerOS/issues/18) | P0 audit + P3 steps 1–2; source reconciliation, relink/quarantine and measured cleanup. |
| [#19](https://github.com/EriArk/TrainerOS/issues/19) | P1 first-run UI, P2 real controller registration after owner migration; shared creation transaction. |
| [#20](https://github.com/EriArk/TrainerOS/issues/20) | P2; multi-Trainer migration, chooser/PIN, scoped history/accounts/save lineage. |
| [#21](https://github.com/EriArk/TrainerOS/issues/21) | P1 delivered 2026-09-13; shared tab/panel/content geometry, verified and installed on Flip. |
| [#22](https://github.com/EriArk/TrainerOS/issues/22) | P1; quiet healthy status, preserved useful faults and diagnostics. |
| [#23](https://github.com/EriArk/TrainerOS/issues/23) | P1; shared cached pattern; later screenshots remain beneath it. |
| [#24](https://github.com/EriArk/TrainerOS/issues/24) | P6, or earlier after ownership/read proof; distinguish earned/locked/unknown states. |
| [#25](https://github.com/EriArk/TrainerOS/issues/25) | P6; verified deduplicated return-time unlock banner/jingle; live overlay conditional. |
| [#26](https://github.com/EriArk/TrainerOS/issues/26) | P1 Start Power menu delivered; P2 Switch Player remains unavailable. Durable gates and fresh confirmation preserved. |
| [#27](https://github.com/EriArk/TrainerOS/issues/27) | P1 shared physical chrome; new service consumers follow the same components. |
| [#28](https://github.com/EriArk/TrainerOS/issues/28) | P3 domain foundation → P4 complete Multiverse system/game browser within Worlds. |
| [#29](https://github.com/EriArk/TrainerOS/issues/29) | P5; explicit local ScreenScraper video, idle delay and one bounded player. |
| [#30](https://github.com/EriArk/TrainerOS/issues/30) | P3; explicit curated-eligibility change, preserved personal records and eligible checklist. |
| [#31](https://github.com/EriArk/TrainerOS/issues/31) | P4; Home X contexts, independent selections/scoped Continue and shared history. |
| [#32](https://github.com/EriArk/TrainerOS/issues/32) | P4; distinct Multiverse Home composition, honest general metrics, common controls. |
| [#33](https://github.com/EriArk/TrainerOS/issues/33) | P1 compact shared headings; explicit Home/onboarding/banner exceptions. |
| [#34](https://github.com/EriArk/TrainerOS/issues/34) | Every delivered screen, then P12 complete physical content/state audit. |
| [#35](https://github.com/EriArk/TrainerOS/issues/35) | P1 Start volume/brightness delivered, coalesced input and external refresh through actual device services; dedicated-session physical volume keys added. |
| [#36](https://github.com/EriArk/TrainerOS/issues/36) | P5 selectable licensed music/UI audio, lifecycle and jingle arbitration. |
| [#37](https://github.com/EriArk/TrainerOS/issues/37) | P5 targeted support probe -> optional debounced charger haptics; no sleep resumption. |
| [#38](https://github.com/EriArk/TrainerOS/issues/38) | P5 targeted support probe -> Armada capability-driven RGB; Flip support unproven. |
| [#39](https://github.com/EriArk/TrainerOS/issues/39) | P10 Settings front end to #11 helper; optional Steam restore/install, no second installer. |
| [#40](https://github.com/EriArk/TrainerOS/issues/40) | P11 viewer + product articles + complete offline/runtime-fact audit; engineering docs stay current throughout development. |
| [#41](https://github.com/EriArk/TrainerOS/issues/41) | P12 supported-layer startup branding with profile routing, update safety and recovery. |
| [#42](https://github.com/EriArk/TrainerOS/issues/42) | P1 minimal UI/source contracts -> late P8 evidenced reads then independent guarded writes; #50 research. |
| [#43](https://github.com/EriArk/TrainerOS/issues/43) | P1 shared L2/R2 routing → each P4/P6/P7/P8 companion consumer; keep five primaries. |
| [#44](https://github.com/EriArk/TrainerOS/issues/44) | P1 complete Center UI; late P8 real consumers after context/exact reads; practical Party/Storage then services/writers. |
| [#45](https://github.com/EriArk/TrainerOS/issues/45) | P1 Link Counter UI/fixtures, late P8 functional Link Counter; exact-pair writers, durable two-device transaction/recovery before real exchange. |
| [#46](https://github.com/EriArk/TrainerOS/issues/46) | P7/P8 selected-save primary Dex; source-aware manual history retained separately. |
| [#47](https://github.com/EriArk/TrainerOS/issues/47) | P1 Journey UI and early P7/#64 exact badge art; late P8 real Journey and preserved verified Champion snapshots. |
| [#48](https://github.com/EriArk/TrainerOS/issues/48) | P6 shared-Adventure RA companion; external truth, active-Trainer account and verified matching. |
| [#49](https://github.com/EriArk/TrainerOS/issues/49) | P1 screenshot-first save-confirmed exit and safe legacy-state migration; supersedes #6/U6 state expansion. |
| [#50](https://github.com/EriArk/TrainerOS/issues/50) | Alongside each P8 provider: private exact-ROM ready/fresh/intermediate save research and controlled diffs. |
| [#51](https://github.com/EriArk/TrainerOS/issues/51) | Early P7 optional sprite/portrait assets/provider and per-asset credits; early detail rendering; late P8 actual-Party consumers, not primary Dex list. |
| [#52](https://github.com/EriArk/TrainerOS/issues/52) | P5 bounded animation primitive → P8 actual-Party Home/Playroom; hidden pause and device performance. |
| [#53](https://github.com/EriArk/TrainerOS/issues/53) | P1 practical Party UI, late P8 real management; separate Playroom entry, verified write capabilities only. |
| [#54](https://github.com/EriArk/TrainerOS/issues/54) | P1 Playroom UI/fixtures, P8 real Party after #51/#52/exact reads; controller interaction/portraits, no save mutation. |
| [#55](https://github.com/EriArk/TrainerOS/issues/55) | P1 practice UI/fixtures; late P8 read-only 1v1 after Party/scene boundaries and ARM64 exact-rules engine spike; no save rewards. |
| [#56](https://github.com/EriArk/TrainerOS/issues/56) | P4 large environmental World cards; intentional semantic diagonal pairs and strong golden focus. |
| [#57](https://github.com/EriArk/TrainerOS/issues/57) | P7 required runtime contract/shared validator after #60; Settings does not wait for Studio. Final authoring-format stabilization belongs to the last #59 stage. |
| [#58](https://github.com/EriArk/TrainerOS/issues/58) | First P7 art stage: [whole configured-source PC pass delivered](CLASSIC_ART_CORPUS.md#completed-run-and-handoff), ready for #60. 2,068 image records, candidates for all 1,025 species, 1,437 exact-form associations and 142 explicitly unresolved forms. All 256 planned candidate pages have outcomes and all 192 acquisitions were visually reviewed; retain unknown identity/style/attribution rather than claiming universal coverage. |
| [#59](https://github.com/EriArk/TrainerOS/issues/59) | P7 Qt desktop Pack Studio in final execution row 14 after UI/asset structure stabilization and #60/#57; shared maps/profiles/validator, no invented target dimensions. |
| [#60](https://github.com/EriArk/TrainerOS/issues/60) | Second P7 art stage: real Flip importer, canonical species/form map and measured slot profiles. |
| [#61](https://github.com/EriArk/TrainerOS/issues/61) | P7 runtime prerequisites #58 -> #60 -> #57; early art remains independent from saves. Owner amendment 2026-09-20 moves #59 to final row 14; Settings does not wait for Studio. |
| [#62](https://github.com/EriArk/TrainerOS/issues/62) | P0 documentation reconciliation delivered in this increment; not feature implementation or issue closure. |
| [#63](https://github.com/EriArk/TrainerOS/issues/63) | P1 list fixtures, P3 data-driven platform/release chronology with stable search/filter/focus; no save dependency. |
| [#64](https://github.com/EriArk/TrainerOS/issues/64) | Early P1/P7 accurate badge assets, explicit exact-set mapping, neutral fallback and Credits; existing verified providers first, new formats late P8. |
| Owner UI-first follow-up, 2026-09-19 | Early whole-handheld UI, Pokedex artwork/sprites and badges; new save integration/heavy research near the end. Full Help remains P11. |
| Owner follow-ups, 2026-09-13 | P10 / steps 9–10: Mobile-only maintenance if viable; careful software/background cleanup with measured results. Preserve all old work and future ideas through this replan. No new GitHub issue is implied. |
| Owner test-content and classics follow-up, 2026-09-13 | Standing permission to use the server's ROM disk for tests; updated 2026-09-20: missing viable emulators, required BIOS and four server-disk ROMs per supported system accompany real Multiverse binding in P4; P9 retains unresolved gaps. Source files and personal saves remain protected. |

## Historical milestone map

Older module notes use milestones **0–14** from the [previous plan](https://github.com/EriArk/TrainerOS/blob/7c7fd786f1eab44427d0abd5f709c1c4f95012b6/docs/ROADMAP.md), not the new queue numbers. Checkpoints remain in Git history and module evidence documents.

| Former milestone | Current location / status |
| --- | --- |
| 0–3: skeleton/controller/mock/backend | Working baseline; extend only what the active feature needs. |
| 4: device baseline | U1/U12 remaining evidence/performance; U13 deferred wake. Later checks supersede early “unverified” notes. |
| 5–6: Worlds/Home/Continue | U4–U6 plus steps 1–4; identity, archive completeness, normal launch, #49 legacy migration and ordinary-backup maintenance remain explicit. |
| 7: Trainer/Hall | U3/U7/U9 plus step 5; production reads, matching/earning and derived personal views. |
| 8–9: Pokédex/Center | U2/U6/U8/U9 plus steps 6–8; finish DS backup, add verified formats and source-aware collection/services. |
| 10–11: dedicated/default session | U1/U11 plus step 9; preserve tested session and finish installation/update recovery. U13 wake is deferred. |
| 12–13: adapters/progress | U5/U7–U10; concrete per-title launch, content matching, save readers and feature projections. |
| 14: packaging/polish | U11/U12; themes/reduced motion and initial ARM build already delivered. U13 remains explicitly deferred. |

## Delivery gate for each increment

Review actual changes and acceptance; run affected native checks, broader tests for shared input/storage/lifecycle, and rendered SDL controller scenarios for QML. Inspect 960×540/1920×1080 and Flip layouts. Preserve L1/R1, A/Y selection-versus-launch, planned Home X context switching, Back/modal priority; cover offline/unknown/empty/corrupt states. Fixtures are not device proof. Update engineering docs and capability facts with each delivered feature; update the matching product Help article once P11 exists; complete the #34 screen/state matrix as screens land.

Keep credentials, private reports/manifests, ROMs, BIOS, saves and unlicensed artwork out of Git. Documentation-only planning needs content/link/diff checks, not an unrelated rebuild. Stage reviewed paths, commit, fetch/integrate remote work, push normally and verify remote SHA/final status. Do not run/poll GitHub Actions. See [workflow](DEVELOPMENT_WORKFLOW.md).

### Settings refinement - owner follow-up, 2026-09-20

The current P1 slice embeds Trainer/profile editing and System readings/actions
in the Android-style category/detail layout. Controller moves from Start into
Settings; Start retains only volume/brightness quick controls, with theme in
Appearance. X/Y join A/B in the complete Switch shell layout. This does not
advance P2 ownership, PIN enforcement, save research or device capabilities;
all remaining roadmap commitments and the P1 audit keep their order.

### Delivery clarification - 2026-09-23

Implement real features rather than replacing them with explanatory labels.
The removed proposal for a new shared-save label does not ship. Per-Trainer
ordinary-save routing moves into the next P2 increment for the first supported
RetroArch route; all unproven adapters and semantic save research keep their gates.
