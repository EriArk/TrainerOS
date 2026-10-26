# First-run setup

## Активное расширение — 24 октября 2026

Владелец начал работу над визардом, установщиком **поверх Armada** и OTA
TrainerOS. Портативки заняты отдельным ремонтом мультиплеера: в этом проходе
не подключаться, не устанавливать сборки и не менять их состояние.
Разработка ведётся отдельно в ветке `codex/first-run-installer-ota` от `761046b`.

### Текущая реализация — вход по месту использования

Последнее решение владельца 24 октября **отменяет обязательность аккаунтов в
визарде**. Welcome → Controls → Connection → Time → Games/storage → Trainer →
**Comfort → Ready → Home**. Весь путь завершается офлайн.

- Шаг Accounts удалён. Social предлагает Sign in с объяснением общения, звонков
  и приглашений; ScreenScraper в Settings — вход для обложек и сведений об играх;
  RetroAchievements на своём экране — вход для достижений поддерживаемых игр.
  Поставщики авторизации и область хранения аккаунтов не меняются.
- Comfort использует общие настройки экрана, звука, тихих видеопревью,
  коммуникации и nearby visibility. Любые из них можно оставить как есть.
- Сохранённый старый `accounts` переходит в Comfort, если Trainer уже создан,
  иначе — в создание Trainer с сохранённым черновиком. Путь библиотеки и профиль
  сохраняются. Возврат из Comfort к хранилищу не создаёт второй профиль.
- Незавершённый Ready тоже завершается без аккаунтов. Существующие completed
  установки не возвращаются в настройку. Формат `first-run.json` остаётся version 2:
  структура данных не меняется, version 1 по-прежнему читается.
- Перед личными настройками сохраняется обычный выбор Trainer/PIN. PIN и пароли
  не записываются в progress. Проверка направлений/Confirm/Back имеет отдельный
  touch-путь, который не выдаёт физические кнопки за проверенные.
- Сеть необязательна. Подтверждение ScreenScraper остаётся свойством его сервиса,
  но больше не управляет завершением визарда. Вход сам по себе не доказывает
  работоспособность каждого игрового транспорта.

Текущая локальная проверка: 17 QtTest checks (включая init/cleanup) и настоящий
QML-путь до Home с SDL/pointer прошли на Windows/UCRT64 Qt 6.11.1. Проверены
продолжение старого Accounts, отсутствие дубликата Trainer, офлайн-Ready,
PIN при восстановлении и переходы общих настроек. QML-проход занял 11.31 s;
новые кадры Comfort 960×540 и Ready 1920×1080 сохраняются в локальном
`build/native/screenshots/first-run/`. Это не онлайн-авторизация или аппаратная проверка.

### Принятый состав и оставшиеся зависимости

| Область | Состав / состояние |
| --- | --- |
| Язык | Выбор реальных переводов; общая локализация ещё отсутствует. Сейчас интерфейс английский. |
| Управление | Controller/touch, корректные glyphs и определение модели. Touch-путь добавлен; полный #77 остаётся незавершённым. |
| Сеть/время | Wi-Fi/Bluetooth, завершение без сети, часовой пояс/NTP/ручное время через общие сервисы. |
| Trainer | Имя/эмблема, существующий профиль, необязательный PIN/семейный код. Полный перенос backup/restore #73 отдельно от PIN recovery. |
| Библиотека | Существующие игры, внутренняя память/карта, место/папки/сканирование; стабильная идентичность съёмного носителя и сводка runtime/BIOS ещё требуют доводки. |
| Fluxer | Вход на Social через действующий handoff; друзья/общение/приглашения. Не условие настройки. |
| RA | Логин/пароль; необязательный history Web API key внутри сервиса. Вход не доказывает earning на всех runtime. |
| ScreenScraper | Вход в Settings по желанию пользователя, язык/регион, выбор и показ медиа. Developer credentials не запрашиваются у обычного игрока; их поставка в публичном пакете ещё требует решения. |
| TrainerOS Account | Собственный постоянный ID через подтверждённую Fluxer-привязку, без нового пароля. Backend/recovery ещё не реализованы; не показывать ложный успешный вход. |
| Steam | Возможный переход в штатный вход и обратно; не копировать форму пароля. Полный маршрут пока не включён. |
| Patreon/Ko-fi | Необязательные серверно проверенные привязки после готовности Backend; не обязательный шаг визарда. |
| Crates/GitHub | Публичные пакеты без обязательного отдельного GitHub login. |
| Экран/звук/общение | Общие настройки выше; микрофон передаёт звук только по отдельному действию/согласию. |
| OTA | Settings → System → Updates; signed check/download и managed apply/rollback. Production source и аппаратная приёмка ещё открыты. |
| Завершение | Ready → Home; Games для локальной библиотеки, Apps → Crates для добровольных загрузок. Расширенная сводка готовности платформ ещё нужна. |

