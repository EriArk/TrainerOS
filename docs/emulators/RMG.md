# Nintendo 64 native netplay — 11 October 2026

Status: **partial source integration**. Registered as researching before code;
the implementation below is prepared for delivery, not installed N64 gameplay.

## Upstream and decision

- [RMG](https://github.com/Rosalie241/RMG/tree/4777ed3f1fb7f1e4a0e70438a27dc4a5e9fb1927),
  revision `4777ed3f1fb7f1e4a0e70438a27dc4a5e9fb1927`, GPL-3.0-only.
- [rmg-netplay-server](https://github.com/Rosalie241/rmg-netplay-server/tree/bf0f28746971fcb6341fac55c0d2d19f646a6e09),
  revision `bf0f28746971fcb6341fac55c0d2d19f646a6e09`, GPL-3.0-only.
- RMG's existing Mupen64Plus core supplies TCP settings/save synchronization and
  UDP controller synchronization. Its existing server supplies rooms and native
  game sockets. TrainerOS must not implement another emulator synchronization
  protocol. RMG's upstream build matrix includes Linux aarch64.
- The lobby protocol is version 17. Native controller numbers follow server
  admission order; they must be read from `reply_players`, never assumed from
  invitation arrival order. Start only after the complete accepted roster joins.
- Stock RMG exposes netplay through dialogs, not its ordinary ROM CLI. A bounded
  launch bridge is needed. Existing RetroArch N64 availability is not evidence
  of deterministic RetroArch netplay.

No game SHA-256 is fabricated: eligibility will require an exact local cartridge
and matching runtime bundle. No semantic save access is granted. 64DD, transfer
packs and cross-build compatibility are outside this first route.

## Required delivery and remaining evidence

Automatic private room creation/join, roster validation, cancellation and owned
launch must use the normal invitation flow. Nearby requires a managed local
server; internet requires a configured operator server, never player-entered
addresses or router/VPN setup. A server build is not a deployed online service.

Use fresh session-local configuration, SRAM, controller packs and states; no
ordinary progress import/export until separately verified. Preserve normal solo
launch and existing saves. Prefer Vulkan where the installed profile works.

Pending: complete Linux/ARM64 RMG bridge build/install and operator online service,
actual paired controls/gameplay, four real devices, distinct networks/NAT,
loss/reinvite, Home/input and audio acceptance.
Devices remain owner-deferred; do not repeat unrelated hardware checks.

## Implemented preparation

[RmgNetplay](../../src/integrations/adventure/standalone/RmgNetplay.cpp) and the
ordinary RuntimeMultiplayer flow now create/join a password-protected upstream
room, wait for the accepted roster, read the server-assigned controller number,
and request launch through the shared application checkpoint/save/runtime barrier.
The host's running ordinary game retains its save question. Cancellation,
changed identity, unexpected participants and early start cannot launch a game.
An ended invitation closes its lobby and owned local server. No new emulator
input/synchronization protocol is introduced.

The [launch patch](../../packaging/emulators/rmg/traineros-launch.patch) and
[bridge](../../packaging/emulators/rmg/TrainerNetplay.inc) call RMG's existing
native netplay launch. They select the bundled Vulkan paraLLEl RDP/RSP pair by
default, with an explicit OpenGL GLideN64/HLE bundle alternative. The renderer
is part of matching settings. One local SDL controller uses upstream's fallback
gamepad mapping; devices need real mapping/Home verification. Native controller
numbers follow lobby admission order, which can differ from invitation order.

ROM headers accept the three cartridge byte orders (`z64`/`v64`/`n64` contents),
up to 64 MiB. Peers must have identical full bytes and the identical fingerprinted
bundle/settings; renamed library titles do not break matching. A SHA-256 occupies
the upstream lobby's opaque `MD5` comparison field: stock RMG public-room clients
are not asserted compatible with this controlled TrainerOS bundle. No archive,
64DD, transfer-pak, cross-architecture or universal-game compatibility is claimed.

Every launch gets fresh XDG config/data/cache roots. Upstream may synchronize
temporary SRAM/controller-pack data, but this route never imports or writes back
ordinary solo saves, states, settings or cheats. These sessions currently start
with temporary progress. Solo RetroArch stays selected and is not migrated.

## Server and installation ownership

[build.sh](../../packaging/emulators/rmg/build.sh) builds a separate Linux bundle
into a new destination and pins both repositories. RMG needs upstream's Qt 6,
SDL 3/SDL_net 3, Vulkan/OpenGL, minizip, audio, USB and font dependencies; the
server needs Go as specified in its pinned `go.mod`. No Actions were enabled.
[bundle.py](../../packaging/emulators/rmg/bundle.py) fingerprints binaries,
plugins and shared input/game configuration. Install the complete reviewed
bundle under `TrainerOS/emulators/rmg-netplay` in the user's data directory.
The manifest requires a configured operator `wss` endpoint; absent/incomplete
installation does not advertise an online route. There is no developer opt-in.

Nearby starts the bundled upstream server on the host (45000 lobby, 45001 native
TCP/UDP for this one-room process). Internet uses an operator-hosted server;
ordinary players neither enter addresses nor forward ports. The operator must
supply the public endpoint/certificate, reachable native TCP/UDP game ports and
service monitoring/capacity. **No public server was deployed in this increment.**

The small [server TLS patch](../../packaging/emulators/rmg/server-tls.patch) adds
Go's standard HTTPS listener and `--tls-cert`/`--tls-key`; it leaves upstream
room/game protocols intact. Terminate TLS directly in that server when using
`--verify-ip`, so lobby/native connections retain the same real client address.
This secures the lobby, not the native gameplay sockets. Upstream's `auth` hash
identifies a client/version; it is not a TrainerOS account authorization token.
Private room passwords travel in accepted invitations and TLS lobby messages,
never public room/player names or launch arguments. Roster names use a one-way
derived room ID. A local test certificate is not a deployed service identity.

## Bounded evidence and update procedure

Final local verification, 11 October: Windows `traineros` and the affected test
executables built successfully. CTest **3/3 passed** (`rmg_netplay`, `game_party`,
`runtime_multiplayer`, 5.96 seconds), with the pinned Go executable and temporary
trusted loopback TLS certificate supplied for the real-server cases. The
adapter knowledge/source snapshot check and 544 local documentation links also
passed. No handheld installation, N64 ROM execution or internet deployment ran.

- Pinned Go server built for Windows x64 and cross-built for Linux ARM64. Four real lobby clients reached a private
  room/start with reverse arrival and unique native seats; both Nearby and TLS
  lobby transports were exercised. This did not execute an N64 ROM.
- Qt source checks cover cartridge/bundle mutation, session isolation/cleanup,
  renamed titles, malformed/incomplete/foreign rosters and cancellation.
- Shared lifecycle checks retain host saving, recipient invitation behavior and
  cancellation after delayed preparation. Native RMG startup/gameplay remains
  separate from these checks. Both patches have apply/reverse preflight; the
  launch bridge compiles against pinned RMG-Core declarations in an isolated
  harness. This does not build/link the complete emulator or validate its GUI.

For updates, rebase both small patches against deliberately pinned revisions,
review native/lobby versions and settings/input defaults, rebuild the whole
bundle identically for the target pair, refresh fingerprints and repeat affected
protocol/lifecycle checks. Require actual paired game/controller/Home evidence
before device promotion. Keep the old bundle for rollback; close active games
before replacing it. Remove/restore only this bundle to disable/roll back the
route; ordinary RetroArch files and user progress remain untouched. Preserve
upstream GPL notices and the repository's #115 redistribution gate.
