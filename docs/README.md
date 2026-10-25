# Документация TrainerOS

[Начать внешнюю игру/программу/виджет](../sdk/README.md) — SDK v1 preview,
[реализация и границы](PLATFORM_SDK.md).

[Полное покрытие требований](REQUIREMENTS_COVERAGE_RU.md): 256 issues (с дополнением #257–259 от 24.10) → устойчивые
R-ID готовности → S-владельцы → F-сценарии. Старые решения и открытые условия
сохранены отдельно от фактической реализации.

## Постоянные точки входа

| Нужно понять | Документ |
| --- | --- |
| Правила работы | [AGENTS](../AGENTS.md), [workflow](DEVELOPMENT_WORKFLOW.md) |
| Какой продукт делаем | [продукт и архитектура](PRODUCT_ARCHITECTURE_RU.md), [экранные решения](PRODUCT_SPEC.md), [визуальный язык](DESIGN_LANGUAGE.md) |
| Владельцы функций и связи | [Карта структуры](STRUCTURE_MAP_RU.md): входы/выходы, зависимости, жизненные циклы и код |
| Готовность функций и остаток | [Карта с галочками](FEATURE_READINESS.md), [условия проверки](CURRENT_TASKS.md) |
| Какие игры проверены и где есть проблемы | [Таблица проверок мультиплеера](MULTIPLAYER_TESTED_GAMES.md) |
| Что уже сделано | [PROJECT_STATUS](../PROJECT_STATUS.md) |
| Как развивалось | [CHANGELOG](../CHANGELOG.md), [коммиты](https://github.com/EriArk/TrainerOS/commits/codex/apps-workspace/) |
| Требования к функциям | [GitHub issues](https://github.com/EriArk/TrainerOS/issues) |
| Как устроено/собрать | [ARCHITECTURE](ARCHITECTURE.md), [DEVELOPMENT](DEVELOPMENT.md) |

Текущий договор важнее старого отчёта; последние явные решения владельца важнее
противоречащего issue. PRODUCT_SPEC содержит короткую таблицу таких замен.
Исходники, установленная сборка и фактическая приёмка — разные уровни доказательств.

## Технические справочники — читать по задаче

- Запуск: [весь каталог](LAUNCH80_PLATFORM_REGISTER.md), [лаунчеры](LAUNCH80_LAUNCHER_AUDIT.md),
  [стандарт](EMULATOR_STANDARD.md), [мультиплеер](EMULATOR_MULTIPLAYER_MATRIX.md),
  [обслуживание эмуляторов](emulators/README.md).
- Данные: [модель](DATA_MODEL.md), [persistence](LOCAL_PERSISTENCE.md),
  [save lineage](SAVE_LINEAGE.md), [backup](SAVE_BACKUPS.md), [выход](ADVENTURE_EXIT.md).
- Адаптеры: [реестр знаний](adapters/README.md), [границы framework](UX_FRAMEWORK.md).
- Общение: [Social](SOCIAL.md), [возможности Fluxer](FLUXER_CAPABILITIES_RU.md),
  [Lobby](LOBBY.md), [Remote Play](REMOTE_PLAY_TRANSPORT.md).
- Apps: [Files](FILES68_IMPLEMENTATION.md), [Browser](BROWSER71_IMPLEMENTATION.md),
  [Crates installer](CRATES76_INSTALLER.md), [каталог](CRATES77_CATALOGUE.md).
- Устройство: [платформа](ARMADA_PLATFORM.md), [диагностика](DEVICE_DIAGNOSTICS.md),
  [производительность](PERFORMANCE72.md), [масштаб](DISPLAY_SCALING.md),
  [Settings и профили ввода](INPUT_PROFILES.md).
- Выпуск: [первая бета](EARLY_BETA_INSTALLER_RU.md), [открытые gates](PUBLIC_READINESS.md),
  [лицензия](../LICENSING.md), [third-party notices](../THIRD_PARTY_NOTICES.md).

Датированные разделы технических записей сохраняют конкретные проверки, хеши,
ограничения и восстановление. Их старые «следующие шаги» не являются очередью.
Ссылки на `blob/<commit>/…` — неизменяемая историческая запись, не текущий контракт.
Промежуточные планы экранов, копии issues и прежние очереди удалены из рабочей
документации 18 октября; они доступны в Git. Архив старого AGENTS из main сохранён как историческая запись; удалённые промежуточные планы не возвращаются.

## Согласованные будущие контракты

- [Аккаунт и экономика](TRAINEROS_ACCOUNT_ECONOMY_RU.md), [backend](TRAINEROS_BACKEND_PLAN_RU.md), [Crates storage](CRATES_SERVER_STORAGE_RU.md).
- SDK #235–244, PC #246–251 и Picker #252–254 — в актуальных issues. Цель не означает установленную функцию.
