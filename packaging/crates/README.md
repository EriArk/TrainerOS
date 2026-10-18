# Crates package recipes and host installer

Each `recipes/<id>.json` pins an upstream revision and the entry point prepared
by `build.py`. Run the build inside the reviewed Armada Fedora 44 ARM64 builder,
never on the player's device. Required build packages are CMake, GCC, Git,
Qt 6 Quick/WebEngine development files and SDL2; FeatherPad additionally uses
Qt SVG/Tools, Hunspell and X11 development packages. Target runtime dependencies
are the matching host Qt packages, SDL2 and Hunspell; no second browser engine or
compiler is included in an installed package. This is a baseline-specific package,
not a compatibility promise for every ARM distribution.

```
python3 packaging/crates/build.py 2048 /work/crates-output
python3 packaging/crates/build.py featherpad /work/crates-output
```

The command produces the binary archive, original upstream source archive and
release record. The 2048 package adds the GPL-3.0-or-later runner in `webgame/`;
publish this repository's exact source revision/build instructions alongside the
MIT game source. FeatherPad retains its original GPL license and source. The
standalone launch flag prevents attaching to another, unmanaged editor instance.

`packages.json` is a maintainer-reviewed, locally installed trust registry of
exact GitHub Release URLs, sizes and SHA-256 digests. It is installed with the
host helpers under `/var/opt/traineros/crates/`, not read from the presentation
catalogue. It contains no installer commands or host paths. It is a bootstrap
for the first packages; it does **not** implement the remote delta catalogue or
release signing of #146. Do not replace it with unverified remote JSON.

The host launches `installer.py` in a separate process. Packages are extracted
without links/special files into bounded staging. A durable receipt selects a
content-addressed version; the previous version survives interrupted replacement.
Data stays under `data/<project>` and survives remove/reinstall. Updates and
downgrades are explicit, never scheduled by browsing. Retry currently restarts
the download; HTTP range resume is not advertised. Tasks persist in the common
Downloads provider. A launcher-held `flock` protects live/minimized applications.

`launch.py` is host code; metadata cannot replace its arguments with shell text.
On the current Odin 2/Flip 2 Armada images the managed web runner uses Vulkan,
matching the browser composition workaround. Other applications are unaffected.
Apps get a standard Desktop Entry and use existing Apps window/input ownership.
Games register one stable `crates:<id>` entry and use AdventureLaunchController,
the ordinary checkpoint, history and game overlay. Package removal tombstones
that registration without moving a ROM or erasing history; reinstall restores it.
System-module SPI, additional recipes, signed server sync, and broad device
compatibility remain separate work.

Checks: `tests/CratesInstallerTests.py`, `crates_showcase`, `crates_qml_smoke`,
managed registration case in `LibraryTests`, and the managed game close case in
`OverlayHelperTests`. Synthetic packages never enter the published registry.
