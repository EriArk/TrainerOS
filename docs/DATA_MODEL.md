# TrainerOS Domain Model

The domain model should use TrainerOS language first and integration terminology second. Linux/ArmadaOS/emulator details belong behind integration and platform boundaries rather than shaping the user-facing model.

## World

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

## ResumePoint

Represents one item in Continue Adventure.

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

Many values will be unavailable before save parsing exists. UI/domain code must handle `unknown` cleanly rather than inventing zero values.

Examples:

- unknown badge count ≠ zero badges
- unknown playtime ≠ 0 hours
- unsupported direct resume ≠ broken Adventure

Graceful partial data is a core design requirement.
