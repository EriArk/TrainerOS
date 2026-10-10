# Common emulator set

**10 October battery continuation:** [SameBoy](emulators/sameboy.md) adds its native
two-ROM/SRAM subsystem for generic supported GB/GBC battery link sessions. Ordinary
Gambatte, DoubleCherryGB volatile/netpacket and gpSP routes are preserved. The
automatic preparation and completed From Below Pocket match/save readback on both
handhelds are recorded in [MP-02](HANDHELD_MULTIPLAYER.md); wider compatibility and
external acceptance remain open.

**10 October 2026:** DoubleCherryGB also supplies its existing two-machine
rollback mode for GB/GBC link games without persistent cartridge memory. The
existing RetroArch relay/invitation integration is reused; each player sees and
controls their own machine. Ordinary Gambatte and the independent-save netpacket
profiles remain intact. This does not claim universal battery-save or all-game
compatibility; [MP-02 evidence](HANDHELD_MULTIPLAYER.md) owns the boundary.

2026-10-03. Owner-requested selection for every TrainerOS handheld/image variant.
This is the target distribution set and migration contract, **not an installed
image manifest or completed multiplayer support**. The current ARM64 targets are
Flip 2 and Odin 2. Other devices require their own build/input/performance proof.
[ROADMAP](ROADMAP.md) remains the execution queue; the
[multiplayer matrix](EMULATOR_MULTIPLAYER_MATRIX.md) owns route evidence.

**First installed baseline, 2026-10-03:** both devices now verify the same 72
core artifacts and RetroArch/PPSSPP/Dolphin package revisions. Odin's previous
cores and extra installed cores are preserved. The
[manifest](../packaging/emulators/arm64-baseline.json) and
[maintenance/recovery record](emulators/retroarch.md) identify actual delivery.
Remaining standalone selections below are still targets; this does not finish
the whole standard image. The subsequent [Dolphin bridge](emulators/dolphin.md)
adds bounded two-device Melee NetPlay evidence with a separately maintained
native artifact; broader routes and internet acceptance remain open.

**Flycast package checkpoint, 2026-10-03:** Flip now has the same standalone
2.7 Flatpak commit and executable hash as Odin, recorded in baseline revision
`arm64-20261003.2`. Existing core routes/settings/saves were not migrated.
[Maintenance and WAN findings](emulators/flycast.md) keep installed-package
alignment separate from the still-missing standalone/network integration.
Internet reachability is the priority; local gameplay alone will not close it.

## Selection

One primary runtime per platform, common across devices. Device profiles change
render resolution/backend and controls where needed, not arbitrarily the emulator
or multiplayer protocol. Retain existing user routes/saves until a verified
migration. Do not silently launch an incompatible fallback when a friend joins.

