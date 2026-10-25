# Accounts: вход через Fluxer без второго логина

25 октября 2026. Владелец подтвердил: постоянный аккаунт TrainerOS привязан к
Fluxer; отдельного логина/пароля TrainerOS нет. Для OAuth создаётся отдельное
приложение **TrainerOS**, а существующее **TrainerOS Lobby Directory** остаётся
без изменений. Новое приложение зарегистрировано владельцем **EriArk** через
его постоянный аккаунт. Отдельный HTTPS origin — `https://traineros-api.abysstail.art`.
Получение client secret ожидает подтверждения владельцем в окне Fluxer;
provider login до установки конфигурации остаётся выключен.

**Уточнение пользовательского сценария:** создание приложения и настройка HTTPS —
разовая работа оператора. Игрок не создаёт OAuth-приложение и не настраивает сервер.
Принят обычный путь «Войти через Fluxer → подтвердить доступ → вернуться в TrainerOS»
с сохранением входа в оболочке. Обязательный ручной код ниже — промежуточный
серверный механизм, а не принятый игровой UX. Безопасный native return и хранение
сессии оболочкой ещё нужно реализовать; просто убрать проверку привязки нельзя.

## Реализованный серверный путь

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

Далее в командах управления API сохранять оба Compose-файла. Базовый запуск без
overlay намеренно отключает provider login (`provider_not_configured`, 503), а
capabilities остаётся пустым. Включение конфигурации объявляет только
`accounts.fluxer_login`/`accounts.sessions`; оно само по себе не означает приёмку
Fluxer, shell/SDK или портативок. Ключ обязателен в зашифрованной эксплуатационной
копии секретов; его смена завершает незаконченные попытки входа, но не меняет
существующие хешированные access/refresh credentials.

## Проверки и оставшиеся условия

Тесты выполняются на сервере с отдельной PostgreSQL, без service volume и
внешней сети. OAuth upstream заменён контролируемым test provider/HTTP fixture.
Проверяются cookies/state/PKCE/claim, конкурентная выдача, стабильность identity,
изоляция устройств/игроков, rotation/replay/revoke/expiry/suspension, отсутствие
секретов в БД/ошибках и границы HTTP adapter. Это не live OAuth round trip.

Остаются: подтверждение получения client secret в Fluxer, сохранение callback,
включение provider, реальный consent/cancel/relogin, безопасный native return без
обязательного ручного кода, интеграция в оболочку с её текущим Fluxer account и
локальным Trainer, защищённое хранение/очереди/отзыв SDK grants, recovery/смена
привязки/удаление аккаунта и очистка auth-записей по принятой retention policy.
Истечение сейчас запрещает использование записей, но не удаляет их автоматически.
Никакие покупки, результаты или косметика не привязываются по имени Fluxer.
Обычные локальные игры не требуют Backend-входа.
