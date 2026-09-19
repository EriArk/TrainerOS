# TrainerOS expansion acceptance — issues 19–41

**2026-09-19 reconciliation:** this register remains active for unchanged #19–41 acceptance. Updated #9/#28/#31 and [#42–62](EXPANSION_42_62.md) explicitly amend navigation, save lifecycle, current progression and artwork ordering below. Planned features are not delivered by this docs change.

Reviewed in full on **2026-09-13**, then integrated with all earlier commitments in [ROADMAP.md](ROADMAP.md). This is the detailed acceptance register for the new batch, **not a second execution queue**. Roadmap phases P0–P12 choose the order; existing steps 1–10 and U1–U13 retain their identities and acceptance. Planning alone does not certify implementation; dated delivery notes below link actual verification.

The important changes are personal-data ownership, two library contexts and curated catalogue eligibility. Shared visual improvements can ship before those migrations; expensive media, save writes and platform customization retain independent gates. Each slice includes the relevant Help article as that viewer becomes available.

## Shared chassis and headings

Issues: [#21 geometry](https://github.com/EriArk/TrainerOS/issues/21), [#22 status](https://github.com/EriArk/TrainerOS/issues/22), [#23 pattern](https://github.com/EriArk/TrainerOS/issues/23), [#27 physical chrome](https://github.com/EriArk/TrainerOS/issues/27), [#33 headings](https://github.com/EriArk/TrainerOS/issues/33). Phase P1; later features reuse the result.

**Delivered 2026-09-13:** shared #21 geometry plus the owner's follow-up lip height; #22 quiet status, #23 cached ornament and #33 compact headings are implemented. The combined visual pass also covers existing Start/services/confirmations from #27 and existing feature content from #34. [Visual review and validation boundary](VISUAL_REVIEW.md). Future Power, Multiverse, onboarding and media surfaces must reuse these primitives and still receive their own #27/#34 review.

1. Establish one content rectangle: the panel rises to the start of the inactive tabs' bevels (owner follow-up, 2026-09-13). Their tips overhang slightly; the unchanged active tab overlaps further. The enlarged title grows from a continuous upper/side/lower body around one beveled aperture, shared by pages and services; no footer seam splits the sidewalls. Centralize insets, title baselines, tab height and content bounds instead of per-page Y corrections.
2. Share a compact heading family: standard, contextual and service variants, with optional eyebrow/subtitle/trailing actions, consistent elision and no dead gap when optional text is absent. Home, onboarding, notifications and compact confirmations have deliberate compositions. Reclaimed height serves larger World cards and the four-visible-game-row target, not smaller type.
3. Reuse one subtle, theme-aware, cached background pattern across all five pages and service screens. Layer base → context-appropriate screenshot → ornament → content. Avoid per-page Canvas copies, per-frame texture work or texture that competes with focus.
4. Remove healthy-state prose such as controller-connected/on-battery labels. Keep compact battery/charging indicators and actionable faults; unknown is not zero. Preserve diagnostics, accessibility labels and source/account/save-error information.
5. Polish the shared shell, Start, Settings, confirmations and their mounted controls: molded edges, recessed wells, raised button caps, contact shadows, pressed/disabled/warning states. Restrained wear remains optional under the existing design language; this is not a new photorealistic distress pass. Feature-content composition is #34, not another chassis redesign.

**Done when:** shared geometry works across all pages, themes, reduced motion and 960×540 / 1920×1080 scaling; long names/optional fields do not collide; rapid SDL controller navigation preserves focus and input semantics; physical Flip inspection confirms legibility and no decorative frame-pacing regression. Later pages must use these primitives rather than reopen five independent layouts.

## Trainer ownership, onboarding and Power

Issues: [#19 registration](https://github.com/EriArk/TrainerOS/issues/19), [#20 multiple Trainers and PIN](https://github.com/EriArk/TrainerOS/issues/20), [#26 Power menu](https://github.com/EriArk/TrainerOS/issues/26). P2; Power off/restart presentation can ship in P1 using the existing device service.

**Foundation first:** separate device-wide library/installations/shared media from Trainer-owned journal, favorites, Hall, play history, Home/navigation and external account identity/cache. One global RA service serves the **active Trainer's** account; #12 never means one credential shared among all Trainers. Preserve the existing Trainer ID and migrate all personal records into that owner without copying the library per account. Check interrupted migration, reopening and rollback; a pre-migration database backup must remain usable with its matching release.

Do not attach another Trainer's progress to a shared external save. Document the existing save lineage as shared/legacy where appropriate. Per-Trainer save namespaces require one verified adapter at a time and protected migration; no blind save renaming/copying. Capture the initiating Trainer on play sessions and asynchronous writes. Refuse switching while an Adventure/external save writer is active or safety-critical work cannot drain. Late responses must not mutate the next Trainer's views.

Then deliver end-to-end profile creation/selection:

- No usable Trainer → controller wizard: welcome, name, emblem, optional favorite, optional PIN once supported, review, atomic creation. Reuse the Add Trainer creation service. No separate firstRunDone flag, fabricated history or half-created profile after interruption; Back preserves the draft until cancelled. Existing profiles bypass registration.
- Multiple Trainers, or a PIN-protected single Trainer → chooser/unlock; one unprotected default Trainer may enter Home. Use original/user-provided identity art. Registration and chooser precede ordinary primary-page access; Start exposes only safe applicable actions there.
- Optional 4–6 digit local PIN uses a compact controller numeric keypad, a reviewed memory-hard salted verifier and bounded retry delay. No plaintext PIN, secret logging or stored input buffer. Owner-authenticated change/remove and documented local recovery must not become an unauthenticated bypass. PIN gates shell access; it does not promise encryption against maintenance/root access.
- Switch Player drains writes, clears drafts/credentials/pending notifications, invalidates old RA/progress requests and loads the new Trainer without reboot. Profile deletion is explicit and preserves shared game/media files and other Trainers' records.
- Replace production Leave TrainerOS with mounted Power → Power off / Restart / Switch Player / Cancel. Hide or explain Switch Player until its service works. Destructive actions need fresh A confirmation, B cancellation and the existing durable exit gate; failure keeps a usable shell. Dev exit remains explicit development behavior.

**Input reconciliation:** #26/#35 wording must not silently overturn global shoulders. In the established post-login shell, L1/R1 cancels transient menus/confirmations and switches primary pages; it never adjusts a slider, submits a destructive action or commits a draft. Repeated/held A cannot cross into a fresh confirmation. Before a Trainer is unlocked, there is no accessible primary-page destination. Any future change to this precedence requires an explicit UX decision and tests, not an incidental menu implementation.

**Done when:** empty/existing/multiple/PIN startup paths, failed writes, interrupted migration, restart, two-Trainer read/write/cache isolation, switching/deletion and all controller cancellation paths pass. Verify on Flip that no game, journal, achievement, selected Adventure or personal aggregate is misattributed. External saves and shared ROMs remain unchanged by account creation/switching.

## Library domains, Multiverse and catalogue curation

Issues: [#28 Multiverse browser](https://github.com/EriArk/TrainerOS/issues/28), [#30 catalogue curation](https://github.com/EriArk/TrainerOS/issues/30). P3 identity/curation, P4 complete browser; depends on #2 ownership and P2's Trainer references before mutations.

**Intentional scope extension:** the primary page is still Worlds. Its Pokémon mode remains region-first; its separate **Multiverse** mode browses non-Pokémon games by system. Multiverse is not a new L1/R1 page, fake Pokémon region or miscellaneous Other Worlds bucket. Genuine Pokémon ROM hacks with unknown geography remain identifiable as fan content; they are not reclassified merely because their region is unknown.

Extend the existing playable-library identity and adapter pipeline minimally: a stable game/variant/content record, explicit Pokémon/Multiverse domain, runtime/media references, optional Pokémon World relations, and a system category for Multiverse. Personal names and classifications are editable metadata, not filename-derived IDs. Resolve legacy/unclassified records through an explicit migration review. Shared library records and Trainer-scoped histories stay separate; no duplicate launch/history framework.

Audit reference titles individually with eligibility and runtime evidence: verified / plausible / theoretical / unavailable / service closed. Normal Pokémon Worlds retain substantial games with a credible Flip/controller path, including eligible spin-offs, remakes, translations, meaningful versions and ROM hacks. Review Card Club, Playroom, Puzzle Corner and Crossover groups by their contents; keep substantial playable titles, remove/demote empty or weak groups. Do not blanket-delete TCG, puzzle games or Pokémon mini. Pokémon GO, closed online clients, promotional/browser fragments, accessory tests, distribution packages and trivial utilities do not qualify merely because a historical list mentions them. Do not install Waydroid/proprietary apps simply to justify filler.

This deliberately narrows the old broad historical checklist. **The full-collection objective remains for eligible games:** missing editions stay grey/linkable, and U4 still reconciles every requested archive and meaningful variant. General/crossover games may move to Multiverse where appropriate. Ineligible owned entries retain stable IDs and a maintenance access route; catalogue pruning never deletes their files, saves, states, media or history. Do not merge histories or confuse #30 reference curation with #18 duplicate-file cleanup. Audit retained/moved/hidden/excluded/needs-review decisions in a private manifest, then verify links and owner/domain aggregates.

Build the controller browser on that model: L2/R2 paired Worlds/Multiverse faces and system/game hierarchy (#43), original hardware cards, sensible system order, persistent per-context focus/search and Back hierarchy. PS2/GameCube/Dreamcast/Xbox are design examples, not proof of installed adapters or Flip compatibility. Show curated titles, optional trustworthy year/creator/genre data and a mounted selected-game panel consuming #17 media; missing art has an original placeholder. Runtime validation still controls launch availability. No general ROM downloading or automatic original-file renaming is implied.

**Done when:** Pokémon and general libraries do not contaminate each other's Worlds/progress; classification/relink migrations preserve shared files and every Trainer's references; eligible missing editions remain attachable; existing region navigation and hacks survive; Multiverse browse/link/launch/return works through a verified adapter, with unsupported systems honest and no extra primary page. #28 remains open until its media-backed browser is complete, not merely after adding a domain field.

## Two Home contexts

Issues: [#31 scoped Home and Continue](https://github.com/EriArk/TrainerOS/issues/31), [#32 Multiverse Home](https://github.com/EriArk/TrainerOS/issues/32). P4, after Trainer/domain persistence and the shared #17 media boundary.

On unobstructed Home, **X toggles Pokémon / Multiverse**, Y opens that context's Continue selector, A on a card selects without launching, and Home's fixed large A button launches normally. Higher-priority panels own input. Persist active context and independent selected game for each context **per Trainer**. Before an explicit choice, use that Trainer's most recent actual launch in the same domain; no title guessing or cross-domain latest-game fallback. Keep both choices when toggling X.

Reuse one observed play history and adapter lifecycle, filtered by owner and library domain. Recorded time, distinct games actually launched and systems actually used are valid general metrics; linked-file totals are not played totals, and unknown completion is not zero/completed. Multiverse games cannot add Pokémon badges, Dex observations or visited Pokémon Worlds. Existing unsupported-progress fallbacks remain.

Multiverse Home gets a deliberate inner composition: game/media identity, general play statistics, a dark cosmic/portal atmosphere where readable, and the same chassis, main action position, status and physical controls. This is a scoped exception to the light Pokémon workspace, not a dark theme imposed on all pages. Avoid a second full system browser or Steam-style launcher grid. No-data/missing-art states offer useful browsing rather than sample personal activity.

Scope #15 background selection to the active Trainer/context and selected Adventure. Its latest-image fallback must never pull an unrelated game from the other domain. Use #49 clean exit-image provenance; no state-thumbnail fallback. A Home-launched game returns to its same Home context; a game launched from Worlds preserves the established originating page/route and its domain. Do not force every exit to Home or overwrite an explicit Home choice just because another game launched.

**Done when:** repeated X/Y/A sequences, empty contexts, two Trainers, restart, normal launch and ordinary-save loading, missing content and launch/return retain separate choices/history/images; toggling uses cached projections without deep I/O; both compositions are readable on Flip. Home autoplay video is optional later, not a dependency of #32. Updated #9 shared Choose Adventure is now planned for Pokémon-aware pages with Worlds/modal exceptions, independent domain selections and no duplicate context capsule; see [shared selection](EXPANSION_42_62.md#shared-adventure-and-paired-navigation).

## Local video previews

Issue: [#29](https://github.com/EriArk/TrainerOS/issues/29). P5, after #17 and the P4 selected-game media panel.

Import from an explicitly supplied **local ScreenScraper media root** using a worker, identity mapping and a reviewable manifest. No scraper login/network API is required for v1; ambiguous filename matches need review. Extend the media result with video identity/revision/availability, keeping original files in place unless managed copying is explicitly chosen.

Selected art appears immediately. After roughly 1.5–3 seconds of stable selection, start a valid preview inside the mounted media well. Reset on every selection change; use at most one active decoder/player, preserve aspect ratio, keep playback focusless and muted by default. Stop/release on page/system changes, modal priority, launch, lost foreground, account/session changes and errors. Never decode offscreen items. Unsupported/corrupt/missing/slow media falls back to static art without blocking controls.

**Done when:** actual Qt/Linux codec support and performance are verified on Flip; rapid selection cannot play stale video or grow player count; navigation/launch latency stays acceptable; static fallback and source removal work. Optional audio must respect the shared audio priorities. No new desktop player controls or initial Home autoplay requirement.

## Achievement state and notification

Issues: [#24 earned-state presentation](https://github.com/EriArk/TrainerOS/issues/24), [#25 unlock banner and jingle](https://github.com/EriArk/TrainerOS/issues/25). P6, using U3/#12 ownership/read acceptance; #25 also requires verified earning/deltas and P5 audio arbitration.

First make locked, Standard-earned, verified Hardcore-earned and unknown visibly distinct in Hall lists/details, using shape/icon/text as well as color. Focus must not look like earned status. Only a complete confirmed account/set snapshot establishes locked counts; preserve known earned results from complete cached snapshots offline, label staleness and never downgrade them because a refresh is partial. Save completion and local Hall memories do not award RA achievements.

Then implement a small shared shell banner presenter with bounded queue, duration, dismissal/acknowledgement and an original short jingle. Use verified unlock events or deltas against the last acknowledged complete snapshot, identified by Trainer/account, game/set, achievement and verified mode. Initial sign-in, cache rebuild or historical import must not trigger an old-achievement avalanche; account switching clears pending old-owner presentation. Restarts, duplicate refreshes and repeated returns must not notify twice.

**First supported delivery is after return to TrainerOS.** Live in-game notification is a separate optional gate requiring a real supported event path and Gamescope-safe presentation that neither steals focus nor pauses/resizes gameplay. Never call a return-time refresh a live event. Queue/condense rapid unlocks, avoid overlapping jingles, respect mute/category settings and reduced motion; audio can be disabled independently of banners. No general desktop notification framework is required.

**Done when:** current/cached/partial/unknown/Standard/Hardcore states are unambiguous on Flip; a genuinely new achievement earned in the configured game/account produces exactly one correctly attributed banner with original/licensed audio; historical sets, retries and other Trainers do not. U7's next hash families remain separate complete capabilities, not title matching.

## Quick device controls

Issue: [#35](https://github.com/EriArk/TrainerOS/issues/35). P1, reusing verified volume/brightness services; #27 supplies mounted control styling.

Start gains compact volume and primary-display brightness controls. Up/down selects, left/right adjusts in bounded useful steps, without pointer dragging or requiring A for every change. Coalesce held input, display the actual platform result, refresh external hardware-key changes and reflect mute. Detailed Settings remains available. Use the known safe brightness minimum, not an arbitrary raw zero; unavailable capability disables honestly. System values remain authoritative rather than a duplicate persisted shell setting. Follow the explicit shoulder/confirmation rule above.

**Done when:** controller min/max, mute, unavailable/rejected requests, external changes, rapid repeat and menu/Back restoration pass; independent Flip audio/backlight readings confirm the controls work. No raw QML system commands.

## Audio personality

Issue: [#36](https://github.com/EriArk/TrainerOS/issues/36). P5; a small semantic audio service precedes #25 jingle playback.

Settings → Audio & Feedback supplies independently enabled music and UI sounds, multiple coherent selectable packs, relative category levels and controller preview. Preferences are **device-wide by default**. Music is quiet optional ambience; UI themes share semantic focus/confirm/back/panel/toggle/warning events instead of per-page filenames. Throttle repeated navigation sounds.

One playback owner handles fade/duck/mute/preview: stop/fade before gameplay, resume once after return, yield to an achievement jingle, stop during shutdown/switching, and never leave preview loops behind. Muted video remains the default; any enabled media audio uses the same priority policy. A simple selected pack is enough; adaptive soundtrack logic is future work.

Bundle only original or properly licensed redistributable assets, with source/author/license/reference/modification metadata and Credits attribution. No ripped audio or close imitation of Pokémon melodies/jingles. Missing/corrupt packs fall back without errors interrupting navigation.

**Done when:** several usable themes, independent volume/mute, preview cancellation, repeat limiting, launch/return and jingle arbitration work on Flip without overlapping loops, clicks or excessive loudness; asset licensing is reviewed. Optional context moods and boot audio remain future extensions, not v1 requirements.

## Haptics and lighting capabilities

Issues: [#37 charger feedback](https://github.com/EriArk/TrainerOS/issues/37), [#38 RGB](https://github.com/EriArk/TrainerOS/issues/38). P0 may inspect support read-only; P5 delivers each supported capability independently.

For haptics, observe a debounced **external-power edge**, not every charging/full/wattage update. Establish startup state without a pulse; suppress duplicate/transient/replayed provider updates and account/boot/shutdown transitions. Offer independent disable and only real supported intensity choices. Tune distinct brief connect/disconnect patterns on Flip. Shell feedback must not reconfigure or steal emulator rumble; gameplay delivery requires separate proof and otherwise remains unavailable while playing. Suspend/resume noise can be covered with fake events without reopening U13 sleep tests.

For lighting, use Armada's existing capability/backend: advertised modes, color, brightness and speed only when supported; current-state readback, bounded apply, reversible preview/cancel and supported platform persistence. Do not repeatedly reassert settings against Armada/Desktop, open UART from QML or build another RGB protocol stack. No reactive per-game RGB or guessed battery-saver override in this increment.

The referenced [armada-packages UART change](https://github.com/armada-os/armada-packages/pull/73) and [Pocket S2 integration](https://github.com/armada-os/armada/pull/419) describe a reusable route, **not Flip 2 support**. Check installed version/tool/service/config and actual hardware; record unavailable capability honestly if needed. Do not install experimental device code merely to make a setting appear complete.

**Done when:** fake backends cover absent/limited capabilities, rejected values and noisy power events; supported controls round-trip on physical Flip without affecting input/rumble. Unsupported hardware remains a recorded conditional gate, with no fake active settings.

**Future motion addition (owner, 2026-09-13; P5):** the shared ornament may later gain gyro-driven parallax after a successful ArmadaOS sensor probe, or calm autonomous movement if no usable gyro exists. [Background-motion acceptance](DESIGN_LANGUAGE.md#future-background-motion--owner-request-2026-09-13) covers stationary controls, reduced motion and physical performance checks. This does not delay the static visual pass or replace another backlog item.

## Optional Steam management

Issue: [#39](https://github.com/EriArk/TrainerOS/issues/39). P10, after U11 rollback foundation and step 9 / #11's reviewed helper and manifest.

Settings → System → Steam Gaming Mode is only a front end to that same fixed-operation platform helper: inspect status, restore/install, verify and remove again. Prefer the valid versioned removal manifest. Fresh installation without one needs a reviewed recipe for the **actual Armada version**; missing/drifted/unsupported state must fail with useful recovery guidance, not guessed packages or shell commands from UI input.

Show real phases and verified completion; expose reboot through #26's Power flow when needed. Preserve Steam libraries, compatdata, saves and unrelated configuration. Keep a maintenance launcher and CLI recovery route even if TrainerOS cannot start. Installing Steam never changes TrainerOS's default status or makes ordinary adapter launch/return depend on Steam.

**Done when:** Settings reuses the physically proven remove → restore → boot Steam → remove-again mechanism, with idempotent verification and interruption recovery. #39 adds UI acceptance to #11; it does not replace #11's stronger recovery/data-protection requirements.

## Complete offline Help

Issue: [#40](https://github.com/EriArk/TrainerOS/issues/40). Per the owner's 2026-09-19 clarification, P11 delivers the viewer, product articles and coverage audit together after the main flows stabilize. Earlier increments keep engineering docs, capability evidence and recovery notes current. All coverage below remains accepted; Help implementation is not a prerequisite for feature development.

Provide Start → Help (and an optional Settings alias), a controller category index/article viewer, predictable bounded scrolling, internal links/Back and useful contextual deep links. A lightweight local structured/Markdown renderer is sufficient; no Chromium or online page dependency. Optional local search may follow. Help is version-matched product content, not a copied developer README or a promise that planned features are installed.

Required coverage:

1. What TrainerOS/Armada is; first Trainer, chooser/PIN and A/X/Y/L1/R1/L2/R2/Start.
2. Pokémon Worlds, linking/missing games, hacks and honest progress support.
3. Multiverse systems, titles, media and scoped Continue.
4. Personal game files: configured library roots, removable storage, verified adapter formats, linking versus moving, cleanup boundaries.
5. BIOS/firmware per genuinely supported runtime: required/optional/not used, **verified** active directory/configuration, known filename/hash only with official or validated evidence, diagnosis and rescan.
6. #49 normal saves/autosaves, capture-before-exit question/cancellation and interrupted outcomes; protected backups/restore, managed artifacts versus originals and Trainer/save lineage. Explain legacy migration only where applicable, never offer normal state resume.
7. Active Trainer's centrally managed RA account, Standard/Hardcore, earned states, notifications, offline cache and why imported saves do not award achievements.
8. Adventure exit media, classic illustration packs/credits/coverage, optional detail sprites and local ScreenScraper media/video; generated cache versus originals. #61 tooling order is developer workflow, not a handheld user requirement.
9. Audio themes, quick controls, supported charger feedback/RGB.
10. Optional Steam install/restore/removal and preserved data.
11. Selected Plasma maintenance, diagnostics, failed launch/start/update recovery and return to TrainerOS.
12. Shared Y/current Adventure and paired navigation; current-save Dex, Journey/Champion, practical Center/Party/Storage/services, separate Playroom and read-only practice, Link Counter when verified. No duplicate Current Adventure capsule.
13. Short actionable troubleshooting paths for launch/input/BIOS/save/Continue/media/account/storage/audio/brightness/RGB/Steam failures, plus asset Credits.

Small semantic runtime slots show current version/device, configured roots, backup location and adapter/capability-specific facts. Missing data renders unavailable; never expose tokens/PINs or guess universal paths. No ROM/BIOS/key download indexes; explain authorized personal files and link official project documentation where useful. Recovery instructions stay bounded and version-aware.

**Done when:** all current-feature articles and context links work offline with controller input; long pages remain readable at both target scales and on Flip; runtime facts match configuration with no secrets; internal links and required topic coverage are checked. A fresh user can follow linking/BIOS/recovery instructions without GitHub. Future adapters/settings must update Help in their own delivery gate.

## Branded startup with recovery

Issue: [#41](https://github.com/EriArk/TrainerOS/issues/41). P12; depends on P2 entry routing, U11 repeatable installation/update/rollback and the supported final session arrangement. Original asset design can be prepared earlier.

Audit the actual chain separately: firmware/vendor → bootloader/initramfs/kernel → Plymouth/equivalent if present → display manager → Gamescope/supervisor → first TrainerOS frame. Do not assume every layer exists or is package-owned. Replace supported userspace/session branding with original TrainerOS assets using a versioned managed manifest, drift checks, idempotent install/remove and rollback. Preserve unavoidable early logos as an explicit limitation; no blind early-boot patch or custom distro dependency.

Use the shared technical/material language, matching backgrounds and a calm lightweight progress pulse or real coarse startup phases. No fictitious hardware checks, fixed cinematic delay or fake progress percentage. End as soon as the shell is usable, routing to onboarding, chooser/unlock or Home as appropriate. Eliminate avoidable cursor/desktop/TTY/unthemed-window flashes in the supported normal path; logs and an explicit diagnostic/recovery path remain available.

**Done when:** physical power/reboot and every entry route show the coherent sequence; startup failure still reaches verified maintenance instead of an endless spinner/black screen; install/update/remove/rollback preserve the supported boot path. Measure splash visibility and first usable frame before/after; no delay after readiness. Optional original boot audio and a future custom-image treatment of earlier stages remain separate later ideas. This issue does not implement a distro fork or resume sleep work.

## Feature-content visual acceptance

Issue: [#34](https://github.com/EriArk/TrainerOS/issues/34). Review each feature when delivered; P12 closes a complete screen/state matrix after shared #21/#27/#33 geometry and the new screens exist.

Audit Pokémon/Multiverse Home, World and system cards/lists/details, Pokédex list/forms/detail/Caught/Favorites/Journal, Trainer reports, Journey/Champion/RA, practical Party/Storage, separate Playroom/practice, Link Counter when delivered, and all service/account/onboarding/Center/media/device/power/Help content. Check title baselines, optical icon alignment, proportions, spacing, density, long names, accessible state distinctions and art/pattern contrast. Inspect normal/focus/pressed/disabled/empty/loading/error/offline/missing states. Target four readable game rows and stronger meaningful media, not arbitrary whitespace or tiny text.

Fix recurring flaws in the shared primitive; remove redundant copy before adding decoration. Preserve data provenance, owner isolation, launch/return/save behavior and all controller decisions. Use private screenshots/notes outside Git; only synthetic/original fixtures may be committed.

**Done when:** every delivered surface has an explicit state review at 960×540 and 1920×1080 plus physical Flip acceptance, readable at normal distance with obvious focus and stable frame pacing. U12's measured startup/idle performance, controller remapping/calibration, themes/reduced motion and optional page-wrap decision remain separate acceptance obligations; visual QA does not close them automatically.
