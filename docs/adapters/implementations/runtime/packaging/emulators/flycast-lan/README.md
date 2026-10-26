# Native Flycast cabinet package

Two to four independent Naomi arcade cabinets (Maximum Speed serial: two), selected through the existing GameHub Lobby.
This package does not replace `flycast-netplay` or enable native Dreamcast servers.
The shared GGPO package supplies verified ROM/BIOS dependencies and its native
catalogue; no firmware, ROM or personal progress is included here.

## Build and package

Linux C++ toolchain, CMake/Ninja, Git, SDL2, Vulkan/OpenGL/EGL, ALSA, curl, udev,
Xrandr/Xi and **Asio headers** are required. Build the native executable with:

```sh
JOBS=2 sh packaging/emulators/flycast-lan/build.sh NEW_WORK NEW_PACKAGE
cmake -S tools/flycast-lan -B ENDPOINT_BUILD -DCMAKE_BUILD_TYPE=Release
cmake --build ENDPOINT_BUILD
cmake -S tools/remote-play-stream/transport -B TRANSPORT_BUILD -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build TRANSPORT_BUILD
cp ENDPOINT_BUILD/cabinet_endpoint TRANSPORT_BUILD/stream_transport NEW_PACKAGE/
python3 packaging/emulators/flycast-lan/bundle.py NEW_PACKAGE
```

Use Qt6 Core/Network for both helpers. Keep the transport's pinned libdatachannel
revision and its notices; include `LICENSE` and the dependency licences from
`tools/remote-play-stream/transport/licenses/` alongside `licenses/Flycast.txt` when distributing.
Install the identical verified package at
`~/.local/share/TrainerOS/emulators/flycast-lan` on both participants. Package
activation/rollback is a whole-directory operation; retain the previous shell
and package, never restore personal progress as a side effect.

## Native patches and update procedure

Pin: Dojo `d0e47e572b1e7b355e88bda8308c89d0c5156cbf`, GPL-2.0-or-later.
Reuse the adjacent Flycast launch/private-data and glslang compiler patches.

- `native-inspect.patch` bypasses SDL video initialization only when the private
  inspector is selected. It loads the actual ROM decoder before GUI/input/CPU
  start, reports native Naomi/serial capability, then exits. It cannot declare
  capability from a filename or player-count field.
- `cabinet-session.patch` uses port A for the session's local input; native Naomi
  waits up to 60 seconds and starts only when every cabinet selected in Lobby arrives. Both changes
  require private `network:TrainerCabinet`; ordinary Flycast remains unchanged.

- `private-paths.patch` isolates native lookup to FLYCAST_ROOT for managed cabinet sessions,
  preserving system XDG_DATA_DIRS for Vulkan ICD discovery. The session marker also
  enables the existing owned-window Home/exit path.

For upgrades, rebase all patches and check loader, native capability predicate,
Maximum Speed serial, embedded peer port and native packet bounds. Regenerate
the shared arcade catalogue when its source changes; compare source/runtime/
content/settings on both participants. Do not mix a new native binary with old
GGPO metadata without reviewing their loader contract.

## Checks

```sh
ctest --test-dir TRANSPORT_BUILD --output-on-failure
python3 tools/flycast-lan/check.py TRANSPORT_BUILD/stream_transport ENDPOINT_BUILD/cabinet_endpoint
```

The second check runs on a Linux host supporting unprivileged `bwrap --unshare-net`;
a build container may prohibit nested namespaces. It exercises 2/3/4 real isolated
UDP endpoints and ICE/DTLS with a child protocol fixture, including foreign source
ports, packet limits and connection loss. It is not an emulated game match.
`trainer_flycast_netplay_tests cabinetCapabilityAndPrivateLaunch` checks the real
identity/staging/launch boundary with an explicit native-capability fixture.
`TRAINEROS_TEST_FLYCAST_RECIPE` may locate its test-only recipe on the host.

See [feature, selection rationale and limits](../../../docs/emulators/flycast-cabinet.md).

Group settings/wire version: `flycast-cabinet-private-v2`. Update shell and package together;
retain both previous versions for rollback. See [routing and seat contract](../../../docs/emulators/flycast-groups.md).
