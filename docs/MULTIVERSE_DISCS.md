# Multiverse: Mega-CD and Neo Geo CD

The next P4 runtime slice adds two explicit routes to the existing RetroArch
adapter. Mega-CD uses Genesis Plus GX; Neo Geo CD uses NeoCD. Both accept CUE
and CHD, with the registered platform selecting the core. Mega Drive cartridges
cannot accidentally acquire disc support through their shared core. M3U, GDI,
standalone ISO and disc swapping are outside this slice.

## Firmware and content checks

The machine-local `integrations/retroarch.json` accepts an optional
`discFirmware` object. Its `segacd` and `neogeocd` members map reviewed BIOS
filenames, relative to RetroArch's configured `system_directory`, to lowercase
SHA-256 digests. Installation must first compare the private files against the
core's documented firmware identities; hashing an arbitrary file does not
establish that it is a valid BIOS. No firmware or private manifest belongs in Git.

Mega-CD requires `bios_CD_U.bin`, `bios_CD_E.bin` and `bios_CD_J.bin`.
NeoCD entries live under `neocd/`. Each platform permits at most eight entries,
each at most 1 MiB, with readable nonempty files canonically contained in the
BIOS directory. Startup snapshots readiness for I/O-free capability queries.
The ordinary-launch worker verifies the files again before creating the child;
missing or changed firmware refuses launch with an actionable error.

CUE validation preserves the original track list and companions. The worker
checks a bounded 64 KiB list, at most 99 FILE references, at least one TRACK,
and readable nonempty companions inside that game's directory. It refuses
missing tracks and parent-directory/absolute references. CHD preflight checks
the container signature; full image decoding and track semantics belong to the
core. These checks do not claim to validate every sector or supported game.

## Launch, saves and return

Both routes use the existing ordinary-launch overrides, controlled process
lifecycle, play history and guarded exit. There is no automatic savestate load
or creation. Core/directory/title overrides use the actual library names,
including NeoCD, and conflicting active overrides block launch.

Emulator ordinary-save directories remain unchanged. These routes do not add
per-Trainer save isolation, save parsing, Center writes or backup support.
Those remain separately gated work. The exit-media repository still has its
existing cartridge-size/content-identity limits: large CHD pictures and complete
CUE companion identity need the pending disc-media work, not an increased
unbounded hash budget. Runtime readiness does not promise a Home picture.

## Primary references

- [Genesis Plus GX](https://docs.libretro.com/library/genesis_plus_gx/):
  supported disc formats, regional BIOS identities and ordinary BRAM options.
- [NeoCD](https://github.com/libretro/neocd_libretro/blob/master/README.md):
  CUE/CHD support, firmware discovery and documented BIOS identities.
- [NeoCD implementation](https://github.com/libretro/neocd_libretro/blob/master/src/libretro.cpp):
  actual library name/extensions and backup RAM lifecycle.

## Acceptance

Verify firmware replacement/removal, invalid paths, missing CUE companions,
platform/core/extension mismatch, cancellation and successful launch preparation.
Rehearse configuration and repeat import against a copy of the real database;
preserve every unrelated field and table. Keep paired binary/database/integration
configuration backups before installation. Verify one representative title per
runtime on Flip with controller input and guarded return, then retain title-level
gameplay/save/reload and physical-button checks as explicit remaining acceptance.

## Installed verification - 2026-09-23

- Windows: 39/39 tests passed; ARM64: 43/43. Regression tests include missing
  tracks, unsafe paths, malformed/oversized lists, CHD signature, firmware
  replacement, cached capability versus worker recheck, cancellation and a
  Mega Drive/CD extension mismatch.
- Three Mega-CD and four NeoCD BIOS files matched the primary documentation's
  MD5/SHA-1 identities before SHA-256 registration. The missing AArch64 NeoCD
  core came from the official Libretro build server; archive CRC, ELF machine,
  library name and extensions were checked. Existing cores were preserved;
  private checksums/download provenance remain with device operational records.
- Rehearsal, repeat import and real installation configured exactly eight
  records; all unrelated fields/tables remained identical. Paired binary,
  database and integration-configuration backups were retained. There are now
  88 configured records: 72 RetroArch, eight melonDS and eight Dolphin; 45 still
  need setup.
- Installed production SHA-256:
  `38a7769a7ce9c046360a3c30166b22a88953921e95ca07a498ffe04c0697dd0a`.
- The installed shell browsed both systems, selected for Home and launched
  Sonic CD (CHD) and Metal Slug (CUE/IMG). Injected kernel gamepad events
  navigated the Metal Slug menus and started its first mission. Sonic reached
  the first level, moved on directional input and collected rings. Initial
  Sonic attract-mode pictures were not counted as gameplay acceptance.
- Both returned through the owned confirmation overlay with returned history.
  Exit requests used InputPlumber's interception property remotely; the overlay
  was confirmed programmatically. Actual Gamescope screenshots were inspected
  and shown in chat. This is not fresh hands-on physical Home-button acceptance.
- All 20 collection files for these eight titles (including companion files)
  still match their source-matched SHA-256 manifest. Pokemon registrations,
  profiles and Pokemon history/media remain intact; database integrity passes
  and no game is left running. Metal Slug produced a 960x540 exit picture and
  was selected from Y for Home without launching.
- Extended play, in-game save/reload across restarts and each of the other six
  titles remain unverified. No new per-Trainer save support is claimed. Large
  CHD exit pictures remain pending; CUE-only image identity does not cover its
  companion tracks yet. The existing unavailable-picture notice can still
  appear after a large CHD exit; complete this behavior in the P4 media slice.

PlayStation multidisc/controller swapping, other disc systems and arcade sets
retain their own P4/P9 gates. Static game media and World/Home presentation follow
runtime completion; the rest of [ROADMAP](ROADMAP.md) is unchanged.
