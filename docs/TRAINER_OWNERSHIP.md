# Trainer ownership foundation — 2026-09-23

The P2 foundation introduced owner-filtered repositories in schema 8. Schema 9
now binds real profile creation, selection and Switch Player to that foundation.
Up to eight Trainers share the installed library; personal journals, history,
Hall, navigation and TrainerOS RetroAchievements accounts remain separate.
Schema 10 adds [optional PINs, protected startup and family-code recovery](TRAINER_ACCESS.md).
The implementation/evidence below records the earlier schema-8/9 increments;
the access document supersedes their former unguarded-startup boundary.

## Delivered boundary

- `trainer_owners` holds stable ownership identities; `local_owner` selects the
  active owner. An existing profile retains its exact ID,
  creation time and fields. Startup rejects a missing selected profile.
- A fresh or previously unnamed installation receives one durable unnamed owner,
  without inventing a visible profile. Creating its first profile atomically
  adopts that profile ID through cascading foreign keys. A failed profile write
  rolls back the adoption too; pre-registration marks and navigation survive.
- Favorites, manual journal and every navigation scope use composite
  `(trainer_id, entry_id/scope)` keys. Play sessions and Hall memories retain
  global IDs and gain required ownership. Session rowids remain intact because
  they define launch order. Foreign keys plus non-null write triggers protect
  the added columns on those populated tables.
- SQL reads, totals, inserts and revision-checked updates use an explicit owner.
  Completing another owner's session fails. Exit-media reads and writes require
  matching session ownership as well as the existing build/content checks.
- Library registrations, World relationships, installation configuration and
  device preferences remain shared. No game, save, artwork or account file is
  copied or changed by this migration. External saves remain shared legacy
  sources; database ownership is not separate game progress.

## Real profile switching

Settings → Trainer → Trainers and Start → Power → Switch Player open the same
chooser. Registration uses the controller keyboard, original emblem and optional
starter favorite; review commits a real profile. The live flow never exposes the
sample PIN. Eight profiles are the current bound; the focused chooser row stays
visible with its outline. B cancels and L1/R1 retains page navigation.

Creation is transactional and does not replace the active owner on failure.
Switching refuses an active Adventure, storage/service operation or busy account
provider. SessionState flushes the current navigation and drains submitted writes,
then commits only the next owner ID. The old store is fenced against further
writes. The application destroys the entire old composition and reopens SQLite,
controllers, providers and QML in the same process. No old editor, account callback
or owner projection is reused. A failed activation remains in the existing session
with an error; a failure opening the selected store uses normal startup recovery.

The legacy account/cache directory stays bound to its original owner through
`legacy_account_owner`. Additional profiles use `trainers/<SHA-256 of owner ID>`
under the private data directory. Providers bind once after successful storage
startup; there is no fallback to another Trainer's credentials/cache. Switching
back reopens the same original account. This scopes TrainerOS login/read/cache;
emulator achievement earning/configuration remains a separate P6 gate.

External games and ordinary saves are shared. Creating a Trainer does not copy
saves, import someone else's play history or seed journal marks. Reading the same
ordinary save may show the same in-game progress under either Trainer. Per-domain
Multiverse selection and independently private save namespaces remain planned.

## Migration and recovery

The 7→8 DDL, copied records, ownership assignment and version change commit in
one transaction. A failure rolls back the migration and exposes existing startup
recovery. Older schema migrations remain supported through actual legacy-schema
fixtures. Unknown navigation payloads are copied byte-for-byte, including scopes
that this build does not open. Invalid optional exit pictures are preserved in
storage; the existing reader may still withhold them.

The 8→9 migration replaces the singleton profile table with keyed profiles and
records the original account owner in the same transaction. Existing identity,
creation time and profile fields remain unchanged. No private account file moves.

Before each device installation, retain a SQLite backup and the matching previous
binary while the shell is stopped. Rollback requires that **pair**, with the shell
closed; never lower `user_version` or place an older binary over a newer store.
Keep any post-upgrade database separately before rollback so newer writes are
not lost. This is an operational procedure, not an automatic rollback feature.

`trainer_store_migration_probe --migration-copy <directory>` is a testing-only
executable for an explicitly prepared private database copy. It opens the same
store without GUI, controllers, accounts or platform services, then exits. Its
startup recovery can mark unfinished sessions interrupted, as normal startup
does. Never point a validation run at the live data directory.

## Verification

`ownership` tests cover populated schema-7 migration/reopen, a failure after
partial DDL with rollback/retry, unnamed-owner adoption and failed adoption,
two-owner journal/Hall/session isolation and shared-library preservation.
Existing persistence, exit-media, process and rendered-controller tests cover
the unchanged consumers. Windows **37/37** and ARM64 **41/41** passed. The private
Flip schema-7 database was migrated independently by Windows and native ARM
probes: every legacy column was compared, including 686 Adventures, 36 play
sessions and 2 exit pictures. All rows, payloads and launch-order rowids were
preserved, with foreign-key and integrity checks passing. The non-testing ARM64
build was installed with a paired binary/database backup and a second dry run
against the freshly stopped-shell backup. The real schema-8 store reopened after
a shell restart. Installed-binary hash, profile and library checks passed;
InputPlumber navigation and actual Gamescope captures confirmed Home, its exit
picture, Choose Adventure history and the Trainer overview. This is a shell
restart check, not a power-loss, reboot or physical PIN/switching proof.

## Current profile-binding verification

`trainer_profiles` covers real creation/activation/reopen, independent favorites
and navigation, immutable account binding without credential fallback, the
eight-profile bound, duplicate creation failure, invalid activation, the post-stage
write fence, unsafe-switch refusal, navigation drain, live controller flow and
schema-8 migration failure after DDL followed by rollback/retry. Existing ownership
tests retain journal/Hall/session/media isolation and legacy schema coverage.

Windows **38/38** and ARM64 **42/42** checks passed, including rendered SDL
registration, focus, Back and the eight-profile scrolled chooser. Windows and
native ARM probes independently migrated the real schema-8 copy; every legacy
column, all 686 Adventures, 36 sessions and 2 exit pictures survived unchanged.
The installed non-testing ARM build was verified against its SHA-256, with a
paired schema-8 database/binary backup and a second stopped-shell migration check.

InputPlumber controller events on the installed Flip created ASH through the
on-screen keyboard and review, switched to its empty Home/account, and returned
to the original Trainer's Home/media. Switching retained the same process while
reconstructing the composition. Settings and Power both reached the chooser;
Back restored Power's Switch Player row. ASH then survived a separate normal
shell restart and the original owner was restored. Its profile/account fingerprint,
legacy history/media, shared library and foreign-key/integrity checks passed.
Actual 1920×1080 Gamescope captures were inspected for registration, chooser,
new Home/account and restored Home. This is automated controller input on real
hardware; human button comfort, power-loss and PIN/security proof remain open.

## Next P2 increment

Complete explicit profile deletion and the shared-context/domain ownership audit
before P3/P4 real Multiverse library binding. Preserve the delivered
[PIN/startup/family recovery](TRAINER_ACCESS.md), controller cancellation,
lossless owner data and shared external saves.