Зависимости именно этой работы: законченный first-run consumer общих сервисов;
общий состав/versioned package и ownership для installer/OTA; установка/repair
на Armada; затем trusted download/activation/совместимый rollback. Это не новая
глобальная очередь проекта. ОС Armada, Software TrainerOS и игры Crates имеют
раздельных владельцев обновлений. [Контракт установки](EARLY_BETA_INSTALLER_RU.md).

### Исторические проверки обязательных аккаунтов — 24 октября 2026

Обязательность отменена более поздним решением выше. Следующие результаты
относятся к прежней реализации и не являются текущим поведением.

- Windows/UCRT64 Qt 6.11.1 Debug, `BUILD_TESTING=ON`: приложение и затронутые
  тесты собраны. `first_run_qml_smoke` прошёл без QML warnings: недоступный
  Continue без аккаунтов, fixture-подтверждение сервисов, SDL-прокрутка к
  Continue и касание, Comfort → Ready → Home. Кадры Accounts 960×540 и Ready
  1920×1080 просмотрены; локальный `screenshots/first-run/verification.txt` — PASSED.
- `trainer_first_run_tests`: 16 passed; требование обоих аккаунтов, недопустимость
  восстановления подтверждения из progress, потеря Fluxer перед Ready,
  восстановление через PIN и реальная запись профиля проверяются локально.
- `ScreenScraperTests`: 27 passed, 1 прежний fail на каталожном `xbox` в
  `allCataloguePlatformsHaveAnExplicitServiceBoundary`. Новый тест проверяет
  отсутствие подтверждения из сохранённых паролей, пустой/гостевой/чужой ответ,
  верный username, смену пароля, отказ API, повторное configure и sign out.
  Он прошёл; полный набор не объявляется зелёным.
- Release preparation: 19 тестов плюс запуск через CTest прошли;
  [что именно проверено](EARLY_BETA_INSTALLER_RU.md).
- Реальные онлайн-входы, ARM64 и портативки не проверялись. Production build
  без UI fixtures проверен в предыдущем проходе ниже; текущий testing guard
  в `Main.cpp` не менялся. Проверки не подменяют установку на Armada.

### Проверки расширения до уточнения обязательности аккаунтов

- Windows/UCRT64, Qt 6.11.1: сборки с `BUILD_TESTING=ON` и `OFF` прошли.
  Production CLI запускается; тестовая опция `--first-run-smoke-test` в нём
  отсутствует. Это x86_64 host proof, не сборка/поставка на ARM64.
- `first_run_qml_smoke` прошёл: настоящий QML, виртуальный SDL-контроллер и
  pointer-события, новый Trainer, офлайн-путь, клавиатура RA, возврат из сервисов,
  сохранение настройки, прокрутка до Continue и выход в Home. Кадры 960×540 и
  1920×1080 просмотрены; отчёт и кадры находятся в локальном
  `build/native/screenshots/first-run/`. Исправлено удаление контейнера сервисов
  во время создания строк QML. Это fixture-проверка, не реальные онлайн-входы.
- `storage`, `trainer_access`, `trainer_profiles`, `clock` прошли. Пять целевых
  сценариев `InteractionTests` (плотность/настройки экрана, scraping вместе с
  настройками, ввод сообщений и независимый язык клавиатуры) прошли.
