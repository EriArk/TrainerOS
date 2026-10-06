# TrainerOS licensing

Copyright (C) 2026 EriArk and TrainerOS contributors.

TrainerOS is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

TrainerOS is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
TrainerOS. If not, see <https://www.gnu.org/licenses/>.

**SPDX identifier: `GPL-3.0-or-later`.** The complete, unmodified GPL version 3
text is in [LICENSE](LICENSE). The grant above explicitly permits later versions;
the presence of the version 3 text does not limit the project to GPL-3.0-only.

## Scope and existing notices

This is the default license for original TrainerOS source code, tests, build and
integration scripts, configuration and accompanying original technical documents.
It includes original code copied into the reusable adapter knowledge base.
A more specific existing file/component notice remains authoritative for that
material; preserve its author, license text and attribution when redistributing.

The default software grant does not relicense separately supplied materials:

- Fonts in `assets/fonts/` retain their SIL Open Font License notices.
- PokeAPI-derived reference data retains `data/licenses/PokeAPI.txt`.
- Badge recreations retain the author/license/provenance in `assets/badges/`.
- The Dolphin patch/bridge and its reusable copy retain GPL-2.0-or-later.
- External libraries, engines, emulator sources/binaries and dependencies keep
  their upstream licenses, whether installed separately or included in an image.
- Artwork, sprites, screenshots, audio, game media, game names, character designs
  and trademarks are not cleared or relicensed by this software grant. Existing
  asset-specific permissions remain applicable; unresolved public-art review is #116.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the inventory. No rights
to ROMs, BIOS, firmware, encryption keys, commercial saves or private media packs
are granted by TrainerOS. These boundaries identify separate materials; they do
not add restrictions to the GPL-covered original code.

## Redistribution and source

Keep this notice, the complete license and applicable third-party notices with
redistributed copies. Binary distribution must satisfy GPL requirements for
Corresponding Source and any applicable installation information. Publish the
exact source revision, patches and build/install material that produced the
binary, rather than pointing only to a moving branch or an upstream project.

The license choice does not certify a future Armada-based image or every emulator
package. Release artifact/source/relink/notice verification remains in #115 and
the image work under #70/#71. Independently distributed emulators are not silently
relicensed by sharing an image or being launched by TrainerOS.

## License text provenance

`LICENSE` is the unmodified SPDX license-list copy of
[GPL-3.0-or-later](https://raw.githubusercontent.com/spdx/license-list-data/main/text/GPL-3.0-or-later.txt),
retrieved on 2026-10-06. Its SHA-256 is
`fb981668c18a279e285fc4d83fba1e836cc84dd4daa73c9697d3cfd2d8aca6e0`.
The copyright on the license text itself belongs to the Free Software Foundation;
it is distinct from copyright in TrainerOS.
