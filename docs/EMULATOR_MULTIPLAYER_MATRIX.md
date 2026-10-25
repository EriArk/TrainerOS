# Мультиплеер — механизмы и подтверждённые границы

**Обновление 25 октября: общий проход по оставшимся форматам выполнен.**
[Единый реестр](MULTIPLAYER_CONTENT_FORMATS.md) содержит полную таблицу,
проверки, пакеты и технические границы. Строки ниже обновлены по исходникам;
исторические результаты проверок первоначального аудита сохранены отдельно.

## Аудит доступности игр в Lobby — 25 октября 2026

Проверена цепочка S02 библиотека → S03 runtime/profile → S06 выбор и запуск.
Портативки недоступны: изменения исходников, не поставка. Количество ранее
проверенных игр не задаёт разрешённый список обычного NetPlay. В сканере и
picker нет лимита числа игр; четыре предложения ограничивают голосование,
не каталог. Большие библиотеки появляются постепенно по мере идентификации.

**Исправлено:** локальные метаданные «1 игрок» больше не скрывают самостоятельные
Link/wireless/LAN машины. Shared-console NetPlay сохраняет отсев однозначно
одиночных игр; неизвестные метаданные не равны single-player. Каталог не создаёт
runtime и не увеличивает число мест. Полные файлы, runtime, firmware и настройки
между участниками продолжают сравниваться; Start остаётся ручным.

