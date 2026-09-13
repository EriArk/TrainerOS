# TrainerOS Domain Model

The domain model should use TrainerOS language first and integration terminology second. Linux/ArmadaOS/emulator details belong behind integration and platform boundaries rather than shaping the user-facing model.

The persistent implementation covers personal Worlds/Adventures, the single local Trainer profile, Pokédex favorite marks, shell color/motion preferences and versioned browsing state. SQLite schema, ownership, recovery and migration constraints are documented in [LOCAL_PERSISTENCE.md](LOCAL_PERSISTENCE.md). Only reference region names are seeded; sample Adventures/progress/archive/provider records are not copied into the personal store.

## World

Collection browsing also supports named spin-off settings and explicit thematic groups for titles without a known region; see [collection catalogue](COLLECTION_CATALOGUE.md). These groups do not assert fictional geography.

Represents a Pokémon region.

Suggested fields:

```text
id
slug
name
sortOrder
status
accent/theme key
lastVisitedAt
aggregatePlaytime
badgeProgress
pokedexSeen
pokedexCaught
```

Initial IDs should cover Kanto, Johto, Hoenn, Sinnoh, Unova, Kalos, Alola, Galar, and Paldea.

`WorldStatus` can begin as:

```text
NOT_VISITED
IN_PROGRESS
COMPLETED
CHAMPION
```

Do not assume all metrics are available automatically; nullable/unknown values are valid.

## Adventure

The current model additionally carries `platformId`, `catalogueId` and a separate human-readable `variant`. A reference projection can have `collectionOnly=true`; it cannot be persisted or launched until a file is linked. Owned records do not acquire inferred game progress when linked. The read-only collection reference and personal records have separate ownership.

Represents one configured playable Pokémon journey/title.

Suggested fields:

```text
id
primaryWorldId
additionalWorldIds
displayName
editionName
contentReference
adapterId
integrationConfig
lastPlayedAt
playtime
status
badgeCount
badgeTotal
coverOrHeroAsset
```

`AdventureStatus` can begin as:

```text
NOT_STARTED
IN_PROGRESS
COMPLETED
```

Platform/system information belongs in integration metadata and should not drive the main UI hierarchy.

The native implementation uses stable `id`, primary `worldId`, `additionalWorldIds`, `title`, `adapterId`, `description` and `AdventureKind` (`Original`, `Remake`, `RomHack`). Kind describes an edition; it is not a replacement for region-first grouping. `AdventureRegistration` adds an external `contentPath`, opaque `integrationConfig`, an optimistic `revision` and an optional new World committed atomically with the record. Editing or relocating a file preserves the Adventure ID. Additional World relationships make the same Adventure discoverable under multiple regions; they do not duplicate it. A named custom World supports ROM hacks outside the reference regions.

Worlds and Adventures use an optional `JourneyStatus` (`NotStarted`, `InProgress`, `Completed`) for the initial presentation. Adventures also have optional `badges` and `caught` counts. Unknown status displays as Not recorded; unknown counts display as a dash, while a known zero remains zero. Personal records leave these values unknown; explicit sample progress belongs only to the preview. These fields do not establish save parsing, provenance tracking or aggregate World progress; the later provider model must preserve those distinctions.

## ResumePoint

Implemented provenance, availability and selection rules are in [Resume points](RESUME_POINTS.md). Exact resume requires a complete `ResumeSource`, a valid observation and matching adapter capability. The default is unverified; normal launch history remains separate.

Represents an adapter-backed resumable moment. `ContinueEntry` may instead carry a recent `PlaySession`; a launch record is never represented as an emulator state. Selecting either kind chooses Home's Adventure, and only Home's action button launches/resumes.

Suggested fields:

```text
id
adventureId
adapterId
createdAt
lastUsedAt
screenshotRef
locationLabel
playtime
progressLabel
adapterPayload
directResumeAvailable
```

`adapterPayload` is opaque to feature UI. Only the matching adapter should interpret it.

A ResumePoint can represent an emulator save state, a session snapshot, or another adapter-specific resumable point.

## PlaySession

