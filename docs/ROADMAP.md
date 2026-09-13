# TrainerOS Roadmap

TrainerOS now targets a native Qt/QML Linux shell on top of ArmadaOS rather than Android.

The roadmap follows the user's confirmed **bottom-up dependency order**: project skeleton, interface/controller skeleton, shared backend, individual modules, then deeper integrations.

## Development model

The first mock exists to prove feel, navigation, focus, motion, information architecture, and screen relationships. It is **not the final visual design**.

The 2026-09-06 clarification intentionally replaces the former top-down sequence:

1. Establish the native build and module structure.
2. Prove the shared interface and controller skeleton using mock data.
3. Establish the shared domain, repository/service and persistence foundation.
4. Implement individual modules in dependency order, connecting each to the working foundation.
5. Add real integrations one at a time and validate each on the handheld.
6. Add optional automatic data enrichment only when the relevant modules are stable.

The interface skeleton is not a requirement to finish every visual detail before backend work. The backend foundation is not a speculative framework for every possible emulator. Keep both sufficient for the planned modules; then complete those modules end-to-end.

Research findings do not change execution priority. In particular, the save-reading feasibility review does not make Emerald parsing the next implementation task. Only implement capabilities that can be validated; unsupported data remains unknown or is omitted without blocking normal use.

Visual design remains deliberately fluid. Layouts, hierarchy, component styling, animation, density, and whole compositions can be replaced at any milestone. Product invariants matter more than preserving mock visuals.

## Milestone 0 — Native project skeleton

