# mGBA linked parties - MP-02

MP-02 generic 2-4-player route, 10 October 2026. See the installed evidence below;
this does not complete all handheld mechanisms or external acceptance.

## Existing implementation selected

[Libretro documentation](https://docs.libretro.com/library/mgba_splitscreen/) and
[upstream source](https://github.com/Spuds0588/mgba-splitscreen) provide the GBA
lockstep machines, subsystem SRAM, per-viewer screen/audio, and serial state.
TrainerOS reuses RetroArch rollback and its existing online relay; it does not
implement another cable or network engine. The other reviewed generic candidate,
[Aelvryx/mgba-wifi-link](https://github.com/Aelvryx/mgba-wifi-link), has a two-peer
netpacket implementation but explicitly lacks internet relay and rollback. The
existing gpSP profiles remain available for their reviewed protocols.

Upstream: `b66511a35873bda9c961872813e58b35543bcfc5` (MPL-2.0).
Build: `packaging/emulators/handheld/build-mgba-linked.sh NEW_DIRECTORY` in the
ARM64 build container. RGB565 flags are required by this frontend. Ordinary
solo mGBA is not replaced. The patch is `mgba-linked-pair.patch` beside the script.

Maintained changes:

- Construct the declared subsystem during load, before RetroArch imports SRAM
  or queries serialization, without advancing any emulated frame.
- Terminate the restored lockstep event queue and free list. Upstream appended
  free nodes to restored events; an eight-frame replay diverged on frame 444.
- Restore PSG phase after audio register writes during state loading. Reversing
  that order changed eight serialized bytes on frame 37 of the Kirby replay.
- Preserve write-only PSG frequencies and the multiplayer send register during
  serialization, and restore the idle SIO event timestamp. The expanded menu/link
  replay exposed audio divergence at frame 407 and outgoing cable data divergence
  at frame 1480. These are generic state fixes, without game-specific branches.
- Reserve enough serialization space for save detection growth.
- Advance RTC from an emulated epoch included in host state, rather than sampling
  each device's wall clock during rollback.
- Send warnings/errors through the frontend logger instead of logging every
  serial event to stdout.

The existing driver's state restoration is retained. No game-specific cable
branches or automatic Four Swords assistance are added.

## Regression

`check-gba-linked.cpp` loads two machines using subsystem `0x201`, seeds different
128 KiB SRAM regions before frame one, and verifies restoration into those same
frontend buffers. It runs 3600 frames, repeatedly advances eight frames, rolls
back and checks byte-identical replay. The continuous diagnostic verifies both
players, all four baud rates, data/IRQ/timeout counters and sustained transfers.

The CC0 fixture is from `Aelvryx/mgba-wifi-link` revision
`9e919b0cfbb93af7d1171570dfc6745d00eeebab`,
`tools/gba-link-test-rom/fixtures/gba-link-continuous.gba`, SHA-256
`c1fe01752d4f5863d6e3e1a9866b061aaadf2927ccc3df31ba9ecbf4bc68fe9d`.
Its source/build instructions and licence are in that upstream directory.
No owner ROM or save is included in this repository.

```sh
c++ -O2 -std=c++17 -I SOURCE/src/platform/libretro \
 packaging/emulators/handheld/check-gba-linked.cpp -ldl -o check-gba-linked
./check-gba-linked CORE_SO gba-link-continuous.gba
```

Observed ARM result: 2186 transfers per player, baud mask 15, zero data errors,
missed/duplicate IRQs or timeouts; byte-identical rollback; about 6.6 CPU seconds.
This is core-level evidence, not installed online gameplay. The same harness
accepts `--cartridge [2|3|4]` for the owner-supplied Kirby regression: 3300 frames
through Multiplayer, Game Pak selection and play with separate directional input,
independent batteries and matching state/video replay. The owned Kirby image
passes; the original audio restore order fails that check at frame 37. This mode
uses blank test batteries inside the headless process, never personal saves.

## Integration boundaries

Two to four players, same exact GBA content/core/runtime/helper, no late joining. The
ordinary invitation and consent flow prepares each player's opaque battery.
GBA header validity determines the generic route; names/series are not an
allowlist. Catalogue single-player exclusions remain. Each viewer gets their
own focus screen, audio and controller port. The ROM is copied locally into
private subsystem filenames; it is never transmitted by this preparation.

The upstream exports P1-P4 `.sav`, `.sav2`, `.sav3`, `.sav4`; local progress remains
in the verified mGBA `.srm` target. Existing 512/8192/32768/65536/131072-byte saves
are padded only inside the session, and retain their original size on return.
Only the assigned player's bytes can return, through existing backup, concurrent
change, failed-process and recovery checks. Unknown save trailers are rejected.

Different cartridges, Single-Pak download, wireless RFU, sensors and special
accessories are not delivered by this same-content route.
Actual game support still depends on upstream emulation and the game's own
multiplayer mode. Distinct-internet and physical controller/audio checks remain
separate acceptance.

## Update and rollback

Rebase the maintained patch onto an explicitly reviewed upstream pin. Re-run the
core regression and affected TrainerOS save/party tests, then paired UI gameplay
and protected return before publishing a new bundle. Retain MPL source/notices.
Use `tools/emulator-core-bundle.py` capture/apply/restore with a new transaction
folder and no active emulator; preserve the previous core and app together.
A changed digest requires compatible peers to update before joining.

## Installed evidence - 10 October 2026

App on Flip/Odin: `8a235c1930536d2c956f8495d8ec7b78a5d061db2914cc0cdd221a2404f8c6f6`.
Final core bundle `mgba-linked-20261010.3`: 1,734,968 bytes, SHA-256
`45690d131d29d8a0d9b50e19745ecc05efcf16d38daf7cd47576efa2f26bd3f6`.

Kirby & The Amazing Mirror (USA), already owned on both devices, SHA-256
`cfb194cd0cb373787094a962852e3e564fb2ecdb0609fe065afdd8de3f4e11f1`:
ordinary live Game Options -> Play together -> Online friend -> guest Accept ->
host Start -> existing save confirmation -> automatic SRAM preparation -> public
RetroArch relay, with both player seats accepted. In-game Multiplayer detected
P1/P2, selected the game pak and reached actual play. Flip moved pink Kirby;
Odin controlled yellow Kirby, with independent cameras/HUDs and peer movement.

That paired relay run used the diagnostic build `9d51b22b...`, which differed
from the final core only by temporary bounded input logging. The final core
removes that logging and passes the maintained serial and Kirby replay checks.
The final core was then verified through ordinary Nearby invitation, acceptance,
automatic pinned-TLS battery preparation and Start. Kirby again detected both
linked machines and reached gameplay, with independent pink/yellow controls and
per-player focus/HUD. The staged host batteries matched the two distinct personal
save images, including private padding of Flip's 32 KiB image to 128 KiB.

Both ordinary Home -> Exit confirmations completed. Flip retained its 32 KiB
save and Odin its 128 KiB save, with distinct contents. No save-size migration or
cross-player overwrite was required. The two verified relay and final Nearby session folders were removed on success;
earlier retained recovery folders were preserved. After final Nearby exit, the
personal save hashes matched their pre-session images (no new in-game save was
made in that run), preimage backups remained, and no new recovery file or running
RetroArch process remained. Installed and running app/core hashes matched on both
handhelds; the existing SameBoy and DoubleCherryGB cores were unchanged.
Both handhelds use one home internet connection: public relay traversal is not
separate-internet acceptance. Device output remains quiet; physical audio/input
acceptance is deferred. Two-player capacity is deliberate; upstream four-player
machines do not establish four-party preparation/admission in TrainerOS.

Application validation: the seven affected ARM runtime/party/exit suites passed.
After extending size, stale-preparation and cancellation regressions, the three
changed suites passed again on Windows and ARM (3/3 each). Portable runtime-adapter
build and generated adapter knowledge checks also passed. These checks cover
512/8192/32768/65536/131072-byte batteries, both assigned slots, absent saves,
conflicts, crash/nonzero exits and malformed data; they do not certify every GBA
game or replace the deferred real-controller/network acceptance.

## Two-to-four-player delivery - 10 October 2026

The generic profile now admits up to four participants. Start freezes the accepted
roster, compacts cancelled seats and selects the existing 2/3/4-machine subsystem.
Each accepted guest gets a separate authenticated helper exchange and private
credential. The host waits for every opaque battery; guests also wait for final
runtime readiness. Cancellation or roster changes invalidate unfinished work.
Each client sees/hears/controls its assigned machine and returns only that battery.
GB/GBC remains two-player. Preparation version 2 prevents old clients from joining
this changed handshake; exact runtime/core/helper/content checks remain required.

Final app on both handhelds:
`c9a0d0e7b678a7aefea21173fe5e7f038358e933bf2031d49667c6631daf95c3`.
Final core bundle `mgba-linked-20261010.5`, 1,734,968 bytes:
`fbf14e74472aa9f298f9a8a964b6bd00cae37ccc6563f347a92d2a3f1a088977`.
Earlier `.3` evidence above describes the previous two-player delivery.

The expanded headless test reaches Kirby's actual four-machine match, with
pink/yellow/red/green player HUDs. All 2/3/4-machine runs pass full byte-identical
state and video replay for 3300 frames, including distinct directional controls.
The continuous two-machine CC0 diagnostic passes 1794 transfers per player,
all four baud rates and zero data errors, missed/duplicate IRQs or timeouts.
The harness now restores its original timeline after replay: the earlier harness
accidentally repeated button edges and selected solo mode. Those discarded solo
runs are not multiplayer evidence. Experimental second-pass link restoration was
unnecessary and removed; only reproduced serialization omissions remain patched.

Application checks: 9/9 affected ARM suites, then the three final changed suites
on ARM and Windows (3/3 each). Coverage includes 2/3/4-machine slots, all five
battery sizes, missing/malformed saves, conflicts/crashes, per-member credentials,
cancelled-seat compaction, out-of-order preparation, incomplete preparation,
roster loss, stale completion and final machine-count tampering. Five real helper
process tests pass, including three simultaneous distinct pinned-TLS exchanges
and rejection of another member's credential. The Windows aggregate save matrix
needs a 120-second test budget (observed 38 seconds), without changing runtime
connection deadlines.

Installed journey: the new application completed ordinary Online friend
invitation/Accept/Start through the public RetroArch relay, with real linked
Kirby play and independent pink/yellow controls. That run used `.4` (the audio
fix); after the send-register/timestamp fixes, final `.5` completed the ordinary
Nearby journey and actual paired play with independent cameras and controls.
Both runs used each device's own existing battery and normal Home -> Exit
confirmation. Final save hashes equal the pre-session images, still 32 KiB on
Flip and 128 KiB on Odin (no new in-game save in these runs). Preimage backups
remain; no new recovery file or running RetroArch process remains. Both successful
sessions cleaned their temporary folders, preserving older recovery folders.
Installed/running application hashes and final core hashes match on both devices;
SameBoy and DoubleCherryGB are unchanged. Both audio outputs remain at zero.
The portable runtime module builds and generated knowledge checks pass.
Four emulated machines in a headless test and a two-handheld run are separate
levels of evidence: a real four-client party, distinct internet networks and
owner physical input/audio acceptance remain open. Remaining MP-02 transactions,
Single-Pak/RFU/peripherals and guest content are not completed by this increment.
