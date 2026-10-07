# Third-party materials and licensing status

Original TrainerOS code is licensed under [GPL-3.0-or-later](LICENSE); see
[scope and redistribution](LICENSING.md). This inventory does not relicense
third-party materials or constitute completed legal clearance. Existing terms
remain in force. Distribution/image obligations need a separate review before release.

| Material | Existing record / terms |
| --- | --- |
| Fredoka, Chakra Petch, Bungee fonts | [Font provenance](assets/fonts/README.md); included SIL OFL 1.1 texts |
| PokeAPI reference data | [BSD notice](data/licenses/PokeAPI.txt); names/design rights are separate |
| Community gym-badge recreations | [Author, CC BY 3.0 and provenance](assets/badges/README.md); underlying designs are not cleared by attribution |
| Dolphin integration patch | [Source SPDX notice](packaging/emulators/dolphin/TrainerNetplay.inc), GPL-2.0-or-later; [maintenance](docs/emulators/dolphin.md) |
| Emulator builds and changes | [Per-emulator records](docs/emulators/README.md); each upstream's actual license/source obligations apply |
| Qt, SDL, OpenSSL, QtKeychain and other linked dependencies | External build/runtime dependencies; review the exact versions and redistribution conditions when packaging |
| Practice runtime dependencies | [Runtime installer](tools/install-practice-runtime.py); external components retain their own notices |
| Generated collection illustrations | [Generation record](assets/series/generation.json); provenance is not trademark/design clearance |
| Private art/sprite packs and game media | Not licensed for redistribution by this repository; screenshots may depict separately supplied material |

ROMs, BIOS/firmware, keys, commercial saves and private media packs must not be
added to the repository or assumed to be included in a public image. Existing
README screenshots are retained pending the owner's replacement pass; retaining
them does not resolve their underlying asset rights.

Pending decisions and release checks are tracked in
[public readiness](docs/PUBLIC_READINESS.md), alongside the existing artwork and
image-distribution issues. Game compatibility references do not imply affiliation
with game, console, service or emulator owners.

## Source compatibility review — 2026-10-06

Review scope: direct libraries in `CMakeLists.txt`, maintained emulator integration
boundaries and existing notices. No apparent blocker to GPLv3 was identified in
these declared dependencies. This is not an exhaustive audit of transitive codecs,
system packages or a built image's source/notice delivery.

- **Qt 6:** upstream provides LGPLv3/GPLv3 options for the relevant open-source
  libraries; some Qt modules have GPL-only options. Preserve module-specific and
  third-party notices. [Qt licensing](https://doc.qt.io/qt-6/licensing.html).
- **QtKeychain:** upstream's three-clause BSD terms require preserved source/binary
  notices and prohibit endorsement without permission.
  [Upstream COPYING](https://github.com/frankosterfeld/qtkeychain/blob/main/COPYING).
- **SDL2:** zlib license; preserve its notice and distinguish modifications.
  [Upstream license](https://github.com/libsdl-org/SDL/blob/SDL2/LICENSE.txt).
- **OpenSSL 3:** Apache-2.0. CMake requires version 3; do not substitute the
  differently licensed legacy OpenSSL 1.x without a new compatibility review.
  [Upstream license](https://github.com/openssl/openssl/blob/openssl-3.0/LICENSE.txt).
- **Dolphin modifications:** GPL-2.0-or-later already permits the GPLv3 option;
  existing file notices stay unchanged. These are built with the separate emulator.
- **DoubleCherryGB:** the maintained upstream uses AGPL-3.0 according to its actual
  license file. It remains an external emulator/core with its own obligations;
  TrainerOS's root license does not replace them. See [maintenance](docs/emulators/doublecherrygb.md).
- **Practice engine:** pinned Pokemon Showdown/Node dependencies are external and
  retain their upstream terms/notices; the installer already carries the supplied
  Node license. Review the exact dependency tree with release artifacts.

The reusable adapter folders contain a copy of the GPL text and a scope notice
so original source does not lose its license when copied to another project.

Social uses unmodified **Phosphor Icons** regular SVGs under MIT, pinned to
`2b75f3ad12b420c9504ef05df8d2564a28f8500e`. The complete notice is in
[assets/icons/phosphor/LICENSE](assets/icons/phosphor/LICENSE) and embedded in
the application resources. Fluxer uses the same icon family. Its React application
code is not bundled; the native client uses the provider's documented HTTP API.
