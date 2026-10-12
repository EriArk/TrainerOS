# LRPS2: PS2 shared-console candidate

## Native continuation - 12 October 2026

This is an isolated candidate, not an enabled TrainerOS PS2 multiplayer profile.
Ordinary ARMSX2, library bindings, preferences and personal cards are retained.
The existing RetroArch networking protocol is being evaluated; DEV9 is not used
as a substitute for shared-controller play.

- Upstream [libretro/ps2](https://github.com/libretro/ps2/tree/14e75bd69d0997ea9d51ad9f87d5edfe12f502ef),
  revision `14e75bd69d0997ea9d51ad9f87d5edfe12f502ef`.
- The unmodified full core builds with Fedora 44 ARM64, GCC 16.2.1 and CMake/Ninja
  in the existing build container. Its SHA-256 is
  `649afa38eb59c0c860024f3c185b87c8af50b5df637cfe46a4c738aedf512ec9`.
  Both installed RetroArch Flatpak runtimes resolve its shared dependencies.
- The owner's Tekken Tag Tournament US CHD (`SLUS-20001`, native CRC `67454C1E`)
  was copied into separate test directories on Flip/Odin. CHD SHA-256:
  `74977f5e1bbfcc5c79bd9a61190f1c65dd77f9abfec8500f9130be13d486d12f`.
  The copied owner BIOS SHA-256 is
  `f4c948e61a291d4b3f92a141e550cf8357204287a31ff784caccbedaef910c9d`.
  Firmware/game bytes and captures remain private. Test cards are isolated;
  audio output is disabled. An early probe omitted `global_core_options=true`:
  it used renderer defaults and created a new `config/LRPS2/LRPS2.opt` on Flip.
  Its creation time/hash were checked and that probe-created file was moved to
  the private test directory. Later probes use private global core options,
  inherited input bindings with `udev`, and disabled automatic overrides.
- Vulkan boot reached the game and character selection on Flip. A native state
  of 50,623,976 bytes was saved. Restoring it crashed in `state_read`, while
  copying EE RAM. A second probe intended for software rendering actually used
  Vulkan because of the options issue above; it is not software-renderer proof.
- A first native two-client connection admitted player 1 and player 2. Later
  diagnostic `GET_STATUS` commands also triggered crashes; this attempt does
  not prove a game, determinism or acceptable performance.

### Maintained ARM64 state correction

The [patch](../../packaging/emulators/lrps2/arm64-state-restore.patch) calls the
existing `mmap_ResetBlockTracking()` from the ARM64 EE recompiler's full reset,
alongside clearing its generated blocks. A state restore resets those caches
before copying RAM. Previously their write-protected pages survived the ARM64
reset, and the ordinary host-memory copy faulted with `SEGV_ACCERR`. The crash
dump resolves to `libretro/main.cpp:3071`; this is not a new networking protocol
or a change to serialized game data.

The patched core builds; candidate SHA-256:
`961b738c09552baab78e86a75ecdc98e8fe7160d2e39718497530e79ad764d0b`.
Repeated Vulkan restores now return without that crash, but a captured restored
intro frame has visible corruption. With verified Software (SW) settings, three
restores returned to character selection with a stable picture and continued
input. This bounded state check does not establish deterministic netplay.

### Paired feasibility gate

The corrected software-renderer probe connected Flip as P1 and Odin as P2 using
the existing native protocol, but did not reach a playable fight. The measured
overlay was approximately 12.07/13.54 FPS. Host RSS reached 5,725,920 KiB with only
162,572 KiB available memory; guest RSS was 3,354,072 KiB. Both were stopped after
the bounded probe. Host exited normally; the guest faulted during game unload
(SIGSEGV, after `Core: Unloading game`), without a `GET_STATUS` query. That cleanup
failure remains unresolved and is distinct from both failures described above.

The inspected RetroArch `69a4f0ea1e`
[buffer allocation](https://github.com/libretro/RetroArch/blob/69a4f0ea1e/network/netplay/netplay_frontend.c#L7206)
reserves 124 state slots on a host and 62 on a guest, derived from the fixed
60-frame stall bound. Each native PS2 state is about 50.6 MB. This explains the
large memory requirement; it does not establish that memory alone caused the
low frame rate. There is no stateless-mode setting in this inspected path.
Reducing only allocation would violate its stall/buffer assumptions. A maintained
memory/performance solution, deterministic gameplay and reliable shutdown are
required before a PS2 profile can be admitted. Ordinary ARMSX2 remains selected.

Post-probe checks: neither handheld has a running test emulator; both shell
processes are alive and volume is zero. The bounded ordinary-save baseline is
unchanged (364 Flip / 25 Odin files), both databases pass `quick_check`, and both
ordinary RetroArch configuration hashes are unchanged. Private test progress is
not returned into personal cards. Continue independent Hotseat work while this
PS2 feasibility gate stays open; this is not a permanent impossibility claim.

A separate diagnostic failure was resolved to RetroArch's
[`GET_STATUS`](https://github.com/libretro/RetroArch/blob/69a4f0ea1e/command.c#L972):
it copies `core_info->system_id` without checking the field for null. The renamed
private candidate had no matching core-info entry. The installed binary's crash
return address is `0xe57c0`, immediately after that copy; its string references
are `GET_STATUS`, `PAUSED` and `PLAYING`. This query was removed from the private
probe; it is not evidence of another LRPS2 state-restore failure. Normal delivery
must supply matching core metadata before using frontend status queries.

## Build and ownership

The [recipe](../../packaging/emulators/lrps2/build.sh) checks out the exact source,
applies the maintained patch and emits a separate core/build record. It needs a
native C/C++ toolchain, Git, CMake, Ninja and Python 3.11+. The inspected source
includes its dependency sources and embedded GameIndex database. The recipe
does not fetch firmware, games, cards or shared saves, and does not replace the
ordinary emulator or enable a multiplayer profile. The equivalent native build,
patched rebuild, reverse patch applicability and recipe shell syntax passed;
the complete fresh recipe has not separately been executed. Preserve upstream GPLv3 and
component notices; #115 redistribution acceptance remains separate.

For an update, inspect upstream ARM64 reset, page protection, state format,
thread quiescence and libretro options. Rebase or retire the patch, build a new
directory and verify state restore plus actual network play before promotion.
Keep the previous core and configurations for rollback; never overwrite a live
core or personal cards. The full recipe and gameplay acceptance remain distinct.

Still open: state/render correctness, repeated native gameplay and controls,
latency/performance, exact profile and firmware/settings identity, ordinary
invitation/Home/exit, ordered Hotseat, protected lifecycle, both-device delivery
and retained distinct-network/physical acceptance. No issue is closed.
