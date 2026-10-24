# Мультиплеер — механизмы и подтверждённые границы

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
