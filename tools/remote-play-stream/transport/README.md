# Automatic socket transport

**PS2 source addition, 24 October 2026:** managed `mode: "ps2"` carries bounded
ECLX v1 datagrams on channel 0, rejects the RTSP channel, and authenticates the
mode separately from PSP/media. Its local socket binds loopback only; host
destination is fixed to UDP 19072, guest destination is pinned by its first local
HELLO. The private config supplies role, local port and a fresh room code shared
inside DTLS. The emulator still owns packet authentication and reassembly.
See [PS2 contract and remaining work](../../../docs/emulators/armsx2-lan.md).
Only the portable socket test/configuration tests have passed for this addition;
the changed full Linux helper is not yet built, installed or connected to Lobby.

`-DSOCKET_CHECK_ONLY=ON` builds the Qt socket boundary check without fetching RTC
dependencies, including on Windows. Both PS2 tests require UDP 19072 free and
share a CTest resource lock. Do not stop an existing game to free that port.

Separate helper used by the [L07 lobby service](../../../docs/REMOTE_PLAY_LOBBY.md). Carries
the existing Moonlight RTSP/video/audio/control packets through libdatachannel
ICE + DTLS + SCTP, without another media codec, Fluxer media server, VPN daemon,
application runner or desktop input. The parent must authenticate signaling and
bind it to the accepted lobby/ticket/connection before using this component.
`LobbyConnection` supplies that binding through private provider-authenticated
messages and owns the helper's private temporary directory. The helper alone
never authorizes a controller or owns a game process.

The managed five-field configuration adds `mode: "game"` and permits `seconds: 0`:
the local parent and accepted lobby then own the lifetime. The original four-field
diagnostic configuration remains bounded to 5–120 seconds. Parent EOF, membership
loss or explicit close removes the connection and its private session material.
The ordinary route supplies public STUN for address discovery and keeps media
P2P. Owner-hosted TURN/VPS is outside the early-beta requirement; difficult NAT
combinations are a documented limitation. Separate-network acceptance stays open.

The existing DS/Vita reliable record transport is retained for those families.
Its ordered delivery is not adopted for latency-sensitive Remote Play datagrams.
The new component uses an unordered channel with `maxRetransmits=0` for the three
UDP sockets. Moonlight keeps its own FEC, audio and ENet semantics. A separate
reliable channel carries each bounded RTSP exchange; a third exchanges session
identity and fresh keys **inside DTLS**, never in signaling or a public listing.
There is no new media format or cryptographic implementation.

## Reproduce

```sh
cmake -S tools/remote-play-stream/transport -B build-stream-transport -DCMAKE_BUILD_TYPE=Release
cmake --build build-stream-transport -j2
ctest --test-dir build-stream-transport --output-on-failure
```

Linux, CMake 3.24+, Qt6 Core/Network, OpenSSL and a C++17 compiler are required.
Python3 runs the boundary checks. No capture, codec, uinput, game or credentials
are needed by those checks. TCP/UDP loopback ports 49010–49013 must be free; tests
run serially and never replace an existing listener. Source checkout is pinned:

| Component | Revision | Notice |
| --- | --- | --- |
| libdatachannel 0.24.6 | `6b1e2e620f1e37f0eafeee702eaea0043cb305fd` | [MPL-2.0](licenses/libdatachannel.LICENSE) |
| libjuice | `b89c792e3612faf2f12cf35bcc56857313a06be3` | [MPL-2.0](licenses/libjuice.LICENSE) |
| usrsctp | `fec583d54493f879d2ae44a743423bf8a04371ab` | [BSD notices](licenses/usrsctp.LICENSE.md) |
| plog | `94899e0b926ac1b0f4750bfbd495167b4a6ae9ef` | [MIT](licenses/plog.LICENSE) |

Submodule revisions come from the pinned parent. WebSockets, media support,
upstream examples/tests and unused JSON/SRTP submodules are disabled. Qt/OpenSSL
remain system dependencies with their existing notices. Retain the exact source
and these notices with copies; this development-device gate does not close #115.

