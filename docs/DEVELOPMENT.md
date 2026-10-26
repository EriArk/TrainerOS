# Native development

TrainerOS is a C++20 / Qt Quick handheld shell on ArmadaOS. Development builds
also run as an ordinary application; building the source does not install an
Armada image, configure emulators or replace the desktop session.

Start with [the documentation index](README.md), [current tasks](CURRENT_TASKS.md)
and [engineering workflow](DEVELOPMENT_WORKFLOW.md).

## Dependencies

The authoritative list is [CMakeLists.txt](../CMakeLists.txt):

- CMake 3.24+, a C++20 compiler and Ninja (recommended).
- Qt 6.4+: Core, Gui, Qml, Quick, Sql, Network, Xml, Multimedia, WebSockets and
  Concurrent; Qt Test when `BUILD_TESTING=ON`.
- Linux Files: Qt Widgets, pkg-config, GIO (`gio-2.0`) and libfm-qt6 development library.
- Browser: matching Qt WebEngine Quick 6.9+; without it the build explicitly lacks
  the embedded browser. See [runtime packaging](../packaging/linux/BROWSER_RUNTIME.md).
- Avatar preview: matching Qt Quick 3D, Quick Timeline, AssetUtils and the Assimp
  importer plugins/dependencies. The shared GLB service is independent of the
  preview renderer; see [the avatar contract](../sdk/AVATAR.md).
- Qt6Keychain and a working platform keychain for real account credentials.
- OpenSSL 3 Crypto and SDL2 2.0.14+, including development headers/CMake packages.
- PS2 ZSO identity helper: system liblz4 (`lz4-libs` on Fedora, `liblz4-1`
  on Debian). It is dynamically loaded by the isolated Python reader;
  missing library rejects ZSO without changing other disc routes.
- libarchive headers and runtime library for the PC archive importer and bounded ZIP/7z cartridge reading
  (checked 3.8.7). Fedora: `libarchive-devel` in the builder and `libarchive` at
  runtime; MSYS2 UCRT64: `mingw-w64-ucrt-x86_64-libarchive`. Keep its dependent
  codecs with the host package; linking a Windows build is not ARM64 delivery.
- The QSQLITE driver and Qt Quick, Controls, Window, Shapes, QML WorkerScript/Models and
  QtMultimedia runtime modules. Use one matching Qt/compiler ABI.
- ffmpeg for the generated-video preview test; working multimedia backend/plugins
  and a graphical session for checks that decode and present video.

Package names depend on the build host. The checked Fedora 44 ARM64 builder uses
`qt6-qtwebengine-devel`, `libfm-qt-devel` and `glib2-devel` in addition to the
existing Qt/SDL/OpenSSL toolchain. Install development dependencies in the builder;
do not mutate the handheld host merely to compile. A complete fresh-machine build
remains #117; the old Ubuntu command did not cover Files/Browser and was removed.

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

Five primaries: Home / Games / GameHub / Social / Apps. Returning to a primary
restores its place. Ordinary launch prepares a supported binding and starts the
game; a missing dependency is an explicit preparation error, not an editor.
Home owns current-game options; Start owns system widgets/actions. Minimize keeps
the same process; explicit Exit owns fresh capture and applicable save confirmation.
Game-specific capabilities require exact adapters; ordinary launch does not.
See [product contract](PRODUCT_SPEC.md) and [architecture](ARCHITECTURE.md).

## Handheld deployment

Prepared Flip 2 and Odin 2 installations have device evidence; this is not a
claim of a finished public image or a universal installer. Read the dated
[session installation/recovery record](SESSION_PROTOTYPE.md),
[first-run behavior](FIRST_RUN.md) and [startup acceptance](STARTUP_EXPERIENCE_AUDIT.md)
before reviewing privileged helpers under `packaging/`. Preserve the existing
session, emulators, account settings and saves. Do not run every installer script
as a substitute for understanding its target and rollback.

New work follows the current task register, not the archived prototype's next steps.