- `trainer_first_run_tests`: 15 passed, 0 failed, включая закрытие/повторное
  открытие настоящей базы и PIN перед личными шагами, отсутствие дублирующего
  Trainer, legacy-маркеры и отказ записи. Nearby toggle теперь отражает своё
  состояние и в визарде, и в обычных Settings, независимо от reduced motion;
  это дополнительно проверено UI-сценарием.
- Расширенный набор не полностью зелёный: `ScreenScraperTests` — 26 passed,
  один fail на каталожном `xbox`; `OwnershipTests` — 5 passed, один fail на
  прежнем ожидании schema 14 при фактической 16; общий `InteractionTests`
  падает в `multiverseFilteringAndEmptyProduction`. В `systems()` старого
  `MultiversePresentation(false)` есть обращение к `repository_->editable()`
  при null repository. Эти файлы и соответствующие тесты не менялись относительно
  базового `761046b`; исправление их отдельных требований в этот проход не входит.

Онлайн-авторизация реальных аккаунтов, ARM64/портативки, аппаратный ввод, звук и
чистая Armada в этот проход не проверяются. Исторические результаты ниже не
переносятся на новую сборку.

## Историческая реализация и проверки — 28 сентября 2026

Ниже сохранены первоначальная поставка и её ограничения. Новый состав, version 2
и расширенные сервисы описаны выше; прежний image-first план заменён установкой
поверх Armada для первой беты.

The native development build now has a device setup flow before first Trainer
creation. It is a building block for the Armada-based image, not delivery of
the image, bundled emulators or their updater.

## User path

Welcome → directions/A/B check → optional Connections → date/time → library storage →
Trainer card → optional Trainer PIN/family reset code → Home.

Connections reuses the existing Wi-Fi/Bluetooth pane and masked controller
keyboard. B returns to the setup choice; a connection operation or question
retains its own cancellation behavior first. Continuing offline is supported.
Physical navigation remains trapped in setup until a real Trainer is open.

[Date and time](DATE_TIME.md) shows the actual system clock and initially focuses
Continue. Automatic synchronization, time-zone selection and optional manual
date/time use the same controls as Settings; this step is also resumable.

Storage offers the existing library, internal storage and mounted removable
locations. It recognizes `Emulation/roms`, `roms` and `batocera/roms`, preserves
existing aliases, and creates missing platform directories from the shared
ARM64 registry. It never formats a disk, moves games or rewrites gamelists.
Folder preparation runs off the UI thread, checks the selected mounted device
again and reports write failures without advancing. A disconnected card must
not be replaced by an empty directory on the underlying internal filesystem.

The Trainer card uses the existing real registration transaction. At completion,
optional PIN setup reuses the existing family-code and Trainer-access controls;
a missing family code can be created inline before the personal PIN. Accounts,
including RetroAchievements, remain available later in Settings.

## Persistence and existing installations

`first-run.json` in the application data directory stores version 1, the current
step, selected ROM root and a nonsecret Trainer-card draft. Writes use atomic
replacement. Passwords and PIN drafts are never written there. Corrupt/unreadable
progress stays intact and produces Retry, rather than silently restarting setup.
Completed profile creation is detected after a restart so the wizard does not
create a duplicate Trainer. Finishing removes the temporary card draft.

Existing installations with Trainers and no setup marker retain their current
entry behavior. They are not forced through setup by an upgrade. A completed
new installation enters Home after normal profile/PIN entry on subsequent boots;
personal Adventure selection and browsing records remain stored. Removing the
last Trainer retains device setup and uses the existing Trainer creation gate.

The persisted ROM root feeds the folder scanner and file-picker starting point.
Settings → Library → Game storage reuses the mounted-location service and card
list. A selects and prepares a location; B returns to Library. The committed
choice updates the scanner and file-picker root and starts a scan immediately.
It does not move/delete existing ROMs, saves, installations or gamelists. The
previously registered library keeps its identities/history. Busy preparation
holds navigation and session exit until completion; errors retain the old root.

