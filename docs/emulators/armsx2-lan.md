# ARMSX2: native LAN preparation

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
