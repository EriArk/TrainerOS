# gpSP — independent GBA link

Pinned upstream: `5819380c2ffb0900219d700a382ee68c464ebb99` from
https://github.com/libretro/gpsp. No source modifications.
ARM64 build recipe and deployed artifact digest:
[handheld bundle](../../packaging/emulators/handheld/).

This is an additional networking core; ordinary GBA remains mGBA. TrainerOS
uses netpacket, each console's own cartridge and ordinary SRAM. A private
`gpsp_serial` option selects `mul_poke`, `mul_aw1` or `mul_aw2` from the reviewed
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