`id`, `adventureId`, `startedAt`, optional `endedAt`/`elapsedSeconds`, and `outcome` record a process that actually started through TrainerOS. Outcomes are Running, Returned, Failed and Interrupted. Duration uses a monotonic timer; timestamps use UTC. An interrupted shell has no known final duration. `LibraryRepository::recentSessions()` returns the latest session per Adventure in observed launch order, with a bounded UI snapshot. Recorded totals sum known process durations, independently of game-save playtime and progress. Explicit Home selection is browsing state, not a fabricated session or completion flag.

## TrainerProfile

Suggested fields:

```text
id
name
avatarRef
favoritePokemonId
featuredPokemonId
currentAdventureId
createdAt
totalPlaytime
```

Aggregate metrics should preferably be derived from repositories when cheap/reliable rather than duplicated everywhere.

The initial native profile contains `id`, `name`, `emblemId`, `favoritePokemonId` and `createdAt`. Emblems are original local geometry; favorites currently use a limited text-only sample list. Creation assigns an ID and UTC creation time, while edits preserve both. Profile storage is separate from sample Adventure/progress data. Form and keyboard drafts are transient and must not update the saved profile before Save; in-memory repository lifetime does not satisfy restart persistence.

## PokedexReference

Reference/canonical data, separate from the user's progress.

Possible fields:

```text
pokemonId
nationalNumber
localizedName
speciesLabel
typeIds
evolutionGroupId
assetRef
referenceProviderId
```

Reference data can be replaced/refreshed independently from user history.

The native mock's `PokedexEntry` contains stable `id`, `number`, `name`, `types` and `collectionIds`. `PokedexCatalog` contains reference entries, named collections and an explicit load result. Its small sample collections demonstrate World filtering; they are not a complete regional availability dataset and do not describe where the Trainer encountered a Pokémon.

## PokedexProgress

Trainer-owned state:

```text
pokemonId
seen
caught
favorite
shinySeen
shinyCaught
firstSeenAdventureId
firstCaughtAdventureId
firstSeenAt
firstCaughtAt
notes
```

Additional encounter/history tables may be added later rather than turning this row into an unbounded JSON blob.

The initial `PokedexProgress` has optional Seen/Caught booleans and a TrainerOS-owned favorite mark. Missing records preserve unknown Seen/Caught values. Not caught/Not seen filters require known negative values, while Not recorded explicitly selects unknown progress. A favorite edit changes only that mark and does not mutate the reference entry, Trainer profile identity choice or Seen/Caught. The fake repository keeps successful edits for the current application run and preserves the old mark on failure; durable profile-scoped storage follows in the backend milestone.

## PokemonHistoryEvent

Optional later model for richer personal history.

Examples:

```text
SEEN
CAUGHT
JOINED_PARTY
LEFT_PARTY
SHINY_FOUND
BECAME_FAVORITE
HALL_OF_FAME
```

Fields:

```text
id
pokemonId
adventureId
worldId
type
timestamp
metadata
```

This can power timeline-like Pokédex details without coupling them to save parsers.

## HallOfFameEntry

Suggested fields:

```text
id
worldId
adventureId
completedAt
playtime
screenshotRef
notes
source
```

`source`:

```text
MANUAL
AUTOMATIC
IMPORTED
```

## HallOfFameMember

One party slot in a Hall of Fame entry:

```text
entryId
slot
pokemonId
nickname
level
shiny
formKey
extraMetadata
```

Allow unknown fields. Manual entry must not require data that only a parser could know.

The native mock expands `HallOfFameEntry` with an Adventure ID, title/World display snapshots, optional completion date/playtime, notes, Manual/Imported source and up to six `HallOfFameMember` samples. Missing member/level fields remain Not recorded rather than a fabricated team. Archive entries describe particular past journeys and do not overwrite the current Adventure's progress. The initial archive repository is read-only; creation/editing and durable profile-scoped relationships follow later.

## External achievements in Hall of Fame

Hall of Fame displays external achievements alongside completion records, but these remain different domain objects. Suggested records:

- `AchievementDefinition`: provider ID, external game/set/achievement IDs, title, description, and supported display metadata.
- `AchievementUnlock`: provider account ID, external achievement ID, mode, earned time when available, and last successful retrieval time.
- `AdventureAchievementLink`: explicit association between a configured Adventure/content version and an external game/set; not proof of progress in a particular run.

