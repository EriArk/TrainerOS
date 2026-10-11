# PCSX ReARMed multiplayer preparation

11 October 2026. Reuse the unmodified libretro core with TrainerOS's existing
RetroArch direct/relay networking. This adds a PS1 shared-console route, not
DuckStation netplay or a PlayStation link-cable implementation.

## Source and installation

Inspected upstream: [`c8816799b50388e61cfe237fe2cdbb7d8175f20a`](https://github.com/libretro/pcsx_rearmed/tree/c8816799b50388e61cfe237fe2cdbb7d8175f20a).
The core's [official documentation](https://docs.libretro.com/library/pcsx_rearmed/)
lists netplay; core-info revision `5a74858ab2f7a50cebb5a6330895bc38899531c0`
declares deterministic states. This research pin is not a claim that the existing
binary was compiled from today's source.

The previously recorded ARM64 bundle contains `pcsx_rearmed_libretro.so`,
1,747,256 bytes, SHA-256
`25bfeb6336d7ef573903775fe53733af974c90edd268cf4fb9afa497e188609d`.
See [baseline](../../packaging/emulators/arm64-baseline.json). Devices are off;
that historical installation has not been freshly read. No core/package is
changed by this pass. Discovery and launch compare actual current core/runtime
hashes on each participant rather than trusting the historical manifest.

## Maintained session configuration

[RetroArchNetplay.cpp](../../src/integrations/adventure/retroarch/RetroArchNetplay.cpp)
owns the reproducible integration. It uses two digital pads, disables Multitap,
and selects a private owned BIOS directory. Session-only core options are
`pcsx_rearmed_bios=auto`, `pcsx_rearmed_memcard1=libretro`,
`pcsx_rearmed_memcard2=none`, `pcsx_rearmed_multitap=disabled`, the earlier
`pcsx_rearmed_multitap1/2=disabled` keys and
`pcsx_rearmed_threaded_rendering=disabled`.

RetroArch uses `--sram-mode noload-nosave` and a private save/state directory.
Even cores predating the newer card options cannot reach the ordinary card
directory through this configuration. No personal card import/return is added.
The session copies only the selected BIOS; no BIOS is shipped or exchanged.
Discs stay read-only in their original location. Full content/track and relevant
sidecar hashes are rechecked before launch; the private BIOS copy is rechecked too.
[Format boundaries](../MULTIPLAYER_PREPARATION.md#ps1-runtime-decision--11-october-2026)
remain explicit. Ordinary core options, saves and adapter bindings are unchanged.

## Checks, update and rollback

Local fixture checks cover BIOS/content/sidecar changes, private directory cleanup,
separate controller ports, unsupported descriptor rejection and unchanged ordinary
cards/settings. See the dated preparation register for final build/test results.
Fixtures do not prove a valid retail disc, real core behavior or paired gameplay.
Handheld installation, actual controls, games and separate internet remain pending.

For a core update, review upstream BIOS/card/disc options, align actual peer builds,
rerun affected local checks and the deferred bounded paired game/exit check before
recording installation acceptance. There is no emulator patch to rebase. Rollback
removes this TrainerOS profile or returns to the previous client; no ordinary
memory-card format migration or downgrade is needed.
