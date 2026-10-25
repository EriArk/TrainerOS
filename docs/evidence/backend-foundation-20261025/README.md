# Backend foundation — серверная проверка, 25 октября 2026

Исходники: `codex/backend-foundation`, поверх `6c1c028` (формальные контракты).
Владелец разрешил выполнять работу непосредственно на существующем сервере.
Реализованный пакет — [services/backend](../../../services/backend/README.md).
Публичный ingress, публикация GitHub и портативки в этой проверке не участвовали.

## Реальные результаты

- [17 тестов](tests.txt) прошли на сервере в контейнере без внешней сети, с собственной
  PostgreSQL 17 и непривилегированным пользователем. Сервисный volume не монтировался.
  Проверены конкурентные транзакции, rollback, права, dedup, revoke, fencing,
  retry/dead letter, event retention/reordering и actual API. Одно предупреждение
  Starlette относится к устаревающему httpx TestClient; тесты прошли.
- API, worker и PostgreSQL установлены отдельным Compose-проектом. API и БД имеют
  healthy; worker запущен, доменные handlers пока отсутствуют. API опубликован
  только на `127.0.0.1:19120`, PostgreSQL не имеет host port.
- [Проверка установленного сервиса](server-check.txt): ready=200, product capabilities
  пусты, отсутствующий business endpoint=404; при остановке только своей БД
  live=200/ready=503, после запуска снова ready=200. API/worker перезапущены,
  миграционная история сохранена. Проверены non-root UID, read-only root и loopback binding.
- [Dump/restore](restore-check.txt): custom-format backup восстановлен в одноразовый
  контейнер PostgreSQL без сети и постоянных томов; контрольная сумма миграции
  совпадает. Restore-контейнер удалён. Это backup foundation, ещё без пользовательских
  account/results/economy данных.
- `pip check` в установленном API: `No broken requirements found`.
- После установки службы Lobby directory, Crates catalogue и его tunnel остаются
  active/running. Их конфигурация и процессы не изменялись; пользовательский
  invitation/game flow повторно не проверялся.

Установленный runtime image:
`sha256:12894dad527bef5e9224ab53697e2269b5958d0bf4f4a3aa1de29f2262d3bb8f`.
Digest базовых образов — [images.env](../../../services/backend/deploy/images.env).
Секреты/DSN, содержимое backup и приватные серверные пути в свидетельство не входят.

## Ограничения

Вход, постоянный Trainer ID, провайдеры, результаты, таблицы, турниры и экономика
ещё требуют реализации. Текущие 67 proposed операций не являются установленным
продуктовым API; игровые SDK capabilities не изменялись. Worker умеет исполнять
доверенные короткие транзакционные handlers, но игровых handlers ещё нет.
Внешние/долгие validators и provider effects требуют отдельной интеграции.

Нет проверки публичного HTTPS/provider auth, нагрузки, внешней сети, игры,
физического контроллера или портативок. Нет off-host backup, расписания,
retention и внешних alert-каналов. Восстановление реальных пользовательских
сценариев пока невозможно: эти сценарии ещё не реализованы. R089/R107–R115
остаются открытыми.

Локальные PostgreSQL, тестовые кластеры, venv, wheel/build/cache и временный архив
передачи убраны из рабочего каталога в корзину Windows. Прямое удаление было
отклонено автоматической проверкой; использован обратимый перенос в корзину.
Исходники сохранены. Сборки и дальнейшие runtime-проверки выполняются на сервере.
