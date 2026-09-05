# TrainerOS Roadmap

The roadmap prioritizes the controller experience and information architecture before deep external-app integration.

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

## Milestone 2 — Home + Continue

Deliverables:
- living Home layout with mock trainer/world/progress data
- compact slide-out Continue Adventure drawer
- several mock ResumePoint cards
- screenshot placeholders and metadata
- resume routed through `MockAdventureAdapter`
- empty state
- correct drawer focus trap/restore

Done when Home feels like a trainer overview rather than a launcher grid, and Continue remains compact and secondary.

## Milestone 3 — Worlds

Deliverables:
- World browser from Kanto through Paldea
- World status/progress
- World detail
- Adventures inside Worlds
- launch/resume through adapter interface
- support for an Adventure having one primary World plus additional World relationships

Done when the user never needs to browse by console/emulator to find an Adventure.

## Milestone 4 — Trainer + Hall of Fame

Deliverables:
- editable Trainer profile
- favorite/featured Pokémon placeholder support
- aggregate progress
- Hall of Fame archive and detail
- manual Hall of Fame entry creation/editing

Done when these features are useful even with no automatic game-data import.

## Milestone 5 — Pokédex foundation

Deliverables:
- reference-provider abstraction
- offline/local sample dataset
- number/name browsing
- type/status filters
- entry detail
- Seen/Caught/Favorite state
- local notes

Done when browsing is smooth, offline-capable, and fully controller-driven.

## Milestone 6 — First real Adventure integration

Deliverables:
- integration setup in service/advanced UI
- first real `AdventureAdapter`
- launch validation
- clear failure/fallback states
- state refresh when TrainerOS regains focus

Done when one configured Adventure launches reliably on target hardware without leaking technical jargon into normal UI.

## Milestone 7 — Real Continue data

Deliverables:
- enumerate resumable points for the first supported integration
- screenshots/timestamps when available
- direct resume when supported
- normal-launch fallback when direct resume is unavailable

Done when Continue gracefully handles different capability levels without assuming every integration works the same way.

## Milestone 8 — Pokémon Center / backup services

Deliverables:
- backup browser
- manual backup creation
- restore with explicit confirmation
- integration health/status
- missing-content relink flow

Done when maintenance is safe, clear, and separated from normal adventure browsing.

## Milestone 9 — Automatic progress enrichment

Potential work:
- provider interfaces for game progress metadata
- party information for supported formats
- badge/progress data
- Pokédex Seen/Caught import
- location data
- Hall of Fame suggestions

Done when automatic metadata enriches the experience but is never required for launching or using an Adventure.

## Milestone 10 — Dedicated-device polish

Potential work:
- default launcher/Home mode
- startup polish
- per-World atmosphere/accent packs
- reduced motion
- controller remapping UI
- physical-device performance tuning
- sleep/resume tuning where supported
- optional user asset packs

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
