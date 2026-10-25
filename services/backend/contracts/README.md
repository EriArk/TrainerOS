# Backend — проверяемый проект контрактов

**Позднее обновление 25 октября:** общий [серверный каркас](../README.md) уже
реализован и установлен. Этот каталог по-прежнему содержит проект продуктовых
контрактов: его 67 операций не смонтированы как действующий API. Фактический
OpenAPI сервиса содержит только health/capabilities; игровые и account/economy
capabilities остаются пустыми. Описание состояния «нет API-процесса» ниже относится
к моменту подготовки architecture-draft.2, до установки каркаса.

**25 октября 2026, architecture-draft.2.** Это следующий уровень детализации
[архитектуры v1](../../../docs/TRAINEROS_BACKEND_PLAN_RU.md#architecture-v1),
подготовленный до реализации/полировки сценариев. Здесь нет API-процесса,
рабочих migrations, provider login, развёртывания и серверной игры. Ни одна
новая capability не добавлена в текущий SDK. Источник проверки — checkout
`codex/backend-foundation`; предыдущая архитектура — `7cd25aa`.

## Файлы и единый источник

| Артефакт | Содержимое |
| --- | --- |
| [openapi.json](openapi.json) | OpenAPI 3.1.1: маршруты, тела, ответы, доступы, pagination, idempotency и reauth |
| [schemas.json](schemas.json) | JSON Schema 2020-12: типизированные сообщения и 12 вариантов domain events |
| [data-model.json](data-model.json) | 48 логических сущностей с владельцами, колонками, PK/unique/FK и транзакционными инвариантами; **не SQL migration** |
| [modules.json](modules.json) | Все 13 владельцев, операции, издатели/потребители событий |
| [sdk.json](sdk.json) | Предлагаемые методы поверх существующего Platform SDK v1, отзыв контекста, ограничения размера |
| [policy-gates.json](policy-gates.json) | Решения, без которых соответствующая функция не активируется |

Генератор — [build-backend-contracts.py](../../../tools/build-backend-contracts.py).
Редактировать его декларации, затем пересобирать файлы; `--check` обнаруживает
расхождения. Генератор не выполняет сетевых запросов, не открывает БД и ничего
не устанавливает. Сгенерированные JSON лежат в Git для независимого просмотра.

Проверка — [check-backend-contracts.py](../../../tools/check-backend-contracts.py):
официальные форматы через JSON Schema/OpenAPI validators, целостность ссылок,
покрытие владельцев и отрицательные примеры. Зависимости проверки изолированы
от runtime и перечислены в `validation-requirements.txt`; точные версии реально
использованного окружения — `validation-lock.txt`.

```powershell
python -m venv build/backend-contracts-venv
build/backend-contracts-venv/Scripts/python -m pip install -r services/backend/contracts/validation-lock.txt
build/backend-contracts-venv/Scripts/python tools/build-backend-contracts.py --check
build/backend-contracts-venv/Scripts/python tools/check-backend-contracts.py
```

На Linux используется `build/backend-contracts-venv/bin/python`. Контракт не
предъявляет Windows wheel как проверку Linux; используемые Python библиотеки
валидации требуют соответствующих платформе зависимостей. Полного backend
runtime lockfile пока нет: здесь зависимости только средств проверки проекта.

## Общие правила всех модулей

| Область | Зафиксированный контракт |
| --- | --- |
| Идентичность | `player_id` из проверенной сессии; внешний provider subject и local Trainer — отдельные ID |
| Чувствительные команды | Отдельная повторная авторизация, одноразовый grant для actor/session/purpose; operator audience отдельно |
| Запись | `Idempotency-Key` scoped actor+operation и payload hash; конфликт не перезаписывает прежнее действие |
| Секреты | Login/refresh/recovery claim — one-shot; `no-store`; сырые токены/коды не попадают в общий receipt/dedup-кеш |
| Чтение после неопределённого ответа | Owner-scoped operation/result receipt; новый UUID не является безопасным retry |
| Версии | Изменяемые документы требуют expected revision; опубликованные rules/policies immutable |
| События | At-least-once; source mutation + outbox атомарны; consumer effect + receipt атомарны; stale revision не возвращает отменённый результат |
| История после очистки | Минимальный event index и dedup tombstone сохраняют уникальность после удаления тяжёлого event payload |
| БД | Cross-row balance, actor membership и source-kind relation проверяются транзакцией; обычный JSON Schema/row CHECK их не гарантирует |
| Активация | Неопределённая policy → unavailable/policy_unresolved для зависимой функции, не придуманная награда/рейтинг |

## SDK и передача evidence

Существующий SDK имеет JSON-lines frame **64 КиБ**. Для draft выбраны
16 КиБ декодированных bytes на chunk, максимум 64 chunks / 1 МиБ на журнал.
Это **проектные пределы**, а не доказанная достаточность для всех будущих игр.
Игра с большим журналом требует отдельной версии лимитов после измерений.
Generic HTTP metadata body ограничить 64 КиБ; обход через сжатие запрещён
ограничением decompressed body. SDK-страница содержит максимум 20 элементов,
host дополнительно ограничивает сериализованный UTF-8 frame и возвращает cursor.

Путь: `begin` с размером/хешем/форматом → фиксированные chunks → `finalize` →
immutable evidence reference в submission. Все шаги привязаны к player/match;
index/offset нельзя использовать для записи по произвольному filesystem path.
Повтор идентичного chunk безопасен, другие bytes под тем же index — conflict.
Все chunks кроме последнего имеют полный размер. На finalize проверяются
число/порядок, отсутствие дыр, общая длина и SHA256, допустимый формат и expiry.

Base64 maxLength **не** заменяет проверку decoded length: 16384 и 16385 bytes
могут иметь одинаковую длину кодированной строки. Проверка контрактов специально
показывает этот случай. Валидатор сервера обязан декодировать с лимитом, проверить
точную длину и весь журнал. Загрузка части не создаёт verified result.

SDK получает только результат своего активного контекста. В параметры не
включаются actor/player override и refresh tokens. Смена Trainer отзывает
соединение/grants, отменяет callbacks и изолирует старую очередь. Match admission
не выводится из объявления directory bot. Self-declared app name и подпись
установленного пакета не заменяют evidence verification модифицированного клиента.

Схема облачного аватара сохраняет текущий peer recipe SDK v1: `schema`,
`packRevision`, `appearance`, все цвета/пресеты. Серверная revision и будущий
inventory mapping — отдельные поля. В новый контракт не проходят GLB paths,
локальный profile ID и копия модели. Полный редактор остаётся у TrainerOS.

## Переходы и атомарность, необходимые будущей реализации

| Операция | Предусловие | Атомарный результат / запрет |
| --- | --- | --- |
| Login claim | Свежий provider proof + possession challenge + expiry | Consumed attempt и новая session; повтор не выдаёт секрет заново |
| Refresh | Текущий refresh family, не revoked/expired | Старый credential использован, новый создан; replay отзывает family |
| Match start | Host scope, актуальная revision, каждый участник допущен | Freeze roster/rules; нельзя менять состав через поздний admission |
| Submit | Actor/evidence/match совпадают, finalized blob, допустимая policy | Submission + job + receipt; HTTP 202 остаётся pending |
| Verify/void | Текущая job lease/decision revision, исход validator | Append decision + outbox; нет перезаписи истории |
| Competition enter | Окно/политика, актуальный состав | Pending entry; каждый член отдельно согласен; новый состав отменяет старые consents |
| Competition finalize | Все обязательные fixtures и disputes завершены | Новая revision + event; последующая correction видима и компенсирует награды |
| Reward | Поддержанный verified source и rule | Один claim на source/rule/beneficiary; нельзя начислить при повторе |
| Purchase | Явное host confirmation, точная offer revision/price, баланс | Короткая wallet lock; debit+credit+ownership+receipt+outbox одной транзакцией |
| Purchase event | Покупка уже committed | Только обновление read model; Inventory consumer **не выдаёт предмет второй раз** |
| Account deletion | Fresh reauth, опубликованная retention policy | Отзыв сессий и запрет новых операций; затем owner-specific anonymization, без cascade стирания ledger |

`pending`/`disputed` — не результат состязания. Verified event типизированно
исключает pending/unverified payload. Классификация proof хранится отдельно;
server replay не исключает бота/оптимизацию и не объявляется универсальным античитом.
Отмена рекорда должна открыть предыдущий лучший результат, а отмена старого
рейтингового матча — пересчитать затронутую последующую цепочку по прежнему алгоритму.

## Границы формального покрытия

Все 13 модулей имеют операции и данные; внутренние события имеют известных
издателей/потребителей. `policy-gates.json` не блокирует проектирование и не
возвращает глобальную очередь задач: он запрещает включать конкретную функцию
с неопределёнными правилами в будущем runtime.

OpenAPI описывает нормализованный внутренний provider ingress. Raw webhook,
OAuth callback URL/query/error flow, operator IdP и возможности каждого внешнего
провайдера остаются **provider-owned integration contracts**, которые проверяются
перед подключением. Нельзя отправить пользовательский JSON на ingress и считать
его подтверждённой оплатой. Регистрация/доверие adapter audience обязательны.

Публикация rules/board/competition/offer использует заранее проверенные
definition IDs из versioned server configuration. API не принимает произвольный
код/SQL в policy. Схемы содержимого игровых правил, конкурсов и reward formula
принадлежат их владельцам; placeholder `reviewed_policy_document` в логической
модели **не является свободным runtime JSON**. Пока нет конкретной схемы и review,
publish отвергается. Registry авторов и интерфейс редактирования этих документов
не объявляются реализованными операциями.

Схемы/36 примеров доказывают структуру и согласованность проекта. Не проверены
реальная БД/locks/crash recovery, OAuth/провайдеры, смысловые права, экономика,
валидаторы игр, нагрузка, эксплуатация и портативки. SQL migrations, обработчики
команд и background jobs ещё предстоит реализовать по этим контрактам.

## Версии и передача к реализации

`architecture-draft.2` — версия проекта, не release API/SDK. До публикации
контракты меняются явным коммитом с перегенерацией и повторной проверкой.
После публикации замена типа/единицы/обязательного поля, новый смысл события,
изменение authority или понижение trust требуют новой версии контракта.
Текущие схемы строгие: неизвестные поля запрещены. Нельзя назвать добавление
поля совместимым без проверки/переговоров версий у обоих потребителей.
Новые enum values тоже не предполагаются понятными старому клиенту.

SDK capabilities объявляются по фактически реализованным группам; отсутствие
сервиса не заменяется фиктивным ответом. Версии API, SDK, persisted schema,
rules/content и provider adapter независимы. Миграция БД следует expand/contract
и поддерживаемому окну API/worker; совпадение номера API не доказывает совместимость
схемы хранения. Event schema revision, aggregate revision и projection watermark
имеют разные назначения и не подменяют друг друга.

Пакет проекта покрывает все согласованные модули. Дальнейшая работа — общий
Backend-каркас, реальные migrations/транзакции и владельцы команд по этим
контрактам. Закрытие provider/policy gates необходимо при включении конкретной
функции, а не повод снова проектировать весь Backend или начинать полировку гольфа.

Опоры форматов: [OpenAPI 3.1.1](https://spec.openapis.org/oas/v3.1.1.html),
[JSON Schema 2020-12](https://json-schema.org/draft/2020-12/json-schema-validation).
