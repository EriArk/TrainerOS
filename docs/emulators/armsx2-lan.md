# ARMSX2: native LAN preparation

## Paired runtime alignment — 24 October 2026

Both handhelds now use 2.8.4-433 / `90126bdfea`, SHA
`1c40028a0464bcfab57826518500dcd11ceb472bea16d0900d7146abb11a4224`.
Flip's old AppImage and each previous helper/manifest were backed up locally.
The launcher no longer drops `runtime` when checking the admitted identity;
seven config tests pass. Both installed manifests and their files match.
Personal INIs and memory cards were checked unchanged during this alignment.
No emulator source patch was replaced; PPSSPP remains separate and unchanged.
This delivery does not resolve the Network Configuration Utility input problem
below or prove a PS2 LAN match. [Shared evidence](../LOBBY_RUNTIME_ALIGNMENT.md).

## Continuation evidence — 24 October 2026

The earlier build rejection below is historical: the owner continued, builder
startup succeeded and the changed Linux transport/shell were built and checked.
Current source binds PS2 to the existing two-player Lobby explicit Start through
`Ps2LanNetplay`, RuntimeMultiplayer and LobbyConnection. Native game launch is
deliberate; transport loss keeps the owned game and never automatically rejoins.

The package and shell were installed and restarted on both handhelds. Shell SHA:
`dc42c7c1371f811a55a5e85279e79ee467ae7c93fc44f48259944032c6c51b80`.
Flip remains a desktop application; Odin retains its existing session service.

The host-network endpoint is now a private AF_UNIX pathname socket, with 0600
access in a 0700 session directory. `ps2_endpoint` runs beside ARMSX2 in a bwrap
network namespace containing only loopback. Its UDP endpoint retains the bounded
ECLX forwarding below. The config now has seven fields, adding `socket`.
Namespace checks passed on both handhelds, including the same UDP port occupied
outside, real IPC/UDP exchange, ordinary-network refusal and game survival on loss.
`--dev-bind /dev /dev` preserves GPU/input access; a plain bind is nodev.

The native probe exposed and corrected three preparation errors:

