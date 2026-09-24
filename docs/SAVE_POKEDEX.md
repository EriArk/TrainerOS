# Selected-save Pokédex progression

**2026-09-24 follow-up:** the owner removed manual journal functionality.
The earlier verification screenshots below precede that removal; current
installed checks confirm no journal line, editor or Select legend remains.

## First delivered route — English Emerald, 2026-09-24

The combined Pokédex now shows the shared current Pokémon Adventure's verified
ordinary-save species records as its primary Seen/Caught status and filters.
The header identifies the Adventure and last in-game save, with National Seen
and Caught totals. Per-species status has its own colored strip beside the
reference stats. Species beyond the game's 386-entry range are outside its
coverage, never automatically marked unseen or uncaught.

The owner retired the manual journal on 2026-09-24: no manual Seen/Caught,
notes, editor or Select shortcut remains. Personal favorites still use A.
Legacy journal rows remain inert for database compatibility. Save observations
never invent individuals or award achievements. Forms remain reference/art
selections: flags prove species registration, not ownership of every form.
Reference stats and modern type/form facts are not individual game stats.

The existing exact English Emerald SHA-256 and mGBA ordinary-save route gate
this capability. FireRed's established badge/count observations remain intact,
but do not imply per-species or Party support. Unsupported/missing observations
leave current progression unknown; favorites remain independent.
Ephemeral development compositions without a save provider retain their
explicit sample presentation.

## Format evidence and refresh

The reader reuses [complete Gen III save-slot validation](GAME_PROGRESS.md).
Emerald's National caught flags are at SaveBlock2 `0x28`; Seen flags are at
SaveBlock2 `0x5c`, SaveBlock1 `0x988` and `0x3b24`. Numbers 1–386 map to flag
indices 0–385. All three Seen copies must agree; Caught requires Seen. An
inconsistent record rejects the whole Dex projection, without zeroing individual
flags, mutating source bytes or disabling independently valid Party/badge data.

Evidence is the pinned pret revision
`5eff78649e7170a877b961ef0b3da13b81a16038`:
[save structures](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/global.h)
and [GetSetPokedexFlag](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokedex.c#L4052).
The game may repair inconsistent flags; TrainerOS only reads and declines the
inconsistent projection. No game implementation or save fixtures are distributed.

Worker refresh follows the shared Pokémon context and entering Home/Dex/Center,
with existing launch/backup invalidation. A checking state temporarily clears
the displayed current records. A failed read can reuse the last complete
in-memory Dex snapshot only after the resolver revalidates the same content and
save context; the header explicitly says Last verified save / refresh unavailable.
Changing Adventure, Trainer/context, missing saves, unsupported builds or an
unverifiable source clears it. A successful older save replaces today's flags
rather than unioning discoveries. Nothing new is persisted in SQLite.

## Acceptance and checks

- Synthetic complete saves cover National bit boundaries, Seen-only versus
  Caught, all 386 species, inconsistent mirrored bits and Caught-without-Seen.
  Existing checksum/slot fallback and stale worker checks remain applicable.
- Controller tests cover primary-save filters versus a conflicting manual
  journal, rollback, same-identity failed refresh, source/owner replacement,
  unsupported coverage, favorites/journal access and recovery from empty filters.
- Private full and early Emerald saves have respectively 386/386 and 0/0
  consistent Seen/Caught flags. Private saves and screenshots stay outside Git.
- Native build and full suite passed 42/42, including the private-save check.
- The ARM production binary was installed on Flip; SHA-256:
  `9348941a12a7eb809502cddc461325418e8dc4f2308d81b566173491f4515684`.
  On-device controller injection exercised the Dex, Caught filter and manual
  journal. Installed-build captures show separate save and journal records,
  with the six reference stats fitting in a three-column grid. Source/Trainer
  replacement is covered by automated tests, not a claimed physical-user trial.
- An isolated copy of the full save was loaded in Emerald on Flip. The game's
  National Seen 386 / Own 386 counters match TrainerOS exactly. Neither original
  save changed (both hashes verified); the personal database integrity check
  passed. No in-game save was requested during this check.

Further exact editions, broader field research, Journey/Champion history,
Playroom consumers and independently guarded writers remain in ROADMAP row 7.
