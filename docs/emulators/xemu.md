# xemu — original Xbox ordinary launch

## System Link through Lobby — 26 October 2026

Implemented `runtime.xemu.lan.v1` for 2–4 independent Xbox consoles. Lobby owns
selection, seats, explicit Start and signaling; the existing native LAN coordinator
and ICE/DTLS Ethernet carrier own transport. Players choose System Link and
create/join inside the game. No second Xbox is emulated on a handheld, no VPN,
host TAP, router settings or owner media relay are introduced. Internet reachability
has the same direct-connect limits as the existing LAN routes; no new relay claim.

The route is available for Xbox ISO registrations with the installed verified
package, without an arbitrary title allowlist. It does **not** add System Link to
games which lack it. Native title revisions, title updates in the personal HDD,
player limits and game-specific network behavior still apply; equal ISO bytes alone
do not certify every game's installed update combination. Xbox Live is separate.

### Native contract and personal data

`XboxLan` verifies the package and ISO immediately before launch. Each connection
is bound to the Lobby generation and assigned seat. The existing `bba` carrier
transports bounded Ethernet datagrams; its authenticated Lobby binding now admits
the Xbox profile too. `NativeLanConnection` records the local `xbox` consumer and
its private carrier sockets. The endpoint translates Unix framing to xemu's native
loopback UDP socket; source MAC validation prevents a guest claiming another seat.
The shared transport implementation is unchanged.

Use native `-nic socket,model=nvnet,mac=...,udp=127.0.0.1:PORT,localaddr=127.0.0.1:0`.
A `-netdev socket` plus standalone hubport is insufficient: native QMP showed
`nvnet.0` detached. The corrected launch shows the socket under `nvnet.0`.
The endpoint chooses its port; no player-provided command/path crosses the network.

The private TOML retains graphics, controls and own BIOS/HDD paths; only networking
and its EEPROM path differ. The session EEPROM is copied from this device's own
256 bytes: only the MAC and factory checksum change. Encrypted keys, serial,
region and the original EEPROM remain untouched. Native xemu keeps exclusive HDD
image ownership and writes its ordinary game saves. No peer saves, BIOS or account
identity are sent. This is native own-HDD persistence, **not** a semantic save adapter
or transactional whole-HDD rollback. A network loss revokes forwarding while the
game remains open; normal Home/save/Exit retains process authority. No automatic rejoin.
A missing/invalid personal setup fails preparation; it is never filled from a friend.

### Pin, packaging and maintenance

Reviewed upstream: [`25e1ee2fd59a451f2ab57e825b4f8a5414220c8b`](https://github.com/xemu-project/xemu/tree/25e1ee2fd59a451f2ab57e825b4f8a5414220c8b),
`0.8.139-2`. ARM64 AppImage SHA-256:
`0a2e4ff856644ae741a7476bc9c9dcd115b681f9faae26444df74ace2d275441`.
The already owned package is staged independently; the ordinary Applications xemu
is not replaced. [Official networking](https://xemu.app/docs/networking/),
[CLI](https://xemu.app/docs/cli/), native `ui/xemu-net.c`, `hw/xbox/xbox.c` and
`hw/xbox/eeprom_generation.{h,c}` were checked. No emulator source patch.

[package.py](../../packaging/emulators/xemu/package.py) verifies runtime/helper
hashes, stages a new version, clones files on Btrfs where available and publishes
its manifest last. It never installs BIOS/HDD/EEPROM. Build `bba_endpoint` from
`tools/ps2-lan` with `BUILD_DISC_READER=OFF`; reuse the existing `stream_transport`.
The local manifest records the device's config path; this is not a shared identity.
Updates must repeat native NIC attachment and private-data tests, then verify Lobby
preparation/loss. Rollback selects the previous shell and removes only this package;
never restore an older HDD over new personal progress or discard a native-generated
EEPROM. Source preparation and installation are separate below.

Installed on Flip/Odin from `7220e65b`; production shell SHA-256
`207bc5112d544a823d31389d56bdb36a87b24d7a9e6e7eb8cad211bcacbef4ae`.
[Checks and delivery](../evidence/xbox-system-link-20261026/README.md).

## 18 October 2026 delivery

Original Xbox is catalogue ID `xbox`, ISO content, standalone adapter `xemu`.
TrainerOS discovers an existing native/AppImage or `app.xemu.xemu` installation
and uses the [upstream CLI](https://xemu.app/docs/cli/):
`-full-screen -dvd_path CONTENT`. The common controller bridge and owned-process
Home/exit integration are registered at composition, not in a per-game branch.
The ordinary UI does not ask the player to edit a binding.

Changed files: `data/rom-platforms.json`, `EmulatorDiscovery.cpp`,
`StandaloneAdapter.cpp`, `Main.cpp`, and `overlay_support.py`; standalone and
owned-process tests cover the Xbox command. Existing firmware/HDD/EEPROM and
controller settings are retained. No emulator source patch or firmware redistribution.

Observed Odin `xemu.AppImage` SHA-256:
`3f87b43fc6c080a7106faf44a1d9afd1871fb8ab7d39c7cfb7b6e8f831b6bf32`.
This identifies the tested existing package; it is not a newly selected upstream
version or a reproducible distribution pin. Package version/rebuild acceptance
remains for the emulator standard.

**Device check:** Games → Mojo! → A opened the game; right changed Single Player
to Cooperative. Home/Exit closed it before the next sample. The existing input
configuration worked and was not replaced. No multiplayer, full game, audio,
save persistence or physical-controller acceptance is claimed. Xbox was tested
on Odin; the common TrainerOS binary was also delivered to Flip.

Update: retain per-user firmware/EEPROM/HDD identity and use the same ordinary
launch/input check after changing xemu. Rollback the shell/helper together using
the pre-LAUNCH-80 backup; retain the independent emulator and personal data.
Never infer a working Xbox installation merely from the new catalogue entry.

[Whole sweep and installed shell identity](../LAUNCH80_LAUNCHER_AUDIT.md#общий-проход-до-xbox-original--wii-u).
