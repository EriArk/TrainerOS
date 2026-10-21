# Архитектура TrainerOS

[Карта структуры](STRUCTURE_MAP_RU.md) определяет владельцев, связи и жизненные
циклы конечного продукта; ниже — существующие технические механизмы. Изменение
кода сверять с соответствующим S-узлом и затронутыми E-контрактами.

## Слои

C++20 / Qt Quick, локальное состояние в SQLite за repository/service интерфейсами.
Linux/Armada, процессы, сеть, питание, хранение и ввод отделены платформенными
службами. QML представляет состояние и вызывает семантические действия, не
исполняет команды эмуляторов и не получает секреты.

- Shell: пять стабильных маршрутов Home/Games/GameHub/Social/Apps, общий фокус,
  клавиатура, панели X/Y, Home/Start, уведомления и восстановление навигации.
- Features: библиотека/история/профили, Social/Lobby, Apps/Files/Browser/Crates.
  Общие аккаунты, загрузки, процессы и хранилища переиспользуются.
- Runtime adapters: подготовка запуска, процесс/input, конфигурация и обычные
  сохранения. Обнаруженная установка не равна разрешённому сетевому профилю.
- Game experience adapters: семантические данные, собственные представления и
  действия по подтверждённым возможностям точной игры/ревизии.

Исходные границы: [src](../src), [features](../src/features),
[integrations](../src/integrations), [QML](../src/qml),
[experience](../src/core/experience), [adapters](../src/adapters).

## Experience boundary

Descriptor v2 содержит Home view и `adapter.screens`; `show(screenId)` открывает
именованный экран в адаптерной части GameHub. Два обязательных основных слота
старого дизайна отменены. Нет адаптера — работает универсальная карточка и история.
Все game-specific presentation/actions/navigation/orchestration принадлежат
`src/adapters/<id>/`; общий shell меняется только в явной composition registration.

Manifest допускает независимые альтернативы совместимости: provider/local IDs,
hash/internal code, проверенные наблюдения. Внутри альтернативы нужны все условия;
не использовать имя файла или порядок регистрации. Presentation match не даёт
доступа к семантической записи сейва. Asset packs и entity mapping принадлежат
адаптеру, пользовательские материалы отдельно от обновляемого кода. Внешние
скачиваемые пакеты/доверие/installability — отдельный #92, не следствие built-ins.
Подробности и exact-build доказательства: [реестр](adapters/README.md),
[UX framework](UX_FRAMEWORK.md), [adapter audit](ADAPTER_AUDIT.md).

## Состояние и жизненный цикл

Выбранная игра, владелец Trainer, запущенный process/session, foreground/input
owner и captured frame — отдельные сущности. Асинхронное действие проверяет
исходный target/generation повторно. Переключение Home не подменяет текущую игру.
Minimize возвращает shell без потери процесса/guard; pause зависит от runtime.
Explicit Exit — единственный обычный путь fresh clean capture → подтверждение
сохранения при необходимости → graceful exit. Cancel возвращает прежний процесс.
Краш/потеря процесса не создаёт «успешное сохранение».

Обычные файлы прогресса принадлежат игре/пользователю, metadata им не замена.
Защищённые изменения требуют backup, проверки exact build, атомарного commit,
readback и durable recovery; активная игра исключает запись. История захвата не
может включать private overlay. [Exit](ADVENTURE_EXIT.md),
[capture](GAMESCOPE_CAPTURE_LIFECYCLE.md), [persistence](LOCAL_PERSISTENCE.md),
[save lineage](SAVE_LINEAGE.md) и [backups](SAVE_BACKUPS.md) сохраняют детали.

## Сеть и приложения

Fluxer worker владеет REST/Gateway и защищёнными credential jobs; UI получает
представления и действия с provider-проверками. Чат/членство/голос не дублируются
локальным мессенджером. Lobby координирует выбор, ready, admission и запуск;
созданные им группы имеют устойчивую служебную идентичность и скрыты от Social.
Spectator streaming Fluxer и низколатентный Remote Play P2P — разные пути.
Эмуляторы используют готовый netplay/native online; общее ядро TrainerOS владеет
приглашением, процессом, input, Home/выходом и сохранностью, не новой эмуляцией.
[Стандарт](EMULATOR_STANDARD.md), [матрица](EMULATOR_MULTIPLAYER_MATRIX.md).

Files использует libfm-qt/GIO. Browser — QtWebEngine, lifetime вкладок/загрузок
независим от видимости страницы. Crates применяет подписанные данные каталога и
общую панель Downloads. Провайдеры публикуют задания и исполняют направленные им
команды; очереди/отмена/хранение принадлежат конкретному исполнителю. Общая панель
не требует отдельного общего планировщика. Внешние SPI/SDK остаются незавершёнными контрактами
узла S14; built-in Experience API не объявляется готовым публичным SDK.
Компоненты ОС, TrainerOS и проектов имеют разные владельцы обновления, пользовательские
данные не часть заменяемого пакета. Первая бета — installer поверх Armada;
единый образ не является текущей обязательной архитектурой доставки.

Текущие экраны описаны в [PRODUCT_SPEC](PRODUCT_SPEC.md); стадии реализации —
в [PROJECT_STATUS](../PROJECT_STATUS.md). Исторический прототип не задаёт очередь.
