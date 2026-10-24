# PS2 LAN session preparation (not installed)

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
[the maintenance record](../../docs/emulators/armsx2-lan.md). This helper is not
yet wired into `RuntimeMultiplayer`, game discovery, Lobby or offline admission.
No package is installed and no PS2 title is newly advertised as compatible.

```sh
python3 tools/ps2-lan/check_config.py
cmake -S tools/remote-play-stream/transport -B build-ps2-sockets -DSOCKET_CHECK_ONLY=ON
cmake --build build-ps2-sockets
ctest --test-dir build-ps2-sockets --output-on-failure
```

The socket check needs Qt6 Core/Network and a C++17 compiler; no RTC dependency
download is needed. The full Linux ICE/DTLS check remains in the existing
transport CTest target and must run before packaging or installing that helper.
