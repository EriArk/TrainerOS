# Связь arcade-кабинетов через Lobby — 26 октября 2026

Реализован отдельный двухместный режим `LAN (in-game)` для поддерживаемых
нативным Flycast игр Naomi / Naomi 2 / Atomiswave. У каждого игрока свой автомат
и экран. Existing GGPO NetPlay одного общего автомата остаётся отдельным выбором.
Start выполняет создатель Lobby после Ready; подключение не запускает игру само.
Совмещённая реализация `776bbb25` установлена на Flip/Odin; [хеши и сохранность
состояния](../evidence/cabinet-link-20261026/README.md#delivery).

## Выбор существующего механизма

Сравнены установленный Dojo `d0e47e572b1e7b355e88bda8308c89d0c5156cbf` и upstream
Flycast `e88b0ced2243de137bcccac0072f0c1dd00d41bd`. Dojo уже содержит
[Naomi network](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/network/naomi_network.cpp)
и [Maximum Speed serial](https://github.com/blueminder/flycast-dojo/blob/d0e47e572b1e7b355e88bda8308c89d0c5156cbf/core/network/net_serial_maxspeed.h).
TrainerOS переносит их существующие пакеты, не реализует синхронизацию игры заново.
Выбран отдельный пакет той же ревизии, чтобы сохранить уже поставленный GGPO.

Новый upstream имеет более широкую поддержку native linking и
[DCNet](https://github.com/TheArcadeStriker/flycast-wiki/wiki/DCNet).
Его [ICE](https://github.com/flyinghead/flycast/blob/e88b0ced2243de137bcccac0072f0c1dd00d41bd/core/network/ice.cpp)
не является готовой универсальной заменой Naomi UDP: он обслуживает отдельный
Battle Cable путь. DCNet соединяет Dreamcast с внешними игровыми сервисами;
учётные записи/комнаты отдельных игр не превращаются в общую Lobby-сессию сами.
Native Dreamcast Online, обновление native покрытия и 3–4 кабинета не реализованы
этой поставкой. R039 остаётся частично незавершённым.

## Совместимость и запуск

S03 (`FlycastNetplay`) проверяет пакет, ROM/BIOS/dependencies через общий arcade
reader, затем загружает ROM настоящим native decoder в приватном временном каталоге.
Проверка завершается до окна, input, CPU frames и network handshake. Только native
`NaomiNetworkSupported()` или serial `NetworkHandshake` создаёт дополнительный
`runtime.flycast.cabinet.v1` profile. В закреплённой версии Naomi predicate содержит
17 внутренних game ID; Maximum Speed — отдельный serial путь. Это возможности
этого эмулятора, не список проверенных TrainerOS матчей. Неизвестное/неподдерживаемое
содержимое не получает ложный LAN-режим, но сохраняет подходящий GGPO profile.

Имена карточек и локальная оценка числа игроков не определяют cabinet capability.
Один игрок на локальном автомате не запрещает соединение двух автоматов.
Обе стороны сравнивают content/runtime/settings, включая SHA всех трёх исполняемых
файлов. Изменение зависимости отменяет старый запуск и обновляет выбор Lobby.

S06 сохраняет существующие membership/ticket/Ready/Start. S07 `LobbyConnection`
добавляет режим `cabinet` к существующему ICE/DTLS helper: Nearby без публичного STUN,
Online через существующий Fluxer signaling и публичный STUN. Ручной ввод IP,
VPN и отдельный сервер пакетов не нужны. Возможность direct P2P зависит от сети;
сложный NAT не объявлен решённым. GGPO public relay не выдаётся за cabinet relay.

Каждый native процесс работает через `bwrap --unshare-net`, где есть только loopback.
Endpoint принимает UDP только от своего фиксированного native порта и пересылает
неизменённые пакеты через приватный Unix socket и reliable SCTP channel 2.
Host native port 37391 / endpoint 37392; guest native port 37392 / endpoint 37391.
Так сохраняется встроенный в native sync reply адрес следующего узла без переписывания
протокола. Это контракт двух участников, не произвольная виртуальная локальная сеть.
Максимум пакета `0x4004`; local IPC имеет очереди до 1 MiB с постепенным разбором
склеенных/частичных frames, один клиент,
0700 directory / 0600 config+socket, запрет повторного attach и смешения режимов.

При потере соединения endpoint прекращает пересылку, но оставляет игру живой.
Возврат/переподключение выполняет игрок; автоматического Rejoin/перезапуска нет.
S08 сохраняет временный progress: отдельные config/EEPROM/NVRAM на запуск, без
импорта и изменения личных данных. Завершение удаляет временный каталог.

## Воспроизведение, доставка и границы доказательств

[Рецепт/патчи/обновление](../../packaging/emulators/flycast-lan/README.md) фиксируют
исходники и rollback. [Доказательства](../evidence/cabinet-link-20261026/README.md)
разделяют native build, реальные транспортные проверки, установку и игровой матч.
Новый пакет устанавливается рядом с прежним `flycast-netplay`; другие runtime и
пользовательские настройки не заменяются.
