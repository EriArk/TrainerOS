# DoubleCherryGB — independent GB/GBC link

**10 October 2026:** a second TrainerOS route uses upstream's two-machine linked
pair and ordinary RetroArch rollback for GB/GBC cartridges without battery/RTC
storage. `dcgb_emulated_gameboys = 2`, `dcgb_gblink_enable = enabled`, and
`dcgb_single_screen_mp = player N only` / `dcgb_audio_output = Game Boy #N`
select the assigned player's screen/audio. Both clients emulate both linked
machines; the existing RetroArch netplay route synchronizes their inputs/state.
This is genuine linked-machine emulation, not two pads attached to one Game Boy.
The single-content `retro_get_memory_data` API exports only machine zero's SRAM,
so this route uses `noload-nosave` and rejects persistent-memory cartridge types.
No new emulator source patch is required. Same content/core/runtime is mandatory.
The independent-save/netpacket route described below remains separate. See
[selection and verification](../HANDHELD_MULTIPLAYER.md).

Selection references: [upstream's two-machine recommendation](https://github.com/TimOelrichs/doublecherryGB-libretro/blob/03f58ca3dfb4b716f7e66a0e0467f9e85ef82abb/README.md#link-cable-trading-over-network),
[Gambatte network serial](https://github.com/libretro/gambatte-libretro/blob/d9d6cd06382d1ced30de34d56d3609452323dab1/libgambatte/libretro/net_serial.cpp),
and [Coffee GB source](https://github.com/trekawek/coffee-gb/tree/554ea56b465bb4147e2ed93c06135671c5e8db11).
Only the DoubleCherryGB integration described here was delivered in this pass.

Pinned upstream: `03f58ca3dfb4b716f7e66a0e0467f9e85ef82abb` from
https://github.com/TimOelrichs/doublecherryGB-libretro.
Build recipe and exact installed artifact:
[handheld bundle](../../packaging/emulators/handheld/).

Ordinary play remains Gambatte. Network sessions use one emulated Game Boy,
netpacket and independent SRAM; emulating two local Game Boys is a different
rollback mode. `dcgb_emulated_gameboys = 1` is private to the session.
`dcgb_singleplayer_linked_devive = Off` (upstream spelling) disables the automatic
distribution-machine fallback, and `dcgb_pkmbuddyboy_auto_mew = 0` prevents an
unrequested distribution. No ordinary configuration is changed.

The current profiles cover Pokemon Gen I, Gen II and TCG cartridge headers.
Upstream specifically cautions that battles are unstable; this is not generic
real-time GB link support. Upstream RTC is incompatible with ordinary Gambatte;
the explicit bridge below is required before sharing Gen II battery clocks.
See [actual MP-02 evidence](../HANDHELD_MULTIPLAYER.md).

Update both cores together after reviewing option names, netpacket format and
SRAM/RTC interfaces. Preserve the previous bundle and ordinary saves. Install
and rollback with `tools/emulator-core-bundle.py`. The pinned repository's
`LICENSE` contains AGPL-3.0, despite a different statement in its README; retain
the actual license and corresponding source when distributing the artifact.

## TrainerOS MBC3 clock bridge — 2026-10-07

`packaging/emulators/handheld/doublecherry-rtc.patch` adds clock-option value `2`,
leaving upstream values `0`/`1` intact. `TrainerMbc3Rtc.h` implements the eight-byte
epoch-base battery convention independently; no Gambatte implementation is
copied. The patch also fixes single-game libretro memory-kind/machine-index
confusion: RTC is memory kind 1, not Game Boy number 1. Dual-game mode is unchanged.
Only the reviewed Gen II session selects this option. Ordinary Gambatte remains
installed and its configuration stays intact. Current targets are little-endian.

Reference: Gambatte `d9d6cd06382d1ced30de34d56d3609452323dab1`,
[`cartridge_libretro.cpp`](https://github.com/libretro/gambatte-libretro/blob/d9d6cd06382d1ced30de34d56d3609452323dab1/libgambatte/src/mem/cartridge_libretro.cpp)
and `rtc.cpp`. Compatibility includes its legacy 511-day normalization boundary.
Halt/carry are transient in this battery format; it is not a savestate interchange
or a promise to preserve arbitrary halted hardware RTC across emulator shutdown.

The recipe normalizes line endings only in patched files, checks/applies the
patch, and copies the clock header before compiling. Bundle `handheld-20261007.1`
records the installed ARM64 digest. `check-clock.cpp` generates an original tiny
MBC3 ROM and executes it through the actual core API: reads 300 days/13:27:19,
writes hour 14, then verifies SRAM and the exported epoch. It passed against both
installed Gambatte and patched DoubleCherryGB on both Flip and Odin. This is real
core/CPU evidence, not a commercial-game trade or different-network test.

Gold additionally passed a paired relay save/normal-exit cycle and ordinary
Gambatte Continue/clock/room readback on both devices. Production finalization
returned each independent pair with matching preimage backups. This is not a
Silver/Crystal cable transaction or full GB Home invitation acceptance; see the
dated MP-02 evidence for exact boundaries and the discarded expired-helper probe.

TrainerOS stages each player's clock beside both possible private SRAM paths.
The finalizer backs up SRAM/RTC and writes a durable return intent before replacing
either file. Ordinary TrainerOS launch completes an interrupted matching intent;
an independently changed original blocks replay and retains recovery. Hosts using
the portable adapter must call `recoverHandheldReturn` before ordinary GB/GBC
launch as well. Launching a core independently bypasses this host recovery step.

On update: review the RTC ABI and mode selection, reapply the patch to the pinned
upstream, run the original-ROM check and adapter conflict/recovery tests, then
check a copied commercial save in ordinary play. An unpatched replacement core
is rejected for Gen II linking with an update error; it must not consume the
eight-byte clock as its old four-byte format. Do not weaken that guard to make an
upstream update appear compatible. Preserve and use the prior bundle transaction
for rollback; do not roll back personal SRAM/RTC as an incidental binary rollback.
