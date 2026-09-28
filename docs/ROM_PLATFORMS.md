# Ordinary ROM launch and ARM64 folders

Owner scope, 2026-09-28: broaden ordinary launch toward Batocera's system range,
prepare the folders, exclude only systems without an ARM emulator route rather
than excluding old machines. A small representative launch check is sufficient;
per-title troubleshooting and deep save work remain deferred. This extends the
library priority without replacing any R1–R18 acceptance or Steam integration.

## Delivered contract

`data/rom-platforms.json` is the shared registry for folder aliases, labels,
supported file formats and preferred Libretro cores. Its first 105 categories
include 100 core-backed categories (72 distinct ARM64 cores), existing standalone
DS/GameCube/Wii routes, and reserved PS3/Switch folders with documented native
ARM64 upstream projects. Reserved folders do not advertise a launch capability.
This is not complete Batocera parity: its engine ports, utilities and remaining
machine-specific software-list routes still need their own mapping. They are
unmapped, not declared impossible. ARM availability does not prove performance,
controller suitability, firmware readiness or every game's compatibility.

`tools/prepare-rom-folders.py ROM_ROOT --apply` prepares the tree from that same
registry. Default behavior is a dry run. Existing aliases win; files are never
moved, deleted or replaced. New folders use Batocera names such as `gamecube`,
`3ds`, `megacd`, `wswan`, `wswanc`, and `msx1`, while keeping existing internal
platform IDs and older folder names valid. Empty folders do not create cards.

`tools/install-arm64-cores.py CORES_DIRECTORY --apply` installs only missing
official Libretro Linux/aarch64 binaries. It validates ELF64/AArch64 headers,
uses bounded archives and no-clobber publication, and records URLs and hashes
privately. Existing cores are preserved. It never downloads BIOS, ROMs or keys.
This is an explicit preparation tool, not a resident downloader or an automatic
runtime update policy. Runtime dependency checks are separate from ELF identity.

Discovery uses existing Batocera `gamelist.xml` and local media; XML is optional.
Images outside a suitable cartridge format (PICO-8 PNG) do not become games.
CUE/GDI/CCD/M3U track dependencies stay attached to their game. Existing
registrations and intentional custom bindings remain intact. Refresh configures
previously unconfigured registrations when a matching installed route exists,
without changing IDs, titles, history, Worlds or saves. UI/progress refresh waits
for the batch instead of rebuilding every page after every inserted binding.

Generic RetroArch launch invokes the executable with literal arguments, checks
files off the GUI thread, preserves ordinary emulator save locations and disables
automatic savestate load/save. Missing content/core and reported load failure
return to the shell. The verified mGBA Trainer save route and Sega/Neo Geo CD
firmware gates remain intact. Compressed GBA requires extraction to `.gba` for
that verified route. BIOS, multipart media, keyboard-heavy computers and system
assets retain their emulator requirements; a mapped extension is not proof that
an incomplete game dump can boot.

Exact-build Pokémon providers alone authorize semantic reads/writes. A generic
launch never grants healing, Party edits, Trainer save isolation or another
game's progress parser. ScreenScraper's existing 33 mapped service IDs remain a
separate capability: new core routes do not invent external service identifiers.
Local gamelist metadata is available on all discovered systems.

## Upstream evidence

Checked 2026-09-28:

