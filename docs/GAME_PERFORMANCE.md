# Настройки производительности отдельной игры

26 октября 2026, core/main. Запрос владельца: удобные настройки системы и
прослоек по игре, с режимами питания и доступными частотами. Общая сборка установлена на Odin/Flip. Реальный Armada helper проверен: live
Eco/GPU/governor, Reset, EOF и journal recovery после SIGKILL; исходное состояние
восстановлено на обоих устройствах. Исправлен sticky governor при возврате
профиля Armada. [Точные receipts и границы](evidence/combined-handheld-20261026/README.md).
Игровой UI показал реальные диапазоны; FPS/cooling, полный live игровой цикл
с физическим контроллером и Wine/FEX остаются отдельной приёмкой.

## Вход и поведение

Games → контекст игры → Game settings и Home → Game settings открывают одну
компактную панель Performance / Compatibility. В живой игре Home использует
ID фактической сессии и существующий Minimize, прежде чем открыть эту панель.
Выбранная карточка библиотеки сама по себе ничего в системе не переключает.
Профиль хранится отдельно для локального Trainer и стабильного game ID.

Performance содержит режим питания, частоту GPU и CPU governor. Значения и
доступность приходят от хоста. System default / Automatic наследуют настройки
хоста; Reset + Save снимает все overrides этой игры. Для фактической запущенной
игры кнопка **Apply & save** сохраняет профиль и сразу применяет power/GPU/governor,
без перезапуска процесса. Reset + Apply & save возвращает исходные системные
настройки прямо в сессии. Для другой игры/другого Trainer остаётся Save profile
со следующим запуском. Это заменяет первоначальное ограничение next-launch-only
из `19e8642a`; **Compatibility по-прежнему действует только со следующего запуска**.

В Compatibility у Proton доступны Runtime default / DXVK Vulkan / WineD3D
OpenGL для DirectX 9–11 и диагностическое отключение Esync/Fsync. DirectX 12,
NTSync и FEX не объявляются переключёнными этими действиями. Ссылка Launch &
compatibility ведёт в существующий редактор executable/runtime/prefix/components.
Native Linux и эмуляторы не получают не относящихся к ним Wine-переключателей.
Существующие emulator-owned Game settings остаются отдельным рабочим входом.

## Владельцы и восстановление

- S04 `GamePerformance` владеет редактированием и сохранением профиля; файл —
  `<state>/game-performance/<owner hash>/<game hash>.json`, атомарный QSaveFile.
- S03 `AdventureLaunchController.configureGame` добавляет подготовку к **каждому
  реальному запуску**, сохраняя adapter prepare/finalize/settled. После успешной
  подготовки адаптера и перед запуском процесса оформляется временное владение
  режимом. Отмена, ошибка запуска и завершение проходят тот же finalizer.
- S15 `game-performance.py` обращается к существующим D-Bus providers:
  Armada `org.armada.Power1`, затем Power Profiles daemon. Никаких записей в sysfs,
  новых прав root или глобальных конфигурационных файлов профилей.
- Helper держит stdin pipe: EOF при падении shell возвращает настройки.
  Один flock на Linux-пользователя исключает конкурирующие TrainerOS leases.
  Журнал намерений сохраняется перед записью; незавершённое восстановление
  повторяется при следующем открытии панели/получении lease. Ошибка возврата
  отображается через существующий результат завершения игры.
- Горячие изменения идут по тому же приватному stdin pipe, с ограниченным JSON
  запросом и подтверждением. `ProcessService.updateSettings` исполняет их на
  том же worker, что prepare/finalize: один запрос за раз, завершение ждёт уже
  начатую операцию и затем восстанавливает настройки. Cancel/exit запрещают новые
  запросы; старый ответ не становится успехом следующей сессии/другого Trainer.
- Helper подтверждает поддержку протокола updates=1. На поддержанном хосте lease
  открывается и для игры без overrides: их можно добавить во время игры. Старый
  helper или недоступный provider не блокирует обычный запуск с defaults.
