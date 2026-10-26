# Azahar: native 3DS wireless rooms

## Lobby groups — 26 October 2026

The maintained Azahar 2126.2 room adapter now admits two to four independent
consoles. The upstream [Room](https://github.com/azahar-emu/azahar/blob/219d743a1e698101fe1ab8bef43e6563126c3c27/src/network/room.cpp)
already supports a configurable member count; no 3DS wireless protocol or
per-game implementation is added. The former two-player profile below is historical.

S06 fixes a consecutive P1–Pn roster. S03 supplies players/seat to the private
launcher, and sends each ready endpoint through `GameParty::readyMember` only to
that assigned guest. The host opens one existing secure channel and a distinct
loopback UDP socket per guest; native ENet therefore sees separate clients.
The channel identity binds the ticket-derived secret, roster size and guest seat.
The native room password remains common to that admitted group. P1 creates the
room once all channels connect; each client launches only from the accepted Start.
Running is announced only after all native clients join. Late join/relaunch is
not enabled. A missing game, changed seat/roster or profile rejects preparation.

`native-room-group-private-v2` and, for installed-content matching,
`native-room-group-content-v3` expose capacity four. Old installed two-player
bundles retain capacity two. Own NAND/SDMC, shader cache and ordinary input/graphics
preferences are preserved; the private launch configuration owns only this session.
This extends all eligible 3DS content already supported by the profile, not a title list.

Exit now sends a bounded end-of-room record and awaits its ordinary transport
receipt before closing helpers. Directly terminating a Dilation helper could leave
other players waiting for reconnect/receipt timeout; a native relay group reproduced
that failure. Every edge gets the same bounded cleanup, with graceful helper close
and final child reaping. Native traffic keeps its original ENet/datagram semantics.

Rebuild using `build.sh` and the same pinned unified source and three patches.
Stage via `install-runtime.py`; do not edit only the installed manifest capacity.
Run `check-room-session.py`, the shared `../melonds/check-secure-channel.py`, and
`check-groups.py --python <bundle-python> --helper <bundle-secure-channel>` on Linux.
For native protocol coverage configure the upstream build with
`-DCMAKE_PROJECT_INCLUDE=<absolute-recipe>/native-room-probe.cmake`, build
`traineros_room_probe`, and pass it as `--runtime` to `check-groups.py`.
The probe reuses upstream Room/RoomMember/ENet and tests broadcast and unicast;
it is not an emulator or shipped runtime. Repeat with `--mode online --relay-only`
and `--drop-channel --counts 4` for interrupted transport. Remove the optional
CMake include when maintaining an ordinary release build. Rebase the room patch
and its included file together; preserve the existing content patch and save paths.

Installed on Flip/Odin from `531e9af1`; shell SHA-256 `3317a810674c723f33df2ea928561aac558a0f96b0c5ada44bcdf67408a2f1f9`.
[Checks, delivery and rollback](../evidence/azahar-groups-20261026/README.md).

## Native content installation — 26 October 2026

The pinned `219d743a1e698101fe1ab8bef43e6563126c3c27` source now has a bounded
headless `--traineros-content` / `--traineros-install` entry point in
`TrainerContent.inc`, applied by `traineros-content.patch` after the room patch.
It reuses Z3DS IO, CIAContainer, NCCH/loader and AM installation. It accepts only
decrypted executable base/demo titles, updates and DLC on SDMC; system titles and
encrypted content are rejected. No game, GPU window or room is started. The meta
CLI forwards these commands before ordinary getopt processing. Missing personal
storage roots use private metadata directories with validated path translation,
without creating personal folders during inspection.

`content.py` supplies isolated settings, reads the owner's NAND/SDMC selection,
hashes base plus installed update/DLC trees, and observes missing dependencies.
Explicit Play/Start prepares a base package and substitutes its native APP entry.
Library Properties also exposes cancellable installation for base/update/DLC.
Lobby inspection never installs. The existing package card remains the stable
library identity; a title receipt records source hash, installed hash and entry.
Existing APP registrations still work; this does not discover an unrelated NAND
inventory or make a missing source package unnecessary.

Native installation writes a private stage on the destination filesystem.
Promotion replaces only that title's content and tickets, never its save data,
other titles or personal INI. A file lock excludes concurrent installations;
active 3DS processes and shell game/save operations block installation. A journal
records renames, retains old content and rolls back interruption before retry.
Recovery refuses to overwrite content changed externally after interruption.
The child dies with the cancelled helper; RAM, file/tree sizes and time are bounded.

Rebuild with the existing `build.sh` recipe and ALSA development headers.
Stage using `install-runtime.py`; the manifest adds `content.protocol` and the
helper hash. Old bundles retain their former image-only admission. Run
`check-content.py` on Linux for real filesystem transactions and
`check-native-content.py --runtime /absolute/path/azahar` for generated key-free
CIA/ZCIA containers through the actual native reader/installer. Fixtures contain
no game code and do not prove gameplay. Rebase all three patches together;
rollback changes runtime/helper selection, never restores old NAND/SDMC over saves.

The earlier statement below that installed update/DLC content is not matched is
superseded for the new content-marked bundle. Artic Base and encrypted CIA remain
outside this route. [Current cross-platform delivery](../MULTIPLAYER_CONTENT_FORMATS.md).

## CIA/ZCIA installation boundary — 25 October 2026

Reviewed the pinned `219d743a1e698101fe1ab8bef43e6563126c3c27` implementation;
this section is preparation, not an enabled CIA launch route. The existing
[Qt `--install` command](https://github.com/azahar-emu/azahar/blob/219d743a1e698101fe1ab8bef43e6563126c3c27/src/citra_qt/citra_qt.cpp)
calls native `Service::AM::InstallCIA` and exits with a result code. Linux reports
the result on stdout; Windows uses a message box. No second installer or CIA
decoder is needed. The native [AM implementation](https://github.com/azahar-emu/azahar/blob/219d743a1e698101fe1ab8bef43e6563126c3c27/src/core/hle/service/am/am.cpp)
wraps ZCIA with `Z3DSReadIOFile`, rejects encrypted contents and resolves the
installed executable through TMD content indices. A CIA is not a path accepted
by `Loader::GetLoader`; an update/DLC package must not become a game card.

Remaining integration: explicit import/preparation with cancellation and progress;
isolated native install before committing content into this player's configured
storage; title/content inventory and a stable library binding to the executable;
fingerprints of the base title and loaded updates/DLC; preserve existing saves,
custom NAND/SDMC paths and failure recovery. Metadata probing must reuse native
TMD/NCCH readers, including compressed containers. Existing room preparation
still launches only executable images and does not run installation during a
background Lobby scan. This is a concrete missing installation/identity chain,
not a per-game compatibility whitelist or another networking protocol.

**25 октября — доставка:** расширенные форматы и совместимая со свежим core/main
оболочка установлены на Flip/Odin. [Хеши, native проверки, сохранность данных
и откат](../MULTIPLAYER_CONTENT_FORMATS.md#доставка-на-flip-и-odin--25-октября-2026).
Это заменяет прежние отметки «не установлено» для данного набора, не прежнюю
историю игровой совместимости и не открытые ограничения native механизма.

## Native image aliases and Z3DS containers — 25 October 2026

Закреплённый Azahar `219d743a1e698101fe1ab8bef43e6563126c3c27`:
сверены [native loader](https://github.com/azahar-emu/azahar/blob/219d743a1e698101fe1ab8bef43e6563126c3c27/src/core/loader/loader.cpp)
и `file_derived`/`zstd_compression`. Добавлены APP, ARM32 ELF/AXF и
ZCCI/ZCXI/Z3DSX в регистрацию/допуск; Z3DS v1 проходит bounded header/seek-table
проверки, штатный Azahar декодирует исходный файл. Whole-container hash сохранён.
ARM64 admission проверяет valid/invalid metadata; это не полное native
распаковывание или retail gameplay. CIA/ZCIA installation и updates/DLC отдельно.
[Форматы, проверки, обновление и откат](../MULTIPLAYER_CONTENT_FORMATS.md).
Native package не менялся; новая оболочка не установлена.

**23 October 2026 ? NetPlay cache repair:** The private user profile now links only `shaders` to a persistent cache keyed by the runtime digest. Own NAND/SDMC and private configuration policies are unchanged.
[Implementation, installed build and acceptance limits](../NETPLAY_PLATFORM_REPAIR_2026_10_23.md).

## Handheld delivery — 12 October 2026

Azahar [2126.2](https://github.com/azahar-emu/azahar/releases/tag/2126.2), revision
`219d743a1e698101fe1ab8bef43e6563126c3c27`, is installed on Flip 2 and Odin 2.
Official unified-source SHA-256:
`85c8b56fac703df04720125044b8ff887997911858a6cde545dfb1107ba4cd26`.
Both run the same maintained Linux ARM64 Qt client, native ARM JIT and Vulkan.
The initial TrainerOS profile is two participants; it is not all-game certification.

Each player runs an independent 3DS. Upstream ENet rooms carry its native local
wireless traffic. TrainerOS creates a loopback-only room and joins both clients
through the existing admitted Nearby/Online friend flow; players enter no IP,
room password or VPN setup. The authenticated channel reuses the DS transport
with a distinct application/protocol namespace. It transports bounded native
UDP datagrams, not emulator states or parsed game saves.

### Player-visible evidence

The author-provided [Minicraft3DS v1.5.2](https://github.com/andre111/Minicraft3DS/releases/tag/v1.5.2)
was used privately. Release ZIP SHA-256:
`8c66e69a06c9b3ee16c19f8ed8c29d82206ee689dc8691e5076bcd0544475d8f`;
3DSX SHA-256: `2e00bf65c9fa28d384506e97fa1b0dbb89f68f69540dc72ea7c9fb37558f81b1`.
No retail 3DS content was present in the checked owner folders. Author test
assets are not included in the repository or cleared for image redistribution.

- Separate native checks reached the same world through Nearby and a forced
  public internet relay, with distinct player controls and views. In the final
  relay check the bridge observed 14,733/9,805 datagrams and zero queue drops
  across about 280 seconds including menus; the final gameplay segment exceeded
  one minute. This is not a long gameplay soak or separate-ISP acceptance.
- Ordinary installed **Online friend** invitation passed guest consent while a
  solo game was running, Keep for later and return to the same solo PID, then
  Saved — join. Host Start requested solo save/leave. Both native clients then
  launched automatically and entered the saved common world through the game's
  own Host/Join menus. The actual channel configuration was Online on both.
- Host Save progress wrote the ordinary local world (13,490 bytes). Home worked
  on both devices. Host Minimize returned to the library; Return to game retained
  PID 867196. Guest PID was 477055. Host Exit/Leave closed both native processes
  and removed both private session directories. TrainerOS remained running.
- The normal screenshot channel omits the external invitation badge plane;
  these gameplay captures do not re-certify or disprove that separate overlay.
  Its earlier acceptance remains in [invitation handoff](../INVITATION_HANDOFF.md).

### Data ownership and preservation

Each player keeps ordinary own NAND/SDMC, including configured custom storage,
and own system data. Only network/UI/cache configuration is private. There is
no peer save transfer, mirrored console or temporary-progress warning for this
route. Minicraft itself saves the common world on the host; no guest world file
was fabricated. This is ordinary emulator writing, not a semantic save adapter.

Before this delivery there was no existing 3DS progress on either device.
Flip's earlier Qt configuration/log existed and its Azahar-Enhanced AppImage was
retained. Existing library bindings are preserved; only the newly imported test
entry on each device was explicitly changed from the legacy Citra fallback.
Ordinary native launches may update their own UI settings/logs; custom storage
and existing input preferences are preserved by discovery and covered in tests.

Final audit: 367/25 pre-existing save files and 14/13 configuration files from
other emulator families matched the immediate 3DS-delivery baseline; both
SQLite quick checks returned `ok`. Device volume remained zero. Earlier during
a private focus check, an unintended Wii title launch was closed normally; it
changed five Dolphin console bookkeeping files and Qt/time-played settings,
not game progress. That change was recorded before the current baseline; do not
claim byte-for-byte equality with the preceding Wii-delivery baseline.

### Maintained changes and failure fixes

Sources live in `packaging/emulators/azahar/`:

- `TrainerRoom.inc` and `traineros-room.patch` provide a private launch contract
  around native Room/Create and RoomMember/Join; Qt upstream otherwise ignores
  its multiplayer CLI argument. Join/error/readiness are bounded private IPC.
  Native room creation supplies upstream NullBackend when web services are off.
- `libusb-init-failure.patch` initializes the GC adapter context/thread flags.
  Both devices exposed libusb initialization error -99 and the old destructor
  could crash on normal exit. The final build passed that error branch and
  ordinary solo/paired close without another core dump. Controller support is
  retained. A Flip core at 09:20:32 belongs to the earlier, superseded binary.
- Managed native close bypasses the emulator's second confirmation only under
  the exact TrainerOS environment marker, without persisting that preference.
  The common Home/Exit integration recognizes the managed process.
- `room-session.py` bounds queues, datagram sizes and admission time; coalesces
  each one-millisecond native burst into a bounded record before consuming a
  WAN receipt slot. Earlier per-packet and zero-wait variants exhausted credit
  over the distant relay and stalled gameplay; only the final variant passed.
- `build.sh` validates the official archive, applies both patches and builds in
  a fresh workspace. `install-runtime.py` stages a new version, a frozen isolated
  transport environment and hash manifest; it does not select a bundle or alter
  personal data. Selection uses `azahar-netplay` and the managed launcher.
- Standalone launch uses native `-f`; discovery also checks the user's local bin
  directory when Armada's graphical PATH omits it. Explicit existing runtime
  selections remain intact. `.3dsx` imports use ordinary library discovery.

### Installed fingerprints and checks

| Component | SHA-256 on both handhelds |
| --- | --- |
| TrainerOS | `5cc97f85f402a4dbe45be579cd4527dfdabd655153b851330cdf89b2dee36520` |
| Azahar final native build | `229dcc707f24aa9e1be6f0b06c26752846f3bab2925b5e3b26c0f8be561fc174` |
| Room wrapper | `20c0c82317b43cc756512fee485cddefd7901a0990373340570029a414c47905` |
| Shared channel with 3DS namespace | `5d42c59b3b4d6b08a87715b1dd2738ee0e740d0a22f62ba3e08b6db302c5296e` |
| Transport requirements | `80aa2af03b9016ddc51f45c96ff4ddd1bed1239c0fde4948fbca76c3ef003b26` |
| Transport interpreter | `ef59e677fa67d883e7f6d2a5aabbb99f1a14dd77dad74ada90d613202843cb80` |
| Overlay integration | `e4eef04c4cffd62096f2f8662e63a7971834e2a57c08d989e1b66f0c0a0f8e67` |

Build: Fedora 44 ARM64, Qt 6.11.2, GCC 16, system Qt/SDL2, Release/Ninja,
Vulkan enabled, OpenGL/web service/update checker/scripting/GDB/LTO disabled.
Both final bundle manifests matched their installed files. Focused C++ standalone,
runtime-multiplayer, library and collection checks passed on Windows and ARM64;
the later local-bin discovery change passed standalone checks on both. Python
transport checks passed 5/5; Linux overlay checks passed 17/17. Both maintained
patches reverse-apply cleanly against the final native source.

### Remaining acceptance and updates

Two participants only. Broader games, sustained performance, physical input,
headset/audio and distinct internet connections remain open. Base content and
runtime are hashed; installed retail updates/DLC are **not** included in effective
content matching yet. Retail/firmware/key combinations, encrypted/compressed
images, CIA installation and conversion/migration from other runtimes remain
unverified. Do not infer those capabilities from 3DSX gameplay or room capacity.
The public relay was exercised, but operator-owned service availability and the
fresh-device production journey remain separate release gates. Private transport
loss closed the other native client after roughly 20–30 seconds; it is not an
instant-disconnect guarantee. Ordinary explicit party exit closed both normally.

For updates, unpack the pinned official source into a fresh directory, compare
upstream room/config/CLI/USB interfaces, rebase or retire both patches, build and
run the bounded checks above. Admit a new manifest only after native gameplay,
ordinary invitation/exit and data checks; never modify an installed bundle during
play. The version directory, prior application/overlay and pre-change database
were retained locally for rollback. Roll back executable selection only when no
game/save operation is active; preserve NAND/SDMC, custom paths and newer saves.
No whole multiplayer, licence, external-package or owner-acceptance issue closes.
