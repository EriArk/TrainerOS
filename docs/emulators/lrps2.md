# LRPS2: PS2 shared-console candidate

## Stopped experiment - latest owner correction, 12 October 2026

The owner selected native game online first. "Hotseat" meant couch multiplayer:
one virtual console with separate pads from the players' own handhelds, not P1
handoff. Separately compare ready-made options for "Play on one console"; no
shared-console architecture is selected. LRPS2 remains a stopped experiment.
Do not resume candidate builds,
paired probes or promotion based on the historical next steps below. Preserve
the record and inactive source bindings; no production bundle was installed.

The next route to inspect is ordinary ARMSX2 DEV9. Upstream has a
[DEV9 Local Link adapter](https://github.com/ARMSX2/ARMSX2/blob/9d989ca933a85bb2f1d111fd3b9e5742ac7d2fbe/pcsx2/DEV9/LocalLinkAdapter.cpp);
its source existence is not evidence that the installed Linux build connects.
Native LAN forwarding and a game's online service are different routes. Verify
the installed build and supported configuration before integration; retain
personal cards and shared consent/Home/exit services. Couch multiplayer needs
a separate solution and does not require reviving this experiment.

## Universal integration preparation ? 12 October 2026

The PS2 family now has source bindings to the existing TrainerOS party scan,
Nearby/Online invitations, common prepared-command launcher, RetroArch transport,
Home/exit controls and temporary-progress policy. Ordinary library registrations
continue to use ARMSX2. ISO/CHD bytes, the dedicated frontend/core and the selected
owned 4 MiB BIOS are matched; launch revalidates them. The BIOS is copied into a
private session tree, including private NVRAM/cards/cache locations. Personal
cards and ordinary emulator settings are not imported or written.

The same common controller path prepares two independent digital pads or, for a
separately reviewed handoff bundle, the existing exclusive P1/spectator protocol.
This is preparation code, **not a playable PS2 delivery or real PS2 Hotseat proof**.
No production bundle is installed and no PS2 invitation is advertised. There is
no per-title catalogue requirement or new networking protocol.

`PS2Netplay` discovers `TrainerOS/emulators/ps2-netplay` under the ordinary data
root. A local installer manifest must pin `protocol: 1`, the settings policy
`lrps2-two-digital-private-bios-no-cards-v1`, frontend/core source revisions,
`stallFrames: 8`, selected absolute `biosFile`, and SHA-256 of `retroarch`,
`pcsx2_libretro.so` and `pcsx2_libretro.info`. Admission requires `status: reviewed`;
`hotseatReviewed: true` additionally enables handoff. The known failed candidate
hashes are rejected even if someone changes only the manifest. These fields
record future runtime review; no current artifact has earned that promotion.
The current source pin must be deliberately updated when selecting another core.

### Bounded native candidate result

The core was advanced to upstream
[`5ad55b00aa2331b4c4cc7d990c6c32cab3a88359`](https://github.com/libretro/ps2/commit/5ad55b00aa2331b4c4cc7d990c6c32cab3a88359),
retaining the ARM64 state-reset patch. Upstream now gates each emulated frame on
the input of its own `retro_run`. Its actual-core synthetic BIOS latency test
passed: all 24 input changes appeared in their own frame across 120 pictures,
and the harness exited normally. This does not establish commercial-game speed.
Updated core SHA-256:
`3c6a3783ff3ef751fa2dde565c4c739def6788f3ad79e4ae6c8eb8141dde4e36`.

A separate RetroArch at `69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576` now has a
[reproducible candidate recipe](../../packaging/emulators/lrps2/build-retroarch.sh).
The [maintained patch](../../packaging/emulators/lrps2/retroarch-state-history.patch)
changes the shared stall bound from 60 to 8, so buffer allocation and all stall
calculations stay consistent: 20 host / 10 guest state slots. It does not change
the native network protocol. Both clients must use the matching runtime; Internet
latency tolerance is not established by this experiment.

The native build container's newer libm/ALSA initially introduced a `GLIBC_2.43`
requirement. Relinking against the installed Flatpak runtime's libm and ALSA
removed it without replacing any device library. The recipe accepts `TARGET_LIBS`
for this build-only directory. On this ARM64 build it contained the installed
runtime's `/usr/lib/aarch64-linux-gnu/libm.so.6` as `libm.so`, and `libasound.so.2`
as `libasound.so`. All shared dependencies and `--version` then passed on both
handhelds. Candidate frontend SHA-256:
`f906dda574a49b778daead337f4f94f8dfd71a339d3190dd9212cdb19cf311a8`.
The full fresh recipe remains distinct from this native configure/build/relink.

One short paired Vulkan run admitted P1/P2 using the earlier isolated owned
Tekken disc/BIOS. Host RSS measured 1,191,628 KiB, guest 835,248 KiB: the earlier
multi-gigabyte history problem is reduced. Captures measured about 9.95 and
16.36 FPS during startup; no playable paired fight or independent gameplay proof
was obtained. Host quit with exit code 0; guest again exited 139 during game
unload. The prior truncated dump has no useful symbolic stack. Speed, state/render
correctness and guest teardown remain native-runtime blockers; do not repeat
unchanged pair probes or describe this as working PS2 multiplayer.

Both probes are stopped, the ordinary shells remain running and both devices
remain at zero volume. The retained personal baselines (363 Flip / 229 Odin)
are unchanged; databases pass `quick_check`, and ordinary RetroArch config hashes
match the pre-probe record. Private probe cards are not returned to personal saves.

Current next gate: a maintained runtime correction/alternative that resolves
these general blockers, followed by a bounded real check and ordinary device
integration. Private PSP, the complete Hotseat catalogue and fine title exceptions
remain deferred by the owner. PS2 is still required and unfinished.

### Source validation

The Windows client and the two affected test targets build. The complete
RetroArch suite passes (58 cases, one Linux-specific skip), and the runtime
multiplayer suite passes. The new PS2 preparation case was then built and run
on native ARM64 Linux: it passes candidate rejection, both controller modes,
unchanged ordinary registration/config/card, copied BIOS identity, temporary-tree
cleanup and stale BIOS/core rejection. Its synthetic files are not game/runtime
acceptance. Recipe syntax, reverse applicability of both maintained patches,
portable source exports, local documentation links and diff checks pass.
The source binding has not been installed into the handheld clients: without
an admitted runtime it would provide no usable new player action. Existing
installed multiplayer remains intact; PS2's ordinary device flow is still open.

## Earlier native continuation - 12 October 2026

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
