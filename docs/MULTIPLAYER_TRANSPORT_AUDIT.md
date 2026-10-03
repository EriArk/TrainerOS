# Multiplayer transport and session audit

2026-10-03. **Read-only audit and proposed experiments, not a transport decision.**
The owner suggested virtual LAN for evaluation and explicitly did not request a
rewrite. Accepted UX lives in [Multiplayer experience](MULTIPLAYER_EXPERIENCE.md);
the [roadmap](ROADMAP.md) schedules work. No new network service was installed,
device configuration changed or runtime test performed for this documentation pass.

## Local evidence and gaps

| Finding | Evidence / consequence |
| --- | --- |
| Runtime coordination is currently pair-oriented | `src/features/adventure/RuntimeMultiplayer.h` owns one `OnlineLink`, one `LocalLinkPeer`, singular `peerId_`/`onlinePerson_` and one request. Its .cpp connects/closes that one peer. A list of invite buttons alone cannot provide multi-member parties. Audit lower-level ownership before extending it. |
| Existing routes must be retained | [Runtime evidence](EMULATOR_MULTIPLAYER.md) records Contra LAN P1/P2, Streets online P2 and relay/return checks with precise limits. These are useful baselines, not universal multiplayer acceptance. |
| PSP is already failing on real LAN | [Lumines evidence](PSP_MULTIPLAYER.md): native discovery and challenge succeed; lobby-to-match transition fails in both challenge directions. A VPN alone has no demonstrated reason to repair this failure. |
| Deployment is experimental | Runtime multiplayer remains disabled in ordinary deliveries. No company/multi-party/request-to-join implementation is claimed by the new contract. |
| Two handhelds are insufficient for full four-device evidence | Extra clients may establish protocol/session behavior, but desktop/virtual clients do not prove four ARM handhelds or owner-operated controls. Record topology and limits. |

## What upstream documentation establishes

Sources reviewed 2026-10-03; installed-version behavior still needs runtime proof.

| Route / source | Finding | Implication |
| --- | --- | --- |
| [RetroArch protocol](https://docs.libretro.com/development/retroarch/netplay/) and [controller assignment](https://docs.libretro.com/guides/netplay-multiple-controllers/) | Protocol supports up to 16 players; docs demonstrate four players on two clients. Deterministic core/content compatibility and appropriate input configuration remain necessary. | Capacity is game/core/mode-specific. Shared emulated-console controller synchronization is not link-cable emulation. |
| [PPSSPP quickstart](https://www.ppsspp.org/docs/multiplayer/quickstart/) | Native ad hoc LAN uses one built-in server and peer addresses; modern releases also provide relay mode. | Compare supported native relay with virtual-LAN-backed LAN; both still depend on PPSSPP's own game networking implementation. |
| [melonDS 1.0 RC developer explanation](https://melonds.kuribo64.net/comments.php?id=216&p=4) | That LAN implementation requires tight timing and explicitly warns against VPN/internet tunnels. | A working LAN mode does not establish internet play. Recheck exact current version before any broader claim; this historical warning is not a current-version test. |
| [ZeroTier protocol](https://docs.zerotier.com/protocol/) | Virtual Ethernet supports broadcast/multicast, direct paths and slower fallback relaying. | Useful comparison candidate for LAN discovery and reachability. It is not a latency cure or a selected dependency. |

Virtual LAN can simplify addresses, NAT traversal and some local discovery. It
does not replace deterministic synchronization, fix emulator bugs, make paired
editions interchangeable, create unsupported game modes or remove internet
latency/jitter. Native netplay can run over a virtual LAN: these are layers, not
mutually exclusive multiplayer implementations.

## Comparison to run before selecting a route

Start with a title whose ordinary LAN gameplay works; retain Lumines as a
separate failing control rather than the sole benchmark. Compare the same
content/runtime/settings across real LAN, native internet relay/direct route,
and virtual LAN. Do not repeat already-passed invitation-only walkthroughs.

| Question | Required observation |
| --- | --- |
| Is a match playable? | Deliberate independent player actions, synchronization and correct save ownership; lobby visibility/ping/connected sockets are insufficient. |
| Is it better? | Setup time, RTT/jitter/loss, observable stalls/desync, CPU/memory and background voice continuity under comparable conditions. Record direct versus relayed path. |
| Does it handle a company? | 3-4 actual clients, distinct slots, simultaneous last-slot requests, two parallel parties, late joins where supported, guest and host departure. |
| Does it work over internet? | Two genuinely different internet connections, NAT restrictions, interruption/rejoin and cleanup. A public relay used from one home network is narrower evidence. |
| Can nearby and remote users mix? | Same party with both routes without duplicate identity, routing loops, changed voice destination or forced Wi-Fi disconnection. |
| Is it maintainable? | ARM64/Armada/Flatpak access, privileges, supported configuration APIs, updates, reversible install and stale-interface/session cleanup. |

For any candidate, inspect discovery requirements (broadcast/multicast versus
explicit endpoint), MTU and fallback behavior. Limit access to the intended
participants and necessary game traffic; do not bridge home networks or put all
TrainerOS users in one shared virtual LAN. Avoid new user accounts/manual network
IDs in normal UX. Verify redistribution/embedding terms, hosted limits, operating
cost and self-hosted controller/relay needs before choosing a product. No spending
or service deployment is authorized by this audit note.

## Open implementation decisions

1. Map persistent companies to existing supported Fluxer group/community objects;
   preserve membership limits and permissions rather than build a duplicate chat.
2. Determine how independent party voice audiences fit actual provider facilities.
   A company DM must not be assumed to support arbitrary parallel subcalls.
3. Extend session ownership/participants, admission and organizer authority with
   minimal service changes. Keep company membership, party membership, voice and
   network reachability separate. Define stale-party cleanup and access revocation.
4. Select transport per runtime family from measurements. ZeroTier is a first
   candidate, not a mandate; evaluate an alternative only for a concrete unmet
   requirement. Keep working native routes and the existing Bluetooth/Direct policy.
5. Record limits for host departure, persistence, late admission, mixed topology
   and paired-edition compatibility. Do not promise automatic host migration.

## Exit from audit

Produce a short route recommendation per tested runtime: retain native route,
adopt overlay for that route, or keep unsupported. Include reproducible evidence,
unresolved limits and operating requirements. A partial result may preserve a
working route; it must not mark block 1 complete. Required wider changes follow
the accepted roadmap, not an automatic rewrite triggered by this investigation.
