# TrainerOS Roadmap

TrainerOS now targets a native Qt/QML Linux shell on top of ArmadaOS rather than Android.

The roadmap prioritizes controller feel and information architecture first, then develops real features **top-down in modular vertical slices**.

## Development model

The first mock exists to prove feel, navigation, focus, motion, information architecture, and screen relationships. It is **not the final visual design**.

After the mock is usable, each major feature proceeds through:

1. define/refine the user-visible screen and interaction
2. implement feature domain/use-case behavior
3. define repository/service interfaces
4. connect real adapter/platform integration
5. add persistence/device-specific behavior
6. test on the actual ArmadaOS handheld
7. refactor weak code boundaries and redesign weak visuals

Do not build the entire presentation layer first and postpone real behavior until later. Each major feature should become independently useful end-to-end before the next receives deep integration work.

Visual design remains deliberately fluid. Layouts, hierarchy, component styling, animation, density, and whole compositions can be replaced at any milestone. Product invariants matter more than preserving mock visuals.

## Milestone 0 — Native project skeleton

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

Deliverables:

- real gamepad/D-pad and left-stick navigation
- `A` confirm
- `B` back
- `Start` system menu
- `L1/R1` switch Home, Worlds, Pokédex, Trainer, Hall of Fame
- persistent active page
- clear custom focus treatment
- short horizontal page transitions
- centralized input normalization/repeat/dead-zone handling

Done when every primary page is usable without touch/mouse and `L1/R1` remain globally reserved.

## Milestone 2 — Full mock experience

Deliverables:

- living Home with mock Trainer/World/progress data
- compact slide-out Continue Adventure drawer
- several mock ResumePoint cards
- World browser from Kanto through Paldea
- simple Pokédex browsing/detail mock
- Trainer mock profile
- Hall of Fame mock archive/detail
- screenshot placeholders and metadata
- resume routed through `MockAdventureAdapter`
- empty/loading/error examples
- correct overlay/drawer focus behavior
- system menu with a stubbed Desktop / Maintenance Mode entry

Done when the whole top-level product can be navigated comfortably on a controller and feels like one coherent trainer terminal rather than disconnected demo screens.

**Checkpoint:** after this milestone, assume any visual composition may be redesigned. Implementation completeness is not design approval.

## Milestone 3 — ArmadaOS device baseline

Before deep emulator integration, validate the current real target environment.

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

## Milestone 4 — Worlds / Adventure vertical slice

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

## Milestone 5 — Home + Continue vertical slice

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

## Milestone 6 — Trainer + Hall of Fame vertical slice

Deliverables:

- persistent Trainer profile
- favorite/featured Pokémon placeholder/provider support
- aggregate progress through stable interfaces
- Hall of Fame archive/detail
- manual Hall of Fame entry creation/editing
- optional automatic suggestions behind provider interfaces

Done when Trainer and Hall of Fame are useful independently of game-specific save parsers.

## Milestone 7 — Pokédex vertical slice

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

## Milestone 8 — Pokémon Center / save services vertical slice

Deliverables:

- backup service boundary
- backup browser
- manual backup creation
- safe restore with explicit confirmation
- integration health/status
- missing-content relink flow
- conservative safety rules around external user saves/states

Done when maintenance is safe, clear, modular, and separated from ordinary Adventure browsing.

## Milestone 9 — Dedicated TrainerOS session prototype

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

## Milestone 10 — TrainerOS as default device experience

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

## Milestone 11 — Additional Adventure integrations

For each new emulator/application integration:

- implement/extend an adapter module
- declare supported capabilities rather than assuming feature parity
- add integration validation/tests
- verify launch/resume behavior on target hardware
- avoid integration-specific UI leakage into domain screens

Likely candidates include RetroArch, melonDS, Azahar, and Dolphin, but order should follow actual desired Pokémon titles and reliability on the device.

Done when adding another integration does not require redesigning core TrainerOS domain/UI APIs.

## Milestone 12 — Automatic progress enrichment

Potential modules:

- game-progress metadata providers
- party information for supported save formats
- badge/progress data
- Pokédex Seen/Caught import
- location data
- Hall of Fame suggestions
- playtime/history enrichment

Automatic metadata enriches the experience but must never become required for launching an Adventure.

## Milestone 13 — Packaging and dedicated-device polish

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
