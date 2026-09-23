# Trainer ownership foundation — 2026-09-23

The first P2 increment delivers schema 8 and owner-filtered local repositories.
It does **not** enable multiple profiles, PIN login or Switch Player. The existing
single-profile application uses the migrated owner throughout its real Home,
Pokédex, Trainer and Hall consumers.

## Delivered boundary

- `trainer_owners` holds stable ownership identities; `local_owner` selects the
  sole currently supported owner. An existing profile retains its exact ID,
  creation time and fields. Startup rejects a profile/owner mismatch.
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

The worker owner cannot switch during this increment. Only the initial unnamed
owner can become the first profile, within the serialized worker transaction.
There is no API here for replacing an active owner or opening a second one.
Two-owner SQL tests establish repository isolation, not authenticated switching
or protection from maintenance/root access.

## Migration and recovery

The 7→8 DDL, copied records, ownership assignment and version change commit in
one transaction. A failure rolls back the migration and exposes existing startup
recovery. Older schema migrations remain supported through actual legacy-schema
fixtures. Unknown navigation payloads are copied byte-for-byte, including scopes
that this build does not open. Invalid optional exit pictures are preserved in
storage; the existing reader may still withhold them.

Before device installation, retain a SQLite backup and the matching schema-7
binary while the shell is stopped. Rollback requires that **pair**, with the shell
closed; never lower `user_version` or place the old binary over a schema-8 store.
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

## Next P2 increment

Bind the prepared registration/chooser/PIN screens to real profile storage,
active-owner session lifetime and canonical per-Trainer RA identity/cache.
Before enabling Switch Player, drain submitted work, refuse an active Adventure
or unsafe service, discard transient editors and invalidate late provider
results. Reconstruct all owner views and restore that owner's context on restart.
Prove two real profiles, failed creation/unlock, account isolation and physical
return/recovery. Do not interpret this foundation as those gates passing.