`library-location.json` stores the selected device-wide path with atomic writes.
It takes precedence over the original setup marker's root, allowing older setup
records to remain readable. Preparation reuses mounted-device checks; the GUI
thread checks scanner/database activity before committing and changing the root.
An explicit `--roms-dir` remains a development override. No SQLite migration,
emulator configuration replacement or session-default change is involved.

## Presentation refinement — 2026-09-28

The setup uses a compact six-stop route across the top, an original illustrated
handheld/travel kit on the left and a focused task area on the right. Controls
light up on the handheld drawing when checked. Mounted storage cards show media
type, real free space, capacity use and the current location; the selected path
stays in a separate detail area. Normal controller legends remain in the chassis
footer. The original illustration uses cached Canvas geometry and no ambient
paint timer, downloaded artwork or extra image pack.

Composition references: [Steam Gaming Mode setup in Bazzite's documentation](https://docs.bazzite.gg/General/Installation_Guide/post-installation/)
and [Nintendo's first-time setup guide](https://www.nintendo.com/en-gb/Support/Nintendo-Switch/Nintendo-Switch-First-Time-Setup-and-Connection-1295677.html).
These inform task hierarchy/controller simplicity, not copied artwork or a
replacement for the established TrainerOS chassis.

## Verification — 2026-09-28

- Native build and all 45 CTest targets passed. After the final registration-row
  correction, the affected first-run and Trainer-profile tests passed again.
- Flip ARM64 build installed with SHA-256
  `bb8c9a6cebf2bf8f6b57b4925c0a55cc20730992f25dda7077a6723e76d9f808`.
- On Flip, isolated application data exercised real controller events through
  welcome, controls, the live Connections pane, offline continuation, storage,
  controller text entry, real Trainer creation and Home. Setup and the card draft
  resumed after process restarts. Family-code creation followed by personal PIN
  creation worked; the next launch required that PIN.
- Actual Gamescope captures verified welcome, controls, storage and completion.
  Tests used a separate library/data directory on the configured device; they
  did not reset the owner's installation or test a fresh OS image.
- Production replacement preserved all 3 Trainers and 830 Adventure records,
  passed SQLite integrity checks, restarted normally and did not create a setup
  marker for existing owners. Binary/database backups were retained privately.
- Odin was unreachable over SSH and was not updated. New-network authentication,
  physical card removal and emulator/image update retention were not exercised.

### Refinement and Settings storage verification

- Native and ARM64 builds passed. Eight affected CTest targets passed: first-run,
  Batocera discovery, core, interactions, storage, Trainer profiles and the general
  and Worlds QML smoke scenarios. Final presentation-only refinements were rebuilt
  and inspected on the handheld.
- Real Flip controller events completed the revised wizard in an isolated data
  directory, then opened Settings → Library → Game storage. Selecting internal
  storage prepared the folders and applied the path. A restart without the
  development ROM-root override retained that choice in Settings. The owner's
  production library location was not switched by this test.
- Final Flip binary SHA-256:
  `c75ee950afdcdc6b0c3234ba960fe749fac3b5a9ba21bf3410ab7db9ff788d0e`.
  Replacement preserved 3 Trainers / 830 Adventure records and SQLite integrity;
  private binary/database recovery copies were retained. Odin remained unreachable.

## Remaining first-run acceptance

- Real translations/language selection. Date/time controls are delivered separately above.
- Optional account setup within the wizard; active-Trainer RA is still Settings.
- Nonstandard folder browsing and stable removable-device identity across
  changed mount paths. Mounted-location selection in Settings is implemented.
- General scanner classification/idempotence fixes identified in the startup
  audit; broad emulator readiness/repair and reproducible image installation.
- Full fresh-image → copied ROM → play/save/return and emulator-update retention
  acceptance. A clean app data directory on an already configured handheld does
  not prove these image-level requirements.

See [the full journey](STARTUP_EXPERIENCE_AUDIT.md) and [execution order](ROADMAP.md).
