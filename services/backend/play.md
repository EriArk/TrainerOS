# Registry, Results, Boards — работающий scorecard preview

Входы и пользовательский контракт: [SDK PLAY](../../sdk/PLAY.md). Владельцы S10
(правила, матч, решение, таблицы), S01 (сессия), S14 (ограниченный bridge).
Lobby, игровой transport и аватар не получают второго владельца.

Фактические API: GET `/v1/games`, POST `/v1/play/grants` (account bearer),
POST `/v1/play/commands` (узкий play bearer), GET
`/v1/boards/{ruleset_id}/{mode}/{kind}`. Реальный OpenAPI формируется FastAPI;
архитектурные 67 операций в contracts остаются целевым расширенным интерфейсом.
Не объявляем их все реализованными из-за появления этого ограниченного preview.

## Запись и конкуренция

Миграция 0005 добавляет registry/grants/matches/participants/submissions/decisions/
outcomes/board_revisions. Применённые 0001–0004 не меняются. Registry доступен
runtime только на чтение. Новая immutable версия — новый ID, не правка старой.
Rules hash охватывает game/content/definition. SQL, путь импорта или исполняемый
валидатор из клиентского запроса не принимаются.

Account проверяется в той же транзакции, что команда; grant связан с той же session.
Receipt + domain effect + audit + job фиксируются атомарно через существующий
run_command. Session/share locks исключают запись после уже завершённого revoke.
Match row lock сериализует join/start/submission/decision. Один payload у одного
участника сохраняется один раз, независимо от новых operation IDs. Другой payload
создаёт спор. Payload до записи нормализуется по side; исходная карточка сохраняется.

Задача results.verify исполняет только встроенный ограниченный scorecard.v1.
Нет внешнего кода, сетевых запросов или тяжёлого replay. Существующая очередь
сохраняет lease/fence/retry/dead letter. В get видны состояния проверочных jobs;
ошибка worker не превращается в победу или поражение.

Outcome и append-only decision/outbox возникают в одной транзакции. Boards читают
только confirmed matches и исходные immutable outcomes. Поэтому повтор worker
не удваивает статистику, а dispute/void не оставляет старой победы. Это вычисляемая
SQL-проекция, не отдельный отстающий consumer. Revision и страница читаются в
REPEATABLE READ; pagination отвергает старую revision. Future rewards должны
потреблять decision events и собственные компенсации; сейчас их нет.

## Оператор

Никакой игре не выдано право регистрировать правила или аннулировать чужие матчи.
В deploy, после backup и явной migrate:

```sh
docker compose --profile maintenance run -T --interactive=false --rm migrate \
  python /ops/manage.py register-ruleset /registry/scorecard-demo.json
docker compose --profile maintenance run -T --interactive=false --rm migrate \
  python /ops/manage.py void-match MATCH_UUID OPERATION_UUID
```

Manage читает owner secret только внутри maintenance container. Void записывает
receipt/audit от отдельного local-maintenance operator, новую decision и новую
revision таблицы; повтор UUID безопасен. Решение и карточки не удаляются. При
нужде восстановить спорную игру требуется отдельная операторская процедура:
самовольного выбора первого отправителя победителем нет.

`registry/scorecard-demo.json` — отдельный демонстрационный registry, не прошедшие
приёмку реальные правила Golf. Регистрировать production game можно после привязки
её точной версии контента и результата. Данные демонстрации не смешиваются с игрой.

## Проверки и эксплуатационные границы

Backend tests запускаются только на сервере в одноразовом контейнере/собственной
PostgreSQL. `deploy/check-play-chain.py NATIVE_WORKSPACE OUTPUT_DIR` соединяет пять
настоящих C++ SDK hosts с HTTP/PostgreSQL/worker и Python SDK. Оба контейнера
разделяют изолированный network-none namespace; production volume/секреты не
монтируются. Идентичности провайдера — фикстуры; account sessions/rotation и все
следующие запросы — реальные. После проверки контейнеры и временные credentials
удаляются. Это не новый live OAuth или портативная игровая приёмка.

Запросы ограничены 16 KiB / 10 секунд и 4 KiB query. Auth сохраняет 120/min на
соединение ingress. Остальной API — 120/min на credential/public IP и общий
2400/min на процесс; forwarded headers не используются как доверенный IP.
Дополнительно SQL сериализует quota 16 живых grants на account session и 12
созданных матчей в час на Player. Это ограниченная initial policy, не обещание
нагрузочной ёмкости сервера. Native peers ограничены 16 и одним in-flight запросом.

Реальные игровые валидаторы, offline queue, trusted build admission, moderation
UI/resolution, cursor beyond the bounded page, объёмные leaderboard projections,
anti-abuse по реальному edge client, retention, off-host backup и оповещения
остаются отдельными открытыми условиями. Нет монет, Elo, турниров или принятой
формулы общего статуса. [Приёмка](../../docs/evidence/backend-play-20261025/README.md).