**Обновление каталога:** зависимые файлы RetroArch теперь наблюдаются при проверке
идентичности, включая отсутствующие дорожки и необязательные данные. Фоновая
проверка метаданных обновляет доступность без события библиотеки/перезапуска;
неизменившиеся результаты используются повторно, живая пати сохраняется.
[Границы и проверки](RETROARCH_MULTIPLAYER_PROFILES.md#dependency-aware-lobby-refresh--25-october-2026).

| Семейство | Допуск и устранённые ограничения | Оставшаяся техническая граница |
| --- | --- | --- |
| Классические RetroArch | Общие профили NES/FDS/SNES, SG-1000/SMS/MD/Sega CD/32X, PCE/PCE CD/SuperGrafx, Atari 2600, FBNeo/Neo Geo/MAME. Старые exact-хеши — специальные совместимые профили, после них есть общий путь | Установленные core/firmware, determinism, реальные порты; CUE/arcade сравниваются с зависимыми файлами |
| GB/GBC/GBA | Общие linked-профили по аппаратным данным ROM; ZIP/7z с одним картриджем проходят идентификацию/подготовку; solo-фильтр исправлен | Новая оболочка не установлена; RTC/особые mapper/save layouts и RFU зависят от core. Сохранено отрицательное exact-исключение отказавшего Hnefatafl |
| PS1 | Общий PCSX-ReARMed, CUE/CHD/PBP/M3U/ISO и BIN/IMG/MDF/EXE/CCD/TOC/MDS с зависимостями, без списка названий | Новая оболочка не установлена; CBN/CBIN native index fix, ограничения TOC и смена диска в живом сеансе отдельно |
| PS2 | Все 56 строк LAN-каталога / 164 serial; ISO/CHD/CSO/ZSO/GZ/BIN/IMG/MDF/NRG, исправлен solo-фильтр | SYSTEM.CNF/ELF, поддержанный layout, DEV9-пакет и новый Python reader; ZSO требует liblz4. Каталог обозначает наличие LAN, не только проверенные матчи |
| PSP | Общий Ad Hoc без списка названий; ISO/CSO/PBP/CHD через native reader, исправлен solo-фильтр | Новый helper/оболочка ещё не установлены; wrappers и PBP без корректного PSP DISC_ID остаются открыты |
| N64 | Общий RMG, все три порядка байт ROM, ZIP/7z с одним картриджем; без списка названий | Новая оболочка не установлена; до 64 MiB ROM, установленные плагины/endpoint; порядок байт должен совпадать |
| Dreamcast | Общий Flycast Dojo для CHD v5/GDI/CDI/CUE/M3U, включая зависимости, без списка названий | M3U запускает первый диск; живая смена дисков и native game-server Online отдельно |
| GameCube/Wii NetPlay | Общий Dolphin, без списка названий; GC/Wii native compressed/raw, TGC, split ISO/WBFS; WAD каналов через новый bridge | Платформу проверяет `dolphin-tool`; Wii требует Wiimote runtime, WAD — новый manifest. IOS/system WAD исключены; native Wii Online отдельно |
| GameCube BBA | Семейства GM4/GKY/GTE без allowlist регионов/ревизий; CISO/TGC/split identity | Встроенный LAN Mario Kart Double Dash/Kirby Air Ride/1080, а не ограничение общего NetPlay. PSO/Homeland и серверные режимы не подготовлены |
| DS | Общий melonDS, NDS/SRL/ZIP/7z; подготовка запуска и защита собственных сейвов; исправлен solo-фильтр | Один картридж в архиве; DSi/WFC не становятся обычным DS wireless |
| 3DS | Общий Azahar, 3DSX/CCI/CXI/3DS/APP/ELF/AXF/ZCCI/ZCXI/Z3DSX, с проверкой структуры; исправлен solo-фильтр | CIA/ZCIA installation/encrypted images, отдельная идентичность updates/DLC и серверные механизмы требуют подготовки |
| Vita | Исправлен solo-фильтр действующего vitaQuake LAN | Exact EBOOT/extraContent нужны специальному native-IP профилю; общий commercial AdHoc/PSN не реализован |
| Wii U / Xbox / прочие несвязанные механизмы | В этом admission pipeline общего сетевого профиля нет | Реальные незавершённые runtime-интеграции R046/R048/R049, не ограничение числа игр |

Dolphin использует [штатный Blob reader](https://github.com/dolphin-emu/dolphin/blob/c77bbaa0f372c3f72281602a8b087206706542cb/Source/Core/DiscIO/Blob.cpp).
Azahar вернул `.3ds` в [2124.3](https://github.com/azahar-emu/azahar/discussions/1702),
раньше используемой 2126.2. [Warp Pipe](https://cubeonline.warppipe.com/gettingstarted/what_is_warp_pipe.php)
перечисляет исходные LAN-игры GameCube. Разные контейнеры одного диска пока не
считаются одним содержимым. Контейнеры расширены через существующие readers
в [общем проходе](MULTIPLAYER_CONTENT_FORMATS.md); отсутствие сетевого механизма
остаётся отдельной задачей, без разрешения каждой игры вручную.

### Исторические проверки первоначального аудита

- Собрана Windows-оболочка и профильные тесты. `standalone`: 48 passed,
  6 platform skips; `game_party`, `save_backups`, полный `runtime_multiplayer`
  (122,84 с) прошли. Новый сквозной случай проверяет публикацию wireless/LAN
  при local-solo metadata, сохранение фильтра shared-console и обновление picker.
- Обнаружение всех добавленных форматов и существующие два сценария picker
  (включая 120 игр, обновление/переименование/удаление) прошли.
- Два новых admission-случая DS SRL/3DS `.3ds` с реальными production-функциями
  скомпилированы для Linux ARM64 и прошли под QEMU/Qt 6.11.2. Windows не
  интерпретирует Unix executable-пути этих пакетов, поэтому эти случаи там skipped.
- Настоящий собранный ARM64 `dolphin-tool` преобразовал собственный синтетический
  ISO в RVZ/WIA/GCZ и прочитал их заголовки; также прочитал синтетический CISO.
  ROM/BIOS игр не использовались. Устройств/игрового матча эти проверки не заменяют.
- `export-game-adapters.py`, `check-adapter-knowledge.py` и `git diff --check`
  пройдены.

Полный `LibraryTests` дополнительно выявил три сбоя вне изменённых функций:
`contextualMenuUsesWheelSelectionAndTrapsInput` (ожидание Settings),
`contextualEditsDeleteRomAndPreserveIdentity` (World editing выключен),
`settingsAreCommittedAndRetryable` (ожидание старой темы turquoise вместо light).
Они не исправлены в этом multiplayer-проходе; весь suite не объявляется зелёным.
Новая проверка обнаружения файлов прошла отдельно. Обновлённая ARM64-оболочка
и установка на портативки не выполнены.

## Ранее сохранённые доказательства

Сводка записей репозитория на 18 октября 2026, **не новый прогон устройств**.
С 21 октября глобальная очередь снята; состояние функций — в
[карте готовности](FEATURE_READINESS.md). Датированные доказательства ниже сохранены.
[Стандарт](EMULATOR_STANDARD.md) требует утверждённые runtime/core/build/protocol
и настройки, общий допуск до запуска. Обнаруженная установка, число игроков в
метаданных и работающий solo не являются сетевой совместимостью.

Использовать готовые netplay/rooms/relay/game services, не писать эмуляторный
протокол заново. TrainerOS связывает consent, подготовку, input, Home/выход и сейвы.
Shared-console pads, независимые link-машины, native online и Remote Play различны.
Ниже перечислены семейства механизмов; полный перечень платформ — в
[LAUNCH-80](LAUNCH80_PLATFORM_REGISTER.md), а не только популярные консоли.

| Семейство | Механизм и имеющееся доказательство | Открытая граница |
| --- | --- | --- |
| Классические RetroArch | Синхронизация одной виртуальной консоли, отдельные pads, обычные приглашения/relay; [профили](RETROARCH_MULTIPLAYER_PROFILES.md), [runtime](EMULATOR_MULTIPLAYER.md) | Determinism/core/content, периферия и capacity по профилю; разные сети и реальные большие группы |
| GB/GBC/GBA | Независимые машины/кабель/RFU через DoubleCherryGB, SameBoy, gpSP, mGBA Splitscreen; trades и отдельные игры проверялись. [Handheld](HANDHELD_MULTIPLAYER.md) | Save return/RTC/более широкие игры отдельно; четыре машины не четыре физических игрока |
| N64 / RMG | Native Mupen64Plus netplay, установлен Nearby/Online; Mario Kart 64 forced-relay и обычный Home/Exit. [RMG](emulators/RMG.md) | Не все игры, четыре устройства, разные сети и production service |
| PS1 | PCSX-ReARMed shared pads и отдельный исторический handoff-профиль; [запись](emulators/pcsx-rearmed.md) | Multitap/system link, broader content/карты отдельно. DuckStation solo не означает netplay |
| PS2 | Native DEV9 LAN/game online требуется; [списки владельца и разбор](PS2_NATIVE_MULTIPLAYER.md). [LRPS2 остановлен](emulators/lrps2.md) | Продолжить после PSP; каталог не доказывает реализацию native online. Couch через отдельный Remote Play, не две копии PS2 на одном host |
| PSP | PPSSPP Ad Hoc Nearby/Online, Lumines и lifecycle на двух устройствах. [PPSSPP](emulators/ppsspp.md) | Generic форматы/игры отдельно; private-party admission не завершён |
| Dreamcast / Flycast | Отдельный Dojo bundle установлен; Soul Calibur Nearby/public relay, input/Home/Exit. [Flycast](emulators/flycast.md) | Same-router relay не разные сети; native DC services и arcade cabinet link не равны GGPO |
| GameCube / Wii | Dolphin native NetPlay, отдельные pads/remotes, Melee/NSMB Wii и lifecycle. [Dolphin](emulators/dolphin.md) | Motion, четыре физических участника, разные сети, wider games. Native BBA/game online отдельно |
| DS | Deterministic wireless bundle установлен на Flip/Odin; Mario Kart DS, обычные Nearby/Online приглашения, Home и защищённый возврат собственных сейвов. [melonDS](emulators/melonds.md#ordinary-handheld-delivery--12-october-2026) | Same-router проверки не разные сети; четыре игрока, wider save/game/resource и физическая приёмка открыты; WFC отдельно |
| 3DS | Azahar 2126.2, Minicraft3DS wireless Nearby/relay и ordinary invitation/Home/save/Exit. [Azahar](emulators/azahar.md) | Retail updates/DLC/content identity, wider games/groups и отдельные сети |
| Wii U | Cemu native online требует собственные console/account данные. [Cemu](emulators/cemu.md) | На аудите нет own OTP/SEEPROM/certs, нет принятой пары/TrainerOS network profile; solo source route отдельный |
| Vita | Vita3K, один exact vitaQuake LAN-профиль с own data, native discovery и парным вводом. [Vita3K](emulators/vita3k.md) | Не commercial AdHoc/PSN, не все игры/группы/сети |
| Remote Play | Одна игра на host, поток и гостевой input; [транспорт](REMOTE_PLAY_TRANSPORT.md), [групповой кандидат](REMOTE_PLAY_GROUPS.md) | Iris/IDR, многогостевой lifecycle/Gamescope, физическая задержка и сложные NAT; кандидат не установлен как готовая функция |
| PS3 / Switch | Готовность multiplayer не установлена | Исследовать поддерживаемые режимы/runtime; solo homebrew не является сетевой проверкой |

Явная single-player metadata исключает приглашение; неизвестная metadata не
равна single-player и не разрешает несуществующий runtime. Capacity задаёт
проверенный профиль. При подключении сохраняются личные сейвы/firmware/account
identity и условия host/guest progress; не подменять их скопированной чужой средой.

Обычный PSP имеет сохранённые проверки. Полный каталог передачи одного геймпада
и live handoff остаются незавершёнными задачами. Couch относится к одновременным
независимым pads; эти режимы не заменяют друг друга.
Voice call независим от смены/выхода игровой партии; spectator media через Fluxer,
gameplay Remote Play отдельно. Offline Lobby остаётся задачей; сложные NAT —
известное ограничение прямого соединения, без обязательного тяжёлого relay.

Точные pins, patches, конфигурации, обновление/rollback и прежние результаты
сохраняются в [emulators](emulators/README.md). Непройденные проверки там не закрыты
этой сводкой. Исторические матрицы доступны в Git и не задают очередь реализации.
