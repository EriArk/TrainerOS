# Emulator multiplayer coverage

**Availability, 2026-10-04:** implemented eligible routes are accessible in normal
TrainerOS sessions; no developer opt-in is required. This does not turn installed
emulators without adapters into multiplayer routes or close unverified scenarios.

2026-10-03. Source/device audit and implementation handoff, updated with the
[NES online checkpoint](NES_MULTIPLAYER.md). The [common emulator set](EMULATOR_STANDARD.md) chooses
image defaults; this matrix records the separate network capabilities. The owner
explicitly requires emulator coverage beyond RetroArch/PPSSPP before treating the
online-game block as complete. Keep voice work queued, not a substitute milestone.

2026-10-04 update: [shared RetroArch profiles](RETROARCH_MULTIPLAYER_PROFILES.md)
now cover reviewed classic core/platform pairs beyond the original title list.
Generic profiles default to two pads; reviewed Snes9x/PCE-family layouts select
three/four from metadata, while exact NES Four Score keeps four. European Battle
Circuit has private cabinet preparation and four-active-player relay evidence;
other arcade sets remain two. FBNeo dependencies are title-specific. This expands
implementation, not the ordinary release gate or all-game runtime evidence.

## Current TrainerOS boundary

`RuntimeMultiplayer.cpp` dispatches the inspected PSP and GameCube profiles to
`PpssppNetplay` and `DolphinNetplay`, with other eligible records handled by
`RetroArchNetplay`. Ordinary discovery covers PPSSPP, ARMSX2, melonDS, Dolphin
and RetroArch; installed packages do not automatically acquire network adapters.
Flycast 2.7 is now aligned on both handhelds, but its standalone invitation/launch
adapter remains absent. Retain ordinary one-A launch through existing routes.

**Owner priority:** internet is the primary everyday scenario. LAN and direct
nearby play remain required, but local-only checks do not complete a runtime's
online route. Resolve automatic reachability before calling an emulator delivery
complete; no player-entered IPs, router configuration or personal VPN setup.

## Route matrix

