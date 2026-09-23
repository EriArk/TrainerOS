# Multiverse collection preparation

The owner's 2026-09-23 request brings the server-disk file preparation forward
within P3/P4: approximately four titles per system, with more small games.
This is separate from real TrainerOS library binding and runtime acceptance.
The installed application and personal library database are unchanged by this copy.

## Selection

The initial selection contains **133 games across 22 systems**, approximately
**40.09 GiB including BIOS and disc companions**. Multi-disc games count once.

| Systems | Games per system |
| --- | ---: |
| Game Boy Advance, Super Nintendo, Mega Drive | 12 |
| Nintendo 64 | 10 |
| Nintendo DS, 32X, Neo Geo Pocket Color, Neo Geo, FBNeo arcade | 8 |
| GameCube, Wii, Wii U, PlayStation 2, PSP, PlayStation, Mega-CD, Dreamcast, Neo Geo CD, Atomiswave, NAOMI | 4 |
| Commodore 64 | 1 |
| PC Engine | 2 |

The last two systems had fewer available titles in the inspected source. Other
empty source systems remain content gaps. Existing Pokemon files are retained.
PS3, Xbox, Xbox 360 and other unverified runtime routes were not bulk-copied;
this does not declare them incompatible or remove them from the platform review.
NAOMI's selected lightgun games still need a verified controller aiming route.

## File layout and verification

- Shared microSD library: `roms/<system>/Multiverse/`. Existing Pokemon folders
  are untouched. Canonical folder names include `gc`, `megadrive`, `psx` and
  `dreamcast`; adapters must resolve them explicitly.
- Keep disc sets together, preserve CUE/GDI track references, and use one M3U
  entry for the three-disc PlayStation selection. Preserve arcade set filenames
  and required CHD subdirectories; readable names belong in metadata.
- Place available BIOS under `bios/` with the required subdirectories; arcade
  parent/dependency archives also accompany their sets where required. Do not
  copy source memory cards, NVRAM or personal saves. Runtime-specific installation
  and BIOS selection still require the adapter's configuration step.
- Copy directly from the server into a task-owned microSD staging directory.
  Check every copied file against its source SHA-256 before publication, validate
  referenced disc tracks, and CRC-check selected game ZIP archives. Never replace
  different existing destination files. Source originals remain untouched.
- Two initial Atomiswave choices failed source ZIP validation. The final selection
  uses the CRC-verified Guilty Gear Isuka and Faster Than Speed archives instead.
  Preserve rejected-source details privately; failed files do not enter the library.
- Keep the edition/source/hash manifest under the private device
  `library/manifests/` directory, with a readable collection list under `library/`.
  ROMs, BIOS, full source paths and manifests are excluded from Git.

## Delivery verification

Delivery on 2026-09-23: all **189 files / 43,050,789,132 bytes** passed destination
SHA-256 verification and were published on Flip's microSD. The final 48 selected
game ZIPs and six distinct BIOS ZIPs passed archive CRC checks. Disc references
and final entry paths resolve. The card retains approximately **376 GiB free**.
The task-only transfer key was revoked after publication; originals and existing
library files were preserved. No new runtime launch or physical-input acceptance
is claimed by this file-preparation increment.

## Remaining P3/P4 acceptance

[Schema-11 binding](MULTIVERSE_BINDING.md) now registers these files with stable
Multiverse identities, independent per-Trainer Home choices and shared
launch/history services. Complete the remaining runtime routes and game media. Include every credible
Flip/Armada system route, hiding platforms without content independently of the
current search filter. Missing media must preserve existing identity/history.

Inventory/install the remaining viable emulators and cores, configure BIOS and
controller mappings, then verify ordinary launch, usable input, save creation and
clean exit/return for each runtime. A verified copy is not a verified playable
title. Do not show fixture titles as installed content or infer readiness merely
from an emulator's presence in Armada's wider device catalogue.

The [roadmap](ROADMAP.md#later-collection-deliverable--representative-classics)
retains the full playable-collection acceptance and unresolved P9 runtime work.
The [UI boundary](MULTIVERSE_UI.md) still distinguishes the delivered presentation
from pending real library binding.
