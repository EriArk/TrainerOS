# PS2 LAN session preparation and isolated launch

This module prepares a private ARMSX2 `-gamecfg <session.ini>` using the accepted
PS2 connection's local `session.conf`. It preserves the exact game's existing
overrides and leaves the ordinary configuration, memory cards, BIOS and saves
in place. It does not copy, merge, reset or interpret personal saves.

The launcher must resolve the **actual disc serial/CRC** to its current game INI,
pass `None` only when that game genuinely has no overrides, and retain a private
temporary directory until process settlement. Passing a filename guess or always
using `None` would discard per-title preferences. Use the existing protected
launch/Home/exit lifecycle. Never add the room code to argv, public descriptors,
logs, Fluxer signaling or the library database.

`prepare(source, destination, material)` accepts an absolute new destination in
an owner-only directory. Output is exclusive, mode 0600. Failed validation writes
nothing. `material` comes from the owned transport child, never a remote path.

The source interface is checked against ARMSX2 revisions recorded in
`docs/emulators/armsx2-lan.md` at the repository root. The bundle is wired into
`RuntimeMultiplayer`, exact-disc discovery and explicit Lobby Start, and installed
on Flip/Odin. Offline admission remains unfinished. The initial profile covers
Battlefront II SLUS-21240 / 02F4B541. On 25 October the owner confirmed working
LAN play after completing network setup before the utility lost controller input.
Utility input/graphics defects remain open; see the dated game evidence table.

`launcher.py` copies settings into the private session, removes library scanning
from that copy and launches ARMSX2 plus `ps2_endpoint` in a loopback-only network
namespace. An owner-only Unix socket connects the endpoint to the existing
authenticated ICE/DTLS transport. Memory cards remain owned by each player.
`package.py` assembles the helpers, pinned libchdr and notices; it includes no
emulator, BIOS, disc or personal settings. The private installation manifest
supplies the local runtime and configuration paths.

Preparation now publishes one complete `prepared/` directory by rename. Copy or
validation failure removes only its own staging directory; a killed process may
leave staging files, which the existing outer session cleanup owns. They never
count as ready. A receipt binds the request, connection bytes and private settings.
Before launch, the helper checks that receipt and the admitted runtime/settings;
expired/replaced connections or incomplete/changed INIs fail before `bwrap` starts.
An existing prepared directory is never overwritten. Native settings may change
while playing; a later launch requires a fresh session through existing Lobby.
This adds no automatic launch/rejoin or new networking mechanism.

```sh
python3 tools/ps2-lan/check_config.py
python3 tools/ps2-lan/check_disc.py
python3 tools/ps2-lan/check_launcher.py
cmake -S tools/remote-play-stream/transport -B build-ps2-sockets -DSOCKET_CHECK_ONLY=ON
cmake --build build-ps2-sockets
ctest --test-dir build-ps2-sockets --output-on-failure
```

The socket check needs Qt6 Core/Network and a C++17 compiler; no RTC dependency
download is needed. The full Linux ICE/DTLS check remains in the existing
transport CTest target and must run before packaging or installing that helper.