| Runtime | Actual mechanism / capacity | Required integration and acceptance | Status |
| --- | --- | --- | --- |
| RetroArch, deterministic cores | Shared emulated machine; native direct/relay transport. Slots depend on core/game/peripheral profile, not a universal pair limit. | Align core builds, implement verified 3-4-player port/multitap profiles, finish paired online controls, loss/rejoin, separate networks and parallel parties. Not a generic Game Boy link cable. [Evidence](EMULATOR_MULTIPLAYER.md). | Integrated experimental route; incomplete acceptance. |
| PPSSPP | Native PSP ad hoc, LAN coordination or external relay. Current exact Lumines profile is two-player. | Retain passed short LAN/relay gameplay. Finish mid-round loss, separate networks, same-title party isolation and other selected profiles. A public relay room is not TrainerOS admission enforcement. [Evidence](PSP_MULTIPLAYER.md). | Integrated experimental route; incomplete acceptance. |
| Dolphin | Native NetPlay synchronizes a shared GameCube/Wii; four GameCube pad slots enable a useful multi-player target. Separate BBA/Wii game-network modes exist and must not be conflated with NetPlay. | Same compatible build/content; map participants to pads, automate host/join/start and report status through existing party flow, isolate user directory/saves, test return/loss. Traversal is rendezvous, not a guaranteed payload relay through strict NAT. | Pinned native bridge installed both; exact Melee US v1.01 invitation, assigned P1/P2 gameplay and Home return verified over LAN. Four-port profile, only two physical clients tested. [Evidence](emulators/dolphin.md). |
| Flycast standalone | GGPO rollback for two players in inspected v2.7; separate native Dreamcast networking/DCnet and arcade link modes. | Isolate config/VMUs, map endpoints/ports/player role, verify compiled GGPO and one game; solve WAN reachability without player router setup. Native online games need their own server/mode profiles. | Same 2.7 package and binary verified on both devices; no TrainerOS standalone/network adapter. Native ICE serves Battle Cable, not GGPO. [WAN finding](emulators/flycast.md). |
| Azahar standalone | Each player owns an emulated 3DS, connected through a room server for local wireless. Room membership limit is not game capacity. | Common ARM64 client build, scoped room allocation/cleanup, automatic join, exact content/update compatibility and retained per-player saves. Need a reachable managed room for internet, not manual public-room browsing. | Flip candidate file; no paired proof or TrainerOS network adapter. |
| melonDS | Local wireless LAN sessions and game WFC internet access are separate. Inspected 1.1 GUI exposes LAN host/join; dormant Netplay source does not prove working internet-local-wireless. | Integrate LAN entry/status first; prove a real DS connection with separate saves. WFC requires a supported replacement service/game profile. Do not sell an internet VPN as a fix for timing-sensitive local wireless. | DS launch exists; multiplayer integration absent. |
| ARMSX2 | PS2 DEV9 Ethernet for titles with game networking; inspected upstream also has a UDP Local Link adapter. No general shared-console input-sync route established. | Identify installed build, check DEV9/Local Link availability and a game with native LAN/online support; automate endpoints and isolate memory cards. Any WAN path must satisfy reachability. | Odin ordinary launch exists; network route unverified. |
| DuckStation | Standard installed build has no TrainerOS netplay route. A separate third-party netplay fork exists; its features cannot be attributed to stock DuckStation. | Keep ordinary PS1 primary. Evaluate one compatible libretro route or maintained fork only with ARM build, matching, memory-card and paired-control proof; never silently swap a user's runtime/save format. | Unresolved PS1 multiplayer route. |
| GB/GBC/GBA link | Distinct emulated handhelds need actual cable/wireless emulation. mGBA upstream still lists networked link as planned; local windows are a different feature. | Evaluate a genuine network-link implementation separately if required; preserve current Emerald semantic Link as a different feature. Do not count shared input or saved-Pokemon exchange as active ROM multiplayer. | No general active-ROM link route proven. |
| Cemu | Native game online services, including supported Pretendo configuration, rather than a proven general shared-controller netplay path. | Linux ARM build provenance/performance, per-player online prerequisites and game-specific service admission. Do not copy account/console credentials between players or claim a VPN supplies a game service. | Conditional family; no TrainerOS multiplayer integration. |
| RPCS3 | RPCN client emulates supported matchmaking APIs; game LAN/custom-service routes are title-specific. | Exact ARM build/game performance, account and service setup, game-specific join flow, process/save ownership; no automatic universal PS3 invite claim. | Odin file present; conditional, not paired gameplay evidence. |
| Vita3K | Current upstream FAQ documents partial ad hoc support for some games using overlays. This is not general PSN support. | Identify Odin build, prove a title and adapter, then assess the already-defined managed overlay candidate if needed. No player-managed VPN as normal UX. | Conditional family; no TrainerOS launch/network integration established here. |
| Eden / Switch | Upstream ships Linux ARM64 client and room binaries; room networking must be evaluated per game/mode. Ryujinx LDN is another ecosystem, not interchangeable proof. | First ordinary ARM launch/performance and exact room protocol; then managed room/client join, save/update/firmware isolation and paired play. Do not conflate local wireless with Nintendo online services. | Selected image candidate, absent from bounded device inventory. |
| Other RetroArch families | The catalogue includes many classic systems; core installation alone does not establish deterministic netplay. | Enable only compatible game/core/mode profiles with meaningful controls and save behavior. Preserve offline launch for the rest. | No blanket online claim for all 105 folders. |

## Source evidence

Sources inspected 2026-10-03; source behavior is separate from installed-build
and real-game evidence. No single web guide settles every platform.

