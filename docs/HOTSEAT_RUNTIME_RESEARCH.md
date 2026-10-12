# PS2 and live controller handoff — 11 October 2026

This record does **not** deliver Hotseat #126 or PS2 multiplayer.
The [12 October native PS2 experiment](emulators/lrps2.md) supersedes the earlier
source-only/no-build status: ARM64 builds and repeated software state restores
pass after a maintained reset correction, but paired play is limited by memory,
speed and a guest shutdown fault. No usable PS2 profile is admitted. Continue
ordered handoff on existing PS1/GameCube routes while that gate remains open.

## Original source review — 11 October

The owner asked to reuse emulator networking and defer deep tests while the
handhelds are off. No emulator synchronization patch or device operation was
performed. Existing separate-pad PS1/GameCube routes remain available.

## PS2 candidates

- [ARMSX2](https://github.com/ARMSX2/ARMSX2/tree/e327f0d38ca61dba2be2e0cd0b7785e8436f730a)
  has ARM64 emulation and DEV9 networking. A game's own LAN/server mode is not
  synchronizing one console with remote pads. Native server integration remains
  open; no shared-console profile is admitted.
- [LRPS2](https://github.com/libretro/ps2/tree/14e75bd69d0997ea9d51ad9f87d5edfe12f502ef)
  has new AArch64 work and `retro_serialize_size`, `retro_serialize` and
  `retro_unserialize` in `libretro/main.cpp`. Retain it as a real candidate.
  Those functions do not establish deterministic rollback, compatible peers or
  practical handheld performance. At this review no core had been built or run;
  the later actual-core results are linked above.
- [Libretro's LRPS2 documentation](https://github.com/libretro/docs/blob/master/docs/library/lrps2.md)
  still marks Netplay unsupported and describes x86-only operation. Its
  architecture statement lags the inspected source. Neither that statement nor
  newer serialization alone settles netplay feasibility.

The next PS2 decision gate is a bounded actual-core experiment with isolated
owned BIOS/cards and two clients. This is not a permanent impossibility claim.
Preserve ordinary ARMSX2 and personal data; do not invent a new network protocol
to bypass the missing evidence.

## Existing handoff mechanisms

| Runtime | Native feature | Missing accepted behavior |
| --- | --- | --- |
| [RetroArch](https://docs.libretro.com/development/retroarch/network-control-interface/) / PS1 | Player/spectator switching, including `NETPLAY_GAME_WATCH` | A toggle alone lacks acknowledged, ordered exclusive transfer of P1. Retain the native protocol; party authorization and native role/port acknowledgement are missing. |
| [Dolphin Golf Mode](https://dolphin-emu.org/blog/2019/04/06/netplay-server-browser/) / GameCube | Transfers low-latency input authority | Other players retain their own controller inputs. It is not an exclusive shared P1. |
| Dolphin controller mapping | `SetPadMapping` | The native UI disables Assign Controller Ports during play. Calling the method behind that UI does not prove a safe live handoff. |

Dolphin pin: `c77bbaa0f372c3f72281602a8b087206706542cb`, shared with the installed
TrainerOS bridge. Reviewed paths:

- [NetPlayDialog.cpp](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Source/Core/DolphinQt/NetPlay/NetPlayDialog.cpp):
  `SetOptionsEnabled` disables port assignment while running.
- [NetPlayClient.cpp](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Source/Core/Core/NetPlayClient.cpp):
  `GetNetPads`, `PollLocalPad`, `SendPadHostPoll` and Golf messages operate on
  mapped ports; mapping changes do not themselves drain input queues.
- [NetPlayServer.cpp](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Source/Core/Core/NetPlayServer.cpp):
  Golf authority exchange exists; departure of a mapped non-host disables the
  running game. Seamless pause/pass/reconnect is not supplied by this path.

Remaining work binds accepted party members to native input ownership, rejects
stale/duplicate handoffs, ignores non-owner gameplay input, retains local Home
and touch, and handles loss explicitly. Match the reviewed
[game/mode catalogue](HOTSEAT_GAME_CATALOG.md) to actual editions: empty match
rules cannot authorize title-name guesses. Complete native handoff and real
repeated transfers remain required. Streaming, periodic save exchange and
unverified live remapping were not substituted for this requirement.

The offline continuation advances independent existing routes while these gaps
remain open. PSP Ad Hoc preparation is in [PPSSPP maintenance](emulators/ppsspp.md).
DS, Wii, 3DS, Wii U and Vita remain in scope; PS3/Switch stay excluded.