| Systems / Batocera IDs | Target primary | Selection and remaining work |
| --- | --- | --- |
| NES / FDS | RetroArch + FCEUmm | Retain current route; lock the same core build across the image family. |
| SNES / Satellaview / Sufami / MSU-1 | RetroArch + Snes9x | Retain; extensions/content still need mode-specific multiplayer support. |
| GB / GBC | RetroArch + Gambatte; DoubleCherryGB and SameBoy link companions | Ordinary play remains Gambatte. DoubleCherryGB supplies netpacket/volatile pairs; SameBoy supplies supported battery pairs with automatic own-save preparation. [Evidence and limits](HANDHELD_MULTIPLAYER.md). |
| GBA | RetroArch + mGBA; gpSP and mGBA Splitscreen link companions | Ordinary save integration stays on mGBA. Reviewed cable protocols and bounded RFU profiles use [gpSP](emulators/gpsp.md); generic 2-4-player cable parties use mGBA Splitscreen. [Exact builds, Kirby gameplay and limits](emulators/mgba-splitscreen.md) remain separate from wider game/capacity acceptance. |
| Nintendo 64 | RetroArch + ParaLLEl-N64 | Retain current primary for initial standardization. Align builds before paired proof; no blanket deterministic-netplay claim. Another core becomes primary only after a concrete compatibility/performance comparison. |
| Nintendo DS | melonDS standalone | Common desktop Linux build on both devices; normalize the present AppImage/Flatpak difference. Local wireless and WFC are separate modes. |
| Nintendo 3DS (`n3ds`) | Azahar standalone | Select one maintained Linux ARM64 build for both devices. Flip's Azahar-Enhanced is an observed candidate, not an automatically approved upstream equivalent. Retire the catalogue's legacy Citra core default only with route/save migration. |
| GameCube / Wii | Dolphin standalone | Retain; same NetPlay-compatible build on every peer. Separate GameCube controller and Wii motion profiles. |
| Master System / Game Gear / Mega Drive / Sega CD / SG-1000 | RetroArch + Genesis Plus GX | Retain. |
| Sega 32X / Pico | RetroArch + PicoDrive | Retain; align installed core builds. |
| Saturn | RetroArch + YabaSanshiro | Retain as the initial ARM route, with measured per-device compatibility; networking is not implied. |
| Dreamcast / Naomi / Naomi 2 / Atomiswave / System SP | Flycast standalone | Prefer one native network implementation and configuration family. Existing Flycast core remains a compatibility route until launch/VMU migration is proven. Not all arcade cabinet-link modes equal Dreamcast GGPO. |
| PlayStation (`psx`) | DuckStation standalone | Target ordinary-play primary on ARM64; preserve the existing PCSX-ReARMed route until memory-card/controller/launch migration passes. No automatic switch to a netplay fork or alternate core for online play. |
| PlayStation 2 | ARMSX2 standalone | Preserve Odin's working installation. Standardize its exact ARM build and BIOS/memory-card handling; a PS2 network adapter is not universal couch-game netplay. |
| PSP | PPSSPP standalone | Retain verified isolated configuration route; Lumines evidence is bounded and experimental. |
| FBNeo / Neo Geo | RetroArch + FBNeo | Retain; ROM-set and core revisions must match the selected compatibility profile. |
| MAME | RetroArch + MAME | Retain for content outside the selected FBNeo set. Do not run every arcade ROM through both or silently substitute ROM-set requirements. |
| Remaining classic computers/consoles and fantasy systems | RetroArch + existing registered core | Exact mapping below covers all other catalogue IDs. Keep empty folders hidden; core availability and playable speed remain separate. |
| Wii U | Cemu, conditional device tier | Retain the family, but identify/pin the actual Linux ARM64 fork/build. Upstream release assets inspected were x86_64; installed ARM files do not establish upstream package provenance or acceptable speed. |
| Switch | Eden, conditional device tier | Selected candidate because upstream publishes Linux ARM64 and ARM64 room binaries. No local install/launch/network proof; do not promise all Switch games or substitute an Android package. |
| PS3 | RPCS3, conditional device tier | Preserve Odin's existing file/setup. ARM64 availability is not performance or game-service compatibility proof. |
| Vita (future catalogue entry) | Vita3K, conditional device tier | Preserve Odin's installation. Add the platform/launch adapter separately; do not fabricate a current registry entry. |

The conditional tier is part of the common policy, not a random emulator bundle
for each handheld. A device can lack a supported/usable route without losing ROMs,
metadata or saves. No platform is removed merely because it is old. The initial
standard covers all **105 existing catalogue IDs** plus explicitly pending Vita.

