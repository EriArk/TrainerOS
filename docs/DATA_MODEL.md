# TrainerOS Domain Model

**Target reconciliation — 2026-09-19 (#62).** The accepted [#42–62 specification](EXPANSION_42_62.md) supersedes older product direction. Planned behavior below is not a claim that the deployed build has changed; see the [working baseline](ROADMAP.md#working-baseline) and dated module evidence.

The domain model should use TrainerOS language first and integration terminology second. Linux/ArmadaOS/emulator details belong behind integration and platform boundaries rather than shaping the user-facing model.

The persistent implementation covers personal Worlds/Adventures, the local Trainer profile, favorites/manual Pokédex journal, observed play history, manual Hall memories, preferences and versioned browsing state. SQLite schema, recovery and migration constraints are documented in [LOCAL_PERSISTENCE.md](LOCAL_PERSISTENCE.md) and module documents. Reference catalogue data is composed separately; sample personal progress/archive/provider records are not seeded into the personal store.

## Ownership contract — 2026-09-13

This clarification addresses [issue #2](https://github.com/EriArk/TrainerOS/issues/2) before the [next modules](ROADMAP.md). It defines target ownership; the remaining code/storage audit is planned, and no integration-profile schema migration is claimed here. Older suggested fields below are conceptual UI data unless explicitly described as persisted.

| Data | Single owner / composition rule |
| --- | --- |
| World identity | Reference/personal World IDs, naming, ordering and relationships. Dynamic badge/Dex/time/completion totals belong in derived WorldProgress, never a second independently writable aggregate on World. |
| Adventure identity | Personal library record, catalogue/variant link, content reference and World relationships. A new filename or equal ROM hash does not establish a new/same playthrough. |
| Installation configuration | Adapter installation profile owns executable/runtime, global config and validation. Current registrations select an adapter and hold per-Adventure overrides such as a core; they must not copy device-wide installation config. An explicit integrationProfileId is a possible future reference, not a field already migrated into SQLite. |
| Trainer identity / totals | Profile owns identity/favorite choice. Recorded time is derived from observed PlaySessions; journal/archive/World totals are read models with source labels. No independently editable copies of the same totals. |
| Ordinary save progress | External save is the source; GameProgress is a provider/revision-bound observation. Current badges/caught totals do not write manual journal, historical captures or RA unlocks. |
| Manual Caught | Today's species-wide nullable journal assertion. A Caught collection is its projection; it proves neither a form nor a specific Pokémon, origin or date. |
| Future individual Pokémon | Separately sourced record with provider/artifact revision and Adventure/save lineage where available. Re-reading/rolling back a save is not a new capture event; observed-at is not caught-at. Manual/provider sources remain distinguishable. |
| Account achievements | External account-scoped definitions/unlocks and private cache, independent of saves and manual Hall memories. #12 central ownership is scoped to the active Trainer by #20; no credential/cache is implicitly shared across people. |
| Media | Adventure/catalogue/variant artwork and owned clean exit images (legacy ResumePoint images are migration-only) use semantic handles; species/form artwork has a separate provider. Neither is content ownership, progress or compatibility evidence. |

Unknown values remain unknown; known zero stays zero. Use explicit repositories/read-model composition, not a generic event-sourcing framework. A ROM cleanup may relink proven content but must not merge PlaySessions, Hall memories, save lineages or legacy artifacts merely because files hash identically.

## Accepted owner and library-context extension — planned

Issues #19–20/#28/#31 require the following target contract before new screens; these fields/migrations are **not yet delivered**. [Expansion acceptance](EXPANSION_PLAN.md) and [roadmap P2–P4](ROADMAP.md#unified-execution-order--existing-work-and-new-issues) define the gates.

| Scope | Ownership and migration rule |
| --- | --- |
| Device | Shared library/content registrations, catalogue facts, installations, shared art packs and hardware configuration. Audio preferences are device-wide by #36. Do not clone game rows/files when creating a Trainer. |
| Trainer | Stable profile/PIN verifier; manual journal/favorites, Hall memories, play sessions/derived totals, navigation/Home selections and canonical external account identity/cache. Migrate existing personal rows to the same legacy Trainer ID, preserving counts and references. |
| Playable record | Stable identity/variant/content/runtime/media, explicit Pokémon or Multiverse domain, optional Pokémon World relations and Multiverse system category. Presentation names do not determine identity/domain; personal classification and legacy ambiguity require review. |
| Navigation | Active context plus independent Pokémon/Multiverse Adventure selections per Trainer; useful local World/system focus/search. No second play-history store. Explicit Home choices are not overwritten by unrelated launches. |
| Save lineage | Shared legacy external save remains identified as shared/legacy, not silently assigned to every Trainer. Independently private namespaces require verified adapter support and protected migration. Progress assertions include source/lineage/revision; no per-Trainer separation claim from a new database owner column alone. |
| In-flight work | Capture initiating Trainer/account/revision for writes, launches and provider requests. Drain or invalidate at switch; late results cannot enter the next owner's views. Refuse account switching with an active game or unsafe external writer. |
| Unlock presentation | Persistent acknowledged complete-snapshot/delta identity includes Trainer/account/game/set/achievement/verified mode. Initial import is not a new unlock; notification state is not the source of RA truth. |

Lossless migration, empty/two-owner isolation, deleted-account protection, interrupted writes and restart need explicit acceptance. PIN is optional local shell access protection, not encryption of ROMs/saves or protection from root/maintenance access. Catalogue curation (#30) changes eligibility/navigation only; excluded owned records keep identity/history and maintenance access. No save bytes or unrelated library content change merely because classification changes.

## World

Collection browsing also supports named spin-off settings and explicit thematic groups for titles without a known region; see [collection catalogue](COLLECTION_CATALOGUE.md). These groups do not assert fictional geography.

Represents a Pokémon region.

Suggested fields:

```text
id
slug
name
sortOrder
accent/theme key
optional relationships/metadata
```

Initial IDs should cover Kanto, Johto, Hoenn, Sinnoh, Unova, Kalos, Alola, Galar, and Paldea.

`WorldStatus` can begin as:

```text
NOT_VISITED
IN_PROGRESS
COMPLETED
CHAMPION
```

Status/metrics are presentation projections; do not persist a second copy beside WorldProgress. The initial C++ World still exposes optional sample status for presentation; the ownership audit must preserve unknown semantics while separating real source data.

## Adventure

The current model additionally carries `platformId`, `catalogueId` and a separate human-readable `variant`. A reference projection can have `collectionOnly=true`; it cannot be persisted or launched until a file is linked. Owned records do not acquire inferred game progress when linked. The read-only collection reference and personal records have separate ownership.

Represents one configured playable Pokémon journey/title in the current implementation. The accepted Multiverse extension reuses this playable identity for non-Pokémon titles without requiring a fictional World.

Suggested fields:

```text
id
primaryWorldId
additionalWorldIds
displayName
editionName
contentReference
adapterId
integration profile reference / genuinely Adventure-specific overrides
```

`AdventureStatus` can begin as:

```text
NOT_STARTED
IN_PROGRESS
COMPLETED
```

Last-played/time/status/badges and media may be composed into the Adventure view, but their source owners are PlaySession, verified progress/archive providers and the media layer. Platform/system information does not drive Pokémon World hierarchy; the planned Multiverse context intentionally uses systems. The suggested profile reference above is not an assertion that an explicit integrationProfileId already exists in the implementation.

The native implementation uses stable `id`, primary `worldId`, `additionalWorldIds`, `title`, `adapterId`, `description` and `AdventureKind` (`Original`, `Remake`, `RomHack`). Kind describes an edition; it is not a replacement for region-first grouping. `AdventureRegistration` adds an external `contentPath`, opaque `integrationConfig`, an optimistic `revision` and an optional new World committed atomically with the record. Editing or relocating a file preserves the Adventure ID. Additional World relationships make the same Adventure discoverable under multiple regions; they do not duplicate it. A named custom World supports ROM hacks outside the reference regions.

Worlds and Adventures use an optional `JourneyStatus` (`NotStarted`, `InProgress`, `Completed`) for the initial presentation. Adventures also have optional `badges` and `caught` counts. Unknown status displays as Not recorded; unknown counts display as a dash, while a known zero remains zero. Personal records leave these values unknown; explicit sample progress belongs only to the preview. These fields do not establish save parsing, provenance tracking or aggregate World progress; the later provider model must preserve those distinctions.

## ResumePoint

**Historical/superseded by #49; still present in code.** The former `ResumePoint`/`ResumeSource` and `ContinueEntry` state identities, revisions and opaque payloads are recorded in [RESUME_POINTS.md](RESUME_POINTS.md) and [RETROARCH_RESUME.md](RETROARCH_RESUME.md). Do not extend this into a target domain object or new direct-resume capability.

Migration must identify legacy references/artifacts, stop exposing them as resumable user state, preserve ordinary saves/history/independent images and safely retire only verified TrainerOS-owned artifacts. A database migration must not delete external files merely because an old field disappears.

## Current Adventure and exit media — planned

`CurrentPokemonAdventureContext` is Trainer-scoped: stable Adventure ID, resolved exact build and ordinary-save identity/revision when available. One committed source serves Home, Pokédex, Center, Trainer, Journey and RA. Unsupported fields remain unavailable without changing selection. A separate per-Trainer Multiverse choice and Home domain retain independence. Per-face routes/focus/filters belong to navigation, not extra current-save owners. No separate persistent capsule control.

`savePolicy` is title/integration evidence: `manualConfirm | autosave | unknown`. Unknown asks. A pending exit records session/capture/confirmation outcome; user confirmation is not proof the save was written. Cancellation returns to the same game process. An interrupted session cannot fabricate confirmation.

Clean exit media carries Trainer/domain/Adventure/session/build identity, source revision, capture time, dimensions and availability. The image predates exit UI. It is a media artifact, not a gameplay restore point or proof of progress. Commit successful exit provenance; preserve previous valid media on failed/interrupted attempts as appropriate. Home/Y/history consume the same handle.

## Exact save snapshots and transactions — planned

#42/#50 extend existing resolution/read/backup contracts incrementally. A source identity includes ROM hash/revision, platform, runtime/configuration, save format/resolver, lineage and stable save fingerprint. Translations/hacks/revisions are independent until verified. Store provider/version, source revision, observation time, availability and nullable fields on immutable snapshots; reject stale owner/context responses.

| Semantic object | Meaning / source boundary |
| --- | --- |
| TrainerSaveSummary | Verified save Trainer identity, time, money and title-specific summary; separate from TrainerOS profile and observed process time. |
| PokedexSnapshot | Proven regional/National/species/form Seen/Caught; totals alone cannot enumerate species or individuals. |
| PartySnapshot / StorageSnapshot | Actual occupied/empty slots, title-specific boxes, versioned individual record references; unknown is not empty. |
| PokemonRecord | Proven species/form, level, HP/status, moves/PP, item, OT/origin and other known fields; preserve unknown raw format data for valid round trips. No fictional catches from journal flags. |
| JourneySnapshot | Current title-specific badges/milestones/progression with known time/source; no universal completion formula. |
| ChampionSnapshot | Preserved completion/run/build/source revision and verified historical team; optional actual victory time distinct from observed-at. |

Reads and writes are independently advertised per exact build in a capability registry/evidence matrix. Read capabilities cover the snapshots and their proven fields; write capabilities include healing, reorder, Party/Storage moves, release, money and import/export. Unsupported combinations remain unavailable.

A save mutation records operation identity, original revision, verified protection reference, candidate hash/allowed semantic delta, commit/readback result and recovery state. Require no active writer, stable read, separate candidate/checksum validation, last-moment source recheck, supported atomic replacement and independent verification. Do not report a post-replacement failure as an untouched original. Multi-file saves need a proven protocol.

#45 adds exact-pair peer/transaction IDs, both prepared candidates/READY acknowledgments and durable commit/readback receipts; disconnect can leave an explicit in-doubt recovery state. Sale uses only verified in-game currency. #55 battle owns disposable copies and no save-write capability.

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
```

The conceptual currentAdventureId above projects the single CurrentPokemonAdventureContext, not a second independently persisted profile selection. Current Adventure is selected navigation/history context; aggregate metrics such as totalPlaytime are derived from their repositories rather than independently writable profile fields.

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

**Source distinction:** the existing manual journal remains Trainer-owned secondary history. Target primary current progression is a separate selected-Adventure `PokedexSnapshot` (#46); rollback does not erase manual assertions, and unknown never becomes false.

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

**Target #47/#48:** retain manual entries alongside live Journey and preserved Champion snapshots, with explicit provenance/playthrough identity. RA remains an independent account/set source in the L2/R2 companion; it cannot complete the current save by inference.

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

The first implemented `GameProgress` observation is scoped to Home's selected Adventure and carries availability, an optional badge mask/caught count, provider version, content/save revisions and observation time. It is derived from a verified ordinary save, rebuilt after restart, and never written into the manual journal or account achievements. See [game progress](GAME_PROGRESS.md).

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

Target capability flags describe the exact configured Adventure/build; absence is normal:

```text
LAUNCH / PROCESS_LIFECYCLE / CLEAN_EXIT_CAPTURE / VERIFIED_SAVE_POLICY
ORDINARY_SAVE_BACKUP / ORDINARY_SAVE_RESTORE
READ_TRAINER_SUMMARY / READ_MONEY / READ_POKEDEX / READ_PARTY / READ_STORAGE
READ_POKEMON_RECORD / READ_JOURNEY / READ_CHAMPION
WRITE_HEAL / WRITE_REORDER / WRITE_STORAGE_MOVE / WRITE_RELEASE
WRITE_MONEY / IMPORT_POKEMON / EXPORT_POKEMON
```

These are semantic target names, **not declarations that the current C++ enum or schema already implements them**. State-enumeration/direct-resume/state-screenshot flags in existing code are historical migration inputs only. A verified read never implies a writer, and one tested title never implies generation-wide support.

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

- Adventure launched/returned
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

`PokedexEntry` carries named `PokedexForm` reference facts and evolution-family identities. `PokedexProgress` adds an optional manual Seen/Caught pair, note and edit revision; favorites remain independent. Records apply to the species across the local library, with explicit unknown values and no implied game-save parsing. See [Pokédex](POKEDEX.md).

`HallOfFameEntry` now persists manual archive memories with a revision for conflict detection; see [Hall of Fame](HALL_OF_FAME.md). Its historical title/World and optional manually recorded date/time/team remain independent of current save progress and observed play sessions.

Many values will be unavailable before save parsing exists. UI/domain code must handle `unknown` cleanly rather than inventing zero values.

Examples:

- unknown badge count ≠ zero badges
- unknown playtime ≠ 0 hours
- unsupported save parsing ≠ unplayable Adventure

Graceful partial data is a core design requirement.
