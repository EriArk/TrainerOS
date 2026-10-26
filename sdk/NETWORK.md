# Native activity + reliable records, protocol v1 preview

**25 October 2026, prepared source:** group records extend this contract to
2–16 players through a host-star topology. This change is not an installed
handheld build. Lobby UI is owned by a parallel task; see the
[integration boundary](../docs/SDK_GROUP_LOBBY_HANDOFF.md).

Multiplayer starts in the **existing GameHub Lobby**, for every SDK game:
choose an installed game, prepare, Ready, then the organizer presses Start.
Only Start launches each local game through TrainerOS's existing protected
launch lifecycle. The client attaches to that admitted session after startup.
Do not require players to open the game first or build a second room system.
Registration alone never creates a room, launches a game or authorizes traffic.

## Installed game declaration

A Crates game includes `traineros-activity.json` at its package root:

```json
{"schema":1,"app":"org.example.game","protocol":"game.1","seats":2}
```

`seats` is an integer maximum from 2 to 16. A match may start with fewer than
the declared maximum, but needs at least two connected, seated participants.
All participants install the same protocol and maximum. A spectator has no game
channel. The room owner must participate and is the simulation host; games own
their simulation and fairness rules. Seated IDs and seats are immutable per match.

This file is covered by the package archive hash. TrainerOS discovers it from
the active installed receipt; neither a running process nor an Internet catalogue
refresh is needed to show the game in Lobby. Title and local launch identity come
from that receipt. The declaration cannot supply a command, executable path or
remote URL. Duplicate app IDs, invalid declarations and removed packages are not
advertised. All participants must install matching protocol versions.

After startup the game calls `activity.register` with the declared app/protocol,
then waits for `network.ready`. That event is sent only after the required local
applications have attached, all required secure channels are connected and Lobby
acknowledges the launches. A normal solo launch may attach to the SDK for avatars; registration
alone must leave the local menu intact. Keep game menus free of compulsory
"open the game, then go to Lobby" steps. `activity.open` is only an optional
return shortcut, not multiplayer admission or the normal launch entry.

Initialize expensive scene resources before opening IPC. In a rendering client,
wait for the first drawn frame so shader compilation does not consume the bounded
hello deadline. Send hello in the same network-pump turn that observes TCP
connected, rather than waiting for another rendered frame.

The host's `SdkNetwork::setInstalledActivities` and `launchInstalled` composition
hooks accept trusted local inventory/launch bindings. Crates supplies these in
TrainerOS; an external peer cannot call them. Ordinary emulated games keep their
existing Lobby runtime adapters. Older native packages without the declaration
need a package update; running `activity.register` no longer advertises them.

The hello capabilities include `lobby.activity: 1` and `network.records: 1` only
when the local secure record helper is present. Apps should keep their standalone
behavior when either capability is absent. `network.group: 1` additionally
advertises **2–16 players with reliable ordered messages**. Group games must
check this capability. It does not advertise unreliable updates, universal NAT
connectivity or seamless reconnect. A 16-seat capacity is not a guarantee of
16-player performance on every device or network.

## Wire methods

Use the [base JSON-lines protocol](PROTOCOL.md). All calls require a local Trainer.

| Method | Parameters | Result |
| --- | --- | --- |
| `activity.register` | `title` (1–64), `protocol` (1–64 ASCII letters/digits/underscore/dot/hyphen) | `id`, `mode`, installed maximum `seats`; attaches this connection to its declared activity; does not populate the picker |
| `activity.get` | `{}` | `registered`, `session` (empty until ready) |
| `activity.open` | `{}` | Opens the existing GameHub Lobby; membership is unchanged |
| `activity.unregister` | `{}` | Detaches the running client and closes its channel; installed game stays in Lobby |
| `network.send` | `channel`, `data` (base64, 1–16384 decoded bytes) | Accepted local `sequence` string; **not** a delivery receipt |
| `network.consume` | `channel`, next received `sequence` string | Returns receive credit after the app has handled a message |
| `network.close` | `channel` | Ends this app session; the app process and persistent Lobby membership remain |

Activity identity is `sdk:<hello-app-id>`; the selected protocol must match on
both hosts. The app ID is still a local namespace, not a verified publisher claim.
Each connection registers one activity. Changing its protocol during preparation
or play returns `activity_busy`; unregister first. Titles are presentation, not
compatibility or admission. No remote executable/path is accepted.

## Events

```json
{"event":"network.ready","session":{"channel":"<local-opaque-grant>","self":"<member-id>","peer":"<member-id>","host":true,"lobby":"<room-id>","protocol":"example-1","delivery":"reliable-ordered","maxBytes":16384,"window":64}}
{"event":"network.data","channel":"<grant>","peer":"<member-id>","sequence":"0","data":"aGVsbG8="}
{"event":"network.ack","channel":"<grant>","sequence":"0"}
{"event":"network.closed","channel":"<grant>","reason":"app_disconnected"}
```

Grants differ between hosts and sessions. An app can use only its own current
grant. The helper authenticates the shared room/generation/game/ticket/participants
and fresh connection binding. Raw mailbox codes, certificate pins, session tokens
and private helper configuration are never exposed to the app or public listing.

