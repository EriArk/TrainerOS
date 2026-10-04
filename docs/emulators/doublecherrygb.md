# DoubleCherryGB — independent GB/GBC link

Pinned upstream: `03f58ca3dfb4b716f7e66a0e0467f9e85ef82abb` from
https://github.com/TimOelrichs/doublecherryGB-libretro. No source modifications.
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
real-time GB link support. Ordinary Gambatte RTC bytes are never overwritten
with this core's different clock representation. Gen II clock continuity still
needs an explicit interoperability check; transport startup is not that proof.
See [actual MP-02 evidence](../HANDHELD_MULTIPLAYER.md).

Update both cores together after reviewing option names, netpacket format and
SRAM/RTC interfaces. Preserve the previous bundle and ordinary saves. Install
and rollback with `tools/emulator-core-bundle.py`. The pinned repository's
`LICENSE` contains AGPL-3.0, despite a different statement in its README; retain
the actual license and corresponding source when distributing the artifact.
