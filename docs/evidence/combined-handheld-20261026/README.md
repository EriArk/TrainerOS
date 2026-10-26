# Общая сборка и доставка — 26 октября 2026

Запрос владельца: обновить Odin 2 и Flip 2 с учётом работы других чатов,
проверить не только main. Сведение выполнено в отдельном checkout, исходные
рабочие каталоги и незакоммиченные изменения сохранены.

## Состав

- Core main `20e1aedc` + multiplayer/SDK/Crates `1eb204ce`: merge `dc428af3`.
- First-run/installer/OTA `ba04c44a`: merge `9cf36771`.
- Backend foundation `1e6cc6ca`: merge `418d5b83`.
- После fetch все существующие локальные/remote ветки TrainerOS являются
  предками интеграционной ветки. Это не означает доставку каждого незавершённого
  прототипа: старый незакоммиченный PC-LAN/InputOverlay WIP из TrainerOs-apps,
  исторический detached checkout и исследовательские kernel scripts сохранены
  отдельно. Их замена актуальными путями не выполнялась вслепую.
- 16 установленных Crates-пакетов сверены с отдельными worktrees Crates.
  Golf 0.1.23, Artillery 0.1.4 и восемь arcade 0.1.3 уже актуальны; они и
  установленные native emulator packages сохранены, а не заменены старым main.
- Ни backend server, ни kernel/Armada, ни boot/suspend policy этот проход не меняет.

## Исправления, найденные общей проверкой

1. Armada Eco меняет глобальный CPU governor, который возврат Balanced не
   восстанавливает. Журнал performance lease теперь сохраняет и возвращает
   governor/GPU mode вместе с профилем и clock, включая interrupted undo.
2. SQLite library edits читали старые общие preferences вместо активного Trainer.
   Чтение и проверка World editing теперь используют один owner-aware overlay;
   редактирование не сбрасывает тему. Проверено для двух Trainer.
3. Пустая MultiversePresentation без repository больше не разыменовывает null
   при построении PC-категории. Существующий регрессионный сценарий проходит.
4. Согласованы Updates/System health, тексты тёмной темы и названия новых
   категорий Settings с First-run. Исправлены отсутствующие зависимости
   software_update test и сигнатура browser probe.
5. SDK/Start smoke проверяет widget по ID, сохраняет исходное число страниц
   с большим ScreenScraper и выбирает Output device после открытия микшера.
   Settings smoke повторно открывает Settings перед account-сценарием.

## Проверки

- ARM64 Release, Fedora 44 / Qt 6.11.2; production с BUILD_TESTING=OFF.
- [Итоги прогонов](tests.json): 37 suites прошли после исправлений/коррекции среды;
  два полных старых набора остаются открытыми, см. ниже.
- UI suites Settings, Updates, First-run, SDK/Start, Crates, Apps, Downloads прошли.
- 14 Python CTest suites прошли; software_installation затем отдельно прошёл
  после добавления LICENSE в сборочный архив (24 проверки).
- Native: software_update, backend_account, platform_sdk, browser, ScreenScraper,
  PC import/UI, Crates, first_run, Trainer profiles, library, process, input_profiles.
- [Lobby: 84 проверки](lobby.log), [runtime multiplayer: 101](runtime.log).
  Первый контейнер без сетевых интерфейсов не годился для socket-сценариев;
  окончательный прогон использовал отдельный работающий network namespace.
- Browser WebEngine: 62 этапа на каждой портативке. Touch keyboard: 50 этапов
  на каждой. Изолированные HOME/профили, обычный пользователь, Vulkan;
  Chromium sandbox не отключался. Это программный ввод, не физические кнопки.
- Performance: [Odin](odin-power.json), [Flip](flip-power.json). Реальный
  org.armada.Power, live Eco/GPU/governor, отказ неверного clock без потери
  активного режима, Reset, EOF и SIGKILL/journal recovery. Все исходные значения
  восстановлены. Это аппаратный helper check, не FPS/охлаждение или игровой бенчмарк.
- Визуально просмотрены Settings 960×540/1280×720, светлая и тёмная темы.

## Установлено и проверено на устройствах

Одинаковая production-оболочка установлена на Odin 2 и Flip 2:
`1c04fcd2d9ca621d871311d4ec7895fbd7466c2004b5256b838e146fd3ca0691`.
1412 исходных build-input файлов сверены с подготовленным checkout до установки.
13 системных integration/PC helpers проверены побайтно после установки.
Подтверждены отдельные capabilities account.identity, network.group,
play.scorecards, avatar и Start widgets. [Odin receipt](odin.json),
[Flip receipt](flip.json).

На обоих устройствах просмотрены настоящие Start, Settings и Game settings;
GPU range — Odin 220–680 MHz, Flip 305–587 MHz. Pocket Golf 0.1.23 запущен
обычной кнопкой Start, показал Connected to TrainerOS и завершён через свою Exit.
Оба managed процесса исчезли, в истории получено по одной завершённой сессии
с outcome=returned. Соперники/удары/сетевой матч в этом проходе не запускались.

Проверено 2167 файлов Odin и 1541 Flip. Единственные различия — штатная ротация
логов Godot от этого запуска; сохранения и настройки не изменены. Существующие
243/411 строк истории сохранены без изменения, добавлена одна проверочная
сессия на каждом устройстве. Worlds/Adventures идентичны, SQLite quick_check=ok.
Громкость: Odin 0%, Flip 20%, как до работы. Старые binaries, согласованные
SQLite backups и предыдущие root helpers сохранены. Shell заменён атомарно,
supervisor перезапустил только приложение. Reboot, InputPlumber restart,
сон/крышка и изменение boot policy не потребовались.

Первый preflight Flip остановил установку на несовпадении SHA во время ещё
не законченной передачи; текущая оболочка тогда не изменялась. После завершения
передачи установлен и проверен тот же окончательный SHA, что на Odin.
Временный bind build-directory снят; исходный idle builder восстановлен,
сборка/источники/rollback на SD сохранены.

## Открытые результаты

Полный старый core/interactions набор не зелёный: остаются проверки прежних
индексов Start, поведения Guide, истории/галереи и состава Games. Они не
зачтены как успешно пройденные; отдельное обновление сценариев и проверка
оставшихся расхождений нужны. Найденное там null-падение исправлено и отдельно
перепроверено. Физический контроллер, прослушивание/микрофон, сон/крышка,
разные внешние сети, Wine/FEX/SteamRT и полный игровой цикл не заявлены как
свежая приёмка. Управляемый публичный OTA feed этим ручным обновлением не создаётся.
