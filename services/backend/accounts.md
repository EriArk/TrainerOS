# Accounts: вход через Fluxer без второго логина

**25 октября — scoped game access:** `play.scorecards:1` теперь использует
session-bound grants внутри host, без передачи account/grant token в игру.
[Жизненный цикл и ограничения](../../sdk/PLAY.md). Это закрывает bounded
scorecard access, не trusted publisher/build attestation, push revocation или
полный набор account/economy grants из целевой архитектуры.

25 октября 2026. Владелец подтвердил: постоянный аккаунт TrainerOS привязан к
Fluxer; отдельного логина/пароля TrainerOS нет. Для OAuth создаётся отдельное
приложение **TrainerOS**, а существующее **TrainerOS Lobby Directory** остаётся
без изменений. Новое приложение зарегистрировано владельцем **EriArk** через
его постоянный аккаунт. Отдельный HTTPS origin — `https://traineros-api.abysstail.art`.
Владелец подтвердил выдачу client secret в Fluxer. Секрет и независимый ключ
Backend сохранены только в закрытом каталоге сервера; точный callback сохранён
и проверен через public application metadata. Provider login активирован.
Приложение не доступно для публичного приглашения как бот; запрашивается только `identify`.

**Уточнение пользовательского сценария:** создание приложения и настройка HTTPS —
разовая работа оператора. Игрок не создаёт OAuth-приложение и не настраивает сервер.
Принят обычный путь «Войти через Fluxer → подтвердить доступ → вернуться в TrainerOS»
с сохранением входа в оболочке. Обязательный ручной код ниже — промежуточный
серверный механизм, а не принятый игровой UX. Безопасный native return и хранение
сессии оболочкой реализованы следующим дополнением. Приёмка на портативках остаётся отдельной.

## Обычный native-вход без кода, 25 октября

Владелец состояния — `src/features/accounts/BackendAccount`; композиция — Shell/Main,
потребитель публичной identity — PlatformBinding/PlatformHost. Settings → Trainer
и Trainer & accounts дают общий action для touch/controller: вход, отмена ожидания,
выход и повтор после ошибки. Локальный Trainer, Social token, avatar и saves независимы.

1. Host сначала слушает только `127.0.0.1` на случайном порту и случайном пути
   `/traineros/auth/<43 base64url chars>`. Затем передаёт `return_uri` вместе с
   S256 challenge. Допустимы только HTTP IPv4 loopback, порт 1024–65535 и точный
   случайный путь, без query/fragment/credentials. Backend никогда не запрашивает этот URI.
2. Системный браузер открывает HTTPS start. Backend ставит browser cookie и сразу
   перенаправляет в Fluxer с прежними state/PKCE/identify. Ручного кода нет.
3. После проверенного callback Backend создаёт новый случайный `handoff_token`,
   сохраняет только SHA256 и перенаправляет его с attempt ID на зарегистрированный
   loopback URI. Никакого access/refresh в браузере. Отмена возвращает ограниченный error.
4. Host принимает только точные Host/path/attempt, два уникальных query-поля,
   GET HTTP/1.1, заголовки до 8 КиБ/5 секунд, максимум четыре подключения. Закрывает
   listener и предъявляет **verifier и handoff вместе** в claim. Одного verifier,
   даже после согласия владельца по пересланной ссылке, недостаточно.
5. Ответ с access/refresh и проверенным Fluxer subject остаётся внутри host.
   Если Social уже вошёл, ожидаемый subject проверяется сервером; позднее изменение
   Social identity сбрасывает Backend context. Разные аккаунты не связываются молча.
6. Refresh сохраняется в уже работающем QtKeychain либо существующем
   systemd-creds fallback; plaintext-файлов нет. Ключ разделён по origin/local Trainer
   и назначению Backend, Social credentials не переиспользуются. Процессный lock
   не допускает двух ротаторов одного owner. Перед refresh атомарно сохраняется
   marker `pending`, после успеха — новый refresh. Потеря ответа/перезапуск при
   pending требует входа заново, не воспроизводит использованный refresh.
