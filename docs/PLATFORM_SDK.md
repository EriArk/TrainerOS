# Platform SDK: внешняя программа → консоль → результат

**21 октября 2026.** Первый реализованный вход для #236 и ограниченная часть #237:
локальный versioned bridge, реальный контекст, подписки, один простой тип Start
widget и lifecycle cleanup. [Старт автора](../sdk/README.md) и
[wire contract](../sdk/PROTOCOL.md) — документация для использования; этот файл
фиксирует владение, реализацию и границы проверки. Это не завершение #231/#235–244.

## Используемые владельцы и точки подключения

S14 / E22 / F11 предоставляет внешнему процессу необязательный интерфейс к
существующим владельцам. [PlatformBinding](../src/app/PlatformBinding.h) в точке
композиции читает `TrainerController`, `ShellController`, `AdventureLaunchController`,
`AppsController` и `PowerControl`. Там же определяется новый opaque session ID
при реальном running; selection/preparation не выдаются за игру.

[PlatformHost](../src/platform/sdk/PlatformHost.cpp) обслуживает только local
discovery, протокол и connection-bound регистрации. Он не запускает игры,
не владеет профилем, не пишет saves и не создаёт второго Social/Lobby/Downloads.
Capabilities перечисляют **только подключённые** сервисы. Клиент не получает
provider tokens, paths к ROM/save, wallet rights или возможность вызвать shell.

UI остаётся S04: [ControlsDashboard](../src/features/device/ControlsDashboard.cpp)
принимает plain-data contributions; [ControlWidget](../src/qml/ControlWidget.qml)
рисует их существующими средствами. Пользователь добавляет widget через нынешний
Edit/Add; одно нажатие приходит как событие приложению. Отдельный сигнал
`contributionActivated` не проходит через system actions и не закрывает
внутриигровой dashboard как неизвестная системная команда.

## Вход / работа / выход

1. Самостоятельно запущенный процесс читает приватный local endpoint, согласует
   protocol version и стабильный app ID. Установка/публикация не обязательны.
2. Получает snapshot и подписывается на нужные изменения; библиотечная selection
   и live game/session имеют разные поля. Приложение само выбирает capabilities.
3. Регистрирует ограниченный текстовый widget с актуальной context revision.
   Изменение widget с устаревшим контекстом отклоняется, не переадресуется молча.
4. Host владеет placement/input; callback содержит context на момент нажатия.
   Приложение обновляет только свой widget и свои данные.
5. Disconnect/crash/unregister снимают живой вклад. Незавершённый layout edit
   отменяется до удаления объектов; Back не воскрешает widget умершего процесса.
   Расположение отдельно от содержимого сохраняется на reconnect.
6. Смена Trainer отзывает подключение, а не подменяет владельца работающему коду.
   Shell stop и EOF обрабатываются отдельно; прежние callbacks не повторяются.

## Проверка

**Фактический локальный результат 21 октября 2026:** Windows/UCRT64 Debug,
`platform_sdk` и `controls_dashboard` прошли (2,75 с / 0,25 с);
`platform_qml_smoke` прошёл (12,26 с). Оба кадра просмотрены: текст и focus читаемы,
счётчик меняется с 0 на 1; после аварийного завершения процесса вклад удаляется.
Отдельно собран `traineros` с `BUILD_TESTING=OFF`; startup probe прошёл на обычном
запуске: немедленный handshake после discovery, context/capabilities, no-profile
gate и отсутствие `--platform-smoke-test`. Это всё ещё Debug host build, не release
пакет и не установка на ARM. Публикация endpoint перенесена на рабочий event loop,
чтобы синхронная загрузка QML не вызывала ранний timeout у авторского клиента.

- `platform_sdk`: реальные TCP соединения, version/auth/discovery, конфликт двух
  hosts, неподключённые методы, malformed/oversized input, namespaces, устаревший
  context, subscribe/unsubscribe, отзыв профиля, crash/mid-edit cleanup и layout
  reconnect. Отдельный Python-процесс проходит host widget → callback → изменение
  selection → отзыв профиля через [публичный клиент](../sdk/python/traineros.py).
- `controls_dashboard`: сохранённые проверки built-in управления/редактирования,
  focus, page navigation, cancel и owner isolation.
- `--platform-smoke-test`: изолированная настоящая оболочка с `PlatformBinding`
  запускает публичный Python hello-example отдельным процессом; проверяет
  registration → Add → QML → semantic Confirm → обновление текста через callback
  → завершение процесса/очистку. Снимки показывают реальный widget. Физический
  gamepad/touch этим не проверяется.
- [PlatformSdkStartupProbe](../tests/PlatformSdkStartupProbe.py): обычная оболочка
  без smoke flags публикует endpoint только после синхронной загрузки QML;
  подключение сразу после discovery должно ответить за обычный client timeout.
  Проверяет capabilities/context и no-profile gate; `--production` дополнительно
  проверяет отсутствие тестового флага в сборке с `BUILD_TESTING=OFF`.

Команды локального воспроизведения:

```sh
cmake -S . -B build/native -DBUILD_TESTING=ON
cmake --build build/native --target trainer_platform_tests trainer_controls_tests traineros --parallel 4
ctest --test-dir build/native -R '^(platform_sdk|controls_dashboard|platform_qml_smoke)$' --output-on-failure
python3 tests/PlatformSdkStartupProbe.py build/native/traineros
```

Первую конфигурацию/toolchain выполнять по [инструкции разработки](DEVELOPMENT.md).
Для Windows добавить `.exe` к исполняемому файлу и использовать `py`.
Для отдельной проверки обычной сборки: configure `-DBUILD_TESTING=OFF`, собрать
`traineros` и запустить тот же startup probe с `--production`.
QML-тест сохраняет `platform-widget.png` и `platform-activated.png` в
`build/native/screenshots/platform/`; показаны реальный счётчик до/после callback.

Данные для проверок изолированы; endpoint/token/screenshots не входят в Git.
Windows host proof не равен ARM, Gamescope, физическому gamepad/touch или sleep
при работающем эмуляторе. Портативки остаются у владельца; к ним не подключались.

## Оставшиеся входы по Crates

Новый [разбор потребителей](../sdk/README.md) связывает Crates concepts с host
services. Уже готовый bridge можно расширять по конкретной потребности, сохраняя
один вход и реального владельца каждой операции. Не нужно выдумывать готовые
endpoints для отсутствующего backend.

| Функция | Что ещё нужно подключить / проверить |
| --- | --- |
| Lobby + Network Fabric | App activity admission/starter, реальные participant/channel handles и возврат; существующие runtime transports не объявлены публичным game SDK автоматически |
| GameHub tools / live overlays | Registry/placement/context/input lease для этих slots; Start widget не заменяет их |
| Media / Files / Downloads | Service facade к действующим owners, provider registration, реальные операции/error/cancel; не второй worker или музыкальный сервер |
| System module packages | Проверенная installed identity, lifecycle/autostart/uninstall/update, scopes к чувствительным операциям; app ID пока только namespacing |
| Saves / device/input/capture | Уже существующие protected-service границы и реальные capabilities устройства; никакого произвольного path/command endpoint |
| Results / economy / challenges | Согласованные форматы/validation/authoritative implementation; self-reported данные не становятся наградой |
| Полная авторская приёмка #244 | Четыре независимых network/media/file/tool demos, внешний автор, ARM и supported hosts; один hello-widget их не закрывает |

Конкретные состояния — R098/R099/R106 в [карте готовности](FEATURE_READINESS.md).
Это зависимости отдельных функций, без возвращения глобальной очереди.
