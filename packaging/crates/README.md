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
python3 packaging/crates/build.py hextris /work/crates-output crates-examples-20261018
python3 packaging/crates/build.py breakout /work/crates-output crates-examples-20261018
python3 packaging/crates/build.py qalculate /work/crates-output crates-examples-20261018
```

The command produces the binary archive, original upstream source archive and
release record. The 2048 package adds the GPL-3.0-or-later runner in `webgame/`;
publish this repository's exact source revision/build instructions alongside the
MIT game source. FeatherPad retains its original GPL license and source. The
standalone launch flag prevents attaching to another, unmanaged editor instance.

`packages.json` is a maintainer-reviewed, locally installed trust registry of
exact GitHub Release URLs, sizes and SHA-256 digests. It is installed with the
host helpers under `/var/opt/traineros/crates/`, not read from the presentation
catalogue. It contains no installer commands or host paths. It remains the bundled
bootstrap for the first packages. CRATES-77 adds an Ed25519-signed catalogue;
the installer verifies the signed snapshot again before accepting its records.
The public key is installed with the host helper, never taken from a response.
Do not replace the bootstrap with unverified remote JSON.

The host launches `installer.py` in a separate process. Packages are extracted
without links/special files into bounded staging. A durable receipt selects a
content-addressed version; the previous version survives interrupted replacement.
Data stays under `data/<project>` and survives remove/reinstall. Updates and
downgrades are explicit, never scheduled by browsing. Retry currently restarts
the download; HTTP range resume is not advertised. Tasks persist in the common
Downloads provider. A launcher-held `flock` protects live/minimized applications.

`launch.py` is host code; metadata cannot replace its arguments with shell text.
It resolves `TRAINEROS_SDK_ENDPOINT` against the host's original XDG data directory
before assigning private package data paths. Installed SDK apps therefore discover
the same local host as development launches; only the path, never its token, is passed.
Release admission accepts only `EriArk/TrainerOS` and the owner's `EriArk/crates`
GitHub Releases, retaining exact URL/size/hash checks. For the latter, pass
`--repository EriArk/crates` to `publish_release.py`; project approval and signed
catalogue publication remain required. This does not publish a catalogue entry.
On the current Odin 2/Flip 2 Armada images the managed web runner uses Vulkan,
matching the browser composition workaround. Other applications are unaffected.
Apps get a standard Desktop Entry and use existing Apps window/input ownership.
Games register one stable `crates:<id>` entry and use AdventureLaunchController,
the ordinary checkpoint, history and game overlay. Package removal tombstones
that registration without moving a ROM or erasing history; reinstall restores it.
System-module SPI and broad device compatibility remain separate work.

Checks: `tests/CratesInstallerTests.py`, `crates_showcase`, `crates_qml_smoke`,
managed registration case in `LibraryTests`, and the managed game close case in
`OverlayHelperTests`. Synthetic packages never enter the published registry.

## Catalogue and publication

`catalogue.py` runs in a short-lived worker. On explicit Crates entry it performs
one HTTPS request with revision/digest. There is no startup timer, GitHub polling,
background update or implicit install. Snapshot/delta responses authenticate the
same final canonical JSON. Ed25519 verification uses platform OpenSSL. The cache
is replaced atomically after signature/schema/hash checks; a failed/offline sync
keeps the prior view and installed software remains usable. Unknown/stale cursors
receive a full snapshot in that one request. Unchanged currently takes 109 bytes.

Signed fields include project identity, manual crate ordering, all release
records and optional content-addressed PNG artwork. Bounds: 12 MiB response,
2 MiB presentation, 128 images/8 MiB total, 2048×2048 and 1 MiB per image.
No remote scripts, path-based assets, arbitrary download hosts or installer
commands are accepted. GitHub release URLs, exact size and digest are required.
Withdraw a release with `withdrawn:true`, or remove a project and its releases
from a later snapshot. Installed receipts/data are not touched by syncing.
The server retains 32 historical revisions for delta lookup.

The small `catalogue_server.py` serves only signed metadata, bound to localhost
behind HTTPS. `traineros-crates-catalogue.service` is the service template.
The server has no signing key or executable upload API. Keep the private key
outside Git, the build container and the serving account. `publish_catalogue.py`
checks increasing revision and immutable published package bytes. For approved
project releases, `publish_release.py` publishes assets and then the signed
catalogue in one maintainer action (no second approval or client GitHub scan):

```
python3 packaging/crates/publish_release.py RELEASE_TAG \
  --record /output/hextris-record.json --asset /output/hextris-aarch64.tar.gz \
  --asset /output/hextris-source.tar.gz --asset /output/traineros-crates-source.tar.gz \
  --asset /output/SHA256SUMS --key /private/crates-key.pem --ssh-target catalogue-host
```

Asset retries compare bytes and never overwrite an existing release asset. Server
publication uses a unique staging file, revision comparison, a publication lock,
fsync and atomic replacement. A concurrent-publisher conflict requires re-reading
the latest revision; it cannot overwrite another publication. No GitHub Actions
workflow is installed. The independent signing key must be backed up privately;
key rotation requires a reviewed host update, not a catalogue instruction.

## Remaining example recipes

Hextris retains gameplay/local saves; the port removes ad scripts, score telemetry
and analytics, blocks remote requests and hides web store/share controls. Breakout
keeps its MIT game code; original Freesound recordings/Flash are omitted from the
binary and replaced with original tiny procedural Web Audio cues. Original source
archives retain upstream notices. Both packages use the existing managed web
runner and a responsive viewport; they do not install another browser engine.

Qalculate! pins both Qt UI and libqalculate. Build dependencies additionally include
autoconf/automake/libtool, gettext-devel/gettext-autopoint, autoconf-archive, intltool,
libicu-devel, gmp-devel, mpfr-devel, libxml2-devel and libcurl-devel. The package
contains the stripped library and definitions; Qt resources include translations.
The wrapper uses `QALCULATE_DEFINITIONS_DIR` and `--new-instance`, so it stays inside
the managed data/window lifetime. GMP, MPFR, ICU, libxml2, curl and Qt use the
reviewed Armada runtime; ELF dependency availability must be checked on each new
baseline. Source archives for both UI and library accompany the release.