- [Batocera system definitions](https://github.com/batocera-linux/batocera.linux/blob/master/package/batocera/emulationstation/batocera-es-system/es_systems.yml): folder/category naming.
- [Official Linux/aarch64 core builds](https://buildbot.libretro.com/nightly/linux/aarch64/latest/): binary availability, not a Flip compatibility list.
- [Libretro core info](https://github.com/libretro/libretro-core-info): supported extensions and firmware/resource requirements.
- [PPSSPP setup](https://docs.libretro.com/library/ppsspp/): required redistributable helper assets, separate from a PSP BIOS.
- [ARMSX2](https://github.com/ARMSX2/ARMSX2): native ARM64 PS2 route; requires its resources and a user BIOS.
- [RPCS3 ARM64 binaries](https://github.com/RPCS3/rpcs3-binaries-linux-arm64) and [Eden releases](https://git.eden-emu.dev/eden-emu/eden/releases): reason to retain PS3/Switch folder slots. Their TrainerOS launch adapters are still pending.

## Verification

Native coverage exercises alias discovery, existing-record binding without
overwriting custom routes, ordinary launch independent of exact-save ownership,
literal arguments, unavailable content/core, load errors, companion disc tracks,
PICO-8/media separation and preservation of the existing protected routes.
Private device reports and captures live outside Git under
`work/research/rom-*`; no private content or emulator binaries are bundled.

Flip preparation installed 61 missing cores, preserving the original 11;
all 72 passed dependency resolution inside the actual RetroArch Flatpak runtime.
The total core footprint is about 890 MiB. PPSSPP helper assets are installed;
existing owner PS1/Dreamcast/arcade BIOS remain in place, and the existing PS2
BIOS was copied to the core's expected directory. Additional firmware/resources
for untested systems are not claimed complete.

Actual Flip checks launched PS1 Castlevania: Symphony of the Night and PSP
LocoRoco through short A in Worlds. Physical Home opened the guarded exit
overlay. PSP's core renames the RetroArch main thread to `Main`; the overlay now
checks the executable identity after its existing owned-process/start-time
validation, rather than trusting the thread name. Cancel kept the same game
alive; confirm returned to TrainerOS. That earlier probe used the PPSSPP Libretro
core. The standalone delivery below supersedes its pending-route status.

The earlier 128 MiB exit-picture limitation is resolved by the delivery below.
Native PS3/Switch and per-system resource/controller proof beyond the
representative launches remain pending.

Earlier installed production build:
`b77a24e79e7282fac55ebedbae83eae398207ec88bdc2eb9efe65203669ce49c`.
SQLite schema 14, 830 records (including the removed library-test record) and
all three Trainers were preserved. Actual controller navigation confirmed both
Worlds re-entry roots; captures are `worlds-reentry-pokemon.png` and
`worlds-reentry-multiverse.png` under the private research directory. Closing a
modal or returning from an Adventure does not trigger this primary-page reset.

Verification: all 44 native CTest cases passed across the full run and targeted
reruns after updating old route-retention expectations; the four Linux overlay
helper tests passed on Flip. Adapter knowledge/export checks passed unchanged.
Both native and ARM64 production builds completed. No GitHub Actions gate used.

## Standalone PSP and large-disc pictures — 2026-09-28

The router prefers configured standalone PPSSPP for newly discovered PSP files.
Existing explicit routes are preserved; Flip's four PSP bindings were migrated
after a database backup, retaining identities and history. The adapter accepts
ISO/CSO/PBP/CHD, launches literal file arguments with `--fullscreen` and
`--pause-menu-exit`, and retains PPSSPP's ordinary saves/settings. See the
[upstream command-line reference](https://www.ppsspp.org/docs/reference/command-line/).
No global emulator input remap or new semantic PSP save capability is implied.

PPSSPP 1.20.4 standalone was installed on Flip; Odin's existing installation
was reused. LocoRoco passed launch, game controller input, Home overlay, cancel
to the same process and confirmed exit on both. Linux overlay ownership checks
now accept the actual PPSSPPSDL executable, not an arbitrary renamed thread.

Optional exit pictures now support large ROMs: files up to 128 MiB retain full
SHA-256; larger files use a `sample-v1:` fingerprint over size and three 256 KiB
samples (start/middle/end), alongside registration, owner, size and mtime checks.
This is bounded worker-thread presentation validation, **not a byte-exact hash**;
unsampled edits preserving size/mtime can escape it. Never use it as evidence
for save writes, achievements, lineage or exact-build matching. Their full-build
verification is unchanged. Schema remains 14. A 2 GiB sparse regression fixture
checks reload and sampled mutation; the actual 589,758,464-byte PSP ROM produced
a persisted JPEG after clean exit.

Current ARM64 production SHA-256:
`bc68663a830535bfb6fe7c0b026b85fd9e5e263e662044053c46e23501c68a9a`.

## Native PS2 correction and wider setup — 2026-09-28

Odin already had ARMSX2 2.7.1-35-ge2f7d015cb, a BIOS and an SDL controller
configuration. Its PS2 file was discovered but left unconfigured because the
router had no standalone PS2 adapter. `armsx2` is now a supported standalone
installation, preferred over a core for new PS2 bindings when configured.
It accepts ISO/CHD/CSO/BIN/GZ and invokes `-batch -fullscreen -- FILE` as literal
arguments; no savestate option or semantic save capability is added. Existing
explicit custom routes remain intact. A private `integrations/armsx2.json`
uses the standard version/program/runtimeFile/prefixArguments/validatedPlatforms
contract (`adapter: armsx2`, `validatedPlatforms: [ps2]`). Odin reuses its
existing Armada performance wrapper and AppImage, BIOS, settings and memory cards.

Short A launched The Matrix: Path of Neo CHD on Odin. Home opened the guarded
question, B returned to the running game and confirmed A returned to TrainerOS.
The actual `armsx2-qt` window owner is checked through the existing process
ancestry/start-time boundary. A single pidfd-targeted SIGTERM invokes ARMSX2's
normal Qt graceful shutdown, avoiding a second desktop confirmation. It is
never repeated for the same process: upstream treats a second signal as forced
exit. The upstream busy-memory-card guard remains authoritative. Odin's existing
SaveStateOnShutdown=false was retained; no emulator configuration was rewritten.
This is a launch/return check, not game completion or PS2 save-isolation proof.

Sources: upstream [QtHost command-line and signal handling](https://github.com/ARMSX2/ARMSX2/blob/master/pcsx2-qt/QtHost.cpp)
and [MainWindow shutdown](https://github.com/ARMSX2/ARMSX2/blob/master/pcsx2-qt/MainWindow.cpp),
cross-checked against installed `-help` and actual device behavior.

Owner follow-up: broaden ready-to-use emulator setup ahead of individual games.
Next extend the existing shared registry with native/Flatpak candidate discovery,
standalone preference and explicit controller/firmware prerequisites. The target
is copy ROM → launch A once that platform is ready. Setup must represent missing
prerequisites, not require registering every game. This is still follow-up work,
not a claim all 105 categories are installed or launch-tested. Keep representative
proof and defer per-title tuning; retain Steam/Plasma, user configurations and
every earlier roadmap acceptance.

Current installed ARM64 SHA-256:
`b0b1f608b70cb85e399bd984478ec44fd10958dce9af7f3fcc558f3214773d19`.
