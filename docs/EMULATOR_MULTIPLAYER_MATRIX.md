# Emulator multiplayer coverage

**Owner lobby/core priority, 13 October 2026 (latest):** deliver the shared
lobby foundation and keyboard before resuming platform multiplayer integration.
Use a dedicated Social Lobby tab (L2/R2 and touch), game-free creation, stable
membership across games, per-game seats/readiness and all-Ready automatic launch.
Fluxer community categories/channels group genres/topics; each lobby is an
announcement, not its own category/channel set. The owner authorized initial bot
hosting on their server. Preserve existing transports and prior checks. Read
[the accepted contract](LOBBY.md); implementation, deployment and gameplay proof
remain separate. This supersedes older multiplayer-first scheduling below.

**Owner scope update, 13 October 2026 — supersedes all older PS3/Switch exclusions below:** PS3, Nintendo Switch, Original Xbox and Xbox 360 are all **included in the multiplayer planning and network-adapter preparation scope**, without waiting for emulators to become universally fast or compatible. Read [working coverage](MULTIPLAYER_WORKING_MATRIX.md) and [platform mechanisms](MULTIPLAYER_PLATFORM_MATRIX.md). Existing older wording saying PS3/Switch are excluded/deferred is historical, not current authorization. Actual playable online remains gated by available runtime, game and network acceptance. Original Xbox / Xbox 360 are planned registry additions, **not shipped library platforms**. The owner has not yet selected execution priorities; do not create issues or code changes from this note alone.

**Latest owner PS2 decision, 12 October:** native game online first; inspect
installed ARMSX2 DEV9 and reuse the shared TrainerOS flow. Native LAN and
game-server online need distinct evidence. Separately compare ready-made couch
solutions: one virtual console with separate pads from the players' handhelds.
This is what the owner meant by "Hotseat", not P1 handoff; use "Play on one
console" going forward. No couch architecture is selected, LRPS2 stays stopped,
and neither PS2 route is verified.