- Dolphin: [official guide](https://dolphin-emu.org/docs/guides/netplay-guide/),
  [2606a command-line parser](https://github.com/dolphin-emu/dolphin/blob/2606a/Source/Core/UICommon/CommandLineParse.cpp),
  [native setup](https://github.com/dolphin-emu/dolphin/blob/2606a/Source/Core/DolphinQt/NetPlay/NetPlaySetupDialog.cpp).
  Both installed `--help` outputs agree: `--user`, `--config`, `--exec` exist;
  no host/join/start NetPlay command. Merely writing a host address into an INI
  does not initiate NetPlay. A maintained startup/control patch is a concrete
  option under the owner's image authority; desktop click automation is not the
  shipping integration. Strict-NAT fallback remains a separate transport gate.
- Flycast: [v2.7 options](https://github.com/flyinghead/flycast/blob/v2.7/core/cfg/option.cpp)
  and [GGPO implementation](https://github.com/flyinghead/flycast/blob/v2.7/core/network/ggpo.cpp).
  The latter has `MAX_PLAYERS = 2`, local/remote player handles and peer address/
  port configuration; do not infer four network players from four Dreamcast pads.
  DCnet and GGPO are different paths. No Flycast Dojo capability is assumed for
  the installed upstream Flatpak.
- melonDS: [1.1 menu](https://github.com/melonDS-emu/melonDS/blob/1.1/src/frontend/qt_sdl/Window.cpp),
  [LAN dialog](https://github.com/melonDS-emu/melonDS/blob/1.1/src/frontend/qt_sdl/LANDialog.cpp),
  [Netplay source](https://github.com/melonDS-emu/melonDS/blob/1.1/src/net/Netplay.cpp)
  and [developer LAN/netplay distinction](https://melonds.kuribo64.net/comments.php?id=190&p=1).
  Historical warnings are not a fresh 1.1 latency benchmark; current menu/source
  inspection still does not establish an internet-local-wireless route.
- Azahar: inspected source `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`,
  [room CLI](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/citra_room/citra_room.cpp)
  supplies room name, port, maximum members and password. Client automatic join
  still needs a supported entry point/patch; server CLI alone does not finish it.
  [Archived upstream architecture](https://citra.azahar-emu.org/help/feature/multiplayer/)
  describes separate consoles forwarding wireless traffic through rooms.
- ARMSX2: inspected source `9d989ca933a85bb2f1d111fd3b9e5742ac7d2fbe`,
  [LocalLinkAdapter.cpp](https://github.com/ARMSX2/ARMSX2/blob/9d989ca933a85bb2f1d111fd3b9e5742ac7d2fbe/pcsx2/DEV9/LocalLinkAdapter.cpp)
  uses DEV9 enablement, host/address/port, room code and peer identity. These keys
  cannot be assumed present in Odin's unidentified AppImage version.
- [DuckStation upstream](https://github.com/stenzek/duckstation),
  [separate netplay fork](https://github.com/HeatXD/duckstation-netplay/releases),
  [mGBA planned network-link feature](https://github.com/mgba-emu/mgba#planned-features),
  [Libretro netplay FAQ](https://docs.libretro.com/guides/netplay-faq/).
- [Cemu online guide](https://cemu.cfw.guide/online-play),
  [Pretendo's Cemu prerequisites](https://pretendo.network/docs/install/cemu),
  [RPCS3 RPCN implementation](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/NP/rpcn_client.h),
  [Vita3K multiplayer FAQ](https://vita3k.org/faq.html),
  [Eden ARM/client-room releases](https://git.eden-emu.dev/eden-emu/eden/releases).

## Implementation order within the current multiplayer block

Owner correction, 2026-10-03: **oldest families first, internet first**. The earlier
Dolphin -> Flycast -> Azahar expansion queue is superseded. Existing code and
package work remain; they do not close older-family acceptance.

1. NES, followed by the existing Mega Drive/SNES RetroArch families. Complete
   Online friend invitations, reverse joining, group access, actual controls,
   Home return/reinvitation and bounded loss recovery through the existing frame.
   Keep parallel parties, supported slot capacity and external-relay versus
   distinct-network evidence explicit. No replacement social/lobby architecture.
2. Older handheld/link and 32/64-bit families: genuine GB/GBC/GBA linking remains
   separate from sharing one emulated console. Resolve PS1/N64/other selected
   old-system routes before using new-emulator expansion as the next milestone.
3. Dreamcast, PS2/GameCube, DS/PSP, then 3DS and newer systems. Resume the retained
   Flycast WAN investigation here; preserve and finish already-implemented PSP
   and Dolphin work. Document real unsupported mechanisms rather than pretending
   a launch-only adapter provides multiplayer.
4. Cross-cutting checks belong to each relevant route: different internet
   networks, interruption/reinvitation, departure, parallel parties, background
   call retention and fresh-client automatic setup. Test real 3-4-client gameplay
   when that game's mechanism supports it; two devices cannot prove four clients.

The matrix does not require testing every ROM or installing obscure platforms.
One representative game per real connection mechanism plus explicit compatibility
boundaries is the initial target. Local/direct play remains in scope. Owner
physical checks and separate party voice remain retained, not silently dropped.
