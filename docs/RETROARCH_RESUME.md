# RetroArch saved moments

> **Historical / superseded product semantics — #49, accepted 2026-09-19.** This entire document preserves the legacy implementation contract and its dated tests/device evidence. The scanner/entry-state implementation is now compiled only into its historical test executable; production no longer creates its worker or image provider. None of the state creation, enumeration, thumbnail or direct-resume behavior below is an active target or an invitation to add providers. [Ordinary-save lifecycle and migration](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit) replaces it with normal startup, clean exit capture and save confirmation. Preserve ordinary saves, histories and independent images; retire only verified TrainerOS-owned obsolete artifacts safely. The bounded maintenance tool can reversibly retire only fully recognized owned sessions; see [current exit migration](ADVENTURE_EXIT.md#ordinary-save-decoupling-and-legacy-retirement). Unknown files, ordinary saves and independent media stay protected.

The first filesystem provider supports GBA content through mGBA and the verified RetroArch entry-state protocol. Other integrations retain normal launch. This is state discovery and restoration, not game-save parsing: TrainerOS does not infer badges, party, location or completion from these files.

## Ownership and storage

The provider reads the configured native state directory and TrainerOS-managed moment directories. Each supported launch receives a fresh directory under `resumeDirectory/<Adventure identity hash>/<session UUID>`. RetroArch saves its exit state and optional thumbnail there. Existing manual/automatic states are never reused as output destinations. In-game SRAM remains at RetroArch's existing save-file location.

A direct resume reads a bounded copy of the selected state, checks its SHA-256 and creates an exclusive `.state0.entry` file in the new session directory. All later manual/exit states belong to that new directory. Moment directories are durable external save data, not a disposable application cache. There is no automatic pruning in this increment; cancelled/failed preparation can leave an unused directory for later maintenance.

## Discovery and identity

`ResumeLibraryRepository` composes provider snapshots with the existing library. The provider uses a worker thread, coalesces requests and discards superseded observations. It scans up to 16 requested/recent Adventures, chooses the 128 newest managed session folders among at most 4096 directory entries, and visits at most 128 matching filenames per folder. It fingerprints the 12 newest states per Adventure. The published snapshot holds at most 192 cards. It never recursively walks the ROM collection. Opening a Worlds detail requests that Adventure; library load and launch-history updates request Home/recent Adventures.

Card IDs combine the owned Adventure ID with the adapter-relative source identity. Source revisions are SHA-256 hashes of the state bytes, including same-size/timestamp replacements. Integration revisions cover the content, core, runtime, main config, optional core/game options and overrides, configured BIOS and adjacent patch files. Current compatibility is tested again at launch; these hashes do not claim to identify the core that originally produced an imported external state.

Save time comes from the source modification time; observation time is separate. Missing/unchecked old cards remain explicit. PNG thumbnails are optional, size-limited observations coupled to the state revision and image hash. QML receives only an opaque cached-image key. A picture is never evidence of gameplay progress or successful restoration.

## Launch boundary

Generic process preparation runs off the GUI thread after the navigation checkpoint. The attached opening panel accepts B to cancel; cancellation prevents late worker results from starting a child. Normal navigation returns after failure/cancellation.

Before direct resume, an isolated RetroArch process loads the immutable entry copy with the selected core/content for 60 frames. It uses separate SRAM/config paths, disables external achievements and history, and has a 15-second limit. Validation requires the real state-load callback path, a normal process exit and no state-loading error. State/integration fingerprints are checked again afterward. The visible process also watches for entry-state errors and returns to TrainerOS on failure.

This extra check is necessary because RetroArch's entry loader reports success when it **queues** the load, before the core deserializes it. An invalid state can otherwise start the game from the beginning while RetroArch returns exit code zero. The adapter uses `--appendconfig` and `--entryslot`; the deprecated `--savestate` file argument did not preserve the requested path on the tested build.

The verified baseline is RetroArch 1.22.2, Git `69a4f0ea1e`, on the Flip's installed ARM64 Flatpak. Primary implementation references: [entry-state loading](https://github.com/libretro/RetroArch/blob/69a4f0ea1e/command.c), [state callback](https://github.com/libretro/RetroArch/blob/69a4f0ea1e/tasks/task_save.c), [path/slot handling](https://github.com/libretro/RetroArch/blob/69a4f0ea1e/runloop.c). No upstream source is bundled.

## Setup and limits

The machine-specific `integrations/retroarch.json` retains version 1. Real moments require explicit `resumeProtocol: "mgba-entry-v1"`, an absolute `resumeDirectory` on durable storage, and `runtimeFile` pointing to the installed RetroArch executable (inside the Flatpak deployment for Flatpak installations). These fields are opt-in deployment data, never bundled device paths.

Hardcore achievements, netplay startup and unverified config overrides disable exact resume. Unsupported/incomplete setups preserve ordinary launch. Config includes are not interpreted. Entry-state copies, arbitrary nested directories, compressed ROM archives and other cores are not advertised as supported moments. Source files are limited to 16 MiB and GBA content to 64 MiB; unavailable/oversized data cannot silently fall back to a different state.

## Acceptance

- [x] Byte replacement, missing sources, changed content/core and unknown overrides have explicit outcomes.
- [x] Source timestamps remain stable across observations; thumbnails remain optional.
- [x] Normal launches use independent state destinations and retain native SRAM configuration.
- [x] Preparation errors/cancellation do not start a late child; adapter output errors stop only the owned child.
- [x] Validation configs contain no duplicate settings, disable exit-state/thumbnail writes, and validation-only folders cannot become gameplay cards.
- [x] Full Linux/ARM regression and real controller launch/exit/select/resume validation for this increment.
- [x] Durable installation verified by a fresh SHA-256 read and SQLite integrity check. Commit/push and current-commit CI results are recorded in the delivery report.

The 2026-09-13 Flip check used FireRed with mGBA: exit created a separate state and PNG, Y/A selected without launching, Home A restored the same in-game help screen, B cancelled preparation without a late child, and a separate invalid fixture returned an error without entering gameplay. Original state hashes remained intact and selection survived a shell restart. The invalid fixture was removed afterward. The corruption test also caught RetroArch retaining the first duplicate config value; the fix was rechecked with an unchanged gameplay-state/session count. All 20 checks passed on Windows, Ubuntu and ARM64, followed by focused adapter/process and thumbnail rendering checks for final refinements. Private game data/captures stay outside Git.