- При смене live-профиля старые overrides возвращаются, новые применяются с
  журналом; отказ записи откатывается к последнему рабочему профилю. Невалидный
  запрос не снимает текущие настройки. Ошибка восстановления оставляет журнал.
  Compatibility-only сохранение не переключает питание повторно. После внешней
  ручной смены режима явное новое применение использует её как новую исходную
  точку. Это последовательность host API calls, не атомарная транзакция драйвера.
- Профиль сохраняется **до** live-запроса. При отказе применения UI сообщает,
  что файл сохранён, но текущее применение не удалось. Таймаут закрывает канал
  обновлений и запускает восстановление; запоздалый ответ нельзя принять за
  подтверждение следующего изменения.
- Armada сохраняет governor внутри выбранного power profile. Операции поэтому
  отменяются в обратном порядке: governor целевого профиля, затем исходный режим.
  Сравнение с записанным результатом и проверка текущего профиля не позволяют
  записать старый governor в другой профиль, выбранный пользователем извне.
  Внешнее ручное изменение может оставить часть уже применённых настроек;
  восстановление не должно его перезаписывать. Это не транзакция внутри Armada.
- GPU-частоту округляет и ограничивает Armada. Панель показывает диапазон,
  сообщённый **текущим** host profile; сохранённый режим может сузить фактический
  диапазон при запуске. Точные ручные CPU MHz, TDP в ваттах и собственные fan
  curves не предлагаются: использованный API их не экспонирует. CPU caps и
  охлаждение следуют настоящему host preset, thermal protections сохраняются.

Helpers устанавливаются существующим session support:
`/var/opt/traineros/integrations/game-performance.py` и PC helpers в
`/var/opt/traineros/pc/`. Это регистрация support files, не новый installer/OTA.
Отсутствие helper/provider не превращается в успешное применение: запуск с
явным override останавливается с ошибкой, Reset возвращает обычный запуск.

## Runtime Downloads

PC → Launch → Runtime → Download a runtime получает официальный каталог
GE-Proton для архитектуры хоста. Сначала остаются видны установленные runtimes.
Downloads получает отдельную задачу с байтами, скоростью/графиком, отменой и
повтором. Новая версия не меняет регистрации игр автоматически.

`packaging/pc/runtimes.py` проверяет SHA-256 из официального GitHub release asset,
размер и архитектуру Wine, поддерживаемый manifest и состав архива. UMU 1.4.4
имеет закреплённый digest. Проверенные архивы сохраняются по digest; повтор
может использовать их без сети. Распаковка ограничена объёмом/числом entries,
использует Python tar data filter (сохраняет внутренние Wine symlinks, запрещает
выход наружу), отдельную staging-папку и атомарную публикацию готовой версии.
Отмена/ошибка не создаёт установленную частичную версию. Пользовательские/host
runtimes не перезаписываются; удаления runtime через этот UI пока нет.

Исправлен прежний чрезмерный запрет всех `require_tool_appid`: UMU 1.4.4
поддерживает soldier/sniper/steamrt4 x86_64 и steamrt4-arm64. Неизвестный или
чужой архитектуре appid по-прежнему отклоняется. **Первый запуск нового SteamRT
может потребовать дополнительной загрузки самим UMU**; `UMU_RUNTIME_UPDATE=0`
отключает обновление существующего контейнера, не первичную установку.
Готовый Proton-кеш ещё не означает готовность первого запуска без интернета.
Полный управляемый offline-bootstrap SteamRT, версии FEX и матрица проверенных
игровых сочетаний остаются незавершёнными требованиями #250/R018.

## Проверки и границы

На Windows/Qt 6.11.1/GCC 16.1:

- Тестовая сборка и обычная сборка `traineros` с `BUILD_TESTING=OFF` завершены
  успешно; последняя включает регистрацию новых QML и support files.
- `trainer_pc_tests`: 30 passed, один прежний Linux/libfm skip; включает общий
  контекстный вход, owner/game persistence, live identity, сохранение до ack,
  повторное нажатие и защиту от устаревшего ответа при смене Trainer.
