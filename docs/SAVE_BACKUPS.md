# Pokémon Center save backups

**Emerald healing follow-up, 2026-09-24:** [the first exact-build treatment](EMERALD_HEALING.md)
uses this same worker, resolver, exclusion lock and protection/restore mechanism.
Its protection bundles carry optional `reason: "healing"`, displayed as
**Before healing**. Old version-1 bundles remain compatible; no database migration.

Target services and transaction rules: [Center](EXPANSION_42_62.md#pokémon-center-and-practical-party), [exact-save providers](EXPANSION_42_62.md#exact-save-providers-and-research), [lifecycle/migration](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit). The GBA tests and pending DS gates below remain valid for their tested versions.

**Implemented entry (2026-09-19):** Center is the L2/R2 companion of Pokedex. Start opens this same face at the shared selected Adventure. [Shared selection and remaining scope](SHARED_ADVENTURE.md). Protected GBA/mGBA backups and the opt-in melonDS resolver retain their existing format boundaries. Party/Storage's [P1 presentation](CENTER_PARTY_UI.md) now consumes [verified Emerald records](EMERALD_PARTY.md); its subsequent healing route is described above. Other editions retain their P8 gates. Emulator states are not ordinary-save backups.

## Controller flow

Y chooses the Adventure using the shared drawer. A commits the choice without launching or leaving Center; B cancels. The shelf follows that registration, including honest unavailable/unsupported states. It never substitutes another game's save. There is no independent Adventure picker in the normal Center route.

Center initially shows Party. X switches Party/Storage; Select opens this save shelf. Inside the shelf, Select creates a manual copy, X refreshes, and A opens restore confirmation. B on the idle shelf returns to Party/Storage. B cancels confirmation; L2/R2 switches between Center and Pokedex. The confirmation names the Adventure and copy date: A accepts replacement, B cancels. Y and L2/R2 cannot bypass it. Start may overlay it; L1/R1 discards it and keeps global navigation. A background operation retains its original target even if the shared choice changes; its result cannot populate the new shelf.

Restoration still protects current bytes first. Production state-resume selection is retired; shared choice and history survive restore. Current-save Dex/Journey projections await provider phases.

## Storage and safety

`SaveBackupService` is the asynchronous boundary. `LocalSaveBackupService` resolves an adapter-owned save target and performs filesystem operations on its worker. QML receives titles/status/opaque copy identities, never file paths or emulator commands. The shell prevents owned Adventure launches during operations; normal exit drains the service and the resulting navigation write. Failed operations cancel a pending exit so the error remains reviewable.

Copies live under `AppLocalDataLocation/backups/<sha256(adventure-id)>`, which is internal storage in the deployed Flip configuration. Each `.tosbackup` file is a versioned JSON bundle containing the Adventure identity/title, exact ROM-content SHA-256, UTC copy time, presence/protection flags, byte count, save SHA-256 and base64 save bytes. These are private user files, excluded from Git. Files use owner-only permissions where supported. The initial limits are 512 KiB per save, 768 KiB per bundle and 512 copies per Adventure; full shelves require archiving older copies in maintenance mode. No automatic deletion is performed.

Creation reads a bounded snapshot, checks the source again, writes a unique copy through `QSaveFile` with direct-write fallback disabled, synchronizes it and its parent directories on Linux, and verifies the stored bytes. Existing copies and external saves are never overwritten during backup creation.

Restore re-resolves the current installation/content/save path, checks the previously displayed source token and exact selected-copy revision, and validates all copied bytes against the bundle checksum. A damaged copy, changed ROM, changed current save or ambiguous setup stops the operation. A separate protection bundle records the current save **before** replacement; failure to protect it cancels the restore. A missing or empty prior save is recorded honestly, and those protection records cannot be used as a silent delete/empty operation.

The current target is checked again immediately before atomic replacement. The replacement is flushed, committed and verified; the protection copy remains available to undo the restore. An error after replacement explicitly says the replacement happened, rather than claiming the original was untouched. Directory/file symlinks at the backup shelf and save filename are rejected; canonical parent identities bind source tokens. Missing save directories are not recreated speculatively, avoiding writes into an absent removable mount. A lock serializes operations across TrainerOS instances. The adapter also refuses operations while any RetroArch process is running. This does not coordinate arbitrary third-party editors: external save writers must remain closed while using the service.

## Verified adapter boundary

Installation data explicitly opts in with `backupProtocol: "mgba-sram-v1"` and the verified `runtimeFile`. The ordinary-save resolver is independent of `resumeProtocol` and `resumeDirectory`. Its context fingerprint uses the SRAM protocol plus the same runtime/core/config/content evidence; changing the old state directory no longer changes it. Existing version-1 backup bundles remain valid because they bind to Adventure/content SHA-256, while stale in-flight inspection tokens must be refreshed. The source resolver uses the current GBA registration, configured save directory and explicit sorting flags. It rejects duplicate/include-based configuration, active unverified overrides, unavailable content/runtime/core context and unsupported game/core combinations.

The directory calculation follows [RetroArch 1.22.2's save redirection](https://github.com/libretro/RetroArch/blob/v1.22.2/runloop.c): content-folder grouping precedes core-name grouping, with the content basename and `.srm` extension. The first supported GBA path uses mGBA's save RAM; its [libretro memory interface](https://github.com/mgba-emu/mgba/blob/0.10.5/src/platform/libretro/libretro.c) distinguishes the separate GB RTC path, which is not enabled here. Replacement uses [Qt's atomic save mechanism](https://doc.qt.io/qt-6/qsavefile.html) without its unsafe direct-write fallback. Filesystem synchronization is implemented at the Linux platform layer; Windows is a development/test host.

## Acceptance

### DS cartridge saves

The melonDS profile uses `backupProtocol: "melonds-sav-v1"` and an absolute `configFile` pointing to the installed melonDS 1.1 configuration. This is independent of exact Continue support. The resolver requires an explicit absolute `Instance0.SaveFilePath`, `Savestate.RelocSRAM = false`, DS mode (`Emu.ConsoleType = 0`) and a single configured instance. It rejects ambiguous/duplicate tables or keys, unsupported TOML spellings and multiline values. It never infers a NAND save or an archive member. A running melonDS process blocks inspection/replacement.

The file is the cartridge filename without its last extension, followed by `.sav`, inside the configured directory. This follows the installed version's [asset path and cartridge save handling](https://github.com/melonDS-emu/melonDS/blob/1.1/src/frontend/qt_sdl/EmuInstance.cpp). Relocated SRAM needs separate handling because loading a state can change its filename. ROMs are hashed up to 512 MiB; runtime/configuration identity binds each operation. The existing 512 KiB save limit and protection-before-replacement workflow also apply to DS; larger saves remain unsupported.

Native fixtures verify the exact dotted filename, configuration rejection, disabled opt-in, an externally running emulator and copy/restore with protection. Device profile activation and real cartridge save-path confirmation remain pending for this increment.

### Shared backup workflow

- Copy/restore/undo preserve exact bytes and create independent protection copies.
- Changed source tokens, modified/corrupt copies, different ROM content, failed protection and concurrent operations cannot replace a save.
- Missing/empty saves can be recovered while missing directories and symlinks remain conservative failures.
- The worker stays responsive to controller/UI events; exit waits for completion and failures retain the application.
- Adapter tests check sorted paths, opt-in, duplicate configuration, unsupported cores and an externally running RetroArch process.
- SDL scenarios cover the system menu, empty shelf, backup creation, explicit confirmation/cancel, restore/undo, Home selection reset, background completion and landscape focus/layout.

Device validation on 2026-09-13 used the production ARM build on Flip 2 with real controller events: search a temporary linked GBA Adventure, create a 128 KiB copy, cancel/reopen the confirmation, overlay/close Start, restore exact bytes, then restore the protection copy. Both replacements and all three private bundles passed independent byte/checksum verification. The temporary registration/content/save were removed afterward; the original 686-Adventure collection and navigation were retained. Commercial saves were not used as restore-test fixtures.

The full suite passed 24/24 on Windows, Ubuntu 24.04 and the Flip's ARM64 build environment; rendered scenarios cover 960×540, 1920×1080 and a letterboxed landscape viewport. GitHub Actions did not execute its jobs because the account billing gate rejected them; this is distinct from the completed local/server/device checks.

Additional ordinary-save formats require their own device validation. State-backup/resume expansion is superseded by #49. Existing Adventure relinking and integration setup remain in Manage Adventures.

## Per-Trainer save follow-up - 2026-09-23

### Protected Emerald services — 2026-09-24

Healing and ordinary Poké Mart purchases reuse the same storage worker and
protected replacement path. Automatic copies are identified as **Before healing**
or **Before purchase** and use the existing restore/undo controls. Browsing a shop
does not write the game save. See [healing](EMERALD_HEALING.md) and
[purchases](EMERALD_SHOPS.md) for the verified exact-build boundaries.

### Owner-scoped save route

The first [GBA/mGBA per-Trainer ordinary-save route](TRAINER_SAVES.md) binds
launch, existing progress observations and backup shelves to the active Trainer.
Original legacy saves/backups remain in place; new Trainers start fresh. Other
emulator save routes remain shared until verified separately. This supersedes
earlier statements that all external saves are still shared.
