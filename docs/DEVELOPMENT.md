# Native development

**Current planning boundary:** [UX-02 map](UX_OPTIONS_MAP_RU.md) specifies planned
Options/Select/live-game changes; [#161–173](EXPANSION_161_173.md) maps dependencies.
The 9 October map-first pass edits documentation only, preserving MP-02 timer WIP.
Do not infer these capabilities from the current compact-menu implementation.

TrainerOS is a C++20 / Qt Quick handheld shell on ArmadaOS. Development builds
also run as an ordinary application; building the source does not install an
Armada image, configure emulators or replace the desktop session.

Start with [the documentation index](README.md), [current tasks](CURRENT_TASKS.md)
and [engineering workflow](DEVELOPMENT_WORKFLOW.md). The previous prototype guide
is preserved as [historical evidence](archive/DEVELOPMENT_BOOTSTRAP.md).

## Dependencies

The authoritative list is [CMakeLists.txt](../CMakeLists.txt):

- CMake 3.24+, a C++20 compiler and Ninja (recommended).
- Qt 6.4+: Core, Gui, Qml, Quick, Sql, Network, Xml, Multimedia, WebSockets and
  Concurrent; Qt Test when `BUILD_TESTING=ON`.
- Qt6Keychain and a working platform keychain for real account credentials.
- OpenSSL 3 Crypto and SDL2 2.0.14+, including development headers/CMake packages.
- The QSQLITE driver and Qt Quick, Controls, Window, Shapes, QML WorkerScript/Models and
  QtMultimedia runtime modules. Use one matching Qt/compiler ABI.
- ffmpeg for the generated-video preview test; working multimedia backend/plugins
  and a graphical session for checks that decode and present video.

Ubuntu 24.04 package example (not an Armada installation command):

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qt6-multimedia-dev qt6-websockets-dev qtkeychain-qt6-dev libqt6sql6-sqlite \
  libsdl2-dev libssl-dev ffmpeg fonts-dejavu-core \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-window qml6-module-qtquick-shapes \
  qml6-module-qtqml-workerscript qml6-module-qtqml-models qml6-module-qtmultimedia
```

This dependency list was reconciled with the source on 2026-10-06; a fresh-machine
end-to-end reproduction is still a separate public-readiness acceptance item.

## Build and run on Linux

```sh
git clone https://github.com/EriArk/TrainerOS.git
cd TrainerOS
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/native --parallel 2
./build/native/traineros --windowed --data-dir "$PWD/build/dev-data"
```

Use a disposable data directory while developing. The source checkout does not
contain the owner's library, credentials, saves or private artwork. `--ephemeral` selects
the development fixture instead of persistent personal data; it is not evidence
of a live account, supported save or working multiplayer connection.

Omit `--windowed` to request fullscreen in the existing graphical session.
Supply `-DCMAKE_PREFIX_PATH=/path/to/qt` if Qt is outside the system prefix.
Build natively on ARM64 or use a proper toolchain/sysroot; a Windows executable
cannot run on ArmadaOS.

## Verification

```sh
ctest --test-dir build/native -N
ctest --test-dir build/native --output-on-failure
```

Use the actual CTest inventory rather than historical test counts. For a bounded
change, run the relevant tests; broader input/storage/lifecycle changes warrant
the affected wider suite. Some QML tests configure offscreen/software rendering,
while media/platform tests have their own environment requirements. Host checks
do not establish handheld GPU, physical input, microphone or internet acceptance.
Never point automated tests at personal game saves or the normal data directory.

GitHub Actions is disabled by owner decision. There are no automatic or manual
Actions workflows to run. Use local/native checks and record their exact scope.
Documentation-only changes need content/link/diff checks, not emulator tests.

## Windows development

The established host toolchain is MSYS2 UCRT64 GCC with matching UCRT64 Qt/SDL.
Do not mix those libraries with an MSVC Qt installation. Example after installing
the matching dependencies (adjust the MSYS2 location):

SVG icons require the matching `mingw-w64-ucrt-x86_64-qt6-svg` runtime package,
including its image-format plugin. Qt Base alone builds the app but cannot render
those icons; keep QtSvg and the rest of Qt on the same version/toolchain.

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
cmake --build build/native --parallel 2
./build/native/traineros.exe --windowed --data-dir ./build/dev-data
```

## Current application boundaries

- Primaries: Home / Collections / Companions / Trainer / Social. L1/R1 switches
  primaries; L2/R2 switches supported peer faces, with no collection cycling on Home.
- Collections starts at its grid on primary-page re-entry. Pokemon opens games
  directly, while manual and dynamic collections share stable game identities.
  Batocera platform
  folders determine normal launch routes. A launches an installed ready game;
  holding A opens management. Y selects from global recent games without launching.
- Home during a game opens its compact overlay. Explicit Exit owns capture and
  the applicable save confirmation. Start remains the system menu.
- Game-specific data and protected writes require exact adapter capabilities;
  ordinary launch does not require a semantic save adapter.
- Network services, installed emulators, private media and hardware capabilities
  are optional external dependencies, not supplied by a clean source checkout.

For details use [navigation](NAVIGATION_111.md), [collections](SERIES_COLLECTIONS.md),
[Home overlay](HOME_MENU.md), [ROM platforms](ROM_PLATFORMS.md),
[Social](SOCIAL.md) and [adapter knowledge](adapters/README.md).

## Handheld deployment

Prepared Flip 2 and Odin 2 installations have device evidence; this is not a
claim of a finished public image or a universal installer. Read the dated
[session installation/recovery record](SESSION_PROTOTYPE.md),
[first-run behavior](FIRST_RUN.md) and [startup acceptance](STARTUP_EXPERIENCE_AUDIT.md)
before reviewing privileged helpers under `packaging/`. Preserve the existing
session, emulators, account settings and saves. Do not run every installer script
as a substitute for understanding its target and rollback.

New work follows the current task register, not the archived prototype's next steps.
