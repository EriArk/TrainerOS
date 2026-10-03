# Dolphin integration checkpoint

2026-10-03. Both handhelds retain Flathub Dolphin 2606a, commit
`50741ae7267181560aa88d3a1ba7177321629c171ad2677504409afa6b4462db`,
using `org.kde.Platform/aarch64/6.10`. The common
[manifest](../../packaging/emulators/arm64-baseline.json) records its executable
hash. Neither installation/configuration nor save directory changed in the
common-core delivery. Existing ordinary launch/controller/Home behavior remains.

## Next native bridge

No TrainerOS Dolphin NetPlay bridge is installed yet. The upstream
[2606a frontend](https://github.com/dolphin-emu/dolphin/tree/2606a/Source/Core/DolphinQt)
resolves to `c77bbaa0f372c3f72281602a8b087206706542cb`.
`MainWindow::NetPlayHost` creates the server and joins its local client;
`NetPlayJoin` consumes direct/traversal configuration;
`NetPlayDialog::OnStart` checks available game data and requests native start.
Do not bypass those preconditions or substitute external widget clicking.

Prepared an isolated ARM64 source checkout with pinned submodules in the existing
Fedora 44 build container. Added build-only packages: libcurl-devel,
libusb1-devel, bluez-libs-devel, libevdev-devel, SDL3-devel, libXrandr-devel,
libXi-devel, libXcursor-devel, pulseaudio-libs-devel, alsa-lib-devel,
libglvnd-devel, libXinerama-devel and qt6-qtsvg-devel.
CMake configuration succeeded with Ninja/Release, `ENABLE_TESTS=OFF`,
`ENABLE_LLVM=OFF`, `ENCODE_FRAMEDUMPS=OFF`, `USE_DISCORD_PRESENCE=OFF`,
`ENABLE_AUTOUPDATE=OFF`, `LINUX_LOCAL_DEV=ON`. No compiled/patched artifact is
claimed. This container has Qt 6.11.2 and Dolphin uses Qt private GUI headers:
do not drop its eventual binary into the existing Qt 6.10 Flatpak. Select a
matching build/runtime environment as part of the maintained artifact recipe.

The complete next chain remains isolated session configuration/save ownership,
native host/join/start/status, admitted participants mapped to controller slots,
existing Home/Social consent, real game/control/exit checks on both devices and
direct/traversal failure handling. Four ports alone do not prove four-client
play. Keep strict-NAT and distinct-network acceptance in the
[multiplayer matrix](../EMULATOR_MULTIPLAYER_MATRIX.md).

Any later patch must travel with source revision, build recipe, update/rebase
checks and binary/config rollback. Preserve the current Flatpak route and user
preferences until that replacement passes its actual handheld gates.
