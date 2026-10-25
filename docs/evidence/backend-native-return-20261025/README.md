# Native-вход, host session и SDK identity — 25 октября 2026

Checkout `codex/backend-foundation`, исходная база `9ee6e98`. Изменения подготовлены
в отдельном checkout, WIP основного SDK не переносился. Сборки/БД/runtime только
на сервере; портативки не подключались, GitHub push не выполнялся.

## Доставлено и проверено

- Backend `traineros-backend:native-return-20261025`, image
  `sha256:a6ed4ec12a20ec01ef43b3757e0a1f49dd5986665eed538cdd72613aaa048a66`.
  Миграция 0004 добавляет loopback URI, handoff hash и expected subject.
  Старые миграции не менялись. Backup перед изменением и отдельное восстановление
  новой схемы прошли: [restore-check.txt](restore-check.txt).
- [43 серверных теста](backend-tests.txt), с настоящей отдельной PostgreSQL и
  контролируемым provider. Включают одноразовый native handoff, оба possession
  proofs, запрет внешнего redirect URI, отмену и несовпадение Social subject.
  Один известный warning Starlette/httpx; рабочая БД в тесты не монтировалась.
- [Клиент](account-tests.txt): десять сценариев + init/cleanup, 12 passed. Реальные
  loopback TCP callbacks с fake transport/storage/browser; проверены ложный Host,
  duplicate query, отмена, поздний ответ старого owner, refresh marker до отправки,
  потеря ответа без replay, восстановление, storage failure, Social change,
  logout с истёкшим access, lock при задержанной записи хранилища и launch guard.
- [Живой native cycle](live-check.json): реальный Fluxer consent под ранее выбранным
  владельцем → публичный HTTPS callback → loopback listener **того же C++ BackendAccount**
  → claim → сброс/повторная привязка host → refresh с тем же Player UUID → DELETE сессии.
  Ручной код не вводился. Operator probe — `tools/BackendLoginProbe.cpp`.
  Для браузера на рабочем компьютере временно использовался SSH forward до
  серверного loopback listener; это test harness, не требование игроку.
  Forward закрыт, probe завершён, одноразовый pending-файл удалён,
  временная Backend-сессия отозвана. Браузер возвращён в Fluxer владельца.
- [Public ingress](public-check.json): семь проверок, native capability включена,
  readiness/OpenAPI не опубликованы. Поле `live_oauth_proven:false` в этом файле
  описывает только read-only checker; отдельное live-свидетельство приведено выше.
- [Состояние сервера](server-check.json): API healthy, worker running, четыре
  миграции, прежний Lobby bot active. Secret и его содержимое не экспортировались.
- [Контракты](contracts.txt): architecture-draft.4, 122 schemas, 67 draft operations,
  48 logical entities, 36 boundary examples. Provider-only gate теперь ссылается
  на уже принятую живую проверку; остальные gates остаются открытыми.
- [Полная Linux-сборка](build-check.json) с QML cachegen прошла. [SDK](platform-tests.txt):
  13 passed, 1 прежний Godot/avatar consumer skipped из-за отсутствия внешнего Godot;
  новый account identity/revocation сценарий прошёл. [Settings QML](ui-tests.txt)
  прошёл с нулём warnings: controller navigation до account row, touch login и
  controller cancel. Снимки [входа](settings-account.png) и
  [ожидания](settings-account-waiting.png) на 960×540 просмотрены; текст не обрезан.
  Это синтетические события в настоящем QML, не физическая приёмка кнопок.

## Как повторить ограниченные проверки

Linux builder: Debian 13 / Qt 6.8.2 / GCC 14.2, зависимости из
[`Dockerfile.native-check`](../../../services/backend/deploy/Dockerfile.native-check).
Сборка больших встроенных изображений требует одного compile job и лимита памяти
5 GiB; параллельная первоначальная попытка превысила контейнерный лимит и не
считается успешной сборкой. Это проверочная x86_64-сборка, не ARM64 package.
Для SVG необходим отдельный `qt6-svg-plugins` Debian; первый UI-run без него
показал ошибки декодирования и был повторён после исправления среды. Assertions
и учёт QML warnings не ослаблялись.

В отдельном серверном source/build каталоге:

```sh
cmake -S source -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 1 --target traineros trainer_platform_tests trainer_account_tests
ctest --test-dir build -R '^(backend_account|platform_sdk|settings_qml_smoke)$' --output-on-failure
```

Tests запускаются в контейнере `--network none`, с отдельными временными данными.
Live probe запускается отдельно только по согласованному provider account;
в его хранилище используются память и реальные opaque credentials, которые
не пишутся в evidence/stdout. Сам protocol test не доказывает работу системного vault.

## Что не принято

Полный обычный вход на ARM64/портативках, foreground внешнего браузера и возврат
окна, физический gamepad/touch, реальный QtKeychain/systemd-creds и reboot persistence.
Live probe проверяет повторное создание host context, не физическую перезагрузку.
Также открыты очистка credentials при удалении local Trainer, полные scoped SDK
grants, push revocation, recovery/relink/account deletion, auth retention и
production abuse/off-host backup policy. Identity snapshot не является server
admission, результатом матча или игровым античитом.
