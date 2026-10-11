# Nintendo 64 native netplay — 11 October 2026

Status: **installed Nearby route; Online service pending**. The 12 October
record below supersedes earlier build-only statements. This does not close
N64 or the full multiplayer block.

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

Pending: operator online service, four real devices, distinct networks/NAT,
loss/reinvite beyond the observed clean peer exit, physical input and audio
acceptance. Bundle, paired native gameplay and ordinary Nearby/Home delivery
are recorded in the 12 October section below.
Devices are now available; the bounded recheck below supersedes the earlier build deferral.

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
A configured operator `wss` endpoint enables Online separately from Nearby;
absent/incomplete runtime installation advertises neither route. There is no
developer opt-in. Use `--nearby-only` until the operator service is available.

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

## ARM64 verification — 11 October 2026

The full pinned RMG bridge, core and Vulkan/OpenGL plugins build and install into
a private staging directory on updated Flip (Fedora 44 build container). The
pinned native Go server also passes explicitly enabled four-client Nearby and
loopback-TLS lobby/start tests. These are protocol tests, not N64 gameplay.

Host promotion still requires `libminizip.so.1`, `libSDL3_net.so.0` and
`libspeexdsp.so.1` in a compatible isolated runtime bundle or supported image
packages. The diagnostic configure omitted the recipe's installed RPATH; retain
that existing recipe flag in a release build. No host OS packages were layered,
operator server fabricated, or multiplayer profile enabled by this build.
[Exact build hashes, dependencies and remaining acceptance](../MULTIPLAYER_DEVICE_RECHECK_20261011.md).


## Handheld runtime continuation - 12 October 2026

The pinned native RMG runtime now loads on both updated Armada handhelds. The
[Armada dependency packager](../../packaging/emulators/rmg/armada-dependencies.py)
copies minizip-ng-compat 4.1.0-1.fc44, SDL3_net 3.2.0-2.fc44 and speexdsp
1.2.1-10.fc44 into the isolated bundle, preserving license notices and package /
source-byte provenance. It sets each executable/plugin's relative RUNPATH to the
bundle's own `lib` directory, including nested plugins. `ldd` finds no missing
libraries on either host. No packages were layered into the host image.

Use `ARMADA_BUNDLE_DEPS=1` in the matching Fedora build container, with `patchelf`
and those RPM dependencies installed. The script refuses an already completed or
fingerprinted destination. Fedora's minizip compatibility subpackage omits its
license file, so the recipe retains the exact
[upstream 4.1.0 notice](https://github.com/zlib-ng/minizip-ng/blob/4.1.0/LICENSE)
and rejects a different version without review. This is a supported-image bundle,
not a universal Linux binary or closure of redistribution gate #115.

A real two-handheld Mario Kart 64 US check exposed a graphics failure that the
lobby tests could not: Flip's Turnip Adreno 650 driver lacks the 8-bit SSBO storage
required by paraLLEl-RDP. Odin started Vulkan emulation and then lost the failed
host. The bridge's opt-in `TRAINEROS_RMG_DIAGNOSTICS` output now records native
errors locally; do not publish those private logs without reviewing paths/data.
Both devices then used the existing OpenGL GLideN64/HLE profile and reached the
same two-player 50cc Luigi Raceway race. Host and guest independently selected
Mario / Peach and accelerated their karts through Armada's virtual Xbox inputs.
This is a short native gameplay check, not physical-button, prolonged-race,
four-device or internet acceptance. Each run used fresh XDG roots and temporary
progress; ordinary solo saves were neither imported nor returned by this harness.

Native runtime SHA-256:
`56a39a458fce380e112bcf12949e5bfcc779caa1c67dbb319f3300c9063d1041`.
The complete manifest fingerprints 22 files. The installed target is a versioned
`rmg-netplay-20261012.1` directory, retained independently from normal RetroArch.
The same OpenGL setting is required on both peers; this device-specific fallback
does not replace other emulators' supported Vulkan settings.

### Connection availability

`build.sh` / `bundle.py` accept `--nearby-only` instead of inventing an operator
URL. An absent/empty operator endpoint admits Nearby with the bundled upstream
server and explicitly disables Online. A supplied endpoint must still be `wss`.
The common profile `connections` list gates invitations, reverse Join, company
admission and launch after a changed installation. It is separate from the
emulation/content identity, so compatible machines can use their common route.
Library multiplayer filtering remains separate from Online/Nearby badges.
Profiles without the field retain their existing two-route behavior.

The RMG invitation discloses a fresh session with temporary progress. Home's
owned-window close recognizes only RMG launched through the maintained native
session bridge; ordinary desktop RMG retains its own confirmation behavior.

Upstream [removed its hosted servers](https://github.com/Rosalie241/RMG/releases)
after their protocols diverged. TrainerOS still needs a real operator service
with trusted TLS lobby and reachable native TCP/UDP ports. A local endpoint, fake
hostname or successful LAN race does not deliver that internet route.

### Ordinary Nearby delivery

Both installed clients now run SHA-256
`0f413e80d25305babe38920342f11062e7aa7b899ab4c378ff91e56f61e4ebcf`.

The ordinary library shows **Nearby play**, keeps N64 in the multiplayer
collection, and offers Nearby without an unavailable Online/group action.
Flip launched its ordinary Mario Kart 64 entry, invited Odin, and received
explicit acceptance after the complete temporary-progress notice. The normal
host save/leave barrier then launched both installed RMG instances automatically.
Both reached live native emulation; the longer paired race/control proof above
uses the identical native binary and remains separate from this 72-second
ordinary-session check (which showed title/attract mode, not another human race).

Odin Home recognized the owned RMG process. Minimize returned to the shell while
the game kept running; Game Options reopened, and explicit Exit closed the guest
without a save question for the temporary session. Peer departure closed the host
as well. Both histories record `returned`, all session directories disappeared,
and no RMG/server processes remained. Final database checks returned `ok` and
both devices remained at volume zero. All 363 / 8 pre-existing files in the
bounded save inventories remained byte-identical, as did both separate DS saves.
These inventories are not a claim to cover every personal file on either device.

Odin lacked the ordinary N64 cartridge in its local library. The owner's existing
mounted-source Mario Kart 64 US was copied to a new local N64 file and discovered
normally; source bytes and earlier files were preserved. ROM SHA-256:
`d6b8538dd63f0132ecb2856e7d32816ed3c30e3e479aecd23cf83fb6ba17a5da`.
No ROM or save is included in the public bundle/repository.

The party list can retain an unavailable previous selection when readiness
changes; Start may require scrolling upward. This existing usability limitation
was observed, not hidden by the successful controller/touch path. Home, badges
and the complete guest notice were inspected on the installed handhelds.

ARM64 client/native build and `game_party`, `rmg_netplay`, `runtime_multiplayer`,
`social` plus 15 overlay-helper checks pass. Four-client lobby cases were explicitly
enabled with the pinned Go server and temporary trusted TLS certificate; these
remain protocol tests. The final Windows client/test build also passes; the same
four focused suites pass there (4/4, 6.11 seconds), with native lobby/TLS cases
explicitly enabled.

Keep the online operator, larger group, distinct-network, recovery and physical
acceptance open. No multiplayer issue is closed.
