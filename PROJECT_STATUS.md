# TrainerOS — состояние проекта

Обновлено **18 октября 2026**. Это накопительная текущая картина для публикаций,
не новая проверка устройств. История — [CHANGELOG](CHANGELOG.md), работа сейчас —
[CURRENT_TASKS](docs/CURRENT_TASKS.md), требования — [PRODUCT_SPEC](docs/PRODUCT_SPEC.md).

## Подтверждённая поставка и текущие исходники

Последняя подтверждённая установка на Flip 2 и Odin 2 — **Home/Start-79**,
исходники [`1b3846e`](https://github.com/EriArk/TrainerOS/commit/1b3846e).
Владелец положительно оценил вид, физическое управление/звук остаются в приёмке.
Последующие LAUNCH-80 изменения
[`a09cef0`](https://github.com/EriArk/TrainerOS/commit/a09cef0) добавляют маршруты
Cemu/DuckStation в исходниках; их установка/игровой прогон ещё не подтверждены.
Документационная чистка убрала 82 устаревших/пустых файла и около 45% строк
root/docs. Ссылки и README-изображения проверены; приложение и устройства не менялись.

## Что уже есть

| Область | Доставленный результат и доказательства | Что не следует из доставки |
| --- | --- | --- |
| Сессия и каркас | Родная Qt/Armada оболочка на Flip/Odin; Home, Games, GameHub, Social, Apps; сохранение места, общий controller/touch, профили и first run. [Сессия](docs/SESSION_PROTOTYPE.md), [first run](docs/FIRST_RUN.md) | Универсальная установка/все модели, весь TV/controller scope ещё не приняты |
| Home и навигация | Адаптивная карточка, друзья/уведомления/Lobby, профиль, Recent/★ и Quick Apps; объёмный корпус и масштаб. [Контракт](docs/PRODUCT_SPEC.md), [масштаб](docs/DISPLAY_SCALING.md) | Тестовые данные не являются реальным прогрессом; окончательная приёмка отдельных экранов остаётся |
| Games | Коллекции/платформы, колесо/список/полки/grid, метаданные и медиа, избранное/ручные коллекции; общий прямой запуск. [Библиотека](docs/BATOCERA_LIBRARY.md), [каталог запуска](docs/LAUNCH80_PLATFORM_REGISTER.md) | Наличие core не подтверждает все игры/форматы/скорость |
| GameHub | My Story по библиотеке, таймлайн, галерея с плёнкой/100-фото буфером и отдельным избранным; RA-выбор игр/история и карточка достижения. [RA73](docs/RA73_GAME_SELECTION.md), [RA56](docs/RA56_ACHIEVEMENT_LOOP.md) | Личная RA-history ждёт Web API key; баннер/джингл/физические кнопки на финальной полировке |
| Home/Start | Home — текущая игра/её настройки, лобби/голос/уведомления; Start — виджеты/quick actions и Settings/Power/Switch. [78/79](docs/CONTROLS78_HOME_START.md) | Не новый уровень совместимости настроек всех эмуляторов; физическая приёмка открыта |
| Social | Люди → DM, группы/комьюнити отдельно, Discover; удаление друзей, выход/удаление по правам Fluxer, скрытие служебных групп. [Social](docs/SOCIAL.md), [lifecycle](docs/SOCIAL66_LIFECYCLE_CONTROLS.md), [Discover](docs/SOCIAL67_DISCOVER.md) | Live public discovery, destructive MFA flow и новый Recently Together требуют отдельных проверок |
| Lobby | Каталог 2×3, My Lobbies/расписание, wizard/Game Picker, голосование/Ready, ручной Start хозяина, общий launch splash, HUD/тёмный mini-chat. [Контракт](docs/LOBBY.md), [подготовка](docs/LOBBY63_PREPARATION.md), [HUD](docs/LOBBY64_ENTRIES_HUD.md) | Все runtime/разные сети не закрыты; offline Lobby и Watch/переход роли отложены |
| Apps/Files/Browser | Приложения/избранное/скрытие; Files libfm-qt с breadcrumb/нижним управлением; QtWebEngine, вкладки, bookmark/history popovers, fullscreen и общие загрузки. [Files](docs/FILES68_IMPLEMENTATION.md), [Browser](docs/BROWSER71_IMPLEMENTATION.md) | Не новая универсальная сертификация файловых backend/сайтов/устройств |
| Crates | Тематические ящики, каталог/проект, установка/запуск временных open-source примеров, подписанные обновления каталога. [76](docs/CRATES76_INSTALLER.md), [77](docs/CRATES77_CATALOGUE.md) | Физическая совместимость пакетов вне проверок, полный publishing/reviews ещё не готовы; SPI полностью отложен |
| Отзывчивость | PERF-72 доставлен обоим; исправлены лишние фоновые работы и use-after-free, измерены CPU/PSS на одинаковом маршруте. [Методика](docs/PERFORMANCE72.md) | Ограниченный замер не заменяет длительную приёмку; старые общие UI-fixtures не объявлены зелёными |
| Библиотечные медиа | ScreenScraper, пользовательские материалы и общая очередь Downloads. [ScreenScraper](docs/SCREENSCRAPER.md) | Artwork/redistribution не очищаются наличием scraper |
| Профили и сейвы | Локальные Trainers/PIN, собственная история, обычные save routes/backup, контролируемый Exit и capture. [Trainer saves](docs/TRAINER_SAVES.md), [exit](docs/ADVENTURE_EXIT.md) | Все runtime/миграции и переносимый полный экспорт ещё не приняты |
| Игровые адаптеры | Отдельный Pokemon experience и generic fallback; exact-build progress/read/protected-write операции, Link и журнал восстановления. [Knowledge base](docs/adapters/README.md), [Link](docs/EMERALD_LINK.md) | Не универсальный parser и не поддержка новых франшиз по одному descriptor; внешние пакеты #92 отдельно |

## Запуск и мультиплеер

Полный каталог: **106 ID, 104 в работе; только PS3/Switch отложены**.
LAUNCH-80 исследует готовые лаунчеры общим пакетом и проверяет игры от старых к новым.
Настоящие Cemu/ARMSX2 Libretro на устройствах сохранены; автоматическая миграция
их сейвов не разрешена предположением об «ошибочной привязке».
[Реестр](docs/LAUNCH80_PLATFORM_REGISTER.md), [исследование](docs/LAUNCH80_LAUNCHER_AUDIT.md).

Классические RetroArch, GB/GBC/GBA link, DS, PSP, Dolphin, RMG/N64, Flycast, Azahar и
один Vita homebrew маршрут имеют разные по глубине парные доказательства.
Это не «весь мультиплеер готов»: PS2, Wii U, большие группы/разные сети и
широкая совместимость остаются поимёнными пробелами в
[матрице](docs/EMULATOR_MULTIPLAYER_MATRIX.md) и [записях runtime](docs/emulators/README.md).
Обычный PSP принят как достаточный; private PSP и полный handoff-каталог отложены.

Remote Play использует P2P отдельного игрового медиапотока; Fluxer spectator stream
его не заменяет. Многогостевой кандидат не принят: Iris/IDR и стабильность Gamescope
остаются зависимостями. [Транспорт](docs/REMOTE_PLAY_TRANSPORT.md),
[кандидат](docs/REMOTE_PLAY_GROUPS.md). Собственный тяжёлый relay не добавляется.

Новый обязательный [стандарт эмуляторов](docs/EMULATOR_STANDARD.md) требует
фиксированных runtime/core/build/protocol/sync settings и проверки peers до старта.
Это усиление цели от [`c6aaac2`](https://github.com/EriArk/TrainerOS/commit/c6aaac2),
не доказательство, что единый admission уже завершён во всех маршрутах.

## Выпуск и границы

Первая ранняя бета — **установщик поверх Armada** с определением ОС на microSD /
внутренней памяти, отзывчивым прогрессом, минимальными действиями и восстановлением.
Публичная бета не выпущена; подготовленные портативки не доказывают чистую установку.
[Контракт установщика](docs/EARLY_BETA_INSTALLER_RU.md).

Сохраняются GPL-3.0-or-later, third-party licenses, открытые #115/116/117,
защита личного контента и отдельная юридическая проверка соглашения. Образ,
SteamOS/Bazzite, Pack Studio, Help и полный product backlog не становятся
автоматически блокерами первой беты. README-снимки пока исторические и сохранены.

Следующая работа — проверить production-сборку LAUNCH-80 на Flip и продолжить
реальный запуск по реестру. Открытая приёмка и
отложенные пункты собраны в [CURRENT_TASKS](docs/CURRENT_TASKS.md).