Cache identity must include the provider/account and applicable external IDs. Preserve unknown dates and unavailable data instead of inventing defaults. Manual HallOfFameEntry editing must not alter provider-owned unlocks, and external account achievements must not populate current-save badges or Seen/Caught by inference. Fake records are sufficient until the Hall of Fame integration milestone.

Implemented mock shapes: `AchievementSet` links a namespaced set/game ID to an Adventure; `AchievementDefinition` contains original sample title/description; `AchievementUnlock` contains an achievement ID, optional unlocked flag, optional Standard/Hardcore mode and optional earned date. `AchievementSnapshot` scopes both lists under `AchievementContext` (provider/account), set ID, availability state and retrieval time. Missing unlock records are unknown, while explicit false is Locked. A known unlock can still have an unknown date/mode.

The snapshot boundary permits cached rows during loading/offline/error and rejects foreign account/set snapshots. The mock clears cache/pending work on account changes. All IDs and goals are fixtures, not real external identifiers or copied set definitions; persistent provider/game/set/account keys and full per-mode history remain backend/integration work.

## WorldProgress

World progress may eventually be derived from Adventures, but a dedicated projection/read model is useful for UI:

```text
worldId
status
adventureCount
completedAdventureCount
totalPlaytime
badgeCount
badgeTotal
pokedexSeen
pokedexCaught
lastVisitedAt
```

Treat this as derived/cacheable data unless a real use case requires independent ownership.

## IntegrationProfile

Stores configuration for one external emulator/application integration without exposing it throughout the shell.

Suggested fields:

```text
id
adapterId
displayName
installationRef
config
lastValidatedAt
status
```

`installationRef` is an adapter/platform-owned reference to the installed integration and must not be interpreted by feature UI. It may resolve to an executable, package, launcher entry, runtime profile, or another Linux-specific mechanism.

`config` is adapter-owned serialized data. Prefer typed adapter config internally where practical.

Do not encode distro-specific executable paths directly into World/Adventure presentation models.

## AdventureCapability

Capability flags let the UI adapt to each integration:

```text
LAUNCH
PROCESS_LIFECYCLE
ENUMERATE_RESUME_POINTS
DIRECT_RESUME
STATE_SCREENSHOT
SAVE_BACKUP
SAVE_METADATA
PARTY_METADATA
POKEDEX_METADATA
PROGRESS_METADATA
```

The absence of a capability is normal, not an error.

In the native mock, `AdventureCapabilities` contains launch, direct-resume and screenshot flags and is queried for a specific Adventure. A matching resume point is also required to expose Continue. A sample can support launch without direct resume, or have neither action until setup is available. Capability absence does not erase its library record or imply anything about its progress.

## Activity / milestone model

Home will eventually need recent activity without hard-coding every feature widget to every repository.

A lightweight `TrainerActivity` projection can contain:

```text
id
type
timestamp
worldId?
adventureId?
pokemonId?
title
subtitle
assetRef?
payload?
```

Examples:

- Adventure resumed
- badge milestone
- Pokémon caught
- Hall of Fame entry
- backup completed

## IDs and external references

- TrainerOS-owned entities use stable local IDs.
- External integration identifiers are stored separately.
- Never use a mutable filesystem path as the sole identity for an Adventure; installs/content can move.
- Keep content/install references adapter-owned and relocatable where practical.
- Store source-provider IDs for Pokédex reference records so data can be refreshed or replaced.
- Linux/ArmadaOS-specific paths, package identifiers, desktop entries, and launch arguments belong in adapter/platform configuration, not general feature models.

## Unknown data is valid

`HallOfFameEntry` now persists manual archive memories with a revision for conflict detection; see [Hall of Fame](HALL_OF_FAME.md). Its historical title/World and optional manually recorded date/time/team remain independent of current save progress and observed play sessions.

Many values will be unavailable before save parsing exists. UI/domain code must handle `unknown` cleanly rather than inventing zero values.

Examples:

- unknown badge count ≠ zero badges
- unknown playtime ≠ 0 hours
- unsupported direct resume ≠ broken Adventure

Graceful partial data is a core design requirement.
