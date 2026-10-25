# Dolphin integration checkpoint

## GameCube LAN / BBA — 25 октября 2026, исходники

Обычный Dolphin NetPlay сохранён. Новый `runtime.dolphin.bba.v1` — отдельные
GameCube, собственный экран и P1 у каждого из **двух** участников. Выбор
`LAN (in-game)` появляется рядом с NetPlay в GameHub Lobby при установленном
проверенном пакете. После Ready и ручного Start используются LAN-меню игры.
Автоматического возврата/запуска игры нет.

### Механизм и владельцы

Закреплённый Dolphin `c77bbaa0f372c3f72281602a8b087206706542cb` уже содержит
[EthernetTapServer](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Source/Core/Core/HW/EXI/BBA/TAPServerConnection.cpp).
Это кадры Ethernet с двухбайтовой little-endian длиной по Unix-сокету;
`SerialPort1=11` выбирает этот upstream backend. Протокол/эмуляция сохранены.
При просмотре исходника найден цикл чтения после EOF: закрытый сокет остаётся
постоянно читаемым, старый код повторял ошибки без остановки. Узкий
`traineros-bba-eof.patch` завершает поток чтения при EOF/ошибке заголовка или
кадра; закрытие сокета и join потока остаются у штатного Deactivate. Игра жива.
Старые сборки без manifest-маркера `bba=tapserver-eof-v1` не получают LAN.
HLE BBA подключается к сети хоста, IPC соединяет процессы одного ПК, XLink Kai
требует отдельного сервиса. TAP server подходит к существующему защищённому
каналу TrainerOS без TAP-интерфейса, VPN, root или нового сервера.

S06 владеет комнатой/допуском/Start. S07 `LobbyConnection` связывает комнату,
поколение, состав и launch ticket с существующим ICE/DTLS `stream_transport`.
Обязательный режим `bba` проверяется обоими peers и несовместим с PS2/PSP/media.
Канал пересылает Ethernet-кадры 14–1518 байт без сетевого стека хоста. Unix-сокет
и material принадлежат пользователю; допускается один клиент, очереди ограничены,
переполнение датаграмм сбрасывает кадры. MAC разные у двух консолей и выводятся
из случайного material сеанса. Local использует host ICE candidates без STUN;
Online — существующий signaling и публичный STUN. Сложные NAT и приемлемая
для конкретной LAN-игры задержка через интернет этим не гарантируются.

