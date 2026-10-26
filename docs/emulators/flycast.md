# Flycast: Dreamcast and arcade multiplayer

**26 октября — native Dreamcast Online:** [DCNet исследован отдельно](flycast-dcnet.md).
Flycast 2.7 уже установлен на обеих машинах; запуск через Lobby, постоянные VMU
и вход в игровой матч ещё не интегрированы. Подготовка сети не выбирает комнату
друга автоматически; ручной вариант предложен владельцу.

**26 октября — следующий шаг:** добавлен отдельный native cabinet route для двух
автоматов через существующий Lobby/ICE/DTLS. [Контракт, сравнение реализаций и
границы](flycast-cabinet.md). Это уточняет прежнюю отметку о полностью отсутствующем
cabinet link; native Dreamcast game-server Online остаётся незавершённым.

## Naomi / Naomi 2 / Atomiswave — 26 October 2026

Реализован общий двухместный NetPlay через существующий GameHub Lobby:
Nearby и Online/public relay, явный Start, input/Home/exit и временный прогресс.
Это синхронизация одного автомата через GGPO. Соединение отдельных автоматов
реализуется отдельным [cabinet route](flycast-cabinet.md); native Dreamcast
game-server Online остаётся незавершённым самостоятельным режимом.

Используется прежний Dojo `d0e47e572b1e7b355e88bda8308c89d0c5156cbf` с уже
поддерживаемыми им платформами, без нового сетевого протокола или изменения native binary.
`runtime.flycast-dojo.arcade.v1` подключён к общим identity, picker, Local Lobby,
save/leave и restart guards. Старый Dreamcast profile/digest сохранён; отсутствие
arcade extension в старом пакете не отключает Dreamcast.

### Content and package contract

- Генератор экспортирует все **293 набора и 8 BIOS definitions** из закреплённого
  [native table](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/hw/naomi/naomi_roms.cpp).
  Это каталог эмулятора, не список проверенных TrainerOS игр. Metadata не подтверждает
  наличие второго игрока; явно одиночные игры сохраняют общий фильтр Lobby.
- ZIP/7z читаются системным libarchive с ограничениями размеров/числа записей и
  проверкой CRC. Учитываются parent/merged sets, выбранный BIOS region и CHD v5/GDI
  dependencies. Native set basename обязателен, как в самом Dojo.
- Идентичность строится по реально выбранным распакованным ROM/BIOS, а не ZIP
  timestamps, методу сжатия или лишним BIOS entries. Обе стороны сравнивают content,
  package и settings; изменение зависимостей обновляет Lobby и отменяет устаревший запуск.
- Подготовка перепроверяет состав и создаёт отдельные ROM/data/config directories.
  ROM/BIOS — ссылки на входные файлы; EEPROM/NVRAM создаются в частном сеансе.
  Личные VMU/EEPROM не импортируются и не перезаписываются. Завершение удаляет stage.
- `build.sh` генерирует каталог из pinned source; `install-arcade.py --base ...
  --destination ... --firmware-root ...` готовит **новый** пакет из проверенного
  существующего Dojo и явно указанных собственных BIOS. Установленный выбор не меняется.
  Нужны Linux Python 3 и libarchive; BIOS/ROM не скачиваются и не входят в репозиторий.
  Attribution и лицензии metadata лежат рядом с генератором и копируются в пакет.
- При обновлении native source пересмотреть loader/table, pin и SHA генератора,
  заново экспортировать каталог, пройти helper/native tests и собрать manifest.
  Для отката восстановить прежнюю оболочку и весь прежний Flycast package из backup;
  личные игровые данные откат не затрагивает.

### Evidence and limits

13 helper tests прошли на обеих портативках: все три семейства, ZIP/7z,
parent/merged sets, BIOS/content changes, malformed archives, GD-ROM dependency,
stale launch и private stage. ARM64 `flycast_netplay`, `runtime_multiplayer`,
`local_lobby` прошли; production собрана с `BUILD_TESTING=OFF`.

