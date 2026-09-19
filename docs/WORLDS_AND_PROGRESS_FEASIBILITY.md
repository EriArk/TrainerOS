# Worlds and progress data: feasibility review

**Research record reconciled 2026-09-19:** external links and technical observations below are dated feasibility evidence, not current integration promises. Updated [#42–62 acceptance](EXPANSION_42_62.md) governs exact-build providers, ordinary-save lifecycle and projections. Revalidate version/source details during each implementation.

Reviewed: 2026-09-06.

Status: research and proposed model refinements for discussion. This is not a claim that native integrations have been implemented or tested on the Flip 2. The current interactive design study uses demonstration progress data. No user ROM or game save was supplied or parsed during this review.

Execution clarification, confirmed by the user on 2026-09-06: follow `ROADMAP.md` bottom-up through the project/interface skeleton, shared backend, then individual modules. RetroAchievements belongs inside Hall of Fame. The save-parser investigation below is deferred work, not the next implementation task.

## Finding

The region-first Worlds concept works across hardware generations and ROM hacks. Rich progress is practical for explicitly supported game/save formats, but cannot be promised uniformly across the library. Launch integration, save parsing, reference data, and achievements are separate capabilities.

The reliable baseline is a local library, configured launches, session records, profile editing, notes/favorites, and explicit manual history. Save providers enrich this baseline field by field. Unknown data must not become zero, an invented completion percentage, or a false history event.

## Worlds: interpretation and proposed refinements

Preserve `World → Adventure`, with requested edition platform badges; emulator configuration remains in settings. For example, Kanto can group Red/Blue/Yellow, FireRed/LeafGreen, and Let's Go; Hoenn can group Ruby/Sapphire/Emerald and Omega Ruby/Alpha Sapphire. Each Adventure retains its own content version, launch adapter, ordinary-save source, progress and exit media. Grouping them never implies that their saves are interchangeable.

The existing nine regions are starter catalog entries, not an exhaustive enum. The catalog needs additional official settings, historical settings, and custom regions. A hack that takes place in a new region belongs there even if its engine comes from FireRed or Emerald. A hack set in an existing region appears beside that region's other Adventures, with its own title and version. Unknown imports need a reachable unassigned state and controller-accessible assignment, rather than disappearing from Worlds.

Multi-region games have one Adventure identity and several World relationships. A linked Adventure may be reachable from both relevant Worlds without creating two save sources or doubling playtime. Merely starting an Adventure is not proof that every linked region has been visited in-game.

Separate title/build identity from playthrough/save-lineage identity internally. Restarting an Adventure, using another save profile, or changing a hack version must not silently mix teams, badges, milestones, or achievements from distinct runs. This does not require another top-level page.

World totals need defined meaning:

- Home's badge row belongs to the active Adventure/run. Other games can have different badge totals or different progression systems.
- Region completion is not one universal percentage shared by originals, remakes, hacks, and spin-offs.
- A regional Pokédex is version-specific. Reference membership and the region where this Trainer encountered a species remain separate.
- Cross-Adventure collection totals count distinct known species, not the sum of each save's caught counter. Custom species require a provider/hack namespace and an explicit mapping where one exists.
- An archived discovery and the current state of a rolled-back save are different records. Do not quietly present historical data as current-save state.

The distinction between region, Pokédex, and game version is also represented by [PokéAPI's reference model](https://pokeapi.co/docs/v2#games-section). Reference providers supply descriptions/types/membership, not the user's personal progress.

## Field-by-field audit of the interface

| Interface data | Plausible source | Practical limit / presentation rule |
| --- | --- | --- |
| Adventure title, edition, World, cover | Local catalog and explicit import metadata | Unknown hacks need manual assignment or a matching manifest; filename alone is insufficient identity. |
| TrainerOS name, emblem, favorite, notes | TrainerOS profile repository | Fully owned by TrainerOS. This profile is separate from each game's character name and trainer ID. |
| Last played / recent Adventure | TrainerOS launch/return records | Accurate for sessions TrainerOS observed, not all play on other devices. File modification time is not reliable gameplay history. |
| Total time together | TrainerOS session clock | Define pause/suspend/crash behavior. This is observed elapsed time, distinct from game-clock playtime and fast-forward. Imported historical time must not be double-counted. |
| In-game playtime | Supported save parser | Available in many main-series formats. It describes that save, may have a cap, and may decrease after rollback. |
| Badge row and badge milestone | Supported save flags, plus a game-specific progression definition | Parse current flags; a difference between snapshots can reveal a newly observed badge. RA badges belong to an account/game set and are not automatically the current run's badge state. |
| Seen / caught totals and entry status | Save Pokédex flags plus the matching reference collection | Use actual flags where supported. Party/box contents alone do not reconstruct all historical registrations. Unknown is distinct from not caught. |
| Current party, levels, nicknames, forms, shiny state | Supported party/box record parser | Good prospects for supported main-series saves. Party data is a snapshot; it is not the final championship team or a complete shiny history. |
| Current town/route | Save map/location IDs, or optional live telemetry, plus a version-specific map table | Display the last saved location unless a live provider is proven. Coordinate values alone are not a location label. |
| “Next stop: Fortree City” | Manual pinned goal or a bespoke story-state provider | Not a generic save field. Automatic guidance requires interpretation of game-specific flags/quests and branching routes. Keep optional or manual initially. |
| Latest milestone | A newly observed save change, an RA event, or a manual record | Preserve event source and time meaning. The first import is not evidence that every event occurred today. |
| Champion / completed / Worlds visited | Explicit game-specific conditions or manual confirmation | Starting a game does not prove visiting every region. Achievement mastery, story completion, and postgame completion are separate concepts. |
| Hall of Fame team | A game's Hall of Fame record, captured event, or manual entry | Some supported games retain such records. Historical date/playtime/screenshot may be absent, and old records may have been overwritten. Never substitute today's party without saying so. |
| First encounter, first catch, team changes, shiny history | Provider-supplied event metadata or observations made over time | Cannot reconstruct an entire past from one save. Date met, date imported, and date observed are not interchangeable; trades further complicate identity/history. |
| Clean exit image | Verified capture before exit UI, bound to Adventure/session | Target #49: an image is presentation/history, never a gameplay restore point. Legacy state-based resume is superseded. |
| Battery, time, network, storage, backup state | Linux platform services and TrainerOS backup records | Independent of game formats; device service wiring still needs on-device validation. |

## Source-code evidence

PKHeX was inspected at commit `a595033bc255ca1a172a0aded0eb73eb82f1b2d8`. This proves that substantial format knowledge exists; it does not establish TrainerOS support or validate a user's particular file.

- [Generation 1 parser](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/SAV1.cs) exposes trainer information, time, badges, party, Pokédex flags, and a Hall of Fame reader. The [Generation 2 parser](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/SAV2.cs) likewise handles core progress, including badge data spanning its two regions.
- [Generation 3 parser](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/SAV3.cs) includes playtime, badges, party and Seen/Caught reads. Its [Emerald block implementation](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/Blocks/Gen3/SaveBlock3LargeE.cs) identifies party storage and badge flags. [pret's Emerald structures](https://github.com/pret/pokeemerald/blob/master/include/global.h) separately document saved position, map information, trainer identity, clock, Pokédex and party structures.
- [Generation 3 Hall of Fame reader](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/Substructures/Gen3/HallFame3.cs) reads a bounded archive of six-member records with species, nickname, level and shiny derivation. Those records do not provide a general real-world event timestamp or a victory screenshot.
- [Omega Ruby / Alpha Sapphire parser](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/SAV6AO.cs) exposes badges, Pokédex, game-time blocks and Hall of Fame access. The source tree also contains distinct DS generation implementations; each intended edition still requires validation.
- [Sword / Shield parser](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/SAV8SWSH.cs) exposes trainer, time, party, badges, Pokédex and coordinates. [Scarlet / Violet parser](https://github.com/kwsch/PKHeX/blob/a595033bc255ca1a172a0aded0eb73eb82f1b2d8/PKHeX.Core/Saves/SAV9SV.cs) exposes party, Pokédex, time, trainer and coordinates, and distinguishes save revisions. These are concrete evidence that Switch progress parsing is feasible for supported formats.

[PKHeX's project documentation](https://github.com/kwsch/PKHeX) lists multiple save formats, including GameCube containers, and requires saves without console-specific outer encryption. Container extraction remains an adapter responsibility. Reading a Switch save is separate from running that Switch game acceptably on this ARM64 handheld. No frame-rate or emulator-compatibility promise is made here.

## ROM hacks and reference data

[PKHeX's maintainer explicitly does not promise ROM-hack support](https://github.com/kwsch/PKHeX/discussions/4316). Hacks may change save layouts, species identifiers, types, map tables, event flags, compression, checksums and completion rules. A file that opens without an exception is not proof that the interpreted data is correct.

Proposed support policy:

1. Every import can receive library metadata and a validated launch adapter, independently of progress parsing.
2. Known official editions receive individually validated save providers.
3. A hack receives rich progress only after its exact version/build has a tested compatibility profile. Small patches may preserve a format; this must be verified, not inferred from the base ROM.
4. Unknown versions retain normal launch, session history and manual profile/notes, while unsupported progress fields remain unknown.

A compatibility manifest can associate a content fingerprint/build with its Worlds, reference dataset, save parser, map table, progression definition and supported fields. Do not identify a hack only by the original game's header, nor assume every species ID is a National Dex number. Custom regions and species must remain valid catalog objects.

## Role of RetroAchievements

The [Web API](https://api-docs.retroachievements.org/v1/get-game-info-and-user-progress.html) supplies a game's achievement metadata and the selected user's unlock progress/dates. This supports an achievement view and activity feed. It is not an API for a particular local save's current team, complete Pokédex flags or quest state.

[RA evaluates game-specific RAM conditions](https://docs.retroachievements.org/general/how-ra-works.html). Rich Presence can provide a useful textual hint about an active game, but it is not a stable cross-game structured progress schema. It should not be the sole source for a persistent collection or exact current-save status.

[Supported emulator/core combinations](https://docs.retroachievements.org/general/emulator-support-and-issues.html) must be checked individually. The reviewed list includes GB/GBC/GBA/DS and GameCube; 3DS and Switch are not listed as supported target systems. Merely choosing a supported console or installing RetroArch does not ensure that a specific game, hash and emulator combination has an achievement set. [ROM-hack sets have their own compatibility rules](https://docs.retroachievements.org/guidelines/content/achievements-for-rom-hacks.html).

Account-level unlocks must remain separate from run-level state. An unlock obtained in an earlier playthrough or on another device does not mean a new local save has that badge. Lack of an RA unlock likewise does not prove the save lacks progress.

The [RA Hardcore rules](https://docs.retroachievements.org/general/faq.html#what-is-hardcore-mode) prohibit loading emulator save states. Target #49 uses normal game startup/ordinary saves in every mode; never silently switch achievement modes. This limitation does not affect reading an ordinary save for display.

## Proposed integration boundary

Keep emulator transport and game interpretation replaceable:

```text
Emulator adapter → stable copy of game save / optional telemetry
Game-specific progress provider → normalized observed fields
Reference provider → names, regions, types, version-specific collections
RA provider → external account achievements
TrainerOS repositories → profile, sessions, notes, observations, archives
UI → capability-aware read models
```

An emulator adapter locates/extracts files and handles launch/return. A save provider understands the game/build, even when the same save can come from different emulators. Qt/QML consumes neither binary offsets nor emulator paths.

Read a stable copy, not a file being rewritten. Prefer an initial scan and a refresh after a clean return from an Adventure; add monitored refresh only where flush behavior is known. Keep originals untouched. Validate format/checksums where supported, retain the last known valid snapshot after a failed read, and report stale/unknown fields honestly.

Game saves and emulator save states are different artifacts. The disk save can lag behind the running game until it is flushed. Parsing an ordinary save does not reveal all unsaved progress in an arbitrary state. The target reads ordinary saves only and labels observations as last-saved; no state-specific provider is planned under #49.

Each observation should carry the Adventure/run, content version, source, parser version, artifact fingerprint, observation time, optional actual event time, and availability status. Reimporting the same artifact must not duplicate achievements or Hall of Fame entries. Loading an older state must not be interpreted as a sequence of new catches or a new victory.

For live detail, [mGBA's scripting API](https://mgba.io/docs/scripting.html) provides concrete memory-read and screenshot facilities. That is evidence for an optional future provider, not a capability automatically shared by the libretro core or every other emulator. Live providers need their own version and achievement-mode checks.

PKHeX.Core is a possible parsing dependency or source of format knowledge; no dependency decision is made here. A direct integration would need to resolve its C# runtime, ARM64 packaging, pinned version and license obligations. Running the Windows editor UI is not the proposed TrainerOS architecture.

## Suggested first save-provider exercise, when that milestone is reached

Only after the skeleton, backend, library, launch/return and relevant feature modules are stable, use one known Emerald edition and a user-owned save with a validated GBA adapter:

1. Launch/return, session time and stable save discovery.
2. Read trainer identity, in-game time, badges, Seen/Caught and party.
3. Resolve last-saved location from the matching map table.
4. Compare a later save to the previous snapshot and update Home/Pokédex.
5. Import Hall of Fame records where present, without inventing dates.
6. Check coexistence with the Hall of Fame RA provider and Hardcore-aware Continue, where those capabilities have already been implemented.
7. Reuse the boundaries for one DS title, then one explicitly selected hack; validate 3DS/Switch files separately from device playability.

Acceptance should include unknown/corrupt saves, unsupported hack builds, file replacement during reading, a second playthrough, rollback, duplicate imports, missing screenshots, mixed partial data, suspend/crash time accounting and ordinary-launch/Hardcore behavior and #49 exit confirmation. Verify semantic values against the in-game screens before declaring a format supported.

Until this slice is tested with actual saves and the device, the interface's progress numbers remain mock data and the findings above remain an evidence-based implementation assessment.