- The persisted enum is `EthApi = Local Link`, including the space, as defined in
  [Pcsx2Config.cpp](https://github.com/ARMSX2/ARMSX2/blob/90126bdfea/pcsx2/Pcsx2Config.cpp).
  The initial `LocalLink` spelling left Ethernet disabled.
- Even `-batch` scans configured library paths. Odin waited on an old NFS path.
  A private base INI copy now removes only GameList Paths/RecursivePaths; personal
  settings stay unchanged. The private INI directory is mounted at its usual path.
- Passing the expanded ARMSX2 directory to `-datapath` created a nested first-run
  setup. The launcher now sets the configuration base through XDG_CONFIG_HOME,
  preserving ordinary BIOS/card locations. The unused probe folder was moved aside.

`tools/ps2-lan` contains the reproducible package recipe and bounded ISO9660/CHD
reader. libchdr is pinned to `47d794151b2b64f6aba60304e6480f4f5a2bc24b`.
CHDv5 2048/2448-byte standalone layouts are supported; parent CHDs are rejected.
The CHD raw SHA-1 is a declared image identity, not a full-file integrity check.
Exact SYSTEM.CNF serial and upstream ELF XOR CRC select per-title settings.
One candidate: Battlefront II USA, `SLUS-21240`, CRC `02F4B541`; both copies match.
The 56-game list is not blanket compatibility.

Odin had updated to 2.8.4-433 / `90126bdfea` before this launch. Its AppImage SHA
is `1c40028a0464bcfab57826518500dcd11ceb472bea16d0900d7146abb11a4224`.
The version guard first rejected it; comparison confirmed the same LocalLink
source blobs as below before admission. At that checkpoint Flip was on 2.8.4-390;
the alignment above supersedes that installation state.

Checks passed: full Linux transport CTest (PS2 plus prior PSP/media boundaries),
six config and five synthetic-disc checks, shell standalone/relay/Lobby/GameParty/
RuntimeMultiplayer suites. Real-helper Lobby checks additionally passed all seven
PS2/PSP/media scenarios, with zero skips. Native games booted, menus accepted input,
and the host logged `Local Link peer 2 joined`. Both exited gracefully; the final
private-settings probe left personal INIs and pre-existing card bytes unchanged.
The initial ordinary startup updated Flip's INI through ARMSX2 itself; no existing
card was replaced or formatted. PSP helper/PPSSPP remain separate.

**Still incomplete:** neither card contains a PS2 network configuration. The
game's built-in Network Configuration Utility opens but stops responding to input
after its ELF transition on both devices; foreground ownership, input descriptors
and rendering persist. Native peer admission is not a completed LAN match.
Next: isolate that input transition, finish native setup/discovery/match and the
installed Lobby/Home/Exit cycle. Offline admission remains separate unfinished work.
No VPN, difficult NAT or new relay infrastructure was added.

Build the two CMake projects under tools/remote-play-stream/transport and
tools/ps2-lan; run transport CTest, check_config.py, check_disc.py and host-side
check_namespace.py. package.py collects helpers/scripts and dependency notices.
The private installed manifest adds absolute runtime/configRoot/runtimeSha256;
the bundle lives at XDG_DATA_HOME/TrainerOS/emulators/ps2-lan. No ROM/BIOS/emulator
binary or personal settings enter this repository. Retain previous shell/package
for rollback, settle the game first and remove only session-owned settings.

## Historical foundation before this continuation

**24 October 2026 — partial source implementation; not installed or exposed in
Lobby.** R035 and #259 remain open. The chosen route reuses ARMSX2 DEV9 Local Link
and TrainerOS's existing authenticated unordered ICE/DTLS datagram channel.
No emulator source changes, VPN, TUN/TAP, home-LAN bridge or new relay service.

## Upstream interface

Read against `ARMSX2/ARMSX2` revisions
`83fe4e293f71ab5587ef81b06143725bf8c6a30b` and
`65fd14b9160e7fa3779794fc45c4e92a86772621` (installed Flip/Odin versions).
Their LocalLinkAdapter.cpp/h blobs are identical:
`70f5835b4919c99cdc0f25471a6afa2ed298bdfb` /
`c70770348325bf4848bf26d65109ba8ddf4fb023`.

- [LocalLinkAdapter.cpp](https://github.com/ARMSX2/ARMSX2/blob/65fd14b9160e7fa3779794fc45c4e92a86772621/pcsx2/DEV9/LocalLinkAdapter.cpp):
  ECLX v1, 44-byte header, 1000-byte fragment payload, at most two fragments for
  a 1514-byte Ethernet frame. Native HELLO/ACK, SipHash, nonce/replay window,
  DHCP and Ethernet reassembly remain upstream-owned.
- [QtHost.cpp](https://github.com/ARMSX2/ARMSX2/blob/65fd14b9160e7fa3779794fc45c4e92a86772621/pcsx2-qt/QtHost.cpp)
  and [VMManager.cpp](https://github.com/ARMSX2/ARMSX2/blob/65fd14b9160e7fa3779794fc45c4e92a86772621/pcsx2/VMManager.cpp):
  `-gamecfg` replaces the per-game settings layer before DEV9 opens. It does not
  change the ordinary data root or reset memory cards. Therefore copy the exact
  game's existing overrides before applying session-only Ethernet values.

## Source-owned changes and boundaries

[`ps2_sockets.hpp`](../../tools/remote-play-stream/transport/ps2_sockets.hpp)
adds a bounded two-instance local endpoint to the existing transport. Managed
`mode: "ps2"` is authenticated inside the same DTLS admission exchange; PSP and
media peers cannot substitute for it. ECLX envelopes travel unchanged on channel
0, unordered without retransmission. Channel 2 is unavailable in PS2 mode.
There is no Ethernet decoding or arbitrary peer-selected destination forwarding.

The bridge binds only `127.0.0.1` on an ephemeral port. Host packets go only to
local ARMSX2 UDP 19072. Guest traffic must start with a native HELLO; that local
source port remains fixed for the helper's lifetime. A second local source
cannot replace it. Bounds-invalid and oversized packets are dropped; bounded
draining yields to the event loop. Native retries remain ARMSX2-owned.

The existing secure admission shares fresh room material inside DTLS. The
0700/0600 temporary config contains `mode,address,host,port,peer,room`, with
roles 1/2 and a 12-character room code. EOF/close tears it down. No media/socket
ports or credentials are advertised in public game metadata.

**Important remaining boundary:** upstream ARMSX2 itself binds `INADDR_ANY`,
even with a loopback destination. This change restricts the bridge, not the
emulator listener. Native room authentication still applies. Before product
admission decide and verify loopback-only emulator binding or another bounded
isolation method; do not describe the present source as complete isolation.

[`session_config.py`](../../tools/ps2-lan/session_config.py) replaces only seven
DEV9/Eth keys in an exclusive private per-game INI. It keeps Qt escapes, input,
graphics, card names and other sections; personal/global INIs are never written.
The caller owns exact-game selection and directory lifetime through the ordinary
launch/save settlement. This is not a semantic save adapter.

## Evidence and remaining work

- Windows UCRT64/Qt: portable socket check built and passed; real loopback
  HELLO/ACK and both directions of fragmented data, malformed/oversized traffic,
  fixed local peer and packet boundaries. No emulated game or DTLS in that test.
- Five Python configuration tests passed: per-title/card/input preservation,
  host/guest settings, duplicate/invalid material, exclusive output and no partial
  file on validation failure.
- Full Linux helper tests now include PS2 transport/private material and cross-mode
  admission rejection, but **have not run on the changed helper**. Earlier PSP
  evidence is not a regression result for this source revision.
- Automatic command approval rejected starting the Odin build container and
  fetching the CHD dependency; the tool supplied no more specific reason.
  No build/deployment workaround was attempted and neither device was updated.

Next: finish exact ISO/CHD identity and supported-disc profiles; listener isolation;
connect the prepared helper to the existing Lobby explicit Start and native
launch/Home/exit lifecycle; retain owned saves; build/check the Linux helper and
deliver both handhelds. Offline LAN admission remains separate unfinished work.
Server Online, difficult NAT and VPN are outside this increment. The full list
of 56 LAN candidates is not a list of enabled or verified profiles.

## Update and rollback

For an ARMSX2 update compare the pinned LocalLink header/control semantics and
`-gamecfg`/DEV9 settings application before admitting that binary. Re-run the
configuration/socket checks, full ICE/DTLS suite, then the selected native title.
Do not silently promote a different binary based on its name or version string.
There is no installed patch to roll back in this delivery. Once packaged, keep
the ordinary emulator/configuration independent and remove only session-owned
temporary files after the child closes; no personal save migration is involved.
