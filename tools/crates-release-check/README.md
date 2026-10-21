# Published Crates acceptance (explicit network check)

This small Linux harness builds the production Crates controller, installer
binding, dashboard and SDK host. It uses the real signed catalogue and downloads
the two published 0.1.0 x86_64 packages. It is intentionally not an automatic CTest:
ordinary source checks must not depend on a live service or download applications.

Run in a disposable Linux x86_64 container. The live check sets its own temporary
HOME and package storage, including desktop registration. Requirements:
CMake/Ninja, C++20, Qt 6.4+ Core/Gui/Network/Test/Quick/Qml and Quick Controls runtime,
Qt SVG image plugin, SDL2 runtime, Python 3, OpenSSL CLI and CA certificates. No player data, account,
private signing key or host filesystem mounts are needed. Outbound HTTPS is needed
only for the live commands; build and controller tests can run without networking.

```sh
cmake -S tools/crates-release-check -B build/crates-check -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/crates-check --parallel 3
QT_QPA_PLATFORM=offscreen build/crates-check/crates_controller_tests
QT_QPA_PLATFORM=offscreen build/crates-check/crates_release_check "$PWD/packaging/crates"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/crates-check/crates_preview "$PWD/packaging/crates" "$PWD/build/crates-preview"
```

The live cycle uses actual catalogue entry, Download, compatibility confirmation,
Downloads jobs, receipt, Open/Play signal, the managed launcher, removal and
reinstallation. It checks the timer's real external process/widget callback, a
running-package lease, app desktop registration and retained data. Game routing
is checked at the launch signal; this harness is not the complete shell's game
overlay, controller ownership or persistent Games registration acceptance.
The preview renders the actual QML catalogue and project components using live
signed metadata. It does not represent a full-shell or handheld screenshot.
