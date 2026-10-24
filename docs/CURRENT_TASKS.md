# Задачи и условия готовности

**21 октября 2026: глобальная очередь снята.** Единственный реестр состояния —
[карта готовности функций с галочками](FEATURE_READINESS.md). В ней сохранены
сделанные, недоделанные, несделанные и непроверенные части. Порядок строк не
определяет порядок исполнения. Требования и подробности остаются в issues.
[Карта структуры](STRUCTURE_MAP_RU.md) связывает функции с владельцами,
потребителями и жизненными циклами; её S-узлы указаны в каждой строке готовности.

Портативки у владельца: не подключаться. Продолжать независимую локальную работу;
аппаратные проверки не считать выполненными. Прежние результаты повторять только
при изменении затронутого кода, сбое или конкретном незакрытом результате.

Текущая отдельная работа `codex/first-run-installer-ota`: обязательные Fluxer и
ScreenScraper в новом [визарде](FIRST_RUN.md), общий signed Software preparation
для [Armada installer/OTA](EARLY_BETA_INSTALLER_RU.md), получение подписанных
Beta/Preview по HTTPS и явный `check` → `prepare` без сервера раздачи владельца.
Добавлен consumer в Settings → System → Updates и общем Downloads; обычная сборка
ещё без production source и не выполняет проверку/загрузку. Тестовое подключение
изолировано от пользовательских данных и отключено в production build.
Следующее недоделанное этой функции — production key/bootstrap/публикация GitHub feed,
host/OS-storage/receipt probe и lifecycle-safe
production binding и activation/repair/совместимый rollback. Подготовленный cache не
считается установленной версией; это не новая глобальная очередь.

## Сохранённые условия внешней проверки

Это подробности к строкам карты, а не этапы глобального плана.

| Область | Что остаётся и где доказательства |
| --- | --- |
| Home/Start и последние экраны | Физическая оценка владельцем, голос/звук и ответы другого участника; [78/79](CONTROLS78_HOME_START.md) |
| RetroAchievements | Личная история с Web API key; оценка баннера/джингла и физический цикл на этапе полировки; [RA73](RA73_GAME_SELECTION.md), [RA56](RA56_ACHIEVEMENT_LOOP.md) |
| Social | Реальный tagged community → preview → Join; leave/delete с password/MFA на разрешённом тестовом комьюнити; новая игровая сессия → Recently Together; [Discover](SOCIAL67_DISCOVER.md) |
| Lobby | Импорт отсутствующей игры со съёмного носителя на ARM, physical/audio/distinct networks, старые Exit/PlayerOverlay fixtures не объявлять зелёными; [подготовка](LOBBY63_PREPARATION.md) |
| Files/Browser | Полный набор операций/сетевых мест, реальные сайты/вложения/загрузки и сохранение состояния; [Files](FILES68_IMPLEMENTATION.md), [Browser](BROWSER71_IMPLEMENTATION.md) |
| Производительность | Длительный прогон/оценка владельцем; старые несовпадения общего UI-теста остаются; [PERF72](PERFORMANCE72.md) |
| Crates | Физическое управление/звук, совместимость пакетов вне проверенных устройств, отзывы/публикация; [76](CRATES76_INSTALLER.md), [77](CRATES77_CATALOGUE.md) |
| Мультиплеер | Реальные разные сети, больший состав и более широкие игры по конкретным runtime; [матрица](EMULATOR_MULTIPLAYER_MATRIX.md), точные записи в [emulators](emulators/README.md) |
| Remote Play | Безопасная поставка Iris/IDR, один host → несколько клиентов без сбоя Gamescope, поздний вход/повтор; [группы](REMOTE_PLAY_GROUPS.md), [Iris](ARMADA_IRIS_ENCODER.md) |
| Сейвы/адаптеры | Непройденные exact-build, миграции, двусторонние транзакции и отдельные форматы не закрываются новым UI; [реестр](adapters/README.md), [lineage](SAVE_LINEAGE.md) |
| Публичный выпуск | Установщик microSD/internal/recovery, лицензии #115, art #116, чистое окружение #117; [beta](EARLY_BETA_INSTALLER_RU.md), [публичная приёмка](PUBLIC_READINESS.md) |


Полные прежние галочки/даты сохранены в
[Git](https://github.com/EriArk/TrainerOS/blob/52c39f9/docs/CURRENT_TASKS.md).
Старые очереди и категория «отложено» отменены. При изменении функции обновлять
карту, профильное доказательство и публикационные сводки; не создавать второй
реестр и не закрывать issues по одному лишь переносу записи.