2026-09-06: the native project, shared visual primitives, domain/fake-repository boundaries and mock adapter are implemented. The Windows development build (MSYS2 UCRT64, Qt 6.11.1) passes the native and rendered-QML checks. The [first GitHub Linux CI run](https://github.com/EriArk/TrainerOS/actions/runs/34031993427) also built the project and passed all 15 CTest entries on Ubuntu 24.04, using offscreen/software rendering. Normal Linux/Plasma use and ARM64/ArmadaOS execution remain pending; the milestone's physical-session acceptance gate is not marked complete.

Deliverables:

- C++20 project
- Qt 6 + QML / Qt Quick
- CMake build
- landscape-first full-screen application window
- base theme/tokens
- core package/module boundaries from `ARCHITECTURE.md`
- mock repositories/domain data
- `MockAdventureAdapter`
- Linux development build instructions

Done when a clean checkout builds and launches to Home in a normal Linux/Plasma development session without network access.

## Milestone 1 — Controller shell

Initial implementation: SDL2 semantic input, five routes, per-page focus, transient-layer Back handling, foreground gating, dead zone/repeat and custom focus visuals. Tests use real SDL virtual-controller events and inspect QML's active focus. Physical Flip 2 mapping, device rendering and controller feel remain unverified.

Deliverables:

- real gamepad/D-pad and left-stick navigation
- `A` confirm
- `B` back
- `Start` system menu
- `L1/R1` switch Home, Worlds, Pokédex, Trainer, Hall of Fame
- active page and per-page focus/state in memory; durable restoration follows in the backend milestone
- clear custom focus treatment
- short horizontal page transitions
- centralized input normalization/repeat/dead-zone handling

Done when every primary page is usable without touch/mouse and `L1/R1` remain globally reserved.

## Milestone 2 — Full mock experience

Implemented shared-shell examples: sample Home, three-card Continue drawer, nine region buttons, and system/service placeholders. The second increment adds the shared controller keyboard and Trainer create/edit interaction through an in-memory repository: name, original emblem, limited sample favorite choices, validation, Save/Cancel and retryable failed writes.

The third increment adds region → Adventure list → detail navigation through `WorldsController`. Hoenn contains originals, a remake and a fictional ROM-hack record. The bounded list follows controller focus, retains selection across Back/page changes, and exposes empty-region recovery. Per-Adventure mock capabilities drive launch/resume/setup states; simulated launch failures allow retry and unknown progress stays distinct from zero. Five Windows test suites pass, including two rendered-QML scenarios driven by SDL virtual controllers. Physical-device and Linux execution remain unverified.

The fourth increment adds the Pokédex mock: a 14-entry reference fixture, bounded list/detail, combined collection/type/record filters, exact-number/name search through the shared controller keyboard, sorting and editable in-memory favorites. Separate reference/progress boundaries cover retryable load/write errors, retaining a last-good reference snapshot and recovery when filtering removes every row.

The fifth increment adds Hall of Fame archive/detail and an internal RetroAchievements area: Adventure sets → achievement list → detail, sample unlock modes/dates, unknown records and asynchronous mock refresh. Disconnected, unsupported, loading, offline and error states preserve archive access; cached records retain their context and freshness. Account changes invalidate pending responses and old unlocks. All five primary pages now have mock interactions. Physical-device comfort, Linux/ArmadaOS execution and production integration acceptance remain unverified; this is not completion of the real modules or persistence milestones.

The shared backend foundation is now in progress, while device validation remains an open gate. Read-only archive samples do not fulfill later manual archive creation/editing, and the fake achievement provider does not establish real RetroAchievements support.

Deliverables:

- living Home with mock Trainer/World/progress data
- compact slide-out Continue Adventure drawer
- several mock ResumePoint cards
- World browser from Kanto through Paldea with bounded Adventure lists, detail, capability-based actions and retained local selection
- Pokédex browsing/detail mock with combined World/type/status filters, name/number search, sorting, and controller text entry
- Trainer profile creation/editing interaction with a fake repository; durable storage follows in the backend milestone
- Hall of Fame mock archive/detail and an internal achievements area with representative external-provider states
- screenshot placeholders and metadata
- resume routed through `MockAdventureAdapter`
- empty/loading/error examples
- correct overlay/drawer focus behavior
- system menu with a stubbed Desktop / Maintenance Mode entry

Done when the whole top-level product can be navigated comfortably on a controller and feels like one coherent trainer terminal rather than disconnected demo screens.

**Checkpoint:** after this milestone, assume any visual composition may be redesigned. Implementation completeness is not design approval.

## Milestone 3 — Shared backend foundation

2026-09-06 first backend increment: asynchronous SQLite storage for the real local Trainer profile, Pokédex favorite marks and versioned shell navigation. Startup restores committed projections and stable record selections; failed writes preserve saved values, and normal exit drains pending writes. The original all-sample mode remains available with `--ephemeral`. Corrupt/foreign/newer stores are preserved and get controller-accessible recovery. Tests reopen an actual database and restart the rendered application in separate processes using SDL virtual controls. See `LOCAL_PERSISTENCE.md`.

The following increment adds persistent user-library/configuration storage, atomic region relationships, revision-checked editing, controller file selection, custom Worlds and persisted color/motion preferences. The shared backend code foundation is now in place. A content-free child process exercises checkpoint/start/return/crash behavior, including QML focus restoration; it does not establish real adapter or ArmadaOS session support. No sample game progress, archive or achievement data is migrated into personal history. Windows and Linux CI checks pass; ARM64 and physical-device acceptance remain pending. Device instructions are in `FIRST_DEVICE_RUN.md`.

Deliverables:

- stable domain identities and models for Worlds, Adventures, Resume Points, Trainer and archive/progress records
- typed repository and service contracts consumed by the interface through feature models
- SQLite-backed local persistence and initial schema/migration handling
- persistent profile create/edit/save/cancel behavior and shell page/focus restoration
- foundational library storage and explicit unknown/unavailable values
- fake Adventure/progress/achievement providers behind replaceable interfaces
- asynchronous work and error boundaries that keep storage/provider failures off the UI thread

Done when profile and shell state survive restart, failed writes preserve saved data, and feature models can use fake or persistent repositories without embedding storage or platform details in QML. No real emulator, RetroAchievements account, or game save parser is required for this milestone.

## Milestone 4 — ArmadaOS device baseline

The first native ARM64 build and actual system/display/package findings are now recorded in [ARMADA_DEVICE_BASELINE.md](ARMADA_DEVICE_BASELINE.md). CMake installs a normal desktop launcher, without installing or selecting a dedicated session. Physical controller/readability acceptance remains open. The owner reopened idle/power diagnostics on 2026-09-12; everyday operation remains configured without sleep, and suspend/wake acceptance is still open.

Before deep emulator integration, validate the current real target environment.

Prepared locally: Start → Controller now shows mapped button/axis signals, observed controller actions, foreground/neutral gating and Qt runtime/display readings. It exports a local diagnostic JSON on request, with no personal-library content or session changes. SDL virtual-controller and rendered-panel tests cover the preparation; the actual handheld baseline remains open. See `DEVICE_DIAGNOSTICS.md` for the first-device sequence and the boundary of software observations.

Deliverables:

- document current ArmadaOS session/compositor/display arrangement
- verify Qt 6 runtime/build strategy on target
- verify Retroid controller input mapping
- verify display geometry/density/performance
- identify package/install mechanism
- identify safe process launch approach
- inspect current emulator availability/paths without hard-coding them into feature code
- identify power/audio/brightness/network integration options
- document safe path into/out of KDE Plasma

Done when `docs/ARMADA_PLATFORM.md` reflects actual target findings and a development build runs reliably on the handheld as a normal full-screen application.

## Milestone 5 — Worlds / Adventure module

The user-approved [collection extension](COLLECTION_CATALOGUE.md) adds catalogue editions, platform badges, missing entries and controller file attachment. The user confirmed physical navigation, profile creation and real launch on 2026-09-11. Remaining module work must preserve that tested path while adding one real adapter capability at a time.

Personal library browsing and controller add/edit/file selection work on the common backend. The [first RetroArch adapter](RETROARCH_ADAPTER.md) now launches configured records through the persisted checkpoint/process boundary. Ruby was opened from Hoenn on the Flip 2 and returned to the same detail/focus using injected OS gamepad chords. All sixteen ARM64 tests pass. Hands-on physical control validation remains open, so this milestone is not marked complete. Details and acceptance criteria: `LIBRARY_AND_LAUNCH.md`.

Take Worlds from mock to the first real end-to-end feature.

Deliverables:

- persistent World and Adventure domain data
- region-first World browser/detail
- Adventure configuration model
- repository boundary for user library data
- first real launch-capable `AdventureAdapter`
- process lifecycle through platform service
- integration setup kept outside normal user-facing browsing
- launch validation and clear fallback/error behavior
- support for an Adventure having one primary World plus additional relationships
- real-device controller/focus/performance pass

Done when at least one configured Pokémon Adventure can be found through its World, launched on the target ArmadaOS device, exited, and returned from cleanly without exposing emulator/platform jargon.

Visual redesign of Worlds is explicitly allowed and expected if real data suggests a better composition.

## Milestone 6 — Home + Continue module

The [resume provenance contract](RESUME_POINTS.md) pins Home selection to one source revision, distinguishes unavailable moments and prevents implicit replacement at launch. The first [RetroArch/mGBA moments provider](RETROARCH_RESUME.md) adds asynchronous discovery, optional thumbnails, separate exit-state destinations and guarded entry-state restoration. Its device/delivery acceptance is tracked in that document; other cores retain normal launch.

The [Home/history increment](HOME_AND_HISTORY.md) adds observed launch sessions, honest recorded duration, durable explicit Home selection and a separate physical-style launch button. Y selects Home's Adventure without launching. On 2026-09-13, real FireRed/mGBA exit states, thumbnails, selection, direct resume, cancellation, corruption rejection and restart were checked through the Flip's OS controller path. The user has already confirmed ordinary physical navigation and launch; hands-on feedback on the new state cards remains welcome.

Deliverables:

- real recent-Adventure history
- resumable-point repository boundary
- adapter capability model for enumerate/resume/screenshot/timestamp
- compact Continue drawer fed by real data where available
- normal-launch fallback when exact resume is unavailable
- robust missing-state/error behavior
- persistent Home overview data
- fast restoration after Adventure exit
- real-device performance/focus pass

Done when Continue is genuinely useful for at least one supported integration and gracefully handles missing capabilities.

Home and Continue visuals may be redesigned around actual screenshots/save-state behavior.

## Milestone 7 — Trainer + Hall of Fame module

Deliverables:

- extend the persisted, editable Trainer profile introduced in the first prototype with real aggregate progress
- favorite/featured Pokémon placeholder/provider support
- aggregate progress through stable interfaces
- Hall of Fame archive/detail
- manual Hall of Fame entry creation/editing
- RetroAchievements achievements inside Hall of Fame through an external provider and local cache
- achievement detail and game/Adventure association for supported content
- separate external account unlocks from current-playthrough progress and local completion records
- disconnected, unsupported, cached/offline, loading and error states that leave the archive usable
- optional automatic suggestions behind provider interfaces

Done when Trainer and Hall of Fame are useful independently of game-specific save parsers, supported RA achievements can be browsed entirely with the controller, unavailable achievement data is handled honestly, and no new primary page or local L1/R1 binding has been introduced.

## Milestone 8 — Pokédex module

Deliverables:

- reference-provider abstraction
- offline/local dataset support
- number/name browsing
- type/status filters
- entry detail
- Seen/Caught/Favorite state
- local notes
- persistence/cache
- optional game-progress enrichment through separate providers

Done when Pokédex is smooth, offline-capable, controller-driven, and not coupled to one data source or game format.

## Milestone 9 — Pokémon Center / save services module

Deliverables:

- backup service boundary
- backup browser
- manual backup creation
- safe restore with explicit confirmation
- integration health/status
- missing-content relink flow
- conservative safety rules around external user saves/states

Done when maintenance is safe, clear, modular, and separated from ordinary Adventure browsing.

## Milestone 10 — Dedicated TrainerOS session prototype

Only after normal application mode is stable.

Deliverables:

- dedicated TrainerOS graphical session entry/configuration
- startup directly into TrainerOS in a controlled test setup
- no Plasma panels/window chrome in normal TrainerOS mode
- reliable input/rendering in the dedicated session
- external Adventure launch and return
- crash/restart logging
- known recovery route to Plasma
- Desktop / Maintenance Mode transition
- easy way to revert session configuration

Done when the dedicated session can fail without making the device difficult to recover and can run normal TrainerOS flows reliably.

Plasma should still remain the known-good recovery option during this milestone.

## Milestone 11 — TrainerOS as default device experience

Deliverables:

- configure TrainerOS session as normal/default user experience
- boot/login/startup polish
- wake/resume behavior validation
- fast return from Adventures
- explicit Desktop / Maintenance Mode in system menu
- reliable return from Desktop Mode to TrainerOS
- recovery documentation

Done when ordinary use follows:

**Power on / wake → TrainerOS → Adventure → TrainerOS**

and the user does not need the Plasma desktop for normal play/management.

## Milestone 12 — Additional Adventure integrations

For each new emulator/application integration:

- implement/extend an adapter module
- declare supported capabilities rather than assuming feature parity
- add integration validation/tests
- verify launch/resume behavior on target hardware
- avoid integration-specific UI leakage into domain screens

Likely candidates include RetroArch, melonDS, Azahar, and Dolphin, but order should follow actual desired Pokémon titles and reliability on the device.

Done when adding another integration does not require redesigning core TrainerOS domain/UI APIs.

## Milestone 13 — Automatic progress enrichment

Potential modules:

- game-progress metadata providers
- party information for supported save formats
- badge/progress data
- Pokédex Seen/Caught import
- location data
- Hall of Fame suggestions
- playtime/history enrichment

Automatic metadata enriches the experience but must never become required for launching an Adventure. Validate providers per game/build and field; do not promise universal ROM-hack support, historical event reconstruction, or automatic story guidance. The feasibility review informs this later milestone, not the initial skeleton/backend work.

## Milestone 14 — Packaging and dedicated-device polish

Deliverables/potential work:

- native ARM64 release build
- installable package suitable for the current ArmadaOS base
- dependency packaging/version strategy
- TrainerOS session package/configuration
- setup/update/migration tooling
- startup visual polish
- per-World atmosphere/accent packs
- reduced-motion option
- controller remapping UI
- physical-device performance tuning
- sleep/resume tuning where supported
- optional user asset packs

Done when TrainerOS can be installed/reinstalled on the target ArmadaOS device without development tooling and restored to a working primary-shell configuration predictably.

## Later option — TrainerOS ArmadaOS image

Only after package/session installation is mature, consider a reproducible image based on ArmadaOS with:

- TrainerOS preinstalled
- TrainerOS default session configured
- known integration baseline
- simplified restore/reinstall
- preserved Plasma maintenance/recovery mode

Do not make a custom image an early requirement.

## Later product ideas

- Trainer timeline
- recently caught Home widget
- shiny history
- multiple Trainer profiles
- local achievements/milestones
- optional cloud sync
- theme packs
- deeper supported-game metadata
- restrained Rotom-style system notifications

Physical modification of the handheld remains out of scope.

## Checkpoint after every milestone

1. Does it still feel like a dedicated trainer device?
2. Can normal use be completed without touch/mouse?
3. Are Worlds/Adventures still hiding emulator/platform complexity?
4. Did anything steal `L1/R1`?
5. Did Home become a generic launcher by accident?
6. Are user saves/backups handled conservatively?
7. Is the feature still modular, or did an implementation detail leak upward?
8. Are we keeping a weak visual merely because it already exists?
9. Did ArmadaOS/session-specific logic leak into QML/domain code?
10. Is Plasma still available as a reliable maintenance/recovery path?
