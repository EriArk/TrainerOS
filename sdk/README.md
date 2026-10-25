# Начать проект для TrainerOS

**Platform API v1 preview, 21 октября 2026.** Общий вход из внешнего процесса в
работающую оболочку. Протокол — JSON через локальное соединение; язык, движок и
обычный способ запуска программы остаются на выбор автора. Crates распространяет
пакет, а этот API подключает его к консоли. Публиковаться в Crates для разработки
не требуется.

## Что уже можно использовать

| Capability | Вход / результат | Выход |
| --- | --- | --- |
| `context: 1` | Текущий локальный profile ID, выбранная игра отдельно от живой game session, состояние процесса/minimize и текущая app activity | Snapshot + события изменений; смена профиля отзывает подключение |
| `account.identity: 1` | `account.get` / `context.account`: состояние входа и серверный Player UUID | Смена аккаунта отзывает старое соединение; credentials доступны только host. [Границы](PROTOCOL.md#backend-identity-preview) |
| `lifecycle: 1` | Событие реального PowerControl `wake`, окончание подключения/остановка host | Закрытие сокета снимает подписки и вклады; автоматического запуска программы нет |
| `ui.start.widget: 1` | Текстовый виджет с одной активацией: title/body → существующий Start → Edit → Add | Нажатие A или touch возвращает `widget.activated`; обновление, скрытие, resize, remove/disconnect |
| `lobby.activity: 1` | Установленная игра объявляет app/protocol в `traineros-activity.json`; Lobby → выбор → Ready → Start запускает её | Клиент подключается после запуска; закрытие игры сохраняет комнату и запись в выборе. Нужен shared record helper; [контракт и миграция](NETWORK.md) |
| `network.records: 1` | Два допущенных участника получают надёжный упорядоченный канал; bounded send/consume/ack | Снятие канала при crash/смене контекста; новая сессия получает новый grant |

Это **реализованная основа в исходниках**, не установленный API на портативках.
Она позволяет писать обычное приложение с необязательной интеграцией, наблюдатель
контекста, ручной счётчик/виджет и прототип игры, реагирующий на смену контекста.
Есть первый [сетевой контракт и пример обмена](NETWORK.md) поверх существующего
Lobby и защищённого транспорта. Полный Network Fabric, Media, Files, GameHub tools,
protected saves, input/capture и полные backend facades ещё не подключены. Отсутствующие сервисы
не выдаются в `capabilities`; вызов вернёт
`unsupported_method`, без фиктивного результата.

## Быстрый реальный пример

Для персонажа консоли есть [локальный shared-avatar API](AVATAR.md) и
[Godot-клиент](godot/avatar_client.gd): игра загружает выбранный внешний GLB,
подписывается на сохранённые изменения и не содержит собственных моделей.
Редактор принадлежит профилю TrainerOS. Эмоции и физика ткани в минимальный
контракт не входят; новая возможность ещё не установлена на портативки.

Собрать и открыть [обычную development-сборку](../docs/DEVELOPMENT.md) с отдельными
тестовыми данными. Для нескольких оболочек задать каждой свой endpoint через
`TRAINEROS_SDK_ENDPOINT`; файл должен находиться в отдельном пользовательском каталоге.
Стандартное расположение клиент находит сам. Host включён в обычной сборке;
автоматические старые smoke-сценарии не публикуют endpoint. В интерфейсе
создать/выбрать локального Trainer перед запуском примера: без профиля доступен
context, но личный widget не регистрируется. В `--ephemeral` профиль не сохраняется.

```sh
./build/native/traineros --windowed --ephemeral
python3 sdk/examples/hello_traineros.py
```

Windows: соответствующие команды — `build/native/traineros.exe` и `py` при
настроенном PATH Qt из инструкции сборки. При собственном endpoint передать его
примеру через `--endpoint PATH` либо тот же environment.

1. Пример подключается к настоящему `PlatformHost` и печатает доступные capabilities
   и контекст. Его `app_id` — `org.example.hello`.
2. В оболочке открыть **Start → Edit → Add → Hello TrainerOS**, завершить Edit.
   Существующие controls занимают первую страницу: новый виджет попадёт на свободную.
3. Нажать A/touch: внешний процесс получает callback и обновляет счётчик в виджете.
4. Закрыть процесс: виджет исчезает. Повторить запуск: сохранённое расположение
   того же виджета восстанавливается; закрытие панели Start не закрывает приложение.

`--context-only` запускает пример без UI-вклада. Отсутствующая TrainerOS означает
отсутствие необязательной интеграции: программа может продолжать собственную работу.
Пример ничего не устанавливает, не запускает игру и не подключается к сети друзей.

## Самый маленький клиент

Python-обёртка в [python/traineros.py](python/traineros.py) использует только
стандартную библиотеку. Она иллюстрирует wire contract; другим языкам она не нужна.

```python
from traineros import Client

with Client("org.myproject.counter") as host:
    host.subscribe()
    if host.supports("ui.start.widget") and host.context["profile"]:
        host.widget("counter", "My counter", "Press A or tap")
    while True:
        event = host.poll(1.0)
        if event and event["event"] in ("context.revoked", "host.stopping"):
            break
        if event and event["event"] == "widget.activated":
            print(event["id"], event["context"])
```

Клиент однопоточный. Для игрового цикла `poll(0)` не блокирует ожиданием события;
синхронный `call` стоит вызывать в подходящем app worker, если GUI не должен ждать.
Пример не обязывает строить игру вокруг Python, Qt или определённого event loop.

## Входы/выходы по проектам Crates

Изучен репозиторий [Crates, c5158f6](https://github.com/EriArk/crates/tree/c5158f6):
competitive mini-games, widgets, apps, extensions, game tools и core proposals.
Это идеи приложений, а не готовые реализации. Их точки подключения различаются:

| Группа / пример | Общее даёт TrainerOS | Автор оставляет у себя | Доступность |
| --- | --- | --- | --- |
| Обычные apps: Pixelpad, Comic Shelf | Apps launch/Home и необязательный SDK context | UI/документы/форматы и обычные файлы приложения | Обычный запуск существует; context API в этой поставке |
| Quick Notes, Session Timer, Controller Monitor | Start host/widget input + нужный service capability | Текст, ручные команды и собственное состояние | Текст/активация готовы; редактор текста, metrics/devices API ещё отдельные функции |
| Tanks/Pong и другие 24 mini-games | Один Lobby, допуск/участники, транспорт и Social | Правила, симуляция, синхронизация, match result | Admission + 2-player reliable records bridge реализован; полноценный realtime/результаты и сами игры ещё отсутствуют |
| Music Radio / Audio Profiles | Media Session, system mixer, Start вклад | Источники музыки/auth, playlist или presets | Widget готов в ограниченном виде; media/mixer API не готов |
| Speedrun Timer / Controller Overlay | Selected/live identity, GameHub вклад/ограниченный overlay | Manual timer/splits/свой UI | Context готов; GameHub/overlay/input hooks не подключены |
| Save Journal | Защищённый owner/save backup/read/restore service | Представление существующих backup records | SDK доступа к save ещё нет; нельзя обходить его чтением чужих путей |
| TATE/Dock/Touch Mapper/shaders/Translator | Реальные device/input/display/capture capabilities | Настройки/алгоритм конкретного дополнения | Не выводить поддержку из регистрации виджета; privileged hooks ещё открыты |
| Arcade Today, tournaments, clans, coins | Identity/results/economy сервисы консоли | Поддержанный формат события/правила игры | Target; обычный self-reported результат пока не официальная победа/награда |

Core proposals из Crates отдельно закрепляют performance profiles, gyro bridge и
challenge cards за TrainerOS. Они не должны превращаться в обязательные собственные
подсистемы каждой игры или самостоятельные Crates. Их контракты ещё требуют работы.

## Пакет, запуск и собственные данные

Для самостоятельной программы нет обязательного нового manifest. Стабильные
`app`/`widget id` дают namespacing; они **не удостоверяют издателя** и не дают права
на чужие данные. Один процесс может быть игрой и иметь widget; роли не вводят новых
корневых вкладок. Одновременный второй connection с тем же app ID отвергается.

Обычный local/dev запуск использует этот же API. Для публикации использовать
[существующие Crates recipes/build/installer](../packaging/crates/README.md) и
[путь автора F09](../docs/STRUCTURE_MAP_RU.md#f09), не новый installer. `app_id`
следует сохранять между версиями; реальная связь с подписанным package identity
потребуется перед привилегированными API. Автозапуск внешних расширений, installation
registry и полноценный module SPI #231 этой основой ещё не реализованы.

Собственные документы/настройки остаются приложению. Если они различаются по
локальным Trainer, их хранение нужно отделять по полученному profile ID, не по
имени человека. После `context.revoked` завершить старую работу и подключаться
повторно только для намеренно выбранного контекста. Секрет endpoint нельзя включать
в пакет, журнал, репозиторий или пересылать другому игроку.

## Границы версии и проверки

[Wire contract](PROTOCOL.md) фиксирует точные сообщения, ошибки и lifecycle.
Это preview: совместимые расширения объявляются capabilities; несовместимый wire
version требует отдельной версии, не молчаливой смены значения существующего поля.
Наличие UI-вклада не предоставляет game input, filesystem root, voice/capture,
экономику или semantic-save permissions.

Проверки и актуальное состояние поставки записаны в
[PLATFORM_SDK](../docs/PLATFORM_SDK.md). Приёмка на ARM/портативках остаётся отдельной;
подключения к ним в этой работе не выполнялись.
