# Native activity + reliable records, protocol v1 preview

This optional contract joins a running app to the **existing** TrainerOS Lobby.
It does not install or launch an executable, implement game rules or provide a
second account/room system. Two players start compatible app clients, choose the
activity in GameHub → Lobby, prepare, Ready, and the organizer presses Start.
The existing runtime coordinator performs admission. A successful registration
alone does not create a room, invite a person or authorize traffic.

The hello capabilities include `lobby.activity: 1` and `network.records: 1` only
when the local secure record helper is present. Apps should keep their standalone
behavior when either capability is absent. The initial route is **two players,
reliable ordered messages**. It does not advertise unreliable/realtime updates,
universal NAT connectivity, five-player support or seamless reconnect.

## Wire methods

Use the [base JSON-lines protocol](PROTOCOL.md). All calls require a local Trainer.

| Method | Parameters | Result |
| --- | --- | --- |
| `activity.register` | `title` (1–64), `protocol` (1–64 ASCII letters/digits/underscore/dot/hyphen) | `id`, `mode`, `seats: 2`; adds this connection's app to the ordinary Lobby picker |
| `activity.get` | `{}` | `registered`, `session` (empty until ready) |
| `activity.open` | `{}` | Opens the existing GameHub Lobby; membership is unchanged |
| `activity.unregister` | `{}` | Removes the app's contribution and closes its channel |
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

Start [record_peer.py](examples/record_peer.py) on both hosts, open ordinary Lobby,
choose **Record channel example**, Ready and Start. It exchanges `hello` and
`hello back`; it is a runnable channel example, **not a game**. A communication
account is still required by the current normal Lobby implementation. Offline
discovery/admission without Fluxer is a remaining SDK feature.

The automated paired check substitutes only authenticated provider signaling,
then runs two separate app clients and two unmodified helpers over loopback TLS.
This is local transport/lifecycle proof, not two-account Fluxer, handheld,
different-network, NAT or gameplay acceptance. It also exercises a new session
after an app crash while retaining the same room membership.
