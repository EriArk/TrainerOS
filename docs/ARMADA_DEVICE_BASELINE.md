# Flip 2 application baseline — 2026-09-11

These findings come from the owner's Snapdragon 865 Retroid Pocket Flip 2. They describe this installation, not every ArmadaOS release or Retroid model. Private device reports, addresses, keys and game inventories stay outside Git.

## System and display

- ArmadaOS Preview `20260910.07c9dcb`, signed `armada:testing` image, Fedora 44 ARM64, kernel 7.2.3. It boots from internal UFS. The previous Beta deployment remains available for rollback.
- SDDM offers Armada Plasma, Plasma Mobile and Steam Gamescope sessions. Gaming Mode uses `gamescope-wl`; its process name differs from the original baseline script's `gamescope` assumption.
- The official `/usr/libexec/armada/session-control` switches between desktop and gaming mode. Both modes remain installed. TrainerOS still runs as an ordinary application; no TrainerOS session or default-login change is shipped.
- KDE reports DSI-1 at native 1080×1920/60 Hz, rotated to landscape. Scale 1.5 produces a 1280×720 logical desktop over the 1920×1080 physical display. Advertised DRM modes alone would misleadingly suggest a portrait application viewport.
- The runtime contains Qt 6.11.2, QSQLITE, Qt Quick/Shapes/Wayland and SDL2 compatibility 2.32.72 over SDL3 3.4.16. Build tools and development headers are absent from the host.
- InputPlumber exposes a mapped Xbox controller. Physical button-label agreement, comfort, latency and hinge behavior need the owner at the device; synthetic input does not establish these results.
- Sleep is deliberately disabled at the owner's request. Do not perform the first-run guide's suspend/hinge checks until that request changes. No sleep reliability claim is made.

## Native build and installation

Use a rootless Fedora 44 Podman container on the ARM64 handheld, with only the source/build directory mounted. Build tools belong in the container; the executable uses the existing host Qt/SDL runtime. Do not use `dnf` to mutate the bootc host for this development build.

The tested build environment uses GCC 16.2, CMake 4.3, Ninja, `qt6-qtbase-devel`, `qt6-qtdeclarative-devel` and `sdl2-compat-devel`. Include Qt's SQL plugin and Qt Test for verification. The initial Fedora container image ID was `sha256:cc520476538d32d3a570918d061b7627bcf49432163520dd6168298db51c4fa1`; repository packages can change independently.

Configure with an explicit final prefix before building, for example:

```sh
cmake -S /work/source -B /work/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/home/armada/.local
cmake --build /work/build --parallel 3
ctest --test-dir /work/build --output-on-failure
```

The CMake install now includes the application and a desktop launcher. `CMAKE_INSTALL_PREFIX` is resolved into that launcher's executable path; configure it for the actual destination rather than overriding only `cmake --install --prefix`. Stage installation with `DESTDIR` when the final prefix is outside the container mount, then copy the staged files into the user's matching prefix. No administrator privileges or session installation are needed.

Run the built executable against the host's libraries as well as testing inside the container. An offscreen pass establishes ABI/QML behavior; a Wayland GPU run establishes rendering against the actual graphics stack. Neither is a substitute for physical controller and readability acceptance. Check a `BUILD_TESTING=OFF` build for the installed everyday application.

The desktop launcher's ID is `org.traineros.TrainerOS`, also declared by the Qt application for Wayland identification. Start → Exit Development App remains the explicit return path. The binary and launcher can be replaced independently of personal data.

First verification: all 15 CTest entries passed on the ARM64 Fedora container (75.35 seconds). The same executable passed shell and diagnostic SDL scenarios against ArmadaOS host libraries, then the Worlds rendered scenario in the actual KDE Wayland session with the software-renderer override removed. Captured layouts were inspected. A separate `BUILD_TESTING=OFF` configuration built and staged the executable/launcher successfully. These checks exercise synthetic controller events; physical controls and long play sessions remain unverified.

## Removable content layout

The dedicated microSD uses one ext4 partition, label `TRAINERDATA`, mounted by UUID at `/var/mnt/games`. The mount is optional at boot and uses systemd automount; a missing card must not prevent recovery into the existing OS. `~/Emulation` points to this mount. The mountpoint itself is not user-writable when the card is absent.

```text
Emulation/
  roms/{gb,gbc,gba,nds,n3ds,n64,gc,wii,wiiu,switch,pokemini,...}/
  bios/
  saves/{retroarch,melonds,dolphin,azahar,cemu}/
  states/
  screenshots/
  library/manifests/
  backups/
```

Platform directory names follow ES-DE/Armada conventions (`gc`, `n3ds`). Preserve edition, language, revision and hack names inside those directories. These are external content paths, not World IDs. TrainerOS registers each Adventure under its actual region(s), with a custom World when appropriate; a filename alone must not invent a region or progress.

TrainerOS's SQLite metadata, emulator installations and emulator configuration stay on internal storage. Game content, BIOS and external saves remain separate from the application. Source files are copied, never moved or deleted; a private manifest records original paths, destination paths, sizes and SHA-256 values. Do not commit that inventory or any copied game/BIOS data.

## Emulator packaging observed

Armada Store currently installs RetroArch and Dolphin as system Flatpaks. Its melonDS, Azahar Enhanced, Cemu and ES-DE entries install ARM64 AppImages under `~/Applications`. Store templates and desktop entries are used, preserving its update tracking. Some web documentation still describes older Flatpak packaging for melonDS; inspect the installed catalog rather than hard-coding that older path.

RetroArch's ARM64 core updater needs Armada's configured aarch64 buildbot URL. Core availability differs by build: the observed feed has `parallel_n64`, not `mupen64plus_next`. Installed emulators and copied content do not imply TrainerOS adapters, exact resume, save parsing, achievements or game compatibility.

Sources: [Armada Store](https://armadaos.dev/using-armada/armada-store/), [emulator conventions](https://armadaos.dev/emulation/emulators/), [ES-DE integration](https://armadaos.dev/emulation/es-de/), plus the installed catalog and live reports.