## Trusted local parent contract

The helper reads bounded JSON lines on stdin and emits bounded JSON lines on
stdout. These are **private IPC**, not a network listener or a logging format.
Do not expose/persist them in QML, a public lobby listing or diagnostic logs.

1. Supply `{host: bool, identity: 64-lowercase-hex, seconds: 5..120, ice: []}`.
   `identity` must represent the same accepted room, generation, launch ticket,
   owner, guest, seat and fresh connection on both sides. `LobbyConnection`
   supplies this binding in the managed route; the diagnostic harness tests it separately.
   `ice` accepts at most four operator-provided STUN/TURN URLs. This is not a
   player setting or permission to accept relay credentials from another player.
2. On `event: description`, deliver `identity` and `description` to the **already
   authenticated** intended peer's parent. It supplies them to the other helper.
   SDP contains short-lived ICE credentials and a certificate fingerprint. Only
   one remote description is accepted; a second cannot replace this connection.
   The helper emits this SDP immediately, then `event: candidates` with the same
   identity and a `candidates` array of `{candidate, mid}` records. Forward each
   batch as `{identity, candidates}` over the same authenticated signaling path.
   Trickle ICE allows local/direct checks while STUN is unavailable or still
   gathering an external address. Batches are coalesced for 100 ms, bounded to
   32 records per side/session, 512 characters per candidate and 32 per mid.
   Early remote candidates are buffered until the one remote SDP is installed.
   Both participants need the matching shell/helper; update the bundle and shell
   together. Candidate batches never replace the game, peer or launch ticket.
3. DTLS verifies that fingerprint. Both sides then verify the session identity
   and opposite roles inside the encrypted channel. Only the host creates the
   fresh Moonlight key/ping/connect material; the guest receives it encrypted.
4. `event: ready` gives a **local** config path. Both private directories are 0700,
   files 0600 and removed on ordinary completion, EOF, failure or SIGTERM. Never
   send that local path to another device. The diagnostic config selects the
   synthetic source, muted/headless output and a bounded lifetime. Its existence
   is not a game input grant; the reviewed lobby descriptor is separately needed
   by `run-game.py`. The managed configuration selects game capture/display/sound
   and a parent-owned lifetime. No game is launched by this helper.
5. `{close: true}` closes the peer and local transport. Disconnection fails closed;
   reconnect requires a fresh instance, identity and key. Existing games are not
   terminated. Production supervision must also handle forced process death and
   remove its own private directory; SIGKILL cannot run any process's cleanup.

The host media process binds only `127.0.0.1`; the bridge originates from
`127.0.0.2`. The guest bridge listens on `127.0.0.2` and the media client connects
there. Only the fixed three datagram destinations and one RTSP destination are
forwarded, with bounded message/queue sizes and deadlines. The first local guest
UDP socket remains pinned for that session. This boundary trusts the same OS user;
it is not isolation against other hostile processes with that user's privileges.

The public network sees the ICE/DTLS connection, not the media process's RTSP/UDP
listeners. libdatachannel supports STUN/TURN, but the recorded paired check used
host candidates on one LAN with `ice: []`. **No deployed TURN service, distinct
networks, NAT fallback, live Fluxer signaling, reconnect during play or multiple
guests is claimed.** Seven boundary tests and paired stream evidence are in
[REMOTE_PLAY_TRANSPORT](../../../docs/REMOTE_PLAY_TRANSPORT.md#автоматическое-соединение-и-шифрованный-путь--14-октября-2026).

Upstream API/reference: [pinned configuration](https://github.com/paullouisageneau/libdatachannel/blob/6b1e2e620f1e37f0eafeee702eaea0043cb305fd/include/rtc/configuration.hpp),
[channel reliability](https://github.com/paullouisageneau/libdatachannel/blob/6b1e2e620f1e37f0eafeee702eaea0043cb305fd/include/rtc/reliability.hpp).