7. Открытие браузера использует тот же launch guard, что обычные Apps: активная
   игра или защищённая операция не прерываются входом. Возврат поднимает окно
   оболочки, если игра не активна. Смена owner/отмена
   закрывают listener и отбрасывают старые callbacks; полученный поздно grant
   отзывается. Выход подтверждается серверным DELETE, локальный профиль остаётся.

Основа подхода — external user agent, loopback и PKCE из
[RFC 8252, §§7.3, 8](https://www.rfc-editor.org/rfc/rfc8252.html).
Дополнительный browser-only handoff нужен именно для серверного callback и claim;
это не стандартный device grant. Local endpoint не защищает от полного захвата
учётной записи ОС. При недоступном безопасном хранилище вход не сохраняется и
пользователь получает явную ошибку.

SDK capability `account.identity:1`, `account.get` и `context.account` возвращают
только `state: signed_in|signed_out` и `player_id`. Это снимок host, не proof для
серверного допуска/результата. Изменение Player ID отзывает старое SDK-соединение
через `context.revoked/account_changed`; автоматического принятия нового игрока нет.
Полный проект `account.online.get`, scoped grants и delivery серверных revocation
events ещё не реализованы: удалённый отзыв обнаруживается при следующем refresh.

## Промежуточный серверный путь с кодом (совместимость)

1. Оболочка генерирует случайный `claim_verifier` (32 случайных байта в base64url),
   сохраняет его у себя и передаёт только SHA256 challenge с `S256` в
   `POST /v1/auth/login-attempts`. Обязателен UUID `Idempotency-Key`.
2. Backend возвращает `attempt_id`, срок 10 минут, `verification_code` (8 цифр)
   и свою HTTPS-ссылку начала
   входа. Повтор того же запроса до открытия ссылки возвращает ту же попытку;
   другой payload с тем же ключом отклоняется. Начатая/истёкшая попытка не переиспользуется.
3. Оболочка показывает код только на устройстве, начавшем вход. Браузер открывает
   ссылку, Backend проверяет одноразовый ticket, устанавливает
   Secure/HttpOnly/SameSite=Lax cookie с префиксом `__Host-` и просит ввести код
   с устройства. Код не включается в URL, страницу браузера или автоматическую
   отправку формы. POST подтверждения требует тот же Origin, cookie и скрытый
   proof; после пяти неверных кодов попытка закрывается. Только затем следует
   переход на Fluxer с `scope=identify`, случайным `state` и отдельным PKCE S256 challenge.
   Это не позволяет чужой ссылке автоматически авторизовать устройство,
   используя ранее разрешённое Fluxer-приложение.
   Повторное открытие использованной ссылки требует начать вход заново.
4. Callback принимается только для действующей попытки из того же браузера.
   Код обменивается сервером через confidential client и PKCE; `userinfo`
   определяет числовой subject. `user_id` из клиентского запроса не принимается.
   Привязка UNIQUE `(fluxer, instance, subject)` создаёт/находит внутренний UUID.
   Изменение username/avatar не меняет ID. Callback не выдаёт токены браузеру.
5. Оболочка вызывает `POST /v1/auth/login-attempts/claim` с исходным verifier.
   Только один конкурентный запрос получает access/refresh и session ID.
   Остальные получают ошибку; потерянный ответ нельзя повторять как обычную команду.
   Ревизии в API — десятичные строки, даты — UTC с суффиксом `Z`.
6. `GET /v1/me`, `GET /v1/auth/sessions`,
   `DELETE /v1/auth/sessions/{id}` работают только с собственным Bearer-токеном.
   Последний маршрут отзывает ровно указанную собственную сессию. В той же
   транзакции записываются audit и внутренний outbox `account.session_revoked`.
   Доставка этого события в SDK ещё не реализована.
7. `POST /v1/auth/refresh` однократно меняет оба токена. Повтор старого refresh
   отзывает всю эту сессию; другие устройства остаются активными. Оболочка должна
   сериализовать refresh и не делать слепой повтор после неопределённого исхода.

Access живёт до 15 минут, сессия имеет абсолютный срок 30 дней без бесконечного
продления; одновременно допускается до 10 активных устройств. Это текущие
технические ограничения, не тариф/окончательная продуктовая политика. Revoked,
suspended или expired identity/session не дают читать данные или обновлять токены.

В БД сохраняются хеши access/refresh/state/browser proof, а не их исходные значения.
PKCE/state/ticket выводятся с разными назначениями из отдельного серверного ключа
и случайного attempt UUID. Пароли Fluxer и полный Social user token не используются.
OAuth credentials живут только в коротком серверном обмене и не сохраняются.
Не запрашиваются email/guilds, не меняется профиль/аватар и не отправляются сообщения.

Размер auth body ограничен 16 КиБ и временем чтения, ошибки валидации не отражают
секретные входные значения, provider response ограничен 64 КиБ и таймаутом,
HTTP redirects при обмене запрещены. Есть ограниченная по памяти защита запросов
по адресу подключения и общий SQL-лимит создания 60 попыток в минуту. До публичной
поставки нужна настройка trusted ingress/client address и per-account abuse policy:
поддельные X-Forwarded-For сейчас не используются. Это не доказательство защиты
от распределённого DDoS или завершённая production-приёмка.

## Подтверждённый контракт Fluxer

Общая [документация OAuth](https://fluxerapp-fluxer.mintlify.app/authentication)
описывает authorization-code и `identify`. Точные paths/PKCE проверены по upstream
`7f00da6cef316d7d38ee566baa583fe55790c81c`:

- [OAuth2Controller.ts](https://github.com/fluxerapp/fluxer/blob/7f00da6cef316d7d38ee566baa583fe55790c81c/fluxer_api/src/api/oauth/OAuth2Controller.ts)
- [OAuth2Service.ts](https://github.com/fluxerapp/fluxer/blob/7f00da6cef316d7d38ee566baa583fe55790c81c/fluxer_api/src/api/oauth/OAuth2Service.ts)
- [OAuthSchemas.ts](https://github.com/fluxerapp/fluxer/blob/7f00da6cef316d7d38ee566baa583fe55790c81c/packages/schema/src/domains/oauth/OAuthSchemas.ts)

На целевом instance обнаружение `https://api.fluxer.app/.well-known/fluxer`
подтвердило `api_public=https://api.fluxer.app`, `webapp=https://web.fluxer.app`.
OAuth userinfo без credentials возвращает 401. Реализация использует
`/oauth2/token` и `/oauth2/userinfo` на api_public, а не устаревший пример с
дублирующим `/api/v1`. Проверка upstream/endpoint не равна живому входу.

## Активация отдельного приложения

В постоянном аккаунте владельца: Fluxer → User Settings → Applications → New
Application → **TrainerOS**. Зарегистрировать точный HTTPS callback
`<public_base>/v1/auth/callback/fluxer` после выбора/подключения публичного backend
origin. Тестовые аккаунты не назначаются владельцем постоянного приложения.
Выбранный origin — `https://traineros-api.abysstail.art`; callback должен точно
совпадать с `https://traineros-api.abysstail.art/v1/auth/callback/fluxer`.
Отдельный tunnel публикует только `/v1/*` и `/health/live`; внутренние readiness
и OpenAPI снаружи недоступны. Шаблоны конфигурации находятся в
`deploy/cloudflared.example.yaml` и `deploy/traineros-backend-tunnel.service`.
Реальные tunnel credentials остаются на сервере. Lobby/Crates tunnels не меняются.

На сервере сохранить закрытый `deploy/secrets/fluxer-oauth.json` с полями:
`client_id`, `client_secret`, `public_base` (HTTPS origin без завершающего `/`),
`key` (независимые 32 случайных байта в base64url). Не помещать значения в Git,
командную строку, чат или логи. Не копировать bot token/секрет Lobby-приложения.
Каталог secrets остаётся 0700; файл доступен контейнерному UID только на чтение.

После регистрации, настройки HTTPS и проверки redirect URI:

```sh
cd deploy
docker compose -f compose.yaml -f compose.oauth.yaml up -d --wait api
```

На действующем сервере в закрытом `deploy/.env` добавлено
`COMPOSE_FILE=compose.yaml:compose.oauth.yaml`, чтобы последующие обычные команды
Compose сохраняли OAuth overlay. Для отключения входа явно использовать только
`-f compose.yaml`. Базовый запуск без overlay намеренно отключает provider login (`provider_not_configured`, 503), а
capabilities остаётся пустым. Включение конфигурации объявляет только
`accounts.fluxer_login`/`accounts.sessions`/`accounts.native_return`; оно само по себе не означает приёмку
Fluxer, shell/SDK или портативок. Ключ обязателен в зашифрованной эксплуатационной
копии секретов; его смена завершает незаконченные попытки входа, но не меняет
существующие хешированные access/refresh credentials.

## Проверки и оставшиеся условия

Реальный вход и отказ проверены через встроенный браузер и публичный HTTPS.
Два входа в один Fluxer account получили один Player UUID; собственная сессия,
refresh и отзыв отработали, обе временные Backend-сессии отозваны. Это проверка
промежуточного code flow, не SDK/native handoff.
[Живая приёмка и исправление Origin](../../docs/evidence/backend-oauth-live-20261025/README.md).
Страница подтверждения использует `Referrer-Policy: same-origin`, чтобы HTML POST
сохранял Origin; остальные ответы — `no-referrer`. Проверка Origin не ослаблена.

Тесты выполняются на сервере с отдельной PostgreSQL, без service volume и
внешней сети. OAuth upstream заменён контролируемым test provider/HTTP fixture.
Проверяются cookies/state/PKCE/claim, конкурентная выдача, стабильность identity,
изоляция устройств/игроков, rotation/replay/revoke/expiry/suspension, отсутствие
секретов в БД/ошибках и границы HTTP adapter. 34 теста прошли после исправления
браузерного Origin; fixture-проверки отделены от живой приёмки выше.

Новый native return, host session и SDK identity описаны выше;
их проверка отделена от исторической приёмки code flow. Остаются: физический
вход/возврат/хранилище на целевых системах, очистка online credentials при удалении
локального Trainer, очереди/отзыв SDK grants, recovery/смена
привязки/удаление аккаунта и очистка auth-записей по принятой retention policy.
Истечение сейчас запрещает использование записей, но не удаляет их автоматически.
Никакие покупки, результаты или косметика не привязываются по имени Fluxer.
Обычные локальные игры не требуют Backend-входа.


## Управляемое окно входа — 29 октября 2026

В dedicated handheld session BackendAccount передаёт HTTPS URL существующим
AppsController/apps-service. GIO выбирает установленный браузер; окно получает
обычный input lease, Home и Home+X. Успешный OAuth автоматически сворачивает
браузер и возвращает карточку аккаунта, сохраняя исходный экран оболочки.
Ошибка запуска завершает попытку входа; активная игра блокирует запуск браузера.
Остановившаяся служба приложений запускается заново непосредственно при входе:
ожидание ready ограничено 15 секундами, перед открытием снова проверяется игра.
Для уже открытого браузера проверяются desktop ID/StartupWMClass либо Flatpak
application ID и неизменная идентичность процесса. Другие окна не присваиваются.
Обычные desktop sessions сохраняют системное открытие URL.

На Odin и Flip тестовые Fluxer-аккаунты авторизованы через обычный интерфейс
Firefox; foreground браузера и автоматический native return проверены.
После обновления и supervised restart обе сессии TrainerOS сохранились.
Это заменяет прежнее ожидание ручного входа владельца, но не физическую приёмку
Home/Home+X или все маршруты восстановления аккаунта выше.
[Поставка и проверки](../../docs/evidence/friendprofile-20261028/README.md).
