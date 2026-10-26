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

Поставка будет добавлена после проверки production binary и сохранности состояния.
