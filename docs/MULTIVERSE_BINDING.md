# Real Multiverse library binding

## Scope

P3/P4 replaces production's empty presentation with shared, persistent game
registrations. Pokemon retains region-first Worlds; Multiverse groups separate
records by platform without inventing World relationships. The copied
[133-title collection](MULTIVERSE_COLLECTION.md) supplies files, not play history.

Schema 11 adds explicit `pokemon` / `multiverse` Adventure domains and nullable
World membership for Multiverse only. Legacy registrations retain Pokemon
classification, IDs, revisions, paths, relationships and history. Classification
is not guessed from a filename, and ordinary edits cannot switch a record's domain.
The migration uses SQLite's documented
[create/copy/drop/rename procedure](https://www.sqlite.org/lang_altertable.html#making_other_kinds_of_table_schema_changes),
preserving dependent indexes/triggers and checking foreign keys before commit.
No ROM, save, profile or history deletion belongs to this migration.

## Browsing and selection

The platform registry includes cartridge, disc, arcade and computer categories
with candidate Armada/Libretro routes; registry membership does not establish a
working emulator. Its starting references are [Armada emulators](https://armadaos.dev/emulation/emulators/)
and [Libretro cores](https://docs.libretro.com/guides/core-list/). Remaining
conditional systems still require device/runtime review, not invented readiness.

Production shows only systems with available registered files. Availability is
read on the store worker at startup and refreshed when opening the Multiverse
face or selector; searches do not determine system visibility. A missing file
keeps its registration, selection and history. Launch preflight checks the actual
file again. Unknown imported system IDs remain visible by their ID if populated.

The controller scrolls a bounded three-column system grid and bounded title
lists. X searches, local Y filters and B unwinds detail/list/system routes.
L1/R1 and L2/R2 retain their established page/face meanings.

Choosing a detail card or Y-drawer card selects for Home without launching.
Unobstructed Home A starts that selection through the existing adapter router;
unconfigured records open the existing file/setup editor. Home X, independent
Pokemon/Multiverse selections, paired-face choice and useful browser state are
stored in the existing per-Trainer navigation. An explicit unavailable choice
never silently falls back to a different game. Before a choice, the latest actual
launch in that domain supplies Home. Recent-history limits apply per domain.

The same process lifecycle, owner-scoped play history and guarded exit are used.
Exit images now accept both domains and retain owner/registration/content checks;
the existing bounded cartridge-image limits remain, so disc media is not promised.
Pokemon Home, regional statistics, Center and manual Pokemon Hall pickers exclude
Multiverse records. This does not add new save parsers or shared-save isolation
claims for runtimes whose per-Trainer save routes remain pending.

## Maintenance import

`trainer_library_import DATA_DIRECTORY MANIFEST.json` is built with the native
verification tools. It uses the same locked `LocalStateStore`, worker validation
and installed adapter configuration as the shell. Stop the shell first; retain a
paired binary/database backup before migration/import.

The bounded JSON manifest has `version: 1` and `entries` with explicit `id`,
`title`, `platform`, absolute `path` and optional `description`. IDs are generated
once by the curator and kept in the private import manifest. Changing a filename
is not an instruction to create a new Adventure or merge histories.

The tool validates readable files and all existing ID/file collisions before
inserting records. Repeating the same manifest preserves existing rows, names,
configuration and history. Each successful registration commits independently;
a later failure reports the applied count and an idempotent retry can continue.
There is no automatic deletion or replacement of other registrations. It runs no
game and writes no ROM/save/BIOS bytes. Private manifests stay outside Git.

The optional `--configure-existing` mode subsequently connects newly installed
runtimes to matching unconfigured records while preserving owner edits and
already configured routes. See [cartridge runtime delivery](MULTIVERSE_RUNTIMES.md).

Installed RetroArch cartridge routes, melonDS and Dolphin are selected only
through their adapter setup checks. The initial binding included raw SNES
SFC/SMC; the follow-up cartridge increment expands the routes listed below.
Other copied platforms can be browsed/selected while their setup gaps remain explicit.

## Acceptance

Verify legacy database preservation and foreign keys, repeat import/collision and
live-store lock refusal, domain-separated selection/history/media, reopen and
profile isolation, missing-content visibility, search independence and controlled
launch. Inspect installed handheld screenshots and inject real controller events
for grid scrolling, detail, Y selection, fixed A and L1/R1/Back. Windows renders
alone do not close the device delivery gate.

The initial collection import resolved 38 registrations to existing configurations:
12 GBA and 10 N64 through RetroArch, 8 DS through melonDS, and 8 GameCube/Wii
through Dolphin. At that point, 95 remained unconfigured, including SNES ZIPs;
the first SNES route only supported raw SFC/SMC.
The subsequent [cartridge runtime increment](MULTIVERSE_RUNTIMES.md)
adds those ZIPs and four more platform routes: 80 records are now configured,
53 remain unconfigured. Configuration counts are not individual-title launch acceptance.

Remaining P4 work: generic on-device creation of Multiverse registrations,
missing viable runtimes/BIOS configuration, archive routes, runtime-by-runtime
launch/input/ordinary-save/exit verification, richer game media and the previously
accepted World presentation. Copying or registering a file never checks all those
boxes. P9 retains unresolved runtime/controller gaps; P8 retains new save research.

## Delivered verification - 2026-09-23

- All 39 Windows tests covered successfully (the full run exposed a stale
  schema-9 fixture; its corrected focused rerun passed). ARM64: 43/43 passed.
  Final copy-only QML polish passed both affected Windows rendered SDL scenarios
  and was rebuilt and inspected on the handheld.
- Rehearsed schema 10 -> 11 on a copy of Flip's actual database; compared every
  original table/column and play-session rowid. All 686 Pokemon registrations,
  profiles, relationships and personal records survived unchanged. Imported 133
  records twice without duplicates; foreign-key and quick checks passed.
- Repeated those preservation checks during real installation with a paired
  binary/database backup. Final production binary SHA-256:
  `d991bafd0bec73db72c76111e03262b143c7cd689275b7823de21e1fbb1f3170`.
- Installed-session controller events exercised the 22-system grid through its
  final row, detail/Back, L1/R1, L2/R2, independent Home X and selection-only Y/A.
  Advance Wars launched with fixed Home A and returned through the existing
  guarded exit, storing a real Multiverse session and clean screenshot. Pokemon
  Emerald remained the Pokemon Home choice with its previous history/media.
- Exit was requested remotely through the existing InputPlumber interception
  path and its owned overlay confirmed programmatically. This is not a fresh
  physical Home-button acceptance or a full in-game save/load test. No sleep test.
- Captures came from the installed Gamescope session on Flip, not host previews.
  The final restart also preserves selections and the new exit picture.
