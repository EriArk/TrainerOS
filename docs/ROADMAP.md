# TrainerOS Roadmap

The roadmap prioritizes the controller experience and information architecture before deep external-app integration.

## Development model

The first mock exists to prove feel, navigation, and information architecture. It is **not** the final visual design.

After the mock is usable, development proceeds **modularly and top-down in vertical slices**:

1. define/refine the user-visible screen and interaction
2. implement the feature domain/use-case layer
3. define repository/service interfaces
4. connect adapter/integration behavior
5. add persistence/device-specific behavior
6. test on the actual handheld
7. revisit both code boundaries and visuals when reality exposes weak assumptions

Do not build the entire presentation layer first and postpone all real behavior until later. Each major feature should become independently useful end-to-end before the next large feature receives deep integration work.

Visual design remains deliberately fluid. Layouts, hierarchy, component styling, animation, density, and whole compositions can be reworked at any milestone. Product invariants — controller-first behavior, section structure, terminology, and hidden platform complexity — matter more than preserving mock visuals.

## Milestone 0 — Project skeleton

Deliverables:
- Android/Kotlin project
- Jetpack Compose
- landscape-first activity
- base theme/tokens
- package structure from `ARCHITECTURE.md`
- mock data and repositories

Done when a clean checkout builds and launches to Home without network access.

## Milestone 1 — Controller shell

Deliverables:
- D-pad and left-stick focus
- `A` confirm
- `B` back
- `Start` system menu
- `L1/R1` switch Home, Worlds, Pokédex, Trainer, Hall of Fame
- persistent active page
- clear custom focus treatment
- horizontal page transitions

Done when every primary page is usable without touch and `L1/R1` remain globally reserved.

## Milestone 2 — Full mock experience

Deliverables:
- living Home layout with mock trainer/world/progress data
- compact slide-out Continue Adventure drawer
- several mock ResumePoint cards
- World browser from Kanto through Paldea
- simple Pokédex browsing/detail mock
- Trainer mock profile
- Hall of Fame mock archive/detail
- screenshot placeholders and metadata
- resume routed through `MockAdventureAdapter`
- empty/loading/error examples
- correct drawer/focus behavior

Done when the entire top-level product can be navigated comfortably on a controller and feels like one coherent trainer terminal rather than disconnected demo screens.

**Checkpoint:** after this milestone, assume any visual composition may be redesigned. Do not mistake implementation completeness for design approval.

## Milestone 3 — Worlds / Adventure vertical slice

Take Worlds from mock to a real modular feature.

Deliverables:
- persistent World and Adventure domain data
- region-first World browser and detail
- Adventure configuration model
- repository boundary for user library data
- first launch-capable `AdventureAdapter`
- integration setup kept outside normal user-facing browsing
- launch validation and clear fallback/error behavior
- support for an Adventure having one primary World plus additional World relationships
- real-device controller and focus pass

Done when at least one configured Adventure can be found through its World and launched reliably without exposing emulator/platform jargon.

Visual redesign of Worlds is explicitly allowed during this milestone.

## Milestone 4 — Home + Continue vertical slice

Deliverables:
- real recent-Adventure history
- resumable-point repository boundary
- adapter capability model for enumerate/resume/screenshot/timestamp
- compact Continue drawer fed by real data when available
- normal-launch fallback when direct resume is unavailable
- robust missing-state/error behavior
- persistent Home overview data
- real-device performance/focus pass

Done when Continue is genuinely useful for at least one supported integration and gracefully handles missing capabilities.

Home and Continue visuals may be redesigned based on real save-state data and handheld testing.

## Milestone 5 — Trainer + Hall of Fame vertical slice

Deliverables:
- persistent Trainer profile
- favorite/featured Pokémon placeholder/provider support
- aggregate progress sourced through stable interfaces
- Hall of Fame archive/detail
- manual Hall of Fame entry creation/editing
- optional automatic suggestions behind a provider boundary

Done when Trainer and Hall of Fame remain useful even if no game-specific parser exists.

## Milestone 6 — Pokédex vertical slice

Deliverables:
- reference-provider abstraction
- offline/local dataset support
- number/name browsing
- type/status filters
- entry detail
- Seen/Caught/Favorite state
- local notes
- persistence and caching
- optional game-progress enrichment through separate providers

Done when the Pokédex is smooth, offline-capable, controller-driven, and not coupled to one data source or game format.

## Milestone 7 — Pokémon Center / save services vertical slice

Deliverables:
- backup service boundary
- backup browser
- manual backup creation
- restore with explicit confirmation
- integration health/status
- missing-content relink flow
- conservative safety rules around user saves

Done when maintenance is safe, clear, modular, and separated from normal adventure browsing.

## Milestone 8 — Additional integrations

For each new emulator/application integration:

- implement or extend an adapter module
- declare supported capabilities rather than assuming feature parity
- add integration tests / validation
- verify launch/resume behavior on target hardware
- avoid integration-specific UI leakage into domain screens

Done when adding another integration does not require redesigning core TrainerOS domain/UI APIs.

## Milestone 9 — Automatic progress enrichment

Potential modules:
- game progress metadata providers
- party information for supported formats
- badge/progress data
- Pokédex Seen/Caught import
- location data
- Hall of Fame suggestions
- playtime/history enrichment

Automatic metadata must enrich the experience but never become required for launching or using an Adventure.

## Milestone 10 — Dedicated-device polish and release

Potential work:
- default launcher/Home mode where supported
- startup polish
- per-World atmosphere/accent packs
- reduced motion
- controller remapping UI
- physical-device performance tuning
- sleep/resume tuning where supported
- optional user asset packs
- release signing/build configuration
- installable release APK

Done when TrainerOS can be installed on the target Retroid as a normal signed APK and used as the primary frontend without development tooling.

## Later ideas

- trainer timeline
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
2. Can normal use be completed without touch?
3. Are Worlds/Adventures still hiding platform complexity?
4. Did anything steal `L1/R1`?
5. Did Home become a generic launcher by accident?
6. Are user saves and backups handled conservatively?
7. Is this feature still modular, or did an implementation detail leak upward?
8. Are we keeping a weak visual merely because it already exists?
