# Multiplayer transport and session audit

2026-10-03. **Read-only audit and proposed experiments, not a transport decision.**
The owner suggested virtual LAN for evaluation and explicitly did not request a
rewrite. Accepted UX lives in [Multiplayer experience](MULTIPLAYER_EXPERIENCE.md);
the [roadmap](ROADMAP.md) schedules work. No new network service was installed,
device configuration changed or gameplay test performed for this audit pass.
Code/provider inspection and a read-only inventory of both handhelds were completed
on 2026-10-03. Comparative gameplay measurements remain pending.

**Owner usability gate, 2026-10-03:** first establish whether a candidate can
support the [automatic connection contract](MULTIPLAYER_EXPERIENCE.md#connection-setup-requirement).
At most account setup in the initial wizard is acceptable; no player-managed
VPN/network/device enrollment or router configuration. Existing Tailscale is a
lab convenience only. A successful manually prepared tunnel can inform technical
comparison but cannot qualify a product route. Audit supported provisioning APIs,
account boundaries and operated services before spending time on that candidate's
full gameplay benchmark. Fresh-device setup and reconnect are acceptance gates.

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
4. Select transport per runtime family from measurements. Tailscale is already
   installed on both handhelds and is a candidate for explicit-endpoint trials;
   ZeroTier remains a candidate where virtual Ethernet discovery is needed.
   Neither is a selected dependency. Keep working native routes and the existing
   Bluetooth/Direct policy.
5. Record limits for host departure, persistence, late admission, mixed topology
   and paired-edition compatibility. Do not promise automatic host migration.

## Exit from audit

Produce a short route recommendation per tested runtime: retain native route,
adopt overlay for that route, or keep unsupported. Include reproducible evidence,
unresolved limits and operating requirements. A partial result may preserve a
working route; it must not mark block 1 complete. Required wider changes follow
the accepted roadmap, not an automatic rewrite triggered by this investigation.

## Completed source and device audit — 2026-10-03

Baseline: TrainerOS `c107a0e`, with runtime implementation from `82436c1`.
The findings below are source/configuration evidence, not new multiplayer proof.
Private inventory captures are under ignored `work/research/multiplayer-audit/`;
no account credentials, private network identities or downloaded third-party
source are committed. Both devices were reached and their models verified.

### Concrete session gaps

| Location | Verified limitation | Required change |
| --- | --- | --- |
| `src/features/adventure/RuntimeMultiplayer.h/.cpp` | Singular peer/request ownership; `canInvite()` rejects an already active multiplayer session, and `incoming()` excludes a running Adventure. | One local game session with several member connections; accept invitations while playing through the existing explicit exit/restart flow. Do not create several emulator processes per handheld. |
| `src/platform/network/LocalLinkPeer.h/.cpp` | One `QTcpSocket`; additional connections are rejected. The pending-connection backlog is not a player limit. Discovery broadcasts on all eligible IPv4 interfaces and merges addresses by peer identity. | Multi-member runtime signalling plus explicit route/interface scope. A VPN interface must not silently turn distant players into Nearby or unpredictably replace the selected route. Preserve the separate saved-Link pair contract. |
| `src/integrations/social/OnlineLink.h/.cpp` | One peer, DM, invitation and session; a connected session rejects additional probes. | Add a narrowly scoped runtime-party coordinator/envelope, reusing provider delivery. Keep the protected two-person native Link protocol intact rather than globally loosening it. |
| `src/integrations/social/FluxerSession.cpp` | Runtime probes and received protocol messages require an existing friend and a one-recipient DM. One `online_` instance is shared with native Link. | Group membership and compatible-client admission need an explicit supported path; ordinary group members need not all become mutual friends if provider permissions allow communication. Dispatch runtime-party and saved-Link traffic separately. |
| `src/integrations/adventure/retroarch/RetroArchNetplay.cpp` | Temporary config sets `netplay_max_connections = "1"`. | Derive guest capacity from the game/mode and configure distinct controller ports/multitap. Changing this number alone does not establish four-player support. |
| `src/integrations/social/FluxerSession.cpp` | Voice accepts private channel types 1/3, sends `guild_id = null`, and owns one active voice destination. | Add supported community voice when needed for independent party audiences. Preserve one deliberately selected call and microphone; joining a game must not switch it. |

Retain the existing per-guest `NetplayClient` bridge: a guest needs one connection
to its game's host/relay, so that class being singular is not itself a global
two-player restriction. Likewise, one active local Adventure is correct. Extend
membership around these boundaries instead of multiplying every service.

### Fluxer contracts and company recommendation

Inspected upstream commit
[`cdcaba34ce538ee75c73feaa5aedc0331085add9`](https://github.com/fluxerapp/fluxer/tree/cdcaba34ce538ee75c73feaa5aedc0331085add9).
This pins source evidence; it does not assert the public deployment runs that
commit or that new operations were exercised against real accounts.

- **Activity needs a TrainerOS descriptor.** Published presence has custom status
  but no activities. Do not assume Discord rich-presence/join-secret APIs, overwrite
  the user's status or infer compatibility from a name. See the upstream
  [presence contract](https://github.com/fluxerapp/fluxer/blob/cdcaba34ce538ee75c73feaa5aedc0331085add9/fluxer_docs/src/content/docs/gateway/commands.md).
- **A group DM has one common call.** Each private channel maps to one voice room;
  separate community voice channels map to separate rooms. Ringing selected people
  changes notification recipients, not the audio audience. A client-side filter
  cannot create independent calls inside that room. See
  [voice routing](https://github.com/fluxerapp/fluxer/blob/cdcaba34ce538ee75c73feaa5aedc0331085add9/fluxer_docs/src/content/docs/voice/index.md)
  and [calls](https://github.com/fluxerapp/fluxer/blob/cdcaba34ce538ee75c73feaa5aedc0331085add9/fluxer_docs/src/content/docs/http-api/calls.mdx).
- **Company chat can reuse a group DM; parallel private party voice needs more.**
  Recommended full model: a community-backed company with a common text/voice
  space and distinct party voice channels. Joining requires the real channel
  permissions; creating/managing channels requires organizer authority. Reuse
  existing suitable channels or organizer-managed ones, without granting every
  player administrative powers. Ordinary groups retain common voice; do not
  silently convert them or create a duplicate group DM for every game. This is a
  recommendation pending implementation choice, not a new requirement to migrate
  existing chats. See [guild channels](https://github.com/fluxerapp/fluxer/blob/cdcaba34ce538ee75c73feaa5aedc0331085add9/fluxer_docs/src/content/docs/http-api/guild-channels.mdx).
- **Membership limits belong to the provider.** The reviewed private-channel
  contract defaults to 50 group members including the caller, with instance
  limits and ordinary privacy/relationship rules. Read supported limits instead
  of equating company size with the game's slots. Creating extra party DMs also
  consumes group limits and leaves visible conversations, so it is not a free
  invisible-room workaround. See [private channels](https://github.com/fluxerapp/fluxer/blob/cdcaba34ce538ee75c73feaa5aedc0331085add9/fluxer_docs/src/content/docs/http-api/users/private-channels.mdx).

The current capability exchange is an ordinary persisted message, hidden only by
TrainerOS's presentation. Suppressing notification flags does not make it an
invisible provider channel. Use bounded, event-driven activity publication/query
and fresh admission checks, not heartbeat messages to every contact. Distinguish
party identity/revision and account/device session; expire stale cards, preserve
privacy/invisible state, and do not publish connection secrets in company cards.
These details need implementation within supported provider contracts, not a
second messenger or an assumed custom Gateway event.

### Read-only handheld inventory

| Item | Flip | Odin 2 | Meaning |
| --- | --- | --- | --- |
| OS / architecture | Armada `20260929.5915c28`, ARM64 | Same | Actual installed baseline, not a compatibility promise for other images. |
| RetroArch / PPSSPP | `1.22.2` / `1.20.4` | Same | Both Flatpaks have shared host-network permission. Actual overlay reachability remains untested. |
| Other relevant runtimes | Dolphin `2606a` | Dolphin `2606a`, melonDS `1.1`, Flycast `v2.7` | Installed does not mean multiplayer integration or gameplay has passed. |
| Tailscale | `1.98.8`, daemon inactive | `1.98.8`, daemon active and authenticated | Existing explicit-address overlay candidate. Do not overwrite Odin's current network enrollment. |
| ZeroTier | CLI/service not found in bounded inventory | Same | Not installed by this audit. |
| Tunnel device | `/dev/net/tun` present | Present | Presence is not proof of helper privileges or a functioning overlay. |

TrainerOS was running on both; no inspected emulator process was active. No
daemon was enabled, network enrolled, voice call opened or device rebooted.

### Transport recommendations from this evidence

| Family | Recommendation now | Remaining decision evidence |
| --- | --- | --- |
| RetroArch deterministic netplay | Keep existing native LAN/relay routes and the working bridge. Extend game-specific player slots first. | Comparable actual play over a known-address overlay only if it offers a concrete reachability/quality benefit; three/four clients and separate internet networks remain open. |
| PPSSPP | Keep the isolated native integration. Installed `1.20.4` supports the documented modern relay family; compare native relay before adopting a mandatory VPN. | Resolve or isolate the existing real-LAN Lumines match-transition failure, then compare a working game. Validate isolation of simultaneous parties playing the same title. |
| melonDS | Keep as an unverified separate family. Version `1.1` has LAN and Netplay source/build entries; the old developer warning alone cannot settle its current internet capability. | Exact mode, game, timing and actual two-device match proof. An explicit LAN host address is possible in source, but does not prove VPN play or native online-service compatibility. |
| Overlay | Tailscale is a practical explicit-endpoint comparison candidate already present; ZeroTier is relevant if a route truly needs virtual Ethernet/broadcast. | Isolated party reachability, discovery, direct/relay path, latency/jitter, mixed topology, voice retention and cleanup. No winner is selected without these observations. |

Sources: [PPSSPP multiplayer](https://www.ppsspp.org/docs/multiplayer/how-to-play/),
[PPSSPP relay hosting](https://www.ppsspp.org/docs/multiplayer/setting-up-relay-servers/),
[melonDS 1.1 LAN implementation](https://github.com/melonDS-emu/melonDS/blob/b86390e4428bf38ce4c1ce0e9ca446d6d25955e8/src/net/LAN.cpp),
[melonDS build entries](https://github.com/melonDS-emu/melonDS/blob/b86390e4428bf38ce4c1ce0e9ca446d6d25955e8/src/net/CMakeLists.txt),
[Tailscale routing](https://tailscale.com/docs/features/site-to-site), and
[ZeroTier protocol](https://docs.zerotier.com/protocol/).
Do not assume an explicit-address overlay supplies Ethernet broadcast discovery.

For distribution, a working tunnel is not yet zero-setup onboarding. Assess
account/controller provisioning separately. ZeroTier's current
[hosted plans](https://www.zerotier.com/pricing.md) restrict API availability;
a [self-hosted controller](https://docs.zerotier.com/what-is-a-controller/) is
network admission, not automatically a managed fallback-relay service.
The inspected ZeroTier source separates its
[main license](https://github.com/zerotier/ZeroTierOne/blob/899352e38405968516bb12a770f0ac02f6058fa8/LICENSE.txt)
and [nonfree component terms](https://github.com/zerotier/ZeroTierOne/blob/899352e38405968516bb12a770f0ac02f6058fa8/nonfree/LICENSE.md).
Pin the intended client/controller components and verify their distribution terms
before choosing them for the image; this audit does not authorize paid services.

### Implementation handoff and remaining audit boundary

1. Keep the native runtime baseline. Establish a small runtime-party model with
   organizer, scoped company, game/mode, capacity, members/readiness, revision and
   expiry. Use one admission path for invitation acceptance, reverse requests and
   explicitly permitted Join; reserve the last slot atomically and release it on
   cancellation/expiry. Make organizer departure explicit, without promised host
   migration. Do not change the saved-Pokemon transaction protocol.
2. Represent several parties in the existing conversation/Home surfaces and route
   each member to the correct party. Provider membership, current compatibility
   and access are checked at admission. One handheld joins one game; browsing
   other party cards never launches or switches voice.
3. Add the supported company/party voice mapping described above. Do not present
   a party-private voice option until its real audience exists. Keep existing DM
   calls and normal contacts working independently of TrainerOS capabilities.
4. Execute the comparison matrix above with actual gameplay and explicit topology,
   then select routes. Do not turn this audit's source findings into a claim that
   four-player, virtual-LAN, group voice or mixed-network play has passed.

The code/provider/inventory portion is complete. The empirical comparison and
the implementation/acceptance work remain open under the existing roadmap.
No functional issue or numbered communication block is closed by this report.
