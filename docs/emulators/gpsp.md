# gpSP — independent GBA link

Pinned upstream: `5819380c2ffb0900219d700a382ee68c464ebb99` from
https://github.com/libretro/gpsp. No source modifications.
ARM64 build recipe and deployed artifact digest:
[handheld bundle](../../packaging/emulators/handheld/).

This is an additional networking core; ordinary GBA remains mGBA. TrainerOS
uses netpacket, each console's own cartridge and ordinary SRAM. A private
`gpsp_serial` option selects `mul_poke`, `mul_aw1`, `mul_aw2` or `rfu` from the reviewed
cartridge header. `gpsp_rtc = system` is session-only. Existing core options,
controller mappings and ordinary mGBA selection are not overwritten.

The session loads an isolated copy of the normal save. Normal exit returns it
atomically, retaining a preimage backup. Changed originals, invalid lengths or
abnormal exit keep the original and a separate recovery copy. Four-seat transport
capacity does not claim that every in-game activity supports four players.

Actual paired gameplay and outstanding acceptance are in
[MP-02](../HANDHELD_MULTIPLAYER.md). On update, recheck serial option names,
netpacket compatibility and SRAM size/readback; update both consoles together.
Core/runtime hashes must match before admission. Retain source/license with any
distributed build; use `tools/emulator-core-bundle.py` for journaled install and
rollback. Never roll back personal saves with emulator binaries.

## Wireless Adapter integration - 10 October 2026

The pinned core already implements RFU through `gpsp_serial = rfu` and the
existing RetroArch netpacket interface. Its `gba_over.h` identifies compatible
cartridge codes. The maintainer's [RFU report](https://www.davidgf.net/2024/01/13/gba-wireless-adapter/)
distinguishes working Mario Golf/Mega Man routes from latency-sensitive and
known-failing games. Reuse that mechanism with ordinary invitations, independent
machines and protected own-save return; no new emulator patch is planned.
Keep the delivered cable routes and exact-content matching. Header recognition
selects a runtime profile, never a semantic save capability or compatibility with
another ROM. The first device target is the owner's Mario Golf Advance Tour USA,
code `BMGE`, SHA-256
`ed48b83c7f68f3ae4969efb0a0aef9d40e37782840d53f147d35e62b5aef83df`.
It is already present on both handhelds.

`runtime.handheld.gba-rfu.v1` uses netpacket with `rfu-own-save-v1`, one local
controller/machine and no late joining. Mario Golf codes BMGE/BMGJ/BMGP/BMGS/
BMGF/BMGI/BMGD/BMGU allow up to four party members; Mega Man Battle Network codes
BRBE/BRKE/BR5E/BR6E allow two. These are upstream-backed profiles, not installed
gameplay evidence for every region/title. The complete ROM and runtime/core
digests must still agree; a cartridge code or renamed title cannot authorize
cross-ROM joining. Existing cable profiles remain; other valid GBA cartridges
retain mGBA Splitscreen. RFU is not advertised for the upstream's known failing
or latency-sensitive families.

The existing invitation, public relay, session controls and private own-save
return are reused without linked-pair SRAM preparation. Host and guest tests
cover profile selection independent of filename, malformed headers, exact-ROM
admission, missing core rejection, one controller, absence of a paired subsystem,
RFU core options, original-size save return and preimage preservation. The
RetroArch suite passed on Windows and ARM; party/runtime suites also passed on ARM.

### Installed Mario Golf evidence

Flip/Odin ran the same unmodified gpSP core, SHA-256
`bf0aee3cbb0bbb6d5dcee715a7b3a4da24631bf925a620ab02b937b43a626b4c`.
The ordinary Game Options -> Play together -> Online friend -> Accept -> Start
journey established a public-relay netpacket session (reported RTT 133-216 ms).
Both games exposed Wireless Adapter, discovered players 1/2, selected Mario and
Peach, and entered Marion Course / Stroke Play. Separate physical-device event
injection controlled each player's shot; both consoles displayed each shot and
the same one-shot-per-player score. This is short paired gameplay, not a completed
18-hole tournament, physical owner input or separate-internet acceptance.

The first session created Neil on Flip and Ella on Odin through the game UI.
Normal return produced distinct ordinary saves. Flip retained its 32 KiB size
and original preimage `fe06d8db557113e057e248d8d4ea45eba85a29399db856445de0cba66eb98151`.
Returned Flip SHA-256:
`ebccc07eb900ef15bb74491637df8b9bc5d3c62334998b64bf1860368041c654`.
Odin initially received a new 128 KiB gpSP save; ordinary mGBA subsequently wrote
its normal 32 KiB representation, SHA-256
`0e8eca33e606ca918d0f4b5721df1f129f6e066622e4fc1d0e2dfc64e1487af7`.
A repeated invited RFU launch read both profiles. Isolated ordinary mGBA runs
using copies and diagnostic input displayed the distinct story characters on
both devices; the actual original hashes remained unchanged by this readback.
Inherited ordinary RetroArch input did not respond to the remote test events;
diagnostic readback does not close #77 or physical controller acceptance.

Two limitations surfaced in the real journey. An invited guest with an ordinary
game still running is explicitly rejected by the existing runtime guard; close
that game before accepting a new session. Seamless guest handoff remains open.
Also, host departure during the guest's Exit confirmation dismissed an already
absent multiplayer menu and released the exit input lease. The guest's emulator
and SRAM remained alive; restarting only the overlay helper restored normal
Exit. `dismissMenu()` now leaves a separate exit attempt untouched. A regression
reproduced the failure before the fix and covers capture, confirmation and closing.

Final installed application SHA-256 on both handhelds:
`e3731b5530c6cb9ae640f39feb274f33d4d73e9c55c5a9abf52763a6f3920ece`.
The short Online RFU round used the preceding application build
`335f199ad3bca81924dfa321283208320a1621a9af42b98aa1e748dbc6d9cb0d`;
the only later runtime change is the exit lease guard above. On the final build,
Nearby re-established the paired netpacket session. With both Exit prompts open,
Flip left first; Odin logged disconnection, retained an enabled confirmation and
then exited normally without helper recovery. Both original save hashes above
remained unchanged. A final Online retry failed at public-relay resolution before
runtime restart and retained the ordinary game; it is recorded as an external
relay failure, not another successful internet test.

Four real RFU clients, other regions/Mega Man gameplay, long matches, separate
networks, physical controls/audio and the wider RFU compatibility matrix remain
open. Single-Pak download play and special accessories are separate mechanisms.