Primary-source basis: [DuckStation ARM support](https://github.com/stenzek/duckstation),
[Dolphin NetPlay](https://dolphin-emu.org/docs/guides/netplay-guide/),
[Azahar releases](https://github.com/azahar-emu/azahar/releases),
[Cemu releases](https://github.com/cemu-project/Cemu/releases),
[Eden ARM64 packages and room binaries](https://git.eden-emu.dev/eden-emu/eden/releases).
These support feasibility/selection, not fresh on-device acceptance. An ARM64 room
server asset is not proof that its emulator client asset is Linux ARM64.

## Version and update policy

- Release one tested **emulator/core bundle revision** for all supported images.
  Record upstream revision, package/Flatpak commit or AppImage hash, architecture,
  build flags and TrainerOS patch revision. The same frontend version is not a
  core lock. Do not download arbitrary latest nightly cores during normal play.
- Starting inventory: RetroArch 1.22.2, PPSSPP 1.20.4 and Dolphin 2606a on both;
  Odin melonDS 1.1 and Flycast 2.7. These are observed versions, not a final
  distribution lock. Unidentified AppImage builds remain unpinned until inspected.
- Keep ongoing upstream updates: stage a candidate bundle, run bounded relevant
  launch/input/save/return and paired-network checks, then promote it across device
  profiles. A compatibility group may span versions only after proof; exact binary
  identity across different CPU architectures is not the compatibility definition.
- Keep ordinary saves, per-Trainer routing, device identities, firmware supplied
  by the owner and personal/per-game preferences outside replaceable packages.
  Migrate only TrainerOS-owned settings. Revert binaries/config migrations without
  rolling back personal progress. Do not interrupt an active game to update.
- Before network launch, compare game/revision, runtime compatibility group,
  network mode, deterministic settings, required peripherals and free slots.
  A profile's capacity is not the chat group's size or the number of controller
  ports visible in an emulator menu.
- Network-internal rollback states are distinct from the retired user-facing
  savestate/resume feature; preserve that product decision. Some synchronization
  paths transfer save data: document owner/guest writeback before enabling them.
- A new standard primary never silently converts an existing user's playthrough.
  Prove ordinary save readback and return on both handhelds before migrating it.
  Emulator settings stay automatic in ordinary use; no per-ROM setup screen.
- Maintain [one record per changed emulator](emulators/README.md). Record source
  patches and build recipes in Git; local modifications alone are not the image.

## Initial inventory before alignment

Live read-only inventory on both handhelds, 2026-10-03. No runtime was installed,
updated, removed or launched into a game in this pass. Dolphin `--help` exited
normally in offscreen mode; no host/client commands were advertised.
The later installed-baseline note above supersedes these core/package differences;
the remaining standalone gaps are still open.

| Item | Flip 2 | Odin 2 | Consequence |
| --- | --- | --- | --- |
| RetroArch user core directory | 72 core files | 19 core files | Sets are not standardized yet; these counts are not readiness claims. |
| Snes9x / Gambatte / mGBA / Genesis Plus GX | Same SHA-256 as Odin | Same SHA-256 as Flip | Four sampled builds already match. |
| ParaLLEl-N64 | `7301ca3ea503…` | `06f783cf4d63…` | Different bytes; compatibility must not be inferred from RetroArch 1.22.2. |
| Flycast core | `0f18e86be41e…` | `516f73191c01…` | Different bytes; standalone 2.7 is a separate runtime. |
| FBNeo | `ffb37315a609…` | `cd0c9f649753…` | Align selected build and compatible content set. |
| PicoDrive | `6b0dbe4e21f0…` | `df1cbc80eedd…` | Different bytes, not by itself proof of a desync. |
| melonDS | AppImage present, version not established here | Flatpak 1.1 | Common source/build contract still needed. |
| Flycast standalone | Not found in inspected locations | Flatpak 2.7 | New Flip delivery needed for this route. |
| 3DS | Azahar-Enhanced AppImage present | No Azahar found in inspected locations | Standard client build and Odin delivery needed. |
| Other AppImages | Cemu | ARMSX2, Cemu, DuckStation, RPCS3, Vita3K | Presence only; versions/provenance/capabilities still need pinning. |

Inventory inspected Flatpak app records, PATH candidates, `~/Applications` and
`~/.local/bin`; absence here is not a whole-filesystem assertion. Existing
installations and their ordinary settings remain untouched.

## Catalogue coverage

The following snapshot of `data/rom-platforms.json` groups every ID exactly once.
The explicit standalone choices above override the future image primary, not the
current file or live library. Unchanged rows retain their existing core candidate;
they are not newly validated ARM packages or netplay profiles.

<!-- catalogue-coverage -->

| Current core / slot | Catalogue IDs |
| --- | --- |
| `(standalone slot)` | `gc`, `nds`, `ps3`, `switch`, `wii` |
| `81` | `zx81` |
| `a5200` | `atari5200` |
| `amiarcadia` | `arcadia` |
| `applewin` | `apple2` |
| `arduous` | `arduboy` |
| `armsx2` | `ps2` |
| `atari800` | `atari800`, `xegs` |
| `b2` | `bbcmicro` |
| `bk` | `bk` |
| `bluemsx` | `msx`, `msx2`, `msx2+`, `msxturbor`, `spectravideo` |
| `cap32` | `amstradcpc`, `gx4000` |
| `cemu` | `wiiu` |
| `citra` | `n3ds` |
| `dosbox_pure` | `dos` |
| `emuscv` | `scv` |
| `ep128emu_core` | `enterprise` |
| `fbneo` | `fbneo`, `neogeo` |
| `fceumm` | `fds`, `nes` |
| `flycast` | `atomiswave`, `dreamcast`, `naomi`, `naomi2`, `systemsp` |
| `freechaf` | `channelf` |
| `freeintv` | `intellivision` |
| `fuse` | `zxspectrum` |
| `gambatte` | `gb`, `gbc` |
| `gearcoleco` | `colecovision` |
| `genesis_plus_gx` | `gamegear`, `mastersystem`, `megadrive`, `segacd`, `sg1000` |
| `gw` | `gameandwatch` |
| `handy` | `lynx` |
| `hatari` | `atarist` |
| `lowresnx` | `lowresnx` |
| `mame` | `mame` |
| `mednafen_ngp` | `ngp`, `ngpc` |
| `mednafen_pce_fast` | `pcengine`, `pcenginecd` |
| `mednafen_pcfx` | `pcfx` |
| `mednafen_supergrafx` | `supergrafx` |
| `mednafen_vb` | `virtualboy` |
| `mednafen_wswan` | `wonderswan`, `wonderswancolor` |
| `mgba` | `gba` |
| `minivmac` | `macintosh` |
| `neocd` | `neogeocd` |
| `np2kai` | `pc98` |
| `o2em` | `odyssey2`, `videopacplus` |
| `opera` | `3do` |
| `parallel_n64` | `n64` |
| `pcsx_rearmed` | `psx` |
| `picodrive` | `pico`, `sega32x` |
| `pokemini` | `pokemini` |
| `potator` | `supervision` |
| `ppsspp` | `psp` |
| `prosystem` | `atari7800` |
| `puae` | `amiga1200`, `amiga500`, `amigacd32`, `amigacdtv` |
| `px68k` | `x68000` |
| `quasi88` | `pc88` |
| `retro8` | `pico8` |
| `same_cdi` | `cdi` |
| `sameduck` | `megaduck` |
| `scummvm` | `scummvm` |
| `snes9x` | `satellaview`, `snes`, `snes-msu1`, `sufami` |
| `stella` | `atari2600` |
| `theodore` | `thomson` |
| `tic80` | `tic80` |
| `uzem` | `uzebox` |
| `vecx` | `vectrex` |
| `vice_x128` | `c128` |
| `vice_x64` | `c64` |
| `vice_xpet` | `pet` |
| `vice_xplus4` | `cplus4` |
| `vice_xvic` | `c20` |
| `vircon32` | `vircon32` |
| `virtualjaguar` | `jaguar` |
| `wasm4` | `wasm4` |
| `x1` | `x1` |
| `yabasanshiro` | `saturn` |
