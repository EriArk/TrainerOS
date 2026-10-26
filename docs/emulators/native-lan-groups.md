# PS2 и GameCube BBA: 2–4 независимые консоли

## Контракт — 26 октября 2026

Общий GameHub Lobby подготавливает всех участников, хозяин явно нажимает Start.
`RuntimeMultiplayer::startNativeLanConnections` переиспользует групповое соединение
кабинетов: хозяин создаёт отдельный `LobbyConnection` к каждому гостю, гость — к
хозяину. Каждый канал связан с принятым составом, игрой и ticket. Запуск ждёт все
свои соединения; новый раунд не использует старые каналы.

`NativeLanConnection` — приватный локальный JSON v2: mode, room, seat, players,
сокеты по номеру участника. Его создаёт оболочка, а не сетевой участник.
Места последовательные P1–P4; хозяин P1. Идентификатор сегмента общий для ticket,
а транспортная identity отдельна для каждой пары. Версия settings не позволяет
смешивать старые парные и новые групповые пакеты.

- **PS2:** `eclx1-group-own-cards-v2`. Существующий ARMSX2 Local Link выполняет
  регистрацию, аутентификацию, DHCP, сборку Ethernet и пересылку между гостями.
  Каждый guest имеет собственный peer ID 2–4. В приватной network namespace
  у хозяина отдельный loopback UDP socket на каждый принятый канал; гостевой
  native port остаётся 19073. Сетевые peer IDs проверяются против назначенного
  места. Формат ECLX и исходник эмулятора не меняются.
- **GameCube BBA:** `dolphin-bba-group-isolated-v2`. `bba_endpoint` запускает
  Dolphin с существующим TAP server; каждый игрок получает уникальный MAC.
  Хозяин передаёт Ethernet локальной консоли и другим гостям, исключая источник.
  Broadcast, multicast и unicast сохраняются; native BBA фильтрует назначения.
  Гость не может выдавать кадр за другую консоль. Host TAP, VPN и домашние
  интерфейсы не объединяются. Размеры кадров и очередей ограничены.

Оба endpoint используют существующий ICE/DTLS datagram transport. Для Nearby
STUN не нужен; Online использует текущий direct путь. Это не новый relay-сервис
и не гарантия обхода сложного NAT. Четыре места — вместимость интеграции, не
обещание четырёх игроков в каждом режиме игры. Сохраняются полный PS2 LAN-каталог
и native BBA-family admission; обычный Dolphin NetPlay не ограничивается ими.

При потере любого ребра группа прекращает передачу. Игра продолжает работать;
возврат/выход принадлежит Home и владельцу процесса. Автозапуска/Rejoin нет.
PS2 использует собственные карты и приватную копию INI; Dolphin сохраняет
прежний временный user-dir и не подключает личные карты. Параллельная Crates
recovery `f8244d75` объединена до сборки.

## Опоры и обслуживание

Изучен [LocalLinkAdapter установленной линии ARMSX2](https://github.com/ARMSX2/ARMSX2/blob/65fd14b9160e7fa3779794fc45c4e92a86772621/pcsx2/DEV9/LocalLinkAdapter.cpp):
`RegisterPeer`, `ReceiveDatagram`, `RelayDatagram` уже реализуют звезду с
несколькими peer IDs. Используется прежний pinned Dolphin TAP/EOF пакет.

Сборка endpoint: `cmake -S tools/ps2-lan -B BUILD -DBUILD_DISC_READER=OFF`,
затем `cmake --build BUILD`. Полная PS2 упаковка оставляет pinned libchdr;
Dolphin `package-lan.py` получает `--endpoint BUILD/bba_endpoint`.
Manifest проверяет helper hashes; PS2 сохраняет устройство-специфичные runtime
и configRoot. Обновляются оболочка и оба LAN-пакета совместно, при закрытых играх.
Для отката восстановить все три прежних компонента; настройки/карты не заменять.

Проверки: `check_config.py`, `check_launcher.py`, `check_namespace.py`,
`check_groups.py TRANSPORT ENDPOINT`; последние запускают реальные дочерние
процессы, Unix/UDP/ICE каналы для 2/3/4 консолей с протокольными fixtures.
Они не запускают коммерческие игры. Shell suites проверяют слот, Ready/Start,
частный запуск, отзыв канала и сохранность lifecycle.

Пройдены ARM64 suites `standalone`, `lobby`, `game_party`, `runtime_multiplayer`,
дополнительные проверки группового материала и `crates_showcase`, 7 целевых
library cases. Python: 8 config + 14 launcher, namespace boundary и все 6
комбинаций PS2/BBA × 2/3/4 через реальные процессы/ICE. Проверено отсутствие
эха, чужого MAC и сохранение процесса после отзыва сети.

Полный `library` suite **не зелёный**: 23 passed / 3 failed. Отдельно
воспроизводятся `contextualMenuUsesWheelSelectionAndTrapsInput`,
`contextualEditsDeleteRomAndPreserveIdentity`, `settingsAreCommittedAndRetryable`.
Они относятся к неизменённым Settings/Shell/world-edit путям; последний ожидает
прежнюю turquoise theme вместо текущей light. Тесты не удалены и assertions не
ослаблены. Полный лог сохранён; эта поставка не исправляет эти три несоответствия.

Production-сборка и установка текущего изменения ещё выполняются.