На исправном собственном `dolphin` (Dolphin Blue) обе машины через фактические
`netplayIdentity`/`command` дошли до `[TrainerOSFlycast] running` и `Session Start`.
Это прямой paired native probe в одной сети, не новая проверка публичного relay,
полного пользовательского Lobby-пути или сыгранного матча. Через 100 секунд probe
завершил свои процессы; оба native exit 0, private stages удалены, проверенные
личные файлы неизменны. Первая попытка с ошибочным адресом в диагностическом runner
не засчитана; исправленная попытка подтвердила оба peer.

Пять текущих Atomiswave архивов на обеих машинах не читаются; это отдельно записано
в [игровой таблице](../MULTIPLAYER_TESTED_GAMES.md#arcade--26-октября-2026).
ROM-файлы не заменялись. Доставка и точная оболочка записываются в
[публичном evidence](../evidence/arcade-netplay-20261026/README.md).

**25 октября — доставка:** расширенные форматы и совместимая со свежим core/main
оболочка установлены на Flip/Odin. [Хеши, native проверки, сохранность данных
и откат](../MULTIPLAYER_CONTENT_FORMATS.md#доставка-на-flip-и-odin--25-октября-2026).
Это заменяет прежние отметки «не установлено» для данного набора, не прежнюю
историю игровой совместимости и не открытые ограничения native механизма.

## GDI/CDI/CUE/M3U preparation — 25 October 2026

Исходный Dojo `d0e47e572b1e7b355e88bda8308c89d0c5156cbf`:
проверены `core/imgread/` GDI/CDI readers и standalone launch.
`FlycastNetplay.cpp` теперь проверяет и хеширует GDI layout/дорожки,
CDI footer, CUE dependencies и все диски M3U. У native standalone нет M3U
reader: запускается разрешённый первый диск. Смена диска в живом NetPlay
этим не реализована. Наблюдение файлов подключено к общему обновлению Lobby.
`flycast_netplay` прошёл, включая изменение/утрату дорожки и путь запуска;
сам native Dojo decoder на этих синтетических файлах не проверялся.
[Границы, обновление и откат](../MULTIPLAYER_CONTENT_FORMATS.md).
Эмулятор/настройки/временные VMU не менялись; нужна новая оболочка.

## Handheld runtime continuation - 12 October 2026

The separate native bundle is now installed on Flip/Odin. The ordinary Online
invitation, save/leave barrier and Home/minimize/guest Exit passed. A follow-up
closes the host's native disconnected window automatically for temporary TrainerOS
sessions. The final ordinary repeat invitation and guest-exit check passed: the
host also returned to TrainerOS about three seconds after the guest.

- Native executable SHA-256:
  `e81da3bbbdc14473c095478b969d4bf47b34bee83a58ea35a070d36e0eff5b77`.
  Both hosts resolve all ELF dependencies without new host OS packages. The
  earlier gameplay/first ordinary-flow binary was `2b340b45c9ff3c84a0afc465a22066c55bae6d84a484fc1c7714200ad8f5149c`;
  the final native change is confined to owned disconnected-window closure.
- Soul Calibur US (`T1401N`) booted from fresh private VMUs on both machines,
  entered a two-player fight, and accepted independent virtual Xbox input.
  This passed through direct Nearby and through the upstream public Finnish
  relay `fin-1.match.dojo.ooo:8001`. The short relay fight showed roughly
  200–214 ms in the native ping display; this is not a sustained-latency claim.
  Both handhelds still shared one home network. Distinct networks/strict NAT,
  physical controls, headset/audio and wider games remain open.
- The relay is an upstream public dependency listed in the
  [Dojo release documentation](https://github.com/blueminder/flycast-dojo/releases),
  not a newly deployed TrainerOS operator service or availability guarantee.
  The native room key and per-invitation nonce stay in accepted signalling;
  no manual relay code/address entry is added for players. Relay operators see
  traffic metadata; native UDP remains unencrypted as documented below.
- The maintained SDL launch patch maps local gamepads to port A only for a
  TrainerOS GGPO session. Odin enumerates physical and virtual pads separately;
  assigning ports by enumeration otherwise leaves its virtual pad on port B,
  which GGPO does not consume. This does not implement queued #77 controller
  identification/glyph switching or modify ordinary desktop mappings.
- Native fresh boot exposed a GGPO input-queue assertion with the previously
  configured two-frame delay. The profile now uses upstream's default
  `GGPODelay=0`, with settings identity `dojo-v2-fresh-vmu-analog2-delay0-*`.
  Both direct and relay gameplay passed with unchanged GGPO synchronization.
- Vulkan window creation failed on both hosts with a missing Vulkan surface
  extension; upstream fell back to OpenGL. The installed matching bundles now
  explicitly select OpenGL. This is a route-specific fallback, not a claim
  against other emulators' working Vulkan paths.
- The owner's existing GDI was copied and converted to CHD v5 with Fedora's
  `mame-tools 0.289-1.fc44` inside the build container. All original track hashes
  remained unchanged. The new CHD SHA-256 is
  `c1139f8efdc52d1e807dab790651a4fa484def30694f1ed639f774b361e9c0ed`.
  Separate local library copies were added; source GDI/history and ordinary VMUs
  were retained. ROM, BIOS, VMU and private captures do not enter the repository.

The bundle now also supports explicit `--nearby-only`: absent/empty `onlineRelay`
admits Nearby only, while malformed supplied endpoints fail validation. Profile
connection availability and temporary-progress consent reuse the common services.
The owned Flycast window-close path is gated by `TRAINEROS_FLYCAST_SESSION`.
ARM client/native builds, focused adapter checks and 16 overlay-helper checks
pass. Windows `flycast_netplay` passed in 0.92 s. The final shared compatibility
scan checks pass on ARM (1.58 s) and Windows (3.23 s), with both client builds
completed. Two regression
cases cover retiring an obsolete library scan and moving a newly launched,
unverified game ahead of the pending library. Cancellation finishes the current
file first; already verified hash results are reused. This is separate from the
automatic Multiplayer games collection, which uses catalogue player counts: a
newly imported entry without those counts is reachable through All games /
Dreamcast and can show Online play there, but is not included in that collection.

### Ordinary interface and data evidence

Both handhelds run client SHA-256
`7f489ae897cf4437c97b8d97c9980a937990c30b1682126ca354051a34cc3337`
and the native bundle above. The final Online friend invitation reused the
existing contact, explicit fresh-progress notice and Accept action. Start game
first offered the ordinary game's save/leave barrier, then automatically opened
the two exact native runtimes through the public relay. No room code was typed.
The final check reached synchronized fresh-VMU game startup, not another fight;
the independent-input fights above are separate native gameplay evidence.

The earlier ordinary pass exercised Home, minimize, return and guest Exit.
The repeated final pass exercised Home/Exit and automatic peer closure. History
records returned sessions of 55 seconds on Flip and 41 seconds on Odin (startup
is staggered); both private runtime directories disappeared and neither native
process remained. There was no force-stop or direct process signal in this exit.
The first bundle had required manual host Exit after the guest left; the final
owned-disconnected-window patch corrects that behavior.

All 364 Flip and 25 Odin files in the bounded pre-install save inventory retained
their hashes and none disappeared. SQLite quick checks passed and both devices
remain at zero output volume. Previous clients, helper and native bundles were
retained for rollback. Temporary fresh VMUs never replace ordinary progress.
These checks do not establish arbitrary packet-loss recovery, distinct-home NAT,
physical controller/audio acceptance or wider Dreamcast game compatibility.
The public relay's operation/availability and release dependency review remain
open; no TrainerOS operator service or encrypted native UDP is implied.

## Dojo integration — 11 October 2026

**Historical source preparation; installed evidence is recorded above.** The ordinary Flycast
2.7 package below is retained. A separate pinned Flycast Dojo bundle now has a
TrainerOS runtime adapter: two-player shared-console GGPO, native relay for Online
friend and native direct UDP for Nearby. The existing invitation badge,
save-before-replacement barrier, party and owned Home/Exit lifecycle are reused.
No game/franchise networking or emulation synchronization is reimplemented.

### Source and maintained changes

- Upstream [blueminder/flycast-dojo](https://github.com/blueminder/flycast-dojo),
  revision `d0e47e572b1e7b355e88bda8308c89d0c5156cbf` (upstream dated 30 June 2025).
- [Native relay client](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/dojo/RelayClient.cpp)
  allocates a six-character room key with `host|<base64 game name>` and admits
  the guest with `join|<key>`. Both subsequently use upstream GGPO UDP through
  that relay. This is separate from upstream Flycast's Battle Cable ICE route.
- [GGPO](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/network/ggpo.cpp)
  consumes the local controller at port A on both devices and assigns it to
  host/player 1 or guest/player 2. Two analog axes and native digital triggers
  are selected. Physical mapping, Home and audio still need handheld acceptance.
- [TrainerOS adapter](../../src/integrations/adventure/standalone/FlycastNetplay.cpp)
  recognizes matching Dreamcast CHD v5 content and an exact local bundle.
  Whole-file SHA-256, executable/firmware hashes, renderer and fixed settings
  enter the invitation identity. A CHD header alone is not validation of a
  Dreamcast disc; the runtime still validates it. GDI/CDI/multidisc and arcade
  cabinets are not enabled by this profile. Unknown player metadata is not
  treated as single-player; explicit single-player metadata excludes invitations.
- [Maintained patch](../../packaging/emulators/flycast/traineros-launch.patch)
  skips the external savestate-download prompt for a private TrainerOS session,
  starts the existing relay/direct flow automatically, emits bounded readiness,
  running and error markers, and adds the per-invitation nonce to native peer
  verification. The nonce travels only through accepted TrainerOS signalling.
  Native UDP is not encrypted; this is session binding, not a new authenticated
  transport or protection against an on-path attacker.
- Relay preparation now checks DNS/bind failure and reply source, stops the
  receive loop after five seconds and joins its worker before GGPO rebinds the
  same port. DNS lookup still follows the OS resolver timeout. Native relay and
  rollback protocols remain unchanged. A failed invitation retains owned Exit.

### ARM64 build verification — 11 October 2026

The full pinned Dojo and native launch patch now build in Fedora 44 ARM64
(GCC 16/CMake 4) on Flip. Host library resolution reports no missing dependency.
The recipe now applies [the glslang include fix](../../packaging/emulators/flycast/glslang-gcc.patch)
and [local-data-only build policy](../../packaging/emulators/flycast/local-data-only.patch),
sets the CMake policy minimum and disables unrelated CPR tests. The latter patch
removes upstream build-time shared save/NVRAM and optional training-pack downloads;
ordinary local firmware and private fresh VMUs remain required. Standalone Asio
headers are a build dependency. The Linux integration suite passes after correcting
its fake executable permissions; production executable validation was unchanged.

[Hashes, build evidence and limits](https://github.com/EriArk/TrainerOS/blob/c6aaac25ba4f55262f3f6418e8d29cac7cdc9aec/docs/MULTIPLAYER_DEVICE_RECHECK_20261011.md).
This is a private staged executable, not an installed advertised bundle. Operator
relay deployment, paired game/input/lifecycle and internet acceptance remain open.
Rebase/reverse-check both added patches with the launch patch on upstream updates;
rollback retains the previous ordinary package and user data.

### Configuration, progress and operator dependency

The bundle belongs at `$XDG_DATA_HOME/TrainerOS/emulators/flycast-netplay`
(`~/.local/share` when XDG_DATA_HOME is unset). It contains `bin/flycast`,
`firmware/dc_boot.bin`, `firmware/dc_flash.bin` and `manifest.json`. The manifest
pins the source revision, all three file hashes, `vulkan` (default) or `opengl`
renderer, and an operator-selected `udp://host:port` Dojo relay. Different relay
configuration also changes the identity. No public service is silently selected
and players are not asked for addresses or router/VPN administration.

Each process gets a fresh private `FLYCAST_ROOT`, XDG roots and working directory.
Locally supplied boot/flash are copied and rehashed; VMUs are created inside that
session. Config prevents importing ordinary settings, memory/state packs, replay
recording, streaming, automatic state save/load and shared memory copying. The
controller wrapper is retained without forwarding RetroArch arguments. Temporary
files live through process settlement and are discarded after Exit; no progress
is written back. Ordinary RetroArch/Flycast bindings, VMUs and flash remain owned
by their existing paths. This is not a save migration or semantic save adapter.

Relevant keys are `network:GGPO`, `ActAsServer`, `server`, `GGPOPort=19713`,
`GGPORemotePort`, `GGPOAnalogAxes=2`, `GGPODelay=0` (since the 12 October native check), `EnableUPnP=no`; `dojo:Relay`,
`RelayKey`, `DefaultRelayPort`, `TrainerSession`, `EnableMatchCode=no`,
`HostJoinSelect=no`, `IgnoreNetSave=yes`, `CopyMissingSharedMem=no`; and
`config:pvr.rend=4` for Vulkan or `0` for OpenGL. Firmware/region/renderer must
match. The current fixed US/English/NTSC configuration is not worldwide title
compatibility certification.

An operator must supply and validate a compatible relay, its reachability,
capacity, retention/public listing behavior and service terms before rollout.
The upstream [relay hosting page](https://lobby.dojo.ooo/host-relays.html) offers
server binaries, not a verified TrainerOS deployment/source-provenance record.
This increment does not package that server, authorize its redistribution or
prove strict-NAT connectivity. It does not introduce a player-managed VPN.

### Build, validation, update and rollback

[build.sh](../../packaging/emulators/flycast/build.sh) checks out the exact source
and submodule pins, applies the patch and builds on the target Linux architecture.
It requires SDL2/OpenGL/Vulkan development packages, libzip, libcurl, Lua 5.3,
standalone Asio headers, CMake, a C++17 compiler, Git and Python 3. Example (explicit local firmware only):

```sh
sh packaging/emulators/flycast/build.sh /new/work /new/bundle \
  /owned/firmware/dc_boot.bin /owned/firmware/dc_flash.bin \
  udp://operator.example:8001 vulkan
```

Do not commit or redistribute that local firmware. Retain upstream GPL notices
and [license](../../packaging/emulators/flycast/LICENSE.upstream); #115 image/binary
distribution acceptance remains separate. The recipe itself is not an ARM build.

Bounded local evidence on 11 October:

- Adapter checks pass: exact identity and changed-content/runtime rejection,
  isolated host/guest roots, unchanged source flash, temporary cleanup, invalid
  endpoints/keys, cancellation and bounded marker parsing.
- Shared lifecycle regression covers the host save question, cancellation,
  stale markers, mismatched guest identity and disconnect state.
- Patched native `RelayClient.cpp` compiled in a Windows socket harness, with
  configuration/Dojo globals stubbed (not a full emulator build). Real UDP host
  and guest requests/replies, a foreign-source reply and socket release passed
  against a local fixture relay. No GGPO frames, public relay or ROM ran there.
- Patch reverse-apply and shell/Python recipe syntax checks pass. Full Windows
  `traineros` build passes; CTest `flycast_netplay`, `game_party` and
  `runtime_multiplayer` pass **3/3** (3.74 seconds). These are client/fixture
  checks, not a complete Dojo build. Adapter exports and 555 local documentation
  links validate. The later ARM64 recheck above supersedes the build deferral;
  the 12 October record supersedes the delivery and gameplay deferral.

The 11 October pending list is superseded by the 12 October record above.
Retain broader compatibility/determinism, physical/audio, arbitrary loss,
distinct networks/strict NAT and public-relay release review. No issue or
multiplayer block closes. Native Dreamcast game servers/DCnet, Battle Cable,
arcade cabinet links, Hotseat and personal-progress return are separate work.

For updates, inspect upstream relay/CLI/config/GGPO verification and input/data
paths, rebase or retire each patch, build a new directory and rerun the affected
checks before promotion. Keep the previous bundle and manifest for rollback;
swap only while no owned game/preparation is active. Never overwrite ordinary
VMUs, firmware or user preferences. Removing this alternate bundle removes its
multiplayer availability while retaining ordinary play.

## Historical package checkpoint — 3 October 2026

2026-10-03 evidence only. Package installation and pinned-source audit. No TrainerOS
Flycast network adapter, actual match, standalone launch migration or internet
acceptance is claimed. The owner clarified during this investigation that
internet is the main scenario; LAN and direct nearby play remain in scope.

## Installed artifact

- Upstream `flyinghead/flycast`, tag `v2.7`, source commit
  `5aa091fde632fb332c8d8c34e280d62dc951954c`.
- Flathub system application `org.flycast.Flycast`,
  `app/org.flycast.Flycast/aarch64/stable`, commit
  `1d48669e88c0261c5c07d93bc0d48efbdbcf4be8ec5f6865825f47a7100982ef`.
- Both positively identified devices (Retroid Pocket Flip2 and AYN Odin 2)
  now have the same 29,100,344-byte `files/bin/flycast`, SHA-256
  `4b3b69727bf68781ecdba95a3c672da6279009ab87e9045630ba143b5f407c57`.
- Odin already had this package; this pass installed it on Flip. The shared
  [observed baseline](../../packaging/emulators/arm64-baseline.json) records it.
  Package equality is not proof of paired emulation or compiled feature behavior.

No game was running at the pre-install inventory. No TrainerOS executable,
controller configuration, library binding, ROM, BIOS or save was changed.
Flip still has no ordinary Flycast `emu.cfg`; Odin retains its existing settings
under `~/.var/app/org.flycast.Flycast/config/flycast/emu.cfg`. No source patch
or separate modified runtime has been deployed. The existing libretro route
remains available; installing the standalone does not migrate its VMUs.

## Source findings that change the next implementation step

The inspected [GGPO implementation](https://github.com/flyinghead/flycast/blob/v2.7/core/network/ggpo.cpp)
uses two participants, local/remote player assignment and UDP port 19713.
`network:GGPO`, `network:ActAsServer` and `network:server` initiate this path.
It can request UPnP mapping, but that is not a guaranteed route through double
NAT/CGNAT and cannot satisfy the no-router-administration requirement by itself.
The handshake compares game/BIOS and initial state digests. Different ordinary
VMUs/flash contents can therefore prevent synchronization even with equal ROMs.

The separate [ICE implementation](https://github.com/flyinghead/flycast/blob/v2.7/core/network/ice.cpp)
does contain match-code rendezvous, STUN and TURN. However its connected data
path attaches to the emulated serial/Battle Cable interface. The
[network settings](https://github.com/flyinghead/flycast/blob/v2.7/core/ui/settings_network.cpp)
expose that match-code flow in the Battle Cable branch, not GGPO. Consequently,
enabling existing ICE settings does **not** supply automatic GGPO internet play
for Soul Calibur. Do not advertise such a route based on those settings alone.

[Flycast Dojo](https://github.com/blueminder/flycast-dojo) documents internet
match-code P2P as an alternative implementation to evaluate. Its capability is
not present merely because upstream Flycast is installed. No ARM64 artifact,
strict-NAT fallback or TrainerOS integration for Dojo was verified in this pass.

## Next acceptance, in order

1. Resolve the GGPO WAN route: select and prove native match-code traversal with
   a usable relay fallback, or the already-defined managed transport when needed.
   Do not silently use a player's personal VPN or expose manual IP/port setup.
   Any maintained source bridge needs its own patch, recipe and service contract.
2. Prepare isolated configuration, firmware copies and VMUs/flash for the exact
   game. Preserve ordinary progress and map physical pad A to the assigned player.
   Keep network-internal rollback separate from retired user savestate features.
3. Connect existing Home/Social Invite/Accept/Join, status, cancellation and exit
   to real runtime operations. Retain Nearby and Online friend without new setup
   pages. Do not expose an online invitation that only has a LAN implementation.
4. Prove actual two-player inputs, internet route selection, Home return and
   bounded loss recovery. Same-router play and forced relay can provide useful
   separate evidence; neither proves a real distinct-network test by itself.

Soul Calibur from the existing private collection is the first proposed GGPO
target; no title digest/profile or gameplay claim has been registered yet.
Naomi cabinet links and native Dreamcast internet services retain separate modes.

## Update and recovery

Before promoting an update, compare CLI/config keys, GGPO handshake and transport
behavior with this pinned source. Stage the same package on both devices, verify
its artifact, then run the relevant input/save/return and paired transport checks.
Keep device and user settings outside replaceable packages; never overwrite them
with Odin's configuration as a universal default. Do not freeze upstream updates
permanently or infer compatibility from a matching version string alone.

The installation added a package only on Flip. If it must be withdrawn before
integration, uninstall that application without `--delete-data`; preserve its
user directory and shared Flatpak runtimes. Odin's pre-existing package remains.
Future binary rollback must use a recorded compatible commit and must never
restore older personal saves as a side effect.
