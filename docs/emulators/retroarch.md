# RetroArch common ARM64 baseline

2026-10-03. This is a development-device delivery, not the finished image updater
or a claim that all registered systems have passed gameplay/netplay checks.

## Shared profiles — 2026-10-04

[Core/platform expansion](../RETROARCH_MULTIPLAYER_PROFILES.md) reuses the existing
party/relay adapter for ten reviewed cores. It documents supported folder IDs,
content/firmware identity, private progress, two-pad capacity, actual FBNeo probe
limits and both-device delivery. Keep this profile layer outside replaceable
upstream binaries. Exact known profiles and their prior evidence remain valid.

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

## NES Four Score profile - 2026-10-04

Super Homebrew War 2025 uses the existing FCEUmm core and public relay.
Session-local `--device 1:513` through `4:513`, plus `--nodevice 5`, enable
Four Score; the similarly named config/remap keys do not enable it in an
ordinary append-config. Exact party seats request exactly one controller each.
Existing two-player profile identities/configuration remain compatible.
[Runtime proof and its limits](../NES_MULTIPLAYER.md#four-player-relay-checkpoint---2026-10-04)
cover four emulator instances on two handhelds, not four physical participants.
Do not reinstall/patch the emulator or change ordinary controller mappings for
this route. Preserve the open auxiliary core-options isolation issue above.

## Session option isolation and concurrent rooms - 2026-10-04

The auxiliary core-option preservation gate is verified for the exact NES route. RetroArch
1.22.2's [runloop option-path selection](https://github.com/libretro/RetroArch/blob/v1.22.2/runloop.c#L1078-L1164)
prefers a per-core `.opt` file when `global_core_options` is false; disabling
`config_save_on_exit` does not suppress the core option writer. Netplay now sets
`global_core_options = "true"` together with `game_specific_options = "false"`
and the already-private empty `core_options_path`. This uses core defaults and
keeps subsequent option writes inside the owned temporary session directory.
Ordinary launches and the user's global/per-core/per-game settings are unchanged.
Settings identities advance to `*-default-no-sram-v3` and
`fceumm-four-score-no-sram-v2`, preventing mixed old/new configuration matches.

Verification used the production `prepareNetplay` implementation compiled into
an isolated fixture launcher, plus the existing production password bridge and
stock RetroArch 1.22.2/FCEUmm. Four emulator processes on Flip/Odin formed two
simultaneous public Madrid relay rooms, both using Super Homebrew War 2025. Each
room had a different password, host endpoint and owned options directory. Host A
ran on Flip with guest A on Odin; host B ran on Odin with guest B on Flip.

- Both guests joined their intended P2 at 134/153 ms observed initial ping.
- Advancing room A into player selection left room B at its title screen.
- Closing only room A left both B processes connected and responsive. Host B
  changed its character; guest B independently selected/confirmed its character;
  the surviving pair entered the arena and accepted guest movement/jump input.
- Every process wrote its options to its own temporary `core.opt` on exit.
  SHA-256 comparison of all ordinary `.cfg`/`.opt`/`.rmp` files found no changes:
  25 files on Flip and 1066 on Odin. New/deleted matching files were included in
  the comparison. This is settings preservation, not a claim about every cache.
- Session cleanup removed each owned directory; closing A did not remove B's.
  All probe emulators, password bridges and private endpoint/environment files
  were removed/stopped. Ordinary TrainerOS processes remained running during
  the runtime check. Odin stayed at zero volume, Flip muted; no reboot.
- Harness null-audio and Wayland fallback warnings are not audio acceptance.
  UDP `QUIT` did not finish the probe; scoped SIGTERM did, with the expected
  normal RetroArch option flush. No global kill or setting rewrite was used.

An expanded coordinator regression starts two parties in one company, verifies
separate launch endpoints, ends one without changing the other's running state,
then admits the freed participant into the surviving party's third slot without
restarting its existing guest. The optional exact-ROM adapter regression creates
two production session configs, checks private empty option files and independent
cleanup, and rejects the previous settings identity. This fixture is supplied
privately via `TRAINEROS_TEST_NES_ROM`, never committed.

Windows application build and both targeted tests passed with that fixture;
ARM application build and RetroArch/netplay-client/game-party tests passed.
Installed ordinary executable on both devices:
`000964d6fd459f78dcd6d4a448594fdc0f7f4b616256e6ff528f53449421fa03`.
Experimental multiplayer remains off in the delivered launchers.

Limits: the concurrent payload proof uses a fixture launcher, not four actual
TrainerOS/Fluxer accounts. It proves separate rooms/configuration/input/lifetime,
not the full multi-party Social journey or two simultaneous played rounds.
Distinct-network/NAT, four-user invitations, whole-block multiplayer and human
speech acceptance remain open. No new emulator family or compatibility claim.