**12 October N64 private Online delivery:** installed RMG now supports ordinary
Nearby and Online friend invitations on Flip/Odin. Online uses the unchanged
native protocol over private admitted channels; paired forced-relay Mario Kart
64 and ordinary invitation/Home/minimize/Exit passed separately. Wider games,
four physical devices, distinct networks and production service acceptance remain
open. [Evidence](emulators/RMG.md#private-online-delivery--12-october-2026).

**12 October 3DS delivery:** matching Azahar 2126.2 is installed on Flip/Odin.
Native Minicraft3DS wireless gameplay passed Nearby and a forced public relay;
ordinary Online friend invitation separately passed solo consent/leave, shared
world entry, own host save, Home/minimize/same-process return and bilateral Exit.
Each participant owns its ordinary NAND/SDMC; no peer save exchange. Fixed native
USB-error shutdown and distant-relay burst throttling. Two participants/one
homebrew title do not prove retail/update/DLC parity, wider games or distinct
networks. [Evidence, fingerprints and limits](emulators/azahar.md).
Continue Wii U/Vita and retained service/admission/PS2/Hotseat catalogue gates.
PS3/Switch remain excluded; no whole multiplayer issue is closed.

**12 October Wii delivery:** separate emulated Wii Remotes now use ordinary
invitations, consent, solo save/leave, native traversal and Home/minimize/Exit on
Flip/Odin. NSMB Wii reached two-player World 1-1 with opposite independent input
in a separate native check; the ordinary Online friend journey closed both copies
on guest Exit. Progress is temporary and disclosed. Fixed native port ceilings
keep Dolphin/RMG compatible despite incomplete player-count metadata; actual
runtime/content/settings and Hotseat mode remain strict.
[Evidence, hashes and preservation limits](emulators/dolphin.md#wii-remote-delivery--12-october-2026).
Continue 3DS/Wii U/Vita and retained service/admission gates. Four participants,
motion/physical controls, broader games, distinct networks, PS2 feasibility and
the full Hotseat catalogue remain open. PS3/Switch stay excluded; no issue closed.

**12 October GameCube Hotseat:** two-person exclusive P1 is installed alongside
the existing separate-pad route. Native Mario Golf gameplay and ordinary Online
friend/Home handoff evidence are [recorded separately](emulators/dolphin.md#ordinary-gamecube-hotseat-delivery--12-october-2026).
The maintained patch reuses Golf Mode, with one exposed SI port and native owner
confirmation. Wii input, four-person Hotseat, wider-game/physical/distinct-network
acceptance and the complete catalogue remain open.

**12 October PS1 Hotseat:** two-person exclusive P1 transfer is installed on
Flip/Odin alongside separate pads, with ordinary invitations and Home controls.
Hot Shots Golf 2 passed both players' strokes and native handoffs directly and
through the public RetroArch relay. [Evidence and limits](emulators/pcsx-rearmed.md#ordinary-hotseat-delivery--12-october-2026)
retain physical, distinct-network and wider-game acceptance. GameCube/PS2 Hotseat
and the complete catalogue remain open.

**11 October offline continuation:** prepare/implement through Wii U plus Vita;
PS3/Switch are deferred. [Current preparation register](MULTIPLAYER_PREPARATION.md)
records source-level PS1 shared-console and generic raw GameCube admission, with
device delivery still pending. PS1/PS2/GameCube Hotseat is also in scope; ordinary
shared-console ports, exclusive pass-controller and native game networking remain
distinct. Pending hardware acceptance does not block independent preparation.

**10 October generic GBA update:** mGBA Splitscreen supplies linked GBA machines
through the existing RetroArch rollback/relay and automatic SRAM preparation.
Kirby paired gameplay and protected return are recorded in
[MP-02](HANDHELD_MULTIPLAYER.md) and [core maintenance](emulators/mgba-splitscreen.md).
The route now admits 2-4 players with automatic per-member preparation; real four
physical clients, special mechanisms and external acceptance remain open.
Existing gpSP/DoubleCherry/SameBoy routes are preserved.

**10 October battery continuation:** generic supported GB/GBC battery cartridges
now have an installed [SameBoy linked-pair route](emulators/sameboy.md) using
native subsystem SRAM and RetroArch rollback/relay. Automatic invitation/save
preparation, a completed From Below Pocket match and ordinary own-save readback
passed on both handhelds. Existing DoubleCherryGB/gpSP mechanisms remain separate;
[MP-02](HANDHELD_MULTIPLAYER.md) retains all wider compatibility/external gates.

## Owner selection rule — 10 October 2026

Reuse existing multiplayer solutions across **every emulator family**. This is
not a GB/GBC-only instruction and Pokemon does not define the universal product.
For each route, start with the runtime's existing netplay, room, relay or native
game-network implementation; compare maintained compatible alternatives when the
selected runtime has a concrete gap. Existing route evidence below is retained.

TrainerOS supplies the shared invitation, consent, automatic connection, launch,
participant/session lifecycle and protected-save integration. Reuse established
transport/provisioning mechanisms where needed. Do not start a new emulator
network protocol or synchronization engine simply because TrainerOS lacks a
launch adapter. Any necessary upstream patch or port must identify the missing
capability, alternatives considered, bounded changes and maintenance ownership.
This rule also applies to the planned DS source port and other later families.

The mechanisms in the matrix are distinct integration targets, not instructions
to recreate them. Availability in upstream, support in the installed ARM build,
automatic TrainerOS connection and actual gameplay are separate evidence levels.
Neither a game count nor a list of installed emulators establishes completion.
Use representative games to verify each mechanism and document real exceptions;
do not rebuild networking game by game. Preserve the oldest-first execution order
and retained cross-edition, save, recovery and owner-deferred acceptance.

**DS target correction, 9 October 2026:** [#162–166](EXPANSION_161_173.md) adds
native Linux deterministic mirrored melonDS Local Wireless over internet to
MP-06. Older LAN-only integration plans are superseded; ordinary DS launch and
absence of a delivered DS multiplayer adapter remain the actual boundary.
Source port, privacy/firmware/state design and real-game/performance proof are
separate dependencies. UX-02 comes first, then the retained oldest-first queue.

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

**11 October Dreamcast preparation:** [Flycast Dojo](emulators/flycast.md) now has
source integration for two-player CHD GGPO/relay invitations and isolated launch.
Native relay-client fixture checks are not gameplay. Full Linux/ARM64 runtime,
operator service, handheld delivery and separate-network games remain open;
native game servers, Battle Cable and arcade links are distinct modes.

**11 October N64 preparation:** the [RMG native route](emulators/RMG.md) now has
automatic private room/roster handling and an isolated launch bridge through the
existing invitations/lifecycle. Four lobby clients were checked locally,
including TLS; N64 game execution, full runtime delivery and the operator online
service remain pending. Ordinary N64 RetroArch launch stays unchanged.

**10 October MP-02 update:** generic GB/GBC cartridges without battery/RTC storage
can use DoubleCherryGB's existing linked pair over RetroArch rollback/relay,
with per-player screen/audio and assigned controls. Into the Blue invitation and
actual paired gameplay passed on both handhelds. Supported battery cartridges now
use the SameBoy route above; wider hardware/save exceptions remain open. The older
netpacket routes in the table are retained.
See [the dated evidence and limitations](HANDHELD_MULTIPLAYER.md).

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
| PPSSPP | Native PSP Ad Hoc, LAN coordination or external relay. Exact Lumines and generic raw-UMD ISO preparation are two-player profiles. | Retain exact Lumines LAN/relay evidence; generic discs need actual compatibility, device and network checks. Native room selection remains in-game; public relay is not TrainerOS admission enforcement. Infrastructure is separate. [Evidence](emulators/ppsspp.md). | Lumines installed; generic invitations/launch prepared in source. Wider gameplay, private parties and other formats remain open. |
| Dolphin | Native NetPlay synchronizes a shared GameCube/Wii; four GameCube pad slots enable a useful multi-player target. Separate BBA/Wii game-network modes exist and must not be conflated with NetPlay. | Same compatible build/content; map participants to pads, automate host/join/start and report status through existing party flow, isolate user directory/saves, test return/loss. Traversal is rendezvous, not a guaranteed payload relay through strict NAT. | Pinned native bridge installed both; exact Melee US v1.01 invitation, assigned P1/P2 gameplay and Home return verified over LAN. Four-port profile, only two physical clients tested. [Evidence](emulators/dolphin.md). |
| Flycast standalone | GGPO rollback for two players in inspected v2.7; separate native Dreamcast networking/DCnet and arcade link modes. | Isolate config/VMUs, map endpoints/ports/player role, verify compiled GGPO and one game; solve WAN reachability without player router setup. Native online games need their own server/mode profiles. | Historical 2.7 package alignment retained; separate Dojo GGPO/relay invitation and private CHD launch implemented in source. Full runtime build, operator relay and gameplay remain pending. Native ICE serves Battle Cable, not GGPO. [Record](emulators/flycast.md). |
| Azahar standalone | Native wireless room, one independent 3DS per player; initial profile is two people. | Pinned ARM64 build, authenticated automatic loopback room bridge, own NAND/SDMC and matching base content. Installed retail updates/DLC need additional effective identity support. | Installed Flip/Odin: Minicraft3DS Nearby/public-relay gameplay and ordinary Online invitation/Home/save/Exit. Wider games, larger parties, physical/distinct-network and operator acceptance open. [Record](emulators/azahar.md). |
| melonDS | Local Wireless and game WFC are separate. New #162–166 target: native Linux deterministic DS plus peer mirrors over authenticated internet direct/relay. Existing 1.1 LAN GUI/dormant Netplay is not evidence of that target. | Port/review DetMP, local BIOS and private bounded mirror state, existing GameParty/own saves; prove meaningful two-handheld gameplay and recovery, LAN then distinct internet. Measure 3/4-player budgets separately. WFC/VPN are not substitutes. [Dependencies](EXPANSION_161_173.md). | Ordinary DS launch exists; new core/adapter and real multiplayer proof remain planned. |
| ARMSX2 | PS2 DEV9 Ethernet for titles with game networking; inspected upstream also has a UDP Local Link adapter. No general shared-console input-sync route established. | Identify installed build, check DEV9/Local Link availability and a game with native LAN/online support; automate endpoints and isolate memory cards. Any WAN path must satisfy reachability. | Odin ordinary launch exists; network route unverified. |
| PCSX ReARMed / PS1 | Existing RetroArch shared-console netplay; initial route is two digital pads with private BIOS/cards. | Matching CUE/CHD tracks, sidecars, BIOS and core/runtime. No Multitap/system-link or personal card conversion. | [Source preparation](emulators/pcsx-rearmed.md); local checks and handheld delivery tracked separately. |
| DuckStation | Current stock source has no netplay. HeatXD's separate fork has rollback and host-code traversal. | Preserve ordinary emulator choices. The fork is archived; latest published 0.4 is a Windows-only binary from 2023, with ARM delivery unverified. | Not selected as the handheld network route; [fresh source comparison](MULTIPLAYER_PREPARATION.md#ps1-runtime-decision--11-october-2026). |
| GB/GBC/GBA link | Independent netpacket machines for reviewed cable/RFU protocols; generic paired GB/GBC through DoubleCherryGB/SameBoy and 2-4-machine GBA cable parties through mGBA Splitscreen/RetroArch rollback. Ordinary mGBA/Gambatte remain. | Existing invitations/relay, private own SRAM and protected copyback. See [MP-02](HANDHELD_MULTIPLAYER.md) and per-core maintenance records. | Emerald/Red trades, Gold clock return, From Below/Kirby paired play and Mario Golf RFU shots are recorded separately. Four linked Kirby machines are not four physical clients. Universal live-game invitation handoff is delivered in [its record](INVITATION_HANDOFF.md); remaining transactions, broader compatibility and external acceptance stay open. |
| Cemu | Native game online services, including supported Pretendo configurations; no general shared-controller NetPlay established. | Own console/account prerequisites and per-game service admission; keep running the same game where its native online flow permits. | ARM64 package provenance checked on Odin. Both devices lack own OTP/SEEPROM and MLC certificates; paired service gameplay and TrainerOS integration remain unavailable. [Concrete audit](emulators/cemu.md). |
| RPCS3 | RPCN client emulates supported matchmaking APIs; game LAN/custom-service routes are title-specific. | Exact ARM build/game performance, account and service setup, game-specific join flow, process/save ownership; no automatic universal PS3 invite claim. | Odin file present; conditional, not paired gameplay evidence. |
| Vita3K | Native IP LAN is separate from commercial AdHoc and PSN compatibility. | Maintained two-peer network session reuses the admitted encrypted channel. Ordinary Online friend consent, native discovery without IP entry, paired input, Home and bilateral exit are installed on Flip/Odin. | One exact rebuilt vitaQuake profile; own installed data/save paths, strict effective-content identity. Commercial games/services, broader groups and distinct networks remain open. [Evidence](emulators/vita3k.md). |
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
