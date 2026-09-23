# Multiverse cartridge runtimes

P4 extends the existing RetroArch launch/return adapter with these bounded routes:

| Platform | Core | Accepted content | Copied collection |
| --- | --- | --- | --- |
| SNES | Snes9x | SFC, SMC, ZIP | 12 |
| Mega Drive | Genesis Plus GX | MD, GEN, SMD, BIN, ZIP | 12 |
| 32X | PicoDrive | 32X | 8 |
| Neo Geo Pocket Color | Beetle NeoPop | NGP, NGC | 8 |
| PC Engine | Beetle PCE Fast | PCE cartridges | 2 |

These are cartridge routes, not CD support. ZIP content uses RetroArch's archive
loader; the explicit registered platform selects the core. A ZIP or BIN filename
alone never infers a platform. Capabilities use committed metadata without disk
I/O; content loading and its failures remain in the controlled child lifecycle.
An explicit platform/core mismatch cannot launch. Core presence is configuration
evidence, not a claim that every title has been played.

All 24 copied SNES/Mega Drive ZIPs contain exactly one matching ROM and passed
CRC checks on Flip. The installed cores report archive extraction enabled through
`retro_get_system_info`. This does not validate arbitrary user-supplied archives;
unsupported or malformed content may fail to load and return to the shell.

New routes use the bounded ordinary-launch override: no automatic savestate load,
creation or thumbnail, and no configuration write on exit. Active core/directory/
title configuration overrides are checked against the actual core library name;
unverified active overrides block launch instead of silently being discarded.
Existing emulator ordinary-save locations remain in use. Verified per-Trainer
save namespaces are still limited to the delivered mGBA route; this change does
not introduce save parsing, new backup protocols or cross-platform save isolation.

## Maintenance and provenance

`trainer_library_import DATA_DIRECTORY MANIFEST.json --configure-existing`
prevalidates the same identities/files as normal import, then configures only
matching existing `unconfigured` records. It retains their current names, notes,
integration fields and expected revisions. Configured records are never reassigned.
Unresolved entries and idempotent retries do not increment revisions. New entries
still follow normal import. Store locking and asynchronous repository validation
apply; every successful mutation commits separately. Failures report the applied
count, and repeating the manifest can continue without duplicating registrations.

Four missing AArch64 cores were obtained from the official
[Libretro build server](https://buildbot.libretro.com/nightly/linux/aarch64/latest/).
ZIP CRC, ELF class/machine and actual library identities were checked; download
URLs and SHA-256 values are kept in the private device manifest. Existing cores
were not replaced. No ROM, BIOS, core binary or private manifest is committed.

Primary route references:
[Snes9x](https://docs.libretro.com/library/snes9x/),
[Genesis Plus GX](https://docs.libretro.com/library/genesis_plus_gx/),
[PicoDrive](https://docs.libretro.com/library/picodrive/),
[Beetle NeoPop](https://docs.libretro.com/library/beetle_neopop/),
[Beetle PCE Fast](https://docs.libretro.com/library/beetle_pce_fast/).

## Installed verification - 2026-09-23

- Windows: 39/39 tests passed. ARM64: 43/43 passed. The final maintenance-tool
  error-report refinement was rebuilt and its library test rerun on both hosts.
- A copy of the real schema-11 database passed configuration and repeat-import
  rehearsal. Real installation then repeated the comparison: exactly 42 records
  changed adapter/config/revision; every other field and table remained intact.
  Paired binary/database backup retained. The collection now has 80 configured
  records (64 RetroArch, 8 melonDS, 8 Dolphin), with 53 still unconfigured.
- Installed production SHA-256:
  `86a55cdb3e93a2f14e6518eefba09013d213ea9e45f906ad0939226f4bb8b5a7`.
- The installed shell selected and launched Chrono Trigger (SNES ZIP),
  Castlevania: Bloodlines (Mega Drive ZIP), After Burner Complete (32X),
  Dark Arms (NGPC) and Reflectron (PC Engine) through their real cores.
  Kernel gamepad events exercised shell routes and in-game menus/starting play;
  all five returned through the existing guarded exit and stored clean 960x540
  exit pictures. After Burner and Reflectron reached gameplay. Screenshots shown
  to the owner were captured from Flip's installed Gamescope session.
- Exit requests used the existing InputPlumber interception property remotely;
  the owned exit overlay was confirmed programmatically. This is not a fresh
  hands-on physical Home-button or long play/save/reload acceptance.
- Post-run hashes confirm all 42 source-matched ROM copies are unchanged.
  Pokemon registrations, profiles, Pokemon history/media and the Emerald Home
  choice remain intact. No game was left running; database integrity checks pass.

## Remaining boundaries

Disc systems, arcade sets, PSP/PS2/Wii U and C64 retain individual runtime/BIOS/
controller gates. Preserve the pending on-device registration flow, static media
and World presentation in [ROADMAP](ROADMAP.md). A representative boot/input/exit
check does not close extended gameplay, ordinary-save reload or physical-button
acceptance for every title. Source originals and personal saves remain untouched.
