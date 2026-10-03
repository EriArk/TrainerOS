# Emulator multiplayer coverage

2026-10-03. Source/device audit and implementation handoff. **No new gameplay
acceptance in this pass.** The [common emulator set](EMULATOR_STANDARD.md) chooses
image defaults; this matrix records the separate network capabilities. The owner
explicitly requires emulator coverage beyond RetroArch/PPSSPP before treating the
online-game block as complete. Keep voice work queued, not a substitute milestone.

## Current TrainerOS boundary

`RuntimeMultiplayer.cpp` currently dispatches PSP to `PpssppNetplay` and other
eligible records to `RetroArchNetplay`. Its standalone launch branch likewise has
only PPSSPP. `EmulatorDiscovery.cpp` discovers ordinary PPSSPP, ARMSX2, melonDS,
Dolphin and RetroArch launches; ordinary discovery does not create multiplayer
support. The profile allowlist remains narrower than installed games/platforms.
All other rows below require actual TrainerOS integration before invitations can
be offered for them. Retain ordinary one-A launch when networking is unavailable.

## Route matrix

| Runtime | Actual mechanism / capacity | Required integration and acceptance | Status |
| --- | --- | --- | --- |
| RetroArch, deterministic cores | Shared emulated machine; native direct/relay transport. Slots depend on core/game/peripheral profile, not a universal pair limit. | Align core builds, implement verified 3-4-player port/multitap profiles, finish paired online controls, loss/rejoin, separate networks and parallel parties. Not a generic Game Boy link cable. [Evidence](EMULATOR_MULTIPLAYER.md). | Integrated experimental route; incomplete acceptance. |
| PPSSPP | Native PSP ad hoc, LAN coordination or external relay. Current exact Lumines profile is two-player. | Retain passed short LAN/relay gameplay. Finish mid-round loss, separate networks, same-title party isolation and other selected profiles. A public relay room is not TrainerOS admission enforcement. [Evidence](PSP_MULTIPLAYER.md). | Integrated experimental route; incomplete acceptance. |
| Dolphin | Native NetPlay synchronizes a shared GameCube/Wii; four GameCube pad slots enable a useful multi-player target. Separate BBA/Wii game-network modes exist and must not be conflated with NetPlay. | Same compatible build/content; map participants to pads, automate host/join/start and report status through existing party flow, isolate user directory/saves, test return/loss. Traversal is rendezvous, not a guaranteed payload relay through strict NAT. | Installed both; no TrainerOS netplay adapter. First new family. |
| Flycast standalone | GGPO rollback for two players in inspected v2.7; separate native Dreamcast networking/DCnet and arcade link modes. | Isolate config/VMUs, map endpoints/ports/player role, verify compiled GGPO and one game; solve WAN reachability without player router setup. Native online games need their own server/mode profiles. | Odin 2.7 only; no TrainerOS network adapter. |
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

1. Adopt the common runtime/core bundle contract; resolve current differences
   with versioned artifacts and save-preserving migration, not ad hoc nightly
   updates. Existing RetroArch/PPSSPP gaps stay open; do not repeat passed matches.
   First [installed artifact baseline](emulators/retroarch.md): 72 matching cores
   and three common Flatpak application revisions on both devices, with rollback.
   Remaining standalone migrations and release packaging are still open.
2. **Dolphin** end-to-end integration: isolated profile, native host/client/start
   bridge, participant-to-pad assignment, controller-driven invitation, game,
   exit and recovery. First GameCube, then the separately configured Wii route.
   Use a title supporting four local players to extend beyond pair-only profiles.
3. **Flycast** exact two-player GGPO route, then **Azahar** scoped room/client
   integration. Complete each functional chain rather than exposing setup pages
   for several unfinished runtimes. Keep native Dreamcast services separate.
4. **melonDS** real local wireless and supported WFC path; **ARMSX2** native LAN
   profile. Use a managed overlay only where this concrete route needs it and
   has passed the [automation gates](MULTIPLAYER_AUTOMATION.md).
5. Resolve the remaining PS1/GB/GBA mechanisms and conditional heavy systems
   using the matrix. A known unavailable feature may remain unavailable; an
   uninvestigated route is not the same as an unsupported emulator.
6. Finish cross-cutting acceptance: separate internet networks, abrupt mid-game
   loss, host/guest departure, 3-4 actual clients, concurrent same-title parties,
   background call retention and fresh-device automatic connection. Two handhelds
   cannot establish four-handheld proof; extra actual clients must be identified.

The matrix does not require testing every ROM. One representative game per real
connection mechanism plus explicit compatibility boundaries is the initial target.
User-operated controls/audio remain deferred. Separate party voice is retained
after the currently prioritized emulator coverage, not dropped from the plan.
