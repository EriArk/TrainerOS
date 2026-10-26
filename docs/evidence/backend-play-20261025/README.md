# SDK → серверный матч → результат → общие таблицы

25 октября 2026. Ветка `codex/backend-foundation`, изолированный Backend checkout;
исходная точка `85e9557c`. Реализация, сборка, PostgreSQL и проверки выполнялись
на сервере; рабочий компьютер использовался для source/git/доказательств.
Портативки не подключались; GitHub push не выполнялся.

## Доделано в этом проходе

- Общий host-mediated Play SDK: фиксированные методы, один in-flight запрос,
  account/grant credentials вне игры, грант на session/Player/ruleset. Disconnect,
  смена профиля и поздний grant не подменяют владельца; серверный session revoke
  проверяется каждым запросом. Существующие account lock/storage/refresh сохранены.
- Серверные составы 2–16, самостоятельное вступление, фиксирование при Start,
  команды по 2/3 и общая карточка стороны. Неравные 2 против 3 поддерживаются.
- Idempotent receipts, durable bounded worker, immutable registry, исходные
  submissions/append-only decisions/outbox. Противоречие снимает результат сразу.
- Общие records/wins, личные matches/wins/draws/losses/unique defeated, последние
  50 матчей, равные места 1,1,3, revision pagination/around, privileged void с audit.
- Серверная версия `traineros-backend:play-20261025`, миграция 0005. Образ API и
  worker: `sha256:9b92b47a4573079859266cfeb2c70bc99836404a4e1a6249f04c8fca8a7a99cd`.
  API/БД healthy, worker running, исходный Lobby service active. Порт API остался
  server-loopback-only; отдельный HTTPS ingress и OAuth config сохранены.

Контракты: [SDK](../../../sdk/PLAY.md), [сервер](../../../services/backend/play.md).

## Доказательства

| Проверка | Результат и предел |
| --- | --- |
| [Backend](tests.txt) | **57 passed**, 1 прежний Starlette/httpx deprecation warning. Собственная PostgreSQL/runtime role в network-none контейнере, production volume не монтировался. |
| [Native tests](native-tests.txt) | **14 account passed**, **14 SDK passed / 1 прежний Godot skip** (нужны external Godot/local pack); Settings QML passed, 0 QML warnings. Реальный SDK TCP socket, скрытие токенов, late grant/owner change, существующая auth regression. |
| [Полная сборка](build-check.json) | Linux x86_64, Qt 6.8.2, QML cache generation включён; traineros, account/SDK tests, play probe. SHA256 и размеры приложены. Не ARM/handheld build. |
| [Сквозной сценарий](live-chain.json) | Пять отдельных C++ BackendAccount/PlatformHost процессов и Python SDK. Настоящие TCP SDK, HTTP, PostgreSQL, session refresh, grants, worker, личный матч, 2v3, board, stats, receipt replay, late conflict, stale revision. Только provider identity и vault — изолированные фикстуры; живого OAuth в этом проходе не было. |
| [Публичный ingress](public-check.json) | 7 прежних HTTP checks, включая закрытые readiness/OpenAPI и отказ без account token. `live_oauth_proven:false` — граница этого read-only checker, не отмена предыдущей OAuth-приёмки. |
| [Публичный Play](play-public-check.json) | Registry/пустая demo board доступны; выдача grant без account bearer отвергнута 401. Production-матчи не создавались. |
| [Развёртывание](server-check.json) | Образ/health трёх контейнеров, пять миграций, Lobby active. У worker нет отдельного healthcheck; running не называется health proof. |
| [Восстановление](restore-check.txt) | Backup до обновления; после установки новый dump восстановлен в отдельную PostgreSQL без сети/production volume. Точная история/checksums пяти миграций совпала. Реальных игровых данных на live ещё нет. |
| [Совпадение source](source-hashes.json) | 34 нормализованных UTF-8/LF source hashes локального checkout сверены с native build snapshot и работающим API image. |

Серверные сценарии покрывают: HTTP strict bodies/подмену Player, неизвестные
credentials, readonly registry, cross-ruleset access, session/grant expiry/revoke,
частичный/полный состав, host-only start, roster freeze, 16 concurrent submissions/
workers без второй decision, невалидные метрики, ничьи, неравные команды, late
dispute, void/replay, повтор команды после нового grant, timeout/cancel, независимые
rate budgets игроков за одним ingress. Проверка 16 заявителей не измеряет FPS
или качество realtime сети 16 портативок.

После сквозной проверки temporary fixture containers остановлены/удалены,
пять fixture credentials удалены; соответствующих контейнеров и credential файлов
не осталось. Vault фикстуры в памяти, production secrets в тесты не передавались.

## Воспроизведение на сервере

Backend build/test — обычный `services/backend/Dockerfile --target test`,
`docker run --rm --network none --cpus 1 --memory 768m --pids-limit 128 IMAGE`.
Native targets: `traineros trainer_account_tests trainer_platform_tests
trainer_backend_play_probe`. Фокусированный CTest:

```sh
ctest --test-dir BUILD -R '^(backend_account|platform_sdk|settings_qml_smoke)$' --output-on-failure
python3 services/backend/deploy/check-play-chain.py NATIVE_WORKSPACE OUTPUT_DIR
```

Второй скрипт требует заранее собранные test/native images, shared isolated network
namespace и тестовый probe. Он не использует production endpoint или credentials.
Native probe допускает только `http://127.0.0.1:19091`, остаётся BUILD_TESTING tool
и не устанавливается в обычную оболочку. Production host origin остаётся фиксированным.

## Недоделано и следующий потребитель

Это **peer_confirmed scorecard**, не воспроизведение физики и не общий античит.
В live registry включён только `traineros.scorecard-demo.v1`, явно обозначенный
демонстрацией; Pocket Golf и другие реальные игры ещё не регистрировались.
Нужно связать их точные content/rules revisions, реальные start/end/roster события,
результаты и игровой экран общей таблицы; для Golf отдельно проверить журнал и
допуск solo/bots/командного чередования. Demo не превращается в production board.

Остаются trusted build admission, более сильные игровые validators, offline queue,
dispute resolution UI, полноценная формула статуса/силы, турниры/экономика, retention,
off-host backup/alerts, целевой vault и физические/портативные проверки. Общий
Backend и R089/R107 остаются частично готовыми. Lobby/transport/avatar и чужой WIP
не менялись; исходники сохранены в отдельном backend-коммите.