### Group sessions (capability `network.group: 1`)

The ready session additionally contains `hostMember`, `maxPlayers: 16`,
`members: [{id, seat, connected: true}, ...]` sorted by seat, and
`channels: [{peer, channel}, ...]`. Seat numbers are unique within the installed
maximum and can have gaps. Do not infer the local seat from the host flag; find
`self` in `members`. Host and client grants are different, opaque local strings.
The host receives N−1 channels, each client one channel to the host. The legacy
`channel`/`peer` fields remain when there is exactly one local link.

Send to an individual channel. A broadcast is deliberate app-side fanout; there
is no hidden partial-broadcast success. Send sequences, receive sequences,
acknowledgements and the 64-record credit window are **per channel**. Consume
the exact received channel/sequence pair after handling it. A stalled peer does
not borrow another peer's credit. Host applications must pump IPC regularly and
bound their own queues. `network.data` and `network.ack` include the authenticated
peer. No client can send on another participant's grant or talk directly to
other clients. Applications validate command ownership and host authority.

`network.close` on any admitted channel closes the entire immutable match;
`network.closed` can carry an empty `channel` for a group host and always revokes
all its current grants. Membership/seat/host/game changes or any failed link
also end the match. Persistent Lobby membership is independent. This revision
does not add live joining, host migration or automatic replay/reconnection.

The host reuses one existing secure helper per client (at most 15); each client
uses one helper. Every edge binds the room, generation, ticket, full membership,
host/client identities and fresh connection token. No new operator-owned media
relay is required. More peers increase host memory, upload and startup costs;
device and internet acceptance remain explicit. Use bounded snapshots/deltas
instead of resending a large static map in every update.

`network.ready` follows the helper's secure handshake **and** the Lobby's launch
acknowledgements. `network.ack` means that the peer application returned credit;
it is not a saved game state, authoritative result or awarded victory. Sequence
numbers are zero-based decimal strings per direction and fresh session.

At most 64 records may await consumption in each direction. `backpressure` means
the caller must wait for receipts, not discard/replay a state mutation. Existing
helper bounds also close stalled record windows after 12 seconds. Read local SDK
events regularly; its separate IPC queue limits remain in force. Consume received
records in order. The app owns serialization, conflict handling and simulation.

App crash/disconnect, profile change, participant or game changes, room loss and
helper failure revoke the channel. The shell does not terminate the app or write
its saves. A subsequent session requires registration if disconnected and the
ordinary preparation/Ready/Start flow; old grants and sequence numbers cannot be
reused. There is no automatic replay or silent rejoin. Provider loss currently
ends this app channel; emulator and voice lifecycles retain their existing rules.

Errors additionally include `transport_unavailable`, `invalid_activity`,
`activity_missing`, `activity_busy`, `channel_unavailable`, `invalid_payload`,
`invalid_sequence`, `backpressure`. Connection failure is a `network.closed`
event, never a fake ready result. A host stop may instead be observed as EOF.

## Transport and local development

The implementation reuses the existing
[secure record helper](../packaging/emulators/melonds/secure-channel.py): pinned
Magic Wormhole Dilation for its existing online route and pinned TLS for local
testing. It does **not** send app payloads through Fluxer messages or Remote Play
media. Fluxer remains the existing authenticated invitation/signaling owner.
This preview does not claim to implement the full direct ICE/unreliable/direct
app-side fast path or provider relay acceptance described by #238/#105. The
helper's existing online public rendezvous/transit dependencies are not a newly
hosted TrainerOS relay, and this delivery does not verify their internet behavior.

A native app need not install an emulator. Install the same helper separately:

```sh
python3 packaging/platform/install-records.py "$HOME/.local/share/TrainerOS/platform/records"
```

For a non-default `XDG_DATA_HOME`, use that directory instead of `~/.local/share`.
The installer requires a **new** target directory and an internet connection for
pinned dependencies. It changes neither emulator bundles nor the global Python
environment. The host discovers this shared bundle first, then a pre-existing
`TrainerOS/emulators/melonds-netplay/transport` bundle for backward reuse.
For development, set `TRAINEROS_RECORD_PYTHON` and `TRAINEROS_RECORD_HELPER` to the
isolated interpreter and that same helper. These are host configuration, never
fields supplied by an SDK app or a remote peer. Windows uses the venv's
`Scripts/python.exe` and Qt's local application data directory.

The [record_peer.py](examples/record_peer.py) example exchanges `hello` and
`hello back` after the host launches it. Package it with the accompanying
`record-activity.json` renamed to `traineros-activity.json`, or use the trusted
inventory/launch hooks in a test host. Choose it in Lobby, Ready, then Start;
there is no manual client-first step. It is a channel example, **not a game**. A communication
account is still required by the current normal Lobby implementation. Offline
discovery/admission without Fluxer is a remaining SDK feature.

The automated paired check substitutes only authenticated provider signaling,
then runs two separate app clients and two unmodified helpers over loopback TLS.
This is local transport/lifecycle proof, not two-account Fluxer, handheld,
different-network, NAT or gameplay acceptance. It also exercises a new session
after an app crash while retaining the same room membership.
