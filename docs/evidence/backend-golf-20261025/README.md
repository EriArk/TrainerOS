# Pocket Golf → общий Backend: серверная приёмка

25 октября 2026. Ветка `codex/backend-foundation`. Подготовленный group SDK
`59755fc1` объединён с Backend play `d6989438` коммитом `e6824106`. Владельцы
сохранены: S01 идентичность, S07 игровой транспорт, S10 решения/таблицы, S14 SDK.
Lobby UI, live bot, Crates catalogue и портативки этим проходом не изменены.

## Доделано

Зарегистрирован production ruleset `pocket-golf.island.analog5.v1`: три островных
поля, 1–8 ударов на лунку, минимальная сумма, подтверждение карточки всеми людьми.
[Manifest](../../../services/backend/registry/pocket-golf-island-v1.json) и
[хеши симуляции/контента](../../../services/backend/registry/pocket-golf-island-v1.sources.json)
неизменны. Полный content ID выводится из пяти нормализованных исходников;
сборка игры отказывается использовать старую привязку после их изменения.

В Crates подготовлен Pocket Golf 0.1.22, протокол `pocket-golf.8`: admission каждого
игрока, правильные серверные стороны, Start, вся карточка от каждого, polling
решения, общие records/wins, revision pagination, устойчивый operation journal
и явный retry под тем же Player. 2/4 индивидуально; 4–8 командами по 2–3, включая
2 против 3. Ведущий может быть в любой игровой команде. Реванш создаёт новый матч.
Нет Backend у одного участника — обычная casual-партия без общего зачёта.
Соло/боты/передача контроллера не выдаются за подтверждённый серверный матч.

## Доказательства

- Полная Linux-сборка TrainerOS после объединения group/Play SDK.
- [SDK CTest](sdk-tests.txt): PlatformHost и network. Реальные 16 TLS helper-связей,
  кредит/последовательности, disconnect, совместимые двухместные сценарии.
  Две прежние standalone avatar fixtures пропущены; реальные аватары 4/8 проверены
  в текущей игровой цепочке, это не пропуск текущего backend scenario.
- [2](backend-group-2.txt), [4](backend-group-4.txt), [5](backend-group-5.txt),
  [8](backend-group-8.txt) настоящих Godot процессов → C++ PlatformHost →
  TLS helper → локальный HTTP → PostgreSQL → worker. Все три поля, одинаковые
  карточки, `confirmed` у каждого, таблица, другой match ID при реванше, смена
  starter и отзыв владельца с сохранением комнаты. У каждого процесса отдельные
  данные игры. Только provider/vault/signalling — фикстуры; production secrets/DB
  не монтировались. Восьмерка прогревает процессы перед допуском: это не cold-start/FPS.
- [Нет Backend у одного игрока](backend-group-2-casual.txt): полный casual-раунд,
  реванш, пустой server match, отзыв владельца.
- В Crates: проверки JSON-чисел, журнала/повтора, смены Player, поздних callbacks,
  игрового протокола/управления/команд/бота, рендер UI 960×540, экспорты Linux
  x86_64/ARM64/Windows и фактическая установка/запуск/переустановка Linux-пакета.
- [Live registration](registration.json): перед изменением backup, затем сверка
  manifest через HTTPS и четыре пустые публичные таблицы. Игровых результатов
  в live не создано. API ready. Runtime image `play-20261025` и схема 0005 прежние;
  регистрация выполняется maintenance role, без рестарта API/worker/Lobby.

Предыдущие 57 тестов Backend/receipt/dispute/restore сохраняют силу: runtime код
сервера и schema этим проходом не менялись. Изменена только операторская запись
реестра; тестовый HTTP harness получил явные параметры правил и числа аккаунтов.

## Воспроизведение

В репозитории Crates: `games/pocket-golf/tools/check_backend_chain.py`,
`tools/Dockerfile.backend-ci`, `tests/HostIntegration.cpp` и `docs/SHARED_RESULTS.md`.
Нативный core собирается из объединённой ветки. Standalone Golf CMake получает
`TRAINEROS_SOURCE`, `TRAINEROS_CORE`, а для Linux core с libfm —
`TRAINEROS_CORE_WITH_LIBFM=ON`. Backend fixture использует существующий test image,
отдельную PostgreSQL и namespace `--network none`; игра/host подключаются только
к этому namespace. `--casual --players 2` проверяет отсутствие Backend у гостя.
После проверки удаляются fixture containers и временные credentials.

## Недоделано

Это подготовленные исходники/пакеты Golf и SDK, не публикация или handheld delivery.
Открыты реальный вход/игра пользователей на целевых устройствах, разные сети,
физическое управление и производительность. Публичные имена/аватары таблиц ещё
не поставляются Backend: интерфейс показывает `You` и сокращённые Player IDs.

Scorecard trust остаётся `peer_confirmed`: сервер не воспроизводит удары и не
доказывает независимость людей/отсутствие модификаций. Trusted-build admission,
игровые validators, Elo/статус, турниры/экономика, retention/off-host backup/alerts,
полное удаление/relink/recovery аккаунта и целевой vault остаются открытыми.
R089/R107 не объявляются завершёнными. Следующий независимый блок — публичный
игровой профиль и его жизненный цикл, чтобы таблицы показывали имя/аватар.
