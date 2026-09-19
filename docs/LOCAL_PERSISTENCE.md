# Local persistence foundation

**Target migrations pending — #9/#20/#42/#49:** schema/version evidence below remains current implementation history. Legacy stored resume selection is not the target model. Add shared per-Trainer Adventure/domain context, source-aware observations and safe state retirement through explicit lossless migrations; preserve ordinary saves, history, manual records and independent media. Do not claim these fields/migrations already shipped. [Domain contract](DATA_MODEL.md), [target acceptance](EXPANSION_42_62.md).

Schema 3 adds Adventure platform/catalogue/variant metadata through a transactional 2→3 migration; existing IDs, profile, favorites, configuration and file references are preserved. The bundled collection checklist is not persisted as owned data. See [collection catalogue](COLLECTION_CATALOGUE.md) for attachment and migration acceptance.

Schema 4 adds observed play sessions through a transactional 3→4 migration without seeding historical launches. Interrupted sessions keep unknown end time/duration. `SqlitePlayHistory` owns SQL mapping and the existing worker owns scheduling. See [Home and play history](HOME_AND_HISTORY.md).

Normal runs use SQLite for the personal Worlds library, one local Trainer profile, Pokédex favorites, browsing state, shell preferences and observed launch history. `--ephemeral` uses in-memory fixtures. Process scenarios require isolated test directories. No ROM/save parsing, account connection or system-session mutation is introduced by this storage layer.

## Ownership and schema

`LocalStateStore` implements the Library, Trainer, Pokédex progress and Preferences repositories. Getters read committed projections on the UI thread without I/O. One worker exclusively owns its Qt SQL connection; startup, schema checks, migrations, reads and writes run there. `SqliteLibrary` keeps library/schema mapping separate from worker scheduling.

Schema 5 adds the [local Hall of Fame archive](HALL_OF_FAME.md), with explicit manual memories and revision-checked editing. Sample archive entries are never migrated.

Schema 6 adds the [manual Pokédex field journal](POKEDEX.md), preserving all existing favorites and leaving Seen/Caught unknown until the trainer records them.

