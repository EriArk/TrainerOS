# Native группы Flycast — 26 октября 2026

Исходники: групповой DCNet launch и 2–4 Naomi кабинета; [контракт](../../emulators/flycast-groups.md).
Проверки на ARM64:

- `flycast_netplay`, `lobby`, `game_party`, `runtime_multiplayer`: 4/4 прошли.
- `ps2_socket_boundary`, `stream_transport_boundary`: 2/2 прошли (внутри transport — 18 случаев).
- `tools/flycast-lan/check.py`: реальные ICE/DTLS + отдельные network namespaces,
  2/3/4 кабинета и все пары направлений, native source ports, лимит пакета/очереди,
  foreign source/oversize, завершение сети без завершения child — прошли.
- `cabinetCapabilityAndPrivateLaunch` на Linux host: прошло без пропуска namespace;
  варианты 2/3/4 мест и отдельный serial ceiling. В container этот случай пропущен
  из-за вложенного namespace, поэтому выполнен отдельно на host.
- Экспорт adapter/runtime knowledge: 32/239 файлов проверены.

UDP child — protocol fixture, а не коммерческая игра. Данная проверка не доказывает
новый 3–4-player game match или разные интернет-сети. Предыдущие игровые результаты
не переобозначены как новые. Sonic/Battlefront не запускались; сон/крышка/перезагрузка
не затрагивались. Native polling оставлен upstream 20 ms; 60 seconds относится только
к ожиданию состава, без увеличения интервала обработки пакетов.

## Установлено на Flip и Odin

Исходники `ac71fc83` (реализация `e6cac354`), production shell `53027e6b…`,
`BUILD_TESTING=OFF`. Совпадение 980 входных файлов сборки проверено; девять изменённых
native файлов реконструированы из закреплённого upstream + repo patches и совпали
с реально собранными. Полные хеши в [verification.json](verification.json).

На обеих запущен новый shell; package `flycast-lan` имеет settings v2,
manifest `dc806b3e…`, native `1454e427…`. Сохранены все остальные 18 emulator packages,
в том числе DCNet/GGPO, существующая работа Crates и обычные установки эмуляторов.
Проверенные 1539 файлов Flip / 2166 Odin и история не изменились; QML errors: 0.
InputPlumber mode 0; Odin volume 0%, Flip 20%, без перезагрузки устройств.
Предыдущие shell/package сохранены для совместного отката. Сохранения и профили
при откате пакета не заменяются. Установка не обозначена новым игровым матчем.
