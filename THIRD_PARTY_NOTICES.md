# Third-party materials and licensing status

This is an inventory of known notices, not a new license or a completed legal
clearance. TrainerOS-owned code has no selected project-wide license yet.
Existing third-party terms remain in force and must not be replaced by a future
root license. Distribution/image obligations need a separate review before release.

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