Schema 7 adds optional [clean exit media](ADVENTURE_EXIT.md#durable-exit-media-and-ordinary-home-selection) in a transactional 6→7 migration. Existing library, ordinary saves, profile, history and legacy files are preserved. Returning to a schema-6 binary requires restoring the paired pre-upgrade database backup while the shell is closed; do not discard newer personal writes or just lower `user_version`.

SQLite `user_version` is currently 7:

| Table | Data |
| --- | --- |
| `trainer_profile` | Single profile slot, stable ID, UTC creation time, name, emblem, featured Pokémon |
| `pokedex_favorites` | Favorite reference-entry IDs; absence means no mark |
| `shell_state` | Versioned JSON navigation, scoped to the current library source |
| `worlds` | Nine initial region names and user-created Worlds; no invented progress |
| `adventures` | Stable ID, primary World, edition/title/notes, external file reference, opaque adapter configuration, edit revision |
| `adventure_worlds` | Additional region relationships with foreign-key integrity |
| `preferences` | Color theme and reduced-motion flag |
| `play_sessions` | Identified Adventure process launches, UTC timestamps, optional monotonic duration and outcome |
| `exit_media` | Latest clean exit JPEG per Trainer/domain/Adventure, session FK, registration revision, exact ROM SHA-256, capture time/dimensions and image integrity hash |
| `hall_of_fame` | Manual/imported historical memories, optional date/time/team, notes, source and edit revision |
| `pokedex_records` | Nullable manual Seen/Caught, a field note and revision, keyed by stable species identity |

Migrations 0→1 and 1→2 run in transactions. The latter preserves profile/favorites and existing browsing scopes while adding an empty personal library and region reference names. Nonempty unversioned foreign databases, unsupported versions, unreadable schemas, broken foreign keys and failed integrity checks are rejected without replacing the file. Malformed optional navigation falls back to defaults; newer versions of the active browsing scope require a newer application and are preserved.

Favorites belong to the single local installation even before profile creation, separately from its one featured Pokémon. Multiple profiles are later work. No sample favorites, Seen/Caught, Home progress, World statuses, archive records, achievement unlocks or external game data are seeded. Pokédex Seen/Caught remains unknown until a manual journal record is saved. The offline reference and personal progress providers remain separate and replaceable.

## Storage and recovery

The app chooses `QStandardPaths::AppLocalDataLocation`, or the explicit `--data-dir` development override. The directory holds `traineros.sqlite3`, SQLite transaction files when needed, and an instance `QLockFile` while open. QML receives no database paths.

One instance owns each data directory. Lock failure, missing SQLite driver, inaccessible storage and corrupt/newer data show Retry and Exit controls. B exits startup recovery. There is no silent data replacement or memory-only fallback. For manual recovery, back up the directory while TrainerOS is closed; automatic backup/restore UI is not implemented.

SQLite uses full synchronization and atomic transactions/statements. Lock waits are bounded on the worker. Tests cover SQL failures caused by a competing writer; actual disk-full and target power-loss behavior have not been validated.

## Save and exit

Repository completion is tied to a QObject lifetime. Controllers display Saving and suppress duplicate writes while pending. Projections change only after SQL succeeds. Failed profile writes retain the saved profile and, while the form is open, its retryable draft. Failure after leaving the form produces a notice. Failed favorites retain the previous mark.

Save submits an operation. B or L1/R1 afterward may leave the form but do not undo the submitted write. Before Save, Cancel/B and page changes discard drafts. Drafts are never persisted. L1/R1 works during writes and can leave a browsing-state error; startup recovery is the exception because feature data is not loaded.

`SessionState` restores repositories before showing normal pages, preventing first-render overwrites. Its 300 ms debounce compares navigation snapshots, so keyboard keystrokes, menu changes and unchanged focus notifications do not trigger writes. System-menu exit and native-window close wait asynchronously for pending operations and the latest browsing snapshot. Failed explicit user writes cancel exit and preserve access to their error/draft.

A navigation-write error offers Retry or Keep browsing and pauses automatic retries. Later exit retries unsaved navigation. Exit-time failure also offers explicit Exit without browsing state, skipping only optional navigation while draining submitted profile/favorite writes. Forced termination can lose pending/uncommitted operations; this is not a backup system.

## Navigation

Version 1 stores a named page, Continue card selection, explicit Home Adventure/optional state selection, World/Adventure routes and IDs, applied Pokédex query/filters/order/entry, and Hall of Fame routes/record selections. Local focus is bounded and normalized against available content. Missing records fall back to a reachable row or recovery action. New optional Home keys retain the version-1 fallback behavior for older browsing snapshots.

Keyboard buffers, Trainer/Adventure drafts, unconfirmed picker choices, service panels, notices, system menus and the open Continue drawer are excluded. The drawer's selected card is retained. Normal runs use `user-library-v1`; the older `prototype-library-v1` scope is preserved separately and used only by sample-library persistence tests. Achievement cursors confer no unlock/progress state and populate no provider cache.

Adventure edits compare the loaded revision before committing, preventing a stale form from overwriting a newer record. Creating a custom World, saving its Adventure and updating additional relationships is one transaction. External files are checked as readable regular files on the worker at Save; no file content is read, copied, moved or deleted. Changing a file reference preserves the Adventure ID. Opaque adapter configuration is retained through editing but an unconfigured adapter advertises no launch/resume capability.

See [LIBRARY_AND_LAUNCH.md](LIBRARY_AND_LAUNCH.md) for controller flows and the separate test-process lifecycle boundary.

## Acceptance evidence

- Actual database close/reopen preserves profile identity, creation time, edits and favorites.
- Cancel never writes; another identity cannot replace the local Trainer.
- Locked writes preserve old values/drafts and allow retry while the UI processes timer events.
- Failed Save cannot close the app as if successful; navigation failure allows retry or explicit skip.
- Second-instance locks, unavailable directories and corrupt/foreign/newer files expose recovery and preserve original content.
- Invalid navigation references normalize to reachable focus; drafts/transient layers stay closed after restart.
- Separate rendered processes create data through SDL virtual-controller input, exit and reopen to verify persistence and actual QML focus.
- Captures cover 960×540, 1920×1080, letterboxing and controller-operated startup recovery.

Validation is on the Windows development host. Linux/ARM64 execution, physical controller mapping and power/crash behavior remain open; follow [FIRST_DEVICE_RUN.md](FIRST_DEVICE_RUN.md).