S03 `DolphinLan` проверяет хеши runtime/tool/helper и заголовок диска через
проверенный `dolphin-tool header -j`. Доступны ISO/GCM/RVZ/WIA/GCZ для восьми
изданий: Mario Kart Double Dash GM4E01/GM4J01/GM4P01, Kirby Air Ride
GKYE01/GKYJ01/GKYP01, 1080 Avalanche GTEE01/GTEP01. Идентификаторы сверены с
[GM4](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Data/Sys/GameSettings/GM4.ini),
[GKY](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Data/Sys/GameSettings/GKY.ini),
[GTEE01](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Data/Sys/GameSettings/GTEE01.ini),
[GTEP01](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Data/Sys/GameSettings/GTEP01.ini).
[Список LAN-игр XLink](https://www.teamxlink.co.uk/wiki/Dolphin) подтверждает
назначение режимов. Это доступность, не результаты матчей. Сравниваются полные
образы и runtime: разные контейнеры одного диска пока не считаются одинаковым
содержимым. PSO/Homeland и серверный Online не включены.

### Сохранения и завершение

Копируется только собственный `GCPadNew.ini`. Пользовательский Dolphin.ini,
GameSettings, карты памяти, NAND и читы не подключаются. Приватный user-dir
содержит временные карты памяти; **временный прогресс** обозначен в выборе режима.
Шейдерный кеш использует существующую безопасную схему. Смена игры и Home/Exit
принадлежат общему process lifecycle; потеря связи отзывает сеть и ожидающий
запуск, но не убивает работающую игру. Удаляется только каталог этой сессии.

### Сборка, поставка и откат

`dolphin-netplay` пересобирается из прежнего закреплённого исходника через
`packaging/emulators/dolphin/build.sh`: он применяет сохранённый NetPlay-патч
и дополнительный BBA EOF-патч. `bundle.py` проверяет оба патча и пишет marker.
25 октября изготовлены полные ARM64-пакеты Dolphin и LAN helper. Все три ELF
проверены по архитектуре/хешам и загружаются со своим Fedora 44 sysroot;
через QEMU запущены Dolphin/утилита и проверен native header JSON на
синтетическом заголовке диска без игровых данных.
Дополнительный пакет транспорта строится так:

```sh
cmake -S tools/remote-play-stream/transport -B build/bba-transport -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/bba-transport -j3
ctest --test-dir build/bba-transport --output-on-failure
python3 packaging/emulators/dolphin/package-lan.py --transport build/bba-transport --output build/dolphin-lan
```

Место установки — `TrainerOS/emulators/dolphin-lan` в user data; сейчас пакет
**не установлен на портативки**. Скрипт не перезаписывает существующий каталог
и включает лицензии зависимостей. После обновления helper пакет пересобирается.
При обновлении Dolphin проверить EXI enum/config/TAP framing, нативный парсер
и профильные тесты до изменения pin. Откат — предыдущий пакет/оболочка;
без дополнительного пакета BBA скрыт, обычный NetPlay сохраняется.
Личные карты/настройки при откате не меняются.

### ARM64-сборка без портативок

`Dockerfile.cross-fedora44` использует native x86_64 Clang/Qt tools и отдельный
ARM64 sysroot Fedora 44. Компилятор работает нативно на x86_64 и выдаёт ARM64 ELF;
QEMU user mode позволяет проверить исполняемые файлы. Настройка
host binfmt, привилегированный контейнер и подключение к устройствам не нужны.
Контекст Docker ограничен каталогом рецепта, без локальных ROM/сохранений.

```sh
docker build -f packaging/emulators/dolphin/Dockerfile.cross-fedora44 \
  -t traineros-dolphin-cross:fedora44 packaging/emulators/dolphin
# SOURCE содержит закреплённый checkout с submodules; пути внутри /work.
docker run --rm --cpus=3 --memory=4g -v "$PWD:/work" \
  traineros-dolphin-cross:fedora44 \
  sh packaging/emulators/dolphin/build.sh build/dolphin-source build/dolphin-arm64
```

В той же среде выполнить `bundle.py`, а transport собрать по рецепту выше в
отдельном build-каталоге. Toolchain берётся из CMAKE_TOOLCHAIN_FILE контейнера.
Нельзя повторно использовать CMake cache от native x86_64 сборки.
Упаковка сверяет оба upstream-патча и TrainerNetplay.inc, архитектуру двух ELF,
добавляет исходные патчи/лицензии и записывает архитектуру в оба manifest.
Оба участника должны получить один и тот же пакет: идентичность runtime
по-прежнему проверяется по полному SHA-256. Эти пакеты опираются на библиотеки
Fedora 44; это не обещание совместимости с произвольным Linux-дистрибутивом.

Пакеты 25.10: Qt 6.11.2, Clang 22.1.8, ARM64 JIT, прежний upstream pin.
SHA-256 исполняемых файлов:

- `dolphin-emu`: `1fba2610dcf3082218b40960b23f636c0ad89448b8814ab54a61761072fb6fc5`.
- `dolphin-tool`: `28ecfdec04a773e44319b0056f806385e59715fa54f486007dca63826135bbdc`.
- `stream_transport`: `1a78644ef361fdf259dd479141fac1af943ea81bf9f8031386fdcaad11d4d077`.

Проверки упаковщика подтвердили отказ от перезаписи обоих готовых пакетов и
от смешивания ARM64-эмулятора с x86_64-утилитой до создания выходного каталога.
Списки пакетов сборщика/sysroot и результаты проверки зависимостей сохранены
рядом с артефактом. Оболочка TrainerOS для этой поставки на ARM64 ещё не
пересобрана; предыдущая Windows-проверка её исходников остаётся отдельным фактом.
Готовый ARM64 `stream_transport` через QEMU прошёл все 16 транспортных сценариев
за 26.642 с, включая двусторонние BBA-кадры и отказ несовместимого режима.
Артефакт `TrainerOS-dolphin-arm64-20261025.tar.gz` (18 049 903 байта), SHA-256
`79534e72ea956606bc4b6a2bfb71a720b0e8d2726e213eb4d788795bae9f23fc`, содержит
оба пакета и отчёты. Бинарный архив хранится вне Git; сборочный кеш остановлен
и сохранён для следующего обновления. Портативки не подключались.

### Проверки этого изменения

На Linux x86_64 собран реальный ICE/DTLS helper: пройдены 16 транспортных
сценариев и отдельный PS2 socket boundary. Новые сценарии передают фрагментированный
локальный поток и Ethernet-кадры минимального/обычного/максимального размера в обе
стороны, проверяют права, разные MAC, отказ второго клиента, неверную длину и
несовпадение BBA/PS2. Пакет helper собран с лицензиями. Стенд — отдельная внутренняя
Docker-сеть: libjuice не создаёт нужные ICE candidates в loopback-only namespace.
Сервисы хоста и портативки не участвовали.

На Windows пройдены standalone, runtime_multiplayer, lobby и game_party;
LocalLobbyTests после исправления тестового поиска гостя по ID прошёл 8/8.
Раньше тест ошибочно полагался на порядок случайных сертификатных идентификаторов.
Новые проверки охватывают два профиля одной игры, подготовку LAN, отказ другой
сборки, собственный P1/временные настройки, сохранность обычного INI и сохранение
живого процесса при обрыве BBA. QML/SDL-сценарий прошёл пять условий: отдельные
режимы, выбор касанием и контроллером, сохранение BBA identity, отсутствие QML
warnings. Экраны 960×540 и 1280×720 просмотрены; дефект символа в пояснении исправлен.

`check-tapserver.py <upstream-files>` на Linux компилирует именно закреплённые
upstream cpp/header с заглушками logging/utility/platform helpers (не эмуляции).
Проверяются SHA-256 исходников. Контрольный запуск без патча воспроизводит
десятки тысяч ошибок после EOF за 80 мс; с патчем каждый из трёх разрывов
(до размера / между байтами размера / внутри кадра) завершает чтение после
одной ошибки. Это проверка настоящего socket/thread-кода, не полного Dolphin.
Окончательная Windows-сборка оболочки с BUILD_TESTING=OFF прошла; тестового
входа lobby-smoke-test нет в production --help.
Настоящий матч двух Dolphin в этом проходе не выполнен: пакетные клиенты проверки
проверяют контракт BBA, но не являются эмуляторами. Поставка и игровая совместимость
остаются открытыми; прежние результаты NetPlay ниже сохранены.

**23 October 2026 ? NetPlay cache repair:** The session-owned profile now links only `Cache/Shaders` to a persistent renderer cache keyed by the runtime digest. Two native launches on each handheld retained six cache files; temporary GC/NAND/config paths remain isolated.
[Implementation, installed build and acceptance limits](../NETPLAY_PLATFORM_REPAIR_2026_10_23.md).

## Ordinary launch sweep — 18 October 2026

Odin's ordinary GCPad/Wiimote `Device` still named `SDL/0/Steam Virtual Gamepad`,
which was absent. SDL enumeration identified `SDL/0/Xbox One Elite 2 Controller`.
[Bounded repair](../../packaging/emulators/dolphin/rebind-stale-input.py) replaces
only that stale value in `GCPadNew.ini` / `WiimoteNew.ini`, with a
`.before-launch80` backup and all mappings retained; it refuses a running Dolphin.
This is an ordinary-profile repair, not a rewrite of multiplayer session profiles.

Games → Mario Kart Wii now reaches New Licence with controller input. Auto
Modellista on GameCube renders its title but its input remains unconfirmed even
after that repair. Do not mark both systems passed from the Wii result.
To roll back, close Dolphin and restore the corresponding backed-up INI files.
Recheck runtime SDL device identity after an emulator/input-stack update.

## Wii Remote delivery — 12 October 2026

Wii shared-console NetPlay is installed on Flip/Odin through ordinary Play together,
Online friend consent, the host's solo save/leave barrier and native Dolphin
traversal. Each participant uses their copied local Wii Remote 1 configuration;
native mappings assign separate emulated remotes to party seats. GameCube pads
are disconnected for this route. GC separate pads and two-person Golf Hotseat
remain available. Wii does not advertise the shared-P1 Golf mode.

The native disc reader in the pinned `dolphin-tool header -j` now reports its
parsed platform. Admission verifies that helper and bridge hashes, WiiDisc type,
full image hash and runtime settings. ISO/RVZ/WBFS/WIA/GCZ extensions are candidates;
only the actual Wii volume parser admits them. The paired device case is RVZ,
not certification of every format/game/peripheral. WAD/WiiWare and native game
online servers are separate. No filename/title compatibility guess is introduced.
The ordinary input map is copied into a temporary `-u` profile. No personal NAND,
SD image, per-game settings or save paths are linked. Native save sync/write remain
off. Invitations and party details disclose fresh, temporary progress.

**Gameplay:** New Super Mario Bros. Wii EU revision 1 (SMNP01) reached two-player
World 1-1. Flip selected/controlled Mario; Odin joined as Luigi. Opposite inputs
moved Mario right and Luigi left, and the host paused both copies at the same
positions/timer. The private diagnostic build only sampled existing input button
states to distinguish short test pulses/intro transitions from missing input;
it changed no core/network logic. Earlier idle attempts lost lives in this
fresh session. They were not personal-save tests or completed levels.

**Ordinary installed journey:** Online friend invitation arrived on Odin with an
incomplete local catalogue, consent admitted it, and host Start/save/leave launched
two native copies with `controllers=wiimote`, seats 1/2 and traversal. Guest Home,
minimize, return to the same child and Home Exit worked; guest Exit closed both.
This uses one home network. Traversal is rendezvous, not proof of a payload relay
or success through strict NAT. Four-person play, physical/motion controls, longer
sessions, broader games and distinct-network acceptance remain open.

A real fresh-library mismatch exposed catalogue player counts in strict identity.
Dolphin and RMG now declare their verified four-port ceiling separately. Compatible
ordinary profiles may have different catalogue party limits within that ceiling;
the host still bounds seats. Exact content/runtime/settings and shared-controller
mode remain mandatory. Profiles without the explicit matching ceiling stay strict;
RetroArch's genuinely different two-pad/multitap settings are not normalized.
Regression tests cover both invitation directions, over-capacity/missing-ceiling
rejection, changed runtime/content/settings and exact two-person Hotseat.

Final installed hashes:

- TrainerOS: `447727e3c831c69eab7fc07857a756b0215b363913693a64d0cd4d2842a55ff8`.
- Dolphin: `d493e22390278d923a3c0c470b61e4530130c4c46c97e14bfe0a6c1a0932ea77`.
- dolphin-tool: `b6866ad94392720740b5a4214e4d23d5da8db8d222784485bc8c50ec6648c0c5`.
- Source remains `c77bbaa0f372c3f72281602a8b087206706542cb` plus the maintained patch.
- Owner RVZ: 439,323,844 bytes,
  `e75612ff0bcdc476691e542d2f9faae32be5929ad7b8f982e0ddc589dbcf8740`.
- Ordinary journey used app `9fb2f5b70c8a00a7514f7bad15625205c46f1d6c1aece75dc0a0cb748497d211`;
  final app additionally declares the existing temporary-progress disclosure.
  Its installed guest consent text was inspected; declining that follow-up was verified.

Windows standalone/RMG/party/runtime passed (8.56 s total); ARM64 same four passed
(6.38 s). After the disclosure change, affected standalone/runtime passed again:
Windows 7.36 s, ARM64 4.71 s. Application/native builds, complete native patch
reverse-check and 123-file runtime export passed. Initial failed fixture and
catalogue-admission observations are superseded by these corrected checks.

Preservation: both isolated sessions and initial delivery left all 389 historical
baseline files and ordinary INIs unchanged. Ordinary Flip solo startup created
this title's initial personal save (wiimj2d.sav, banner.bin and title.tmd) and
changed six console/NAND metadata files:
GC/SRAM.raw, Wii/fst.bin, Wii/sys/uid.sys, system-menu play_rec.dat/state.dat and
Wii/shared2/wc24/nwc24dl.bin. TimePlayed.ini and Qt.ini also changed. Existing
personal game saves and both controller maps remain unchanged; Odin's 25 baseline
files and all 13 INIs remain unchanged. These legitimate solo changes are retained,
not rolled back or attributed to isolated multiplayer. Historical and separate
post-solo baselines/backups are retained. Both final database quick checks passed; no game children remain. Device output
stays at zero volume. The final future-work baseline also protects the three
newly created personal title files.

Rebuild with `packaging/emulators/dolphin/build.sh`; it builds both executables.
`bundle.py` copies both, Sys and exact hashes plus Wii/Hotseat capability markers.
Rebase the maintained patch against its pinned source and repeat native mapping,
identity, save-isolation and Home lifecycle checks after relevant upstream changes.
Rollback restores the paired previous TrainerOS/native/manifest backup while idle;
ordinary Flatpak, personal NAND/cards and input settings are independent.
No whole multiplayer issue or external acceptance is closed by this delivery.

## Ordinary GameCube Hotseat delivery — 12 October 2026

Flip/Odin now offer separate pads or **Take turns with one controller** in ordinary
Play together. The latter admits two people and exposes only P1. Home shows the
confirmed owner and **Pass controller to [friend]**; the spectator retains Home
and touch. Progress is temporary. Existing two-to-four separate-pad profiles remain.
Wii input and persistent multiplayer save return are not added.

The maintained patch reuses Dolphin Golf Mode Prepare/Switch/Release/Acquire and
reliable PadHostData. Internal mappings remain for admission; only the current
native authority polls local pad 0, and only one SI device reaches the game.
There is no live port remapping or new emulation synchronization protocol.
Only the accepted host coordinator requests transfer; native acknowledgement
sender/order checks feed TrainerOS's existing confirmed handoff phases.
Missing confirmation ends the temporary session, without uncertain toggle retries.
Private JSON selects this behavior and the bridge activates Golf Mode before
host/join; the shared base INI is not an observation of the active native mode.
The native Golf overlay is hidden to keep admission nicknames and independent
Take Control buttons out of gameplay. The verified local manifest capability
dolphin-golf-p1-v1 enables the choice; ordinary separate-pad behavior is retained.

**Gameplay:** isolated Mario Golf: Toadstool Tour US reached two-character Stroke
Play with one controller. Six transfers were confirmed (initial owner event 1,
release/acquire events through 13). Odin made Mario's first stroke and Flip made
Peach's; both reached flight with one stroke recorded. Non-owner input did not
dismiss the observed prompt, while owner input did. Guest quit closed both copies.

**Ordinary installed journey:** Online friend consent disclosed turns/temporary
progress; the host retained the solo save/leave barrier. Native traversal launched
both copies. Three Home transfers confirmed guest, host and guest ownership;
input from each advanced the peer's game UI. Host Home -> Exit closed both.
This was one home network through Dolphin traversal, not a relay or distinct-network
proof. Physical controls, sustained/wider-game play, four-person Hotseat and
strict-NAT acceptance remain open. Earlier separate-pad Melee evidence is retained.

Final SHA-256 on both devices:

- TrainerOS: 4d055f96d7408ec7b8673aa28a5fba2bea6979805d1b9262bbadb306d3f57fce
- Native Dolphin: 2bddb337a8ca9ea520bba26165ead594c23e9386bd0501a408a3f473ab6fc5c2
- Source remains c77bbaa0f372c3f72281602a8b087206706542cb; Flathub is unchanged.
- Gameplay probe: ff7f792c68813fdfaa88d392b73faead9ff6dfcd8bd1c884baa0cb389f189670;
  the final bridge additionally hides the native Golf overlay.
- Owner ISO: 1,459,978,240 bytes,
  08a1f0c1f7336418fa814a19907f2094d81062d5e7636ab096a43c41132410f0.

Windows standalone/handoff/party passed (3.06/0.22/0.73 s); final runtime passed
32 checks in 2.423 s. ARM64 standalone/handoff/party passed (4.33/0.22/0.14 s),
then final runtime passed (2.09 s). Both application builds, the native bridge
build and 123-file runtime export check passed. Coverage includes native ordering,
guest inability to assert ownership, stale requests, four-pad-to-two-turn admission,
private configuration and real menu/process bindings. Early failures were fixed.

After the isolated pair all 389 baseline files and Dolphin INIs were unchanged.
After the ordinary journey 388 files, including personal game cards, remain
unchanged. Flip's 68-byte console GC/SRAM.raw, TimePlayed.ini and Qt.ini changed
after its solo launch. SRAM contains console settings/clock/card identifiers, not
a game progress card. Historical and separate post-solo baselines are retained.
No personal cards or controller mappings changed. Both databases pass quick_check,
both native children are gone and volume is zero. Private games/captures stay out of Git.

**Maintenance:** apply the complete patch at the pinned revision and use the
existing recipe; reverse patch validation passed on the actual checkout. Recheck
Golf ordering and SI semantics before rebasing, build identical participants and
repeat bounded native/ordinary transfers before promoting the capability manifest.
Rollback restores previous TrainerOS plus native Dolphin binary/manifest together;
ordinary Flathub, cards, INIs and Sys assets remain independent. PS2, the full
catalogue and external acceptance stay open. No whole multiplayer issue is closed.

**11 October 2026 offline preparation:** the source admission now also recognizes
raw GameCube ISO/GCM discs by disc magic and full content hash. The same pinned
bridge handles two-to-four party-assigned pads; the exact Melee profile is retained.
Unknown player counts default to two, known counts are bounded by four physical
ports, and explicit single-player metadata still excludes invitations. Local
names do not affect matching. [Preparation and remaining acceptance](https://github.com/EriArk/TrainerOS/blob/c6aaac25ba4f55262f3f6418e8d29cac7cdc9aec/docs/MULTIPLAYER_PREPARATION.md#gamecube-extension-research--11-october-2026)
separate this source change from installed wider-game compatibility. No new
Dolphin patch is required; Wii, RVZ/other compressed discs and special peripherals
remain outside this admission. Earlier "no other title" text describes the original
installed delivery. The former global experimental opt-in was removed on 4 October.

2026-10-03. Both handhelds retain Flathub Dolphin 2606a, commit
`50741ae7267181560aa88d3a1ba7177321629c171ad2677504409afa6b4462db`,
using `org.kde.Platform/aarch64/6.10`. The common
[manifest](../../packaging/emulators/arm64-baseline.json) records its executable
hash. Neither installation/configuration nor save directory changed in the
common-core delivery. Existing ordinary launch/controller/Home behavior remains.

## Native bridge delivery — 2026-10-03

A separate native 2606a bridge is installed on Flip and Odin. Source revision:
`c77bbaa0f372c3f72281602a8b087206706542cb`. Executable SHA-256 on both:
`30c959ca95242b9011526b89a0d9f626d4cff9f1bf77482bb9cf603870d6660b`.
It runs against Armada's native libraries, built in the existing Fedora 44
ARM64 container with Qt 6.11.2. Do not put it into the Qt 6.10 Flatpak.
Ordinary Dolphin remains the existing independently updated Flatpak.

The bridge is experimental, under the existing TrainerOS multiplayer opt-in.
This delivery proves exact Melee on two devices over LAN, not every GameCube/Wii
title, four physical clients, or distinct-network internet play.

## Implementation and preservation

The maintained [patch](../../packaging/emulators/dolphin/traineros-netplay.patch),
[bridge](../../packaging/emulators/dolphin/TrainerNetplay.inc) and
[build recipe](../../packaging/emulators/dolphin/build.sh) target the revision
above. The Dolphin modifications are GPL-2.0-or-later. Rebase against upstream
deliberately; do not silently patch a newer Flatpak binary. Rebuild both devices
from the same source/patch, compare executable hashes and repeat the bounded
paired launch/controller/exit check before promoting an update. The ordinary
Flatpak installation, updater and personal configuration remain independent.

The first exact content is US Melee revision 1 (`GALE01`, revision byte 1),
1,459,978,240 bytes, SHA-256
`53a5d0f7c480045a435c889ad47b6d351faff0fc409a7285f3eecc61706b3d9e`.
No other GameCube/Wii title inherits this route yet. The native protocol remains
Dolphin's NetPlay: the patch supplies host/join/start, status and explicit
party-seat mapping, rather than another emulation/network implementation.

TrainerOS hands off a private, temporary session JSON and isolated `-u` directory.
Only the ordinary controller map is copied. Cheats, public room indexing,
save synchronization and persistent save writes are disabled for this versus
profile. Ordinary memory cards, per-game settings and Flatpak files are never
linked into it. Temporary session files are removed after the owned child exits.
This is a fresh versus match, not co-op progression or semantic save support.

The accepted roster closes on Start: Dolphin cannot admit another player during
a running game. The host assigns ports from the accepted party slots, not socket
arrival order; unexpected/duplicate admission names are removed before starting.
The token travels only in the accepted invitation/session file and is suppressed
from native chat/OSD. It is a casual private-room gate, not competitive trust or
a replacement for encrypted transport. Native game/version checks remain active.
The copied player snapshot is taken under Dolphin's players mutex.

Nearby uses the existing LAN route. Online uses Dolphin's traversal rendezvous,
without account, manual room-code entry or user-managed VPN. Traversal is not a
packet relay and cannot guarantee passage through every NAT/firewall. Timeout,
connection and traversal errors must return through TrainerOS; keep distinct
networks, strict-NAT recovery and four-client acceptance separate from two-device
LAN evidence. See the actual bounded evidence below.


## Paired runtime evidence

TrainerOS executable on both devices:
`e0121a5b91bf9976579389bea167010735013bdcc5b8c9617d38f7406ccf96a7`.
The normal route was Worlds A -> ordinary Dolphin -> physical Home -> Invite friend
-> Nearby -> named peer acceptance -> 2/4 party -> Start game -> ordinary-game save
confirmation. TrainerOS then launched the same native bridge on both devices,
created/joined the room, assigned P1 to Flip and P2 to Odin and started Melee.
No Dolphin room widgets, typed address or controller-port setup was used.

Remote input through each device's physical evdev/InputPlumber path selected
Mario and Ness independently, started an Onett match and moved/attacked with
both players. Screenshots show the live match on both devices, not a preview.
Home opened over the native game on both devices. Odin Home -> Exit -> Leave
closed its owned process and returned to TrainerOS; peer departure also ended
Flip's native session and returned to Worlds. No device reboot was required.
Physical comfort, audio and prolonged play were not tested.

Two integration fixes were required: an explicit `-b -C
Dolphin.Interface.ConfirmStop=False` launch contract lets the existing Home
helper recognize the native process; the copied Odin SDL mapping referred to a
Steam virtual controller absent from its native session. The bridge rebinds only
the isolated copy when exactly one SDL controller exists. It retains all original
button mappings and refuses ambiguous selection. Remote single-shot analog
injection was also being overwritten by the hardware's neutral reports; sustained
injection confirmed the real two-controller path without changing device drivers.

The versus session uses fresh temporary memory cards; personal ordinary Dolphin
cards and settings are not connected to it. Session files are removed after exit.
The existing conservative Home save question remains for standalone game exits.

## Online-friend route on the shared network

The same installed binaries also completed public-Fluxer query/offer/invite,
explicit Odin acceptance and the 2/4 roster. Host Start closed ordinary Dolphin
through its save confirmation, launched the native host, exchanged the traversal
room code privately and launched the guest automatically. Both isolated configs
reported `TraversalChoice = traversal`, with UPnP/public indexing/save sync off;
slots were host 1 and guest 2. Native NetPlay started the game on both devices,
and Flip input advanced the guest's synchronized game screen. Odin Home exit
then closed its owned process; Flip also returned after peer departure.

This proves the real Online friend invitation and traversal-assisted startup on
the shared router. It does not prove connectivity between different networks,
strict/symmetric NAT, a relayed game connection or a second complete match.
Earlier three-second screenshots preceded invitation delivery; an eight-second
bounded observation caught it. Do not treat that timing artifact as an outage.

Focused standalone/session-isolation and game-party tests passed on Windows and
ARM64. Four existing process/exit/persistence tests passed on ARM64. The recipe's
patch reverse-check and source include matched the built tree; the staged bundle
hash matched both installed bridges. Actual captures 38-42 are in the [screenshot gallery](../../screenshots/README.md).

## Build, update and rollback

Install build-only dependencies in the ARM64 build environment: CMake, Ninja,
compiler, Qt6 private GUI development headers/SVG, libcurl, libusb, BlueZ,
libevdev, SDL3, Xrandr/Xi/Xcursor/Xinerama, PulseAudio/ALSA and GL development
packages. Keep runtime shared-library versions compatible with the target image.

1. Clone the pinned Dolphin revision with its submodules.
2. Run `sh packaging/emulators/dolphin/build.sh SOURCE BUILD`.
3. Stage with `python3 packaging/emulators/dolphin/bundle.py SOURCE BUILD NEW_OUTPUT`.
   This writes `bin/dolphin-emu`, adjacent `bin/Sys` and the digest manifest.
4. With no running game, back up the existing bridge directory and deploy the
   staged tree to `$XDG_DATA_HOME/TrainerOS/emulators/dolphin-netplay` (normally
   `~/.local/share/TrainerOS/emulators/dolphin-netplay`). Preserve executable mode.
5. Verify matching manifest/binary hashes on every peer and library dependencies;
   repeat one invitation -> independent input -> Home return check after an update.

Keep the recipe, GPL-2.0-or-later patch and exact source with release artifacts.
Upstream changes require an intentional patch rebase, version/digest update and
compatibility check. Restore the prior bridge tree/manifest for rollback; ordinary
Flatpak files, updater, controller map and saves require no rollback because this
route never overwrites them. The reusable runtime adapter export includes this
recipe and patch. No compiled emulator, ROM or private admission data enters Git.

## Updated-handheld recheck — 11 October 2026

The current TrainerOS client and existing exact Melee bridge completed a new
Nearby invitation, Mario/Ness match with independent controls and guest Exit
followed by host peer-departure return. Generic Auto Modellista admission was
checked separately; no paired match of that title is claimed. The ordinary
Dolphin SRAM runtime file changed during solo launch/exit; pre-existing memory
cards did not. [Exact integrity scope and remaining acceptance](https://github.com/EriArk/TrainerOS/blob/c6aaac25ba4f55262f3f6418e8d29cac7cdc9aec/docs/MULTIPLAYER_DEVICE_RECHECK_20261011.md).
