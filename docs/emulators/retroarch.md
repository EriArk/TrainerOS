# RetroArch common ARM64 baseline

2026-10-03. This is a development-device delivery, not the finished image updater
or a claim that all registered systems have passed gameplay/netplay checks.

## Exact artifacts and ownership

[The baseline manifest](../../packaging/emulators/arm64-baseline.json) records
72 ELF64 AArch64 core files by name, length and SHA-256, plus observed Flatpak
package commits and executable hashes for RetroArch, PPSSPP and Dolphin.
Revision: `arm64-20261003.1`. The core source is the existing Flip development
installation, including earlier official Libretro downloads. These are captured
binary identities, not invented upstream source commits or reproducible source
builds. Retain the offline artifact bundle; a future nightly download is not an
equivalent source for a pinned hash. Release packaging still needs durable
artifact hosting/source recipes and license material for the selected builds.

RetroArch is Flathub `app/org.libretro.RetroArch/aarch64/stable`, version 1.22.2,
commit `a72a8a0a7cde5c24a0134a32fdc1469fb644c459e9974f644e95170ae5dba8bc`.
The preceding Odin package commit was
`64186232d645f2e9d9fe53c62d67b224662cedd513e753747fa9da5811e3b7ed`.
Both contained the same RetroArch executable; the newer package updates assets,
info, database and controller autoconfiguration. The supported Flatpak update
also updated its required shared runtime components. No global update mask was
added; this is not a promise of permanently frozen dependencies.

Only core binaries are installed by
[`emulator-core-bundle.py`](../../tools/emulator-core-bundle.py). It does not write
RetroArch settings, integration profiles, firmware, ROMs, ordinary saves or
TrainerOS databases. Additional installed cores are preserved. Controller/device
profiles and existing per-game preferences remain outside the bundle.

## Operations and recovery

The tool is a maintenance/image-building utility, not a new player setup screen.
Capture and inspect a candidate, then use the same offline bytes on every device:

```sh
python3 tools/emulator-core-bundle.py capture --directory "$CORE_DIR" \
  --bundle "$BUNDLE_DIR" --revision arm64-20261003.1
python3 tools/emulator-core-bundle.py verify --directory "$CORE_DIR" --bundle "$BUNDLE_DIR"
python3 tools/emulator-core-bundle.py apply --directory "$CORE_DIR" \
  --bundle "$BUNDLE_DIR" --state "$NEW_ROLLBACK_DIR"
```

Capture requires a new destination. Apply validates every bundled file first,
locks the core directory and journal, checks that RetroArch is closed, copies old
cores to a durable `before/` directory and records `transaction.json` before
atomic per-file replacement. It verifies the resulting set. Keep normal game
launches closed during maintenance; the lock coordinates this utility, not every
possible external emulator launcher/updater.

An existing transaction cannot be reused for another apply. An interruption is
recoverable with the same journal, including a partially applied set:

```sh
python3 tools/emulator-core-bundle.py rollback --directory "$CORE_DIR" \
  --state "$ROLLBACK_DIR"
```

Rollback validates all original/current identities before restoring anything;
later user changes stop it rather than being overwritten. Only files newly added
by that transaction are removed. Unrelated files and saves remain untouched.
The update is atomic per file, not one filesystem transaction for the whole set.

For a future candidate, retain the previous artifacts and package commits,
compare core/configuration changes, perform bounded affected launch/save/input
and paired-network checks, then promote a new manifest revision. Do not roll
back shared Flatpak runtimes blindly: other applications may have updated too.
Application rollback uses Flatpak's exact recorded commit; save rollback is never
part of an emulator binary downgrade.

## Device evidence

- Both devices positively identified over pinned SSH. No game was running before
  installation. The TrainerOS shell processes stayed running throughout.
- Flip supplied the 72-core baseline and verified 72/72 afterwards without core
  replacement. Odin changed 68 files: 58 additions and 10 replacements; four
  baseline files already matched. Odin's five extra cores were preserved.
- Both now have the same recorded RetroArch, PPSSPP and Dolphin package commits
  and executable hashes, and all 72 baseline core hashes match. This does not
  standardize the remaining standalone emulator families yet.
- Odin's prior core versions are retained in the update journal. All 77 resulting
  Odin core files resolved shared-library dependencies inside RetroArch's Flatpak.
- Eight real-file Linux tests passed: update/verification/rollback, interruption,
  corrupted artifact, path/duplicate rejection, active-emulator refusal, symlink
  rejection and preservation of a later user modification.
- Bounded Odin mGBA and Genesis Plus GX runs reached 120 frames and exited zero
  with test saves under a separate directory. The initial headless probe failed
  because its Qt companion tried to open an absent SSH display; disabling that
  probe UI / using offscreen fixed the check. Null-audio diagnostic warnings are
  not audio acceptance. The first probe also added a normal history entry; no
  personal game-save path was used. RetroArch also rewrote its usual core-options
  files; these probes do not establish byte-identical preservation of every
  auxiliary settings/cache file. These probes are not controller/rendered or
  multiplayer acceptance, and existing successful multiplayer matches were not
  rerun just for this update.
- Odin output remains 0%; Flip remains muted. No reboot, shell binary replacement,
  save migration, or gameplay compatibility claim for every core.

The common standalone migrations and Dolphin native NetPlay bridge remain next
work in the existing [matrix](../EMULATOR_MULTIPLAYER_MATRIX.md). This baseline
does not close multiplayer block 1.