- `trainer_pc_ui_tests`: 6 passed; реальные QML-render, переключение раздела,
  D-pad и pointer click GPU slider, сохранение и повторное открытие.
- `trainer_process_tests`: 19 passed. Реальный дочерний процесс сохраняет PID
  при обновлении; update/finalize работают на одном worker в нужном порядке,
  stop запрещает новый запрос и не выдаёт поздний applied, следующая сессия
  не наследует канал. Потеря канала отключает повторное предложение live controls.
- `GamePerformanceTests.py`: 19 passed. Несколько live-переключений, Reset,
  отказ/частичный rollback, внешняя смена исходных настроек, compatibility-only
  no-op и настоящий subprocess pipe/EOF проверены с тестовым host provider.
- Сохранённые результаты предыдущего прохода: `PcRuntimeDownloadTests.py`: 7 passed.
  Runtime fixture install/reuse/failed staging реально пишет временные файлы;
  flock в Windows-части подменён и не считается Linux-проверкой.
- `PcSessionTests.py`: 11 passed / 6 Linux skips; `PcSteamTests.py`: 7 passed.
  Preflight отказа session support: 1 passed. Apps smoke оболочки: PASSED.
- Исторический результат `trainer_library_tests` до общей доставки: **17 passed / 3 failed**. Сбои относились
  к Start → Settings (ожидаемый service), разрешению World editing и ожиданию
  прежней темы `turquoise` вместо `light`. Чистая бинарная baseline-сборка для
  сравнения тогда не выполнялась. Эти три сбоя исправлены в `c0ba3834`, полный Library-набор
  прошёл при [общей доставке](evidence/combined-handheld-20261026/README.md).

Локальные evidence: `build/native/{performance,live-performance}-*-tests.txt`,
`screenshots/game-performance/{light-960,dark-800,compatibility-800}.png` и
`screenshots/performance-shell/apps-verification.txt`. Это временные build
артефакты с искусственной библиотекой, не пользовательские данные или README art.
Live-панель: `screenshots/game-performance/live-dark-800.png`; QML включает
переключение подписи Apply & save и controller-confirm после перехода в live.

Локальные тесты проверяют profile/owner isolation, сохранение аргументов и
adapter lifecycle, host rollback/частичную ошибку/журнал/внешнюю смену режима,
отказ неподходящих архивов/архитектур/digest и повторное использование кеша.
QML проверяется в 960×540 и 800×450, в Light/Dark, с controller actions.
Сетевая проверка каталога использует реальные официальные метаданные x86_64/ARM64.
Реальный zipapp UMU загружен и проверен по закреплённому SHA-256; полный GE-Proton
архив и первый запуск SteamRT на устройстве здесь не проверялись.

Следующий абзац описывает первоначальную локальную подготовку, до доставки выше.
Это **не** проверка настоящих D-Bus прав, CPU/GPU/cooling, игры через Wine/FEX,
первичного SteamRT bootstrap, Linux pipe/flock после crash или физических
кнопок. Устройства не устанавливались и не тестировались. Горячее применение
реализовано в исходниках; его реальная аппаратная приёмка, персональные
CPU-frequency/TDP/fan controls и полный compatibility catalogue остаются открытыми.

## Проверенные upstream-контракты

- [Armada Power daemon](https://github.com/armada-os/armada/blob/main/system_files/usr/libexec/armada/armada-powerd)
  и [Armada Control](https://armadaos.dev/using-armada/armada-control/).
- [Power Profiles D-Bus](https://upower.pages.freedesktop.org/power-profiles-daemon/gdbus-org.freedesktop.UPower.PowerProfiles.html).
- [Proton runtime options](https://github.com/ValveSoftware/Proton#runtime-config-options).
- [UMU 1.4.4 runtime mapping](https://github.com/Open-Wine-Components/umu-launcher/blob/1.4.4/umu/umu_runtime.py)
  и [GE-Proton releases](https://github.com/GloriousEggroll/proton-ge-custom/releases).

Код здесь использует публичные API, не копирует реализации upstream. Скачиваемые
артефакты сохраняют включённые в архив лицензии и не добавляются в Git TrainerOS.
