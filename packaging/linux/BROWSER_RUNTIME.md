# Embedded browser runtime

TrainerOS uses the host Qt WebEngine Quick package, matching the other host Qt
libraries. The optional build requires Qt WebEngine 6.9 or later; the checked
Armada devices and builder have `qt6-qtwebengine-6.11.2-2.fc44.aarch64`.
The Fedora builder also needs `qt6-qtwebengine-devel`. Without that dependency
the shell builds with an explicit unavailable browser, not an external browser.

Do not bundle a second mismatched Qt, copy only Chromium's helper, disable its
sandbox, run the shell as root, or propagate browser flags to emulator children.
The host package owns QtWebEngineProcess, its resources/locales and security
updates. Future installer dependency/redistribution checks remain open (#115).

The service lazily creates the engine profile. It enables Chromium's existing
`ParallelDownloading` feature only during initialization, then restores the
environment. Explicit feature overrides are respected. Chromium decides whether
the response qualifies; it owns cookies, validators, range requests and fallback.
There is no second HTTP client replaying authenticated download URLs.

Build/check on an ordinary logged-in ARM device:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target traineros trainer_browser_tests trainer_browser_probe
ctest --test-dir build -R '^browser$' --output-on-failure
python3 tests/browser_fixture.py /tmp/trainer-browser-fixture 18771
# In another terminal in the device's graphical session:
PROBE_DATA=/tmp/trainer-browser-check PROBE_URL=http://127.0.0.1:18771/range \
  PROBE_MODE=close-tab build/trainer_browser_probe
```

On the tested Flip 2 Mesa/OpenGL route, Chromium tiles were visibly corrupted.
The shell's existing device graphics policy now selects Vulkan for Flip 2 when
WebEngine is built, retaining curve text and explicit Qt overrides. Odin already
used Vulkan. This does not export a renderer choice to emulators. Both browser
normal/fullscreen rendering and the ordinary chassis need device acceptance on
new hardware/driver versions; do not generalize this workaround to all devices.

Create `PROBE_DATA` first. Probe duration is 25 seconds. `PROBE_MODE=pause` and
`cancel` exercise the native controls; URLs `/plain`, `/login`, `/blob`, `/changed`
exercise single-stream, cookie authorization, blob and stale-validator rejection.
The fixture writes deterministic test files to the ordinary Downloads directory;
remove only those files after checking their hashes. The probe never bypasses
the engine sandbox. Do not run it over a live emulator/save transaction.

References: [Qt download ownership and lifecycle](https://doc.qt.io/qt-6.10/qwebenginedownloadrequest.html),
[Chromium parallel job admission](https://github.com/chromium/chromium/blob/main/components/download/internal/common/download_job_factory.cc),
[WPE architecture](https://wpewebkit.org/about/architecture.html).
Qt/Chromium retain their upstream licenses; host-package reuse does not close
the distribution license audit. The six new controls use the existing MIT
[Phosphor notice](../../assets/icons/browser-controls/README.md).
