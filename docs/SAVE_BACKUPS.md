# Pokémon Center save backups

Target services and transaction rules: [Center](EXPANSION_42_62.md#pokémon-center-and-practical-party), [exact-save providers](EXPANSION_42_62.md#exact-save-providers-and-research), [lifecycle/migration](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit). The GBA tests and pending DS gates below remain valid for their tested versions.

**Implemented backup entry:** Pokémon Center is currently a service inside Start, not another primary page. **Planned #43/#44:** Center becomes the first-class L2/R2 Pokédex companion, with practical Party/Storage and these protected ordinary-save services. The legacy entry/controls below remain evidence until migration. Normal Center Y will select the Adventure; map backup actions explicitly while preserving modal input priority. It backs up **GBA in-game save RAM through the verified RetroArch/mGBA setup**, with an additional opt-in resolver for DS cartridge saves through melonDS. The existing saved-moment feature is historical and superseded by #49; it is not part of the target backup service. Emulator states, BIOS, ROMs and game-specific progress interpretation are outside these backup formats.

## Controller flow

Start → Pokémon Center opens linked Adventures, initially selecting the Adventure on Home where possible. X searches names without requiring accents/punctuation; arrows browse and left/right jump eight entries. A opens the selected Adventure's save shelf. Unsupported setups are visible and explain their limitation.

On the shelf, Y creates a manual copy, X checks the current save and copies again, and A opens the selected copy's restore confirmation. B returns to the list. The restore panel names the Adventure and copy date: A explicitly accepts replacement, B cancels. Start may overlay it; L1/R1 discard the confirmation and keep their global section-switching role. Missing, empty, unreadable and damaged records retain reachable focus/actions.

**Legacy restore behavior pending #49:** a restore clears a pinned Home moment but retains older state artifacts. The target has no state selection; restore refreshes ordinary-save revision and shared Dex/Center/Journey projections while preserving historical records. Removing legacy state dependencies must not weaken restore protection.

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
