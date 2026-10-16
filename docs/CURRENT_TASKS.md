# Current tasks

**Уточнение владельца, 16 октября 2026 — коллекции:** текущий сценарий —
Select на игре → «Добавить в коллекцию» → компактная всплывашка с выбором
существующей ручной коллекции или созданием новой с выбранной игрой.
Предложение отдельного полноэкранного редактора для этого сценария отменено.
Создание/редактирование автоматических коллекций (#199: правила, предпросмотр,
исключения и миграция) переносится в специальный раздел настроек и откладывается
до очереди настроек. Существующие данные сохраняем. Сейчас записано решение,
реализация и новая поставка не заявляются. [Контракт](SERIES_COLLECTIONS.md).

**GAMES-43:** Grid установлен на Odin/Flip: 5–6 карточек в ряд, рамки по
пропорциям скриншотов без пустых полей, подписи снизу, отдельный вид от Shelves.
Native/ARM, 20 тестов и UI smoke прошли; реальные кадры проверены, приёмка
владельцем остаётся.
Настройки отображения (скриншоты/логотипы/состав элементов) отложены до блока
настроек; сейчас их не реализуем. [План и отложенный пункт](GAMES43_GRID.md).

**GAMES-42:** в выборе игр L2/R2 показывают предыдущую, текущую и следующую
платформу/коллекцию. Соседние названия нажимаются; порядок и область переключения
сохранены. Установлено на Odin/Flip, сборки/UI smoke и реальные кадры проверены.
[Контракт и проверка](GAMES42_CONTEXT_HINTS.md). Grid остаётся отдельным обсуждением.

**GAMES-40:** List принят владельцем. Следующий согласованный экран — Shelves:
до пяти игр — одна полка с крупными обложками по центру; от шести —
два ряда по пять с вертикальной прокруткой. Пустая полка остаётся. Генерируемые
заглушки выбираются по жанру и устойчивому ID; настоящие обложки приоритетны.
R3 циклически Wheel/List/Shelves; Grid остаётся следующим обсуждением.
Native/ARM, 19 тестов и UI smoke прошли; установлено на Odin/Flip.
Реальные кадры проверены, визуальная приёмка Shelves владельцем остаётся. [Контракт](GAMES40_SHELVES.md).

**GAMES-39:** Wheel принят владельцем. List установлен на Odin/Flip с тем же
GameInformation справа. Слева строки с разделителями; по уточнению владельца
прокрутка по семь строк (свайп/стрелки, влево/вправо крестовиной или левым
стиком; вверх/вниз выбирают строку).
Выделение — светящаяся рамка без заливки. Вид переключается R3 и нажатием
на нижнюю подсказку; верхний выбор Wheel/List убран по уточнению владельца.
Вид Wheel/List сохраняется для платформы/коллекции текущего Trainer; выбранная
игра, Home и запуск не меняются от переключения. 17 тестов, UI smoke и реальные
кадры/переходы проверены; List принят владельцем. Shelves — GAMES-40, Grid — позже.
[Точный план](GAMES39_LIST.md).

**GAMES-38:** Wheel установлен на Odin/Flip: крупное колесо логотипов с золотым
кристаллом; в правой области метаданные слева, пропорциональная рамка медиа справа.
Описание обтекает рамку; название однострочное с прокруткой при переполнении.
Повторная строка платформы/Search/Filter убрана, L2/R2 сверху сохранены.
Native/ARM, 14 тестов и UI smoke прошли; реальные оформленные игры, движение
названия и ручная прокрутка проверены. Визуальная приёмка владельцем остаётся.
После просмотра — List с общей правой областью; Shelves/Grid позже.
[План и доказательства](GAMES38_WHEEL.md).

**GAMES-36:** Home временно принят. Следующий согласованный экран — Games:
шесть круглых коллекций в ряду, ниже одна лента вертикальных обложек платформ 3:4.
Количество обложек адаптивное, без растяжения; стрелки указывают доступную прокрутку.
Тонкий разделитель, равные отступы, независимая прокрутка. Search на корне убран.
Сохраняем иллюстрации и объёмный стиль.
Установлено на Odin/Flip; кадры и переходы проверены. Прямой вход в игры,
возврат по ID; следующие виды/редактор — после просмотра
этого экрана. [Точный план и проверки](GAMES36_ROOT.md).

**QUICK-34:** профиль Fluxer на корпусе, редактирование через ключ, объёмная
карточка Home и пять подложек по темам. Значок X правее; нижние подсказки
X/Y скрыты, управление сохранено. Activity убран, друзья/уведомления подняты.
Установлено на Odin/Flip; далее визуальная приёмка владельцем. [План и приёмка](QUICK34_PROFILE_HOME.md).

**QUICK-32:** убрать подсказку L1/R1 со всех экранов, сохранив управление.
L2/R2 показываются отдельно без подложек: «‹ L2» слева от ряда подразделов,
«R2 ›» справа. На Home подсказок нет, переключение блоков L2/R2 сохранено
по последнему уточнению владельца. Recent games: четыре уменьшенные карточки
в ряд и более низкая панель. [Контракт](QUICK32_NAVIGATION_RECENTS.md).

**QUICK-31:** оба нижних угла экрана при раскрытой панели прямые.
Recent games использует тот же корпус и скос, что X, без верхней полосы;
игровые карточки сохранены. При открытии оба ушка уходят вниз, затем панель
растёт снизу вверх сразу полной ширины. Горизонтального растягивания нет;
закрытие обратно, переключение X/Y без наложения. [Контракт](QUICK31_VERTICAL_PANELS.md).

**QUICK-30:** раскрытый X продолжает нижний корпус во всю ширину экрана,
без верхних срезов; внутренний скос повторяет экранную рамку. Избранные
неподвижны и ограничены шириной; после разделителя — только запущенные
вне избранного, с прокруткой. Запущенное избранное остаётся на месте
с индикатором. [Контракт/проверки](QUICK30_GROUPED_PANEL.md).

**QUICK-29:** скрыть заголовок/полоску только раскрытого X, центрировать иконки,
сохранить сложенное ушко и Y. [Уточнение контракта](QUICK28_SHARED_DRAWER.md).

**QUICK-28:** X возвращён к общей панели Recent games/Y, отдельный док отменён.
[Контракт и проверка](QUICK28_SHARED_DRAWER.md); другие экраны пока не начинаем.

**QUICK-27:** ещё ниже — панель 68 вместо 96 без уменьшения кнопок;
названия приложений убраны полностью, включая footer. Уточнение QUICK-26.

**QUICK-26:** компактная панель X с неглубокими кнопками в цвет корпуса,
мягкими скатами и одной подписью выбранного приложения. Массивный QUICK-25
отклонён. [Текущий контракт и проверка](QUICK26_COMPACT_PANEL.md).
Новый экран не начинаем до просмотра владельцем.

**QUICK-25 — окончательная форма X:** цельная панель корпуса с объёмными
кнопками в углублениях, без иконок, стоящих над полкой. По центру снизу,
ширина по содержимому; растёт высотой от фиксированной нижней рамки,
как ушки, а не выезжает/висит над ней. Это заменяет полку QUICK-24.
Порядок/перекрытие ушек и исправленное положение Y сохранены.
Установлено на Odin/Flip; [проверки и границы](QUICK25_CHASSIS_PANEL.md).
Далее — просмотр владельцем, новый экран пока не начинаем.

**QUICK-24 — окончательное уточнение владельца:** ушки слева; Quick Apps
поверх жёлтого Recent games справа, наложение 32 вместо 8 без лишних скатов.
Quick Apps раскрывается как док старого Mac OS по центру снизу: перспективная
площадка с настоящими иконками, ширина по содержимому с ограничением экрана.
Берём концепцию, не копию: док вырастает из корпуса и использует его материал;
стекло и зеркальное отражение исключены. Проверять с настоящими избранными Apps.
Y сохраняет прежнюю панель. Это заменяет геометрию QUICK-23 и промежуточный
вариант QUICK-24 с жёлтым сверху. По дополнительной коррекции аккуратности
ушки чуть шире (174 × 36), Y и подпись сдвинуты внутрь, посадка прежняя.
Установлено на Odin/Flip; [проверки и границы](QUICK24_OVERLAP.md).
Далее — просмотр владельцем, новый экран пока не начинаем.

**QUICK-23 — уточнение владельца:** оба ушка рядом слева, примерно на треть
меньше QUICK-22 (162 × 36 вместо 242 × 53), округлые скосы и круглые Y/X.
Сохраняем посадку на нижней окантовке экрана, без отдельной накладной полосы.
Y жёлтое, X в цвете темы с розовой клавишей; размеры открытых панелей прежние.
Это заменяет размещение справа и зеркальное ушко из QUICK-22; поведение прежнее.
Установлено на Odin/Flip; [проверки и границы](QUICK23_LEFT_HANDLES.md).
Следующий шаг — просмотр владельцем, другие экраны пока не начинаем.

**QUICK-22 — решение владельца:** Quick Apps X становится горизонтально
отражённой правой парой Adventures Y: общая форма корпуса, анимация и логика,
меньшая раскрытая высота под иконки приложений. Оба закрытых ушка короче и
одинаковой ширины. Вертикальный док и сужение экрана заменены этим решением.
**Установлено на Odin и Flip;** [проверки и границы](QUICK22_MIRRORED_DRAWER.md).
Следующий шаг — просмотр владельцем; новые экраны пока не начинаем.

**HOME-21 установлен на Odin и Flip:** четыре недавних человека с никами,
лёгкие уведомления, L2/R2 между людьми/уведомлениями/игровой карточкой. Больше
описания и коллажа, отметки соединений рядом с платформой без чипа.
[Проверки и результат](HOME21_PEOPLE_ACTIVITY.md). Следующий шаг — просмотр Home
владельцем; следующий экран пока не начинаем.

**HOME-20 установлен на Odin и Flip:** только универсальная карточка — достижения
выбранной игры внизу, коллаж справа сверху, узкое описание, чипы метаданных и
доступного мультиплеера. Карточка адаптера остаётся собственной; Emerald пока
без переделки. [Результат и проверки](HOME20_GENERIC_CARD.md).
Следующий шаг — просмотр Home владельцем. Следующие экраны не начинаем.
Ниже сохранена предыдущая поставка HOME-19; её универсальная компоновка заменена.

## Актуально: визуальная коррекция Home, по одному экрану

**HOME-19 установлен на Odin и Flip:** фон карточки для каждой темы без скриншота в фоне;
ширина карточки около 2/3 Home, коллаж наклонённых рамок с переходом в Screens,
надпись Start/Resume/Return внутри A. Захват/история сохранены. Полная галерея
#203 не подменяется коллажем. [Проверки и границы](HOME19_THEME_BACKDROPS.md).
Следующий шаг — просмотр Home владельцем; другие экраны пока не начаты.


Владелец отверг присланные PC-снимки Home. [Визуальная коррекция](HOME17_VISUAL_DELIVERY.md)
установлена на Odin после обновления Armada `20261015.01e50d4` и перезагрузки.
Восстановлены круглая A, красочный первый запуск, значки и полупрозрачный кадр
на всей карточке. Проверены настоящий Home, Y/возврат, процесс и целостность базы.
[DISPLAY-18](DISPLAY_SCALING.md) установлен на Odin: общий масштаб/настройки,
фон только в игровой карточке, исправлена прокрутка выбранной настройки.
Владелец одобрил композицию хост-снимка; **следующее действие — просмотр
установленного Home владельцем**. Games и
отделение адаптера ждут. После возвращения онлайн Flip получил тот же HOME-19. Ранее записанные ниже
«следующий Games/GameHub» и «не установлено» — исторические точки до Home17.

## HOME-16 — текущее продолжение, 16 октября 2026

Основной CORE сохранён в `87bb989`; следом реализован
[Home](HOME16_FRAMEWORK_EVIDENCE.md): общий контроллер трёх зон, точная навигация
в имеющиеся Social/RA, read-only showcase, отдельная прокрутка и успешный launch
checkpoint. Это не закрытие всех #188–196 или всей очереди Home/Games/GameHub.

Следующий блок: перечитать актуальные #197–205 и часть Games/GameHub в
[контракте](UX_CONTRACT_20261016_RU.md), затем выполнить каркас Games/Overview/RA;
после него Social/Lobby (#217–223), затем Apps/system. Не возвращаться к глубокой
доводке multiplayer. Сохранить оставшиеся CORE popup/focus, glyphs/TV и фактическую
установку/приёмку на обеих портативках. Перед устройственными тестами подготовить
Armada по принятому поручению; update пока только обнаружен. Сборочное окружение
Odin отсутствует, требуется восстановить его отдельно от установленной оболочки.

## CORE-16 — состояние продолжения, 16 октября 2026

Реализован и проверен на ПК [основной каркас](CORE16_FRAMEWORK_EVIDENCE.md): пять
разделов, GameHub/optional Adapter, перенос Pokémon, schema 3/descriptor 2,
общие X/Y, маршруты Apps и native window geometry. Это не закрывает весь CORE
backlog: общий popup focus/outside tap, новые Home/Social и полная responsive/TV
приёмка остаются. Следующий пользовательский блок — Home/Games/GameHub,
затем Social/Lobby; актуальные issues читать перед соответствующим блоком.

Odin отвечает по SSH, Flip не ответил. `rpm-ostree upgrade --check` обнаружил
обновление Armada; оно ещё не установлено. Перед устройственными тестами выполнить
принятую владельцем подготовку ОС, сохраняя отдельную Iris/WIP-ветку и активные
процессы/сейвы. Новый каркас на портативках пока не проверен и не установлен.
Не возвращаться к глубокой доводке multiplayer. Документационный DOC-16 ниже —
историческая точка, не новая пауза реализации.

## Текущий этап — DOC-16, 16 октября 2026

Выполнено согласование последних 116 новых/изменённых issues: [реестр](ISSUE_RECONCILIATION_20261016.md),
[единый контракт](UX_CONTRACT_20261016_RU.md), [действующая очередь](ROADMAP.md#current-execution-queue).
Владелец выбрал **CORE → Home/Games/GameHub → Social/Lobby → Apps/системный блок**.
Этот проход только документационный: новый UI не реализован, устройства не проверялись,
issues не закрывались, ветки реализации не сливались. Commit/push фиксируется в отчёте поставки.

**Владелец разрешил продолжение: CORE-16**, затем каркас остальных разделов
по очереди Home/Games/GameHub → Social/Lobby → Apps/системный блок, если работа
продвигается. Сейчас основной каркас и подключение существующих служб; доводка
всех сетевых режимов и глубокая реализация отдельных функций сюда не входят.
Уровень **3/6 Высокое**: навигация, адаптерный контракт, миграции, общий ввод и вывод
затрагивают живые сессии и сохранённый контекст. Результат — пять основных разделов
и рабочая optional Adapter face с переносом текущего Pokémon, без потери Apps,
игровых маршрутов, Social drafts и protected saves. Детали и критерии — ROADMAP;
не возвращаться автоматически к старому AP02 или системному Iris из журнала ниже.

Перед началом перечитать CORE-16, #111/#167/#169/#174–178/#192–193/#206–207/#224/#227/#232
в их **актуальной редакции** и текущий контракт. Проверить real checkout/main,
зависимости Apps и Remote Play, конфликты numeric routes и WIP. Если реализация
потребует выбора между разными UX/архитектурными решениями — спросить владельца,
сохраняя общий стиль и логику TrainerOS. Не переносить подтверждённые старые тесты
в графу «свежая проверка».

### Открытые проверки и работа, которую нельзя потерять

| Область | Подтверждённое / точка продолжения |
| --- | --- |
| Apps AP01 | Каталог и оконный цикл в исходниках, установлен на обеих; Gamescope overlay не виден/не принят. [Evidence](APPS_AP01_EVIDENCE.md) |
| Shared-host Remote Play | Отдельный кандидат, не равен установленной сборке; поздний клиент/Iris и lifecycle остаются открыты. [Evidence](REMOTE_PLAY_GROUPS.md) |
| Библиотеки и очистка | 77 общих игр / 27 платформ; финальная поздняя очистка Flip требует сверки, не повторного полного удаления. ПК-игры Odin сохранены. [Evidence](MULTIPLAYER_TEST_LIBRARY_RU.md) |
| Provider research | Friend-only activity, native tagged discovery, polls/multi-stream — проверить реальные Fluxer contracts перед зависимой функцией |
| Apps/system research | Files/Browser ARM embedding, доверие module SPI, Widgets auto-add policy и platform rollback не выдумывать |
| Релиз | Installer на Armada принят; окончательный список beta gates ещё не утверждён. Новые full-product issues не становятся ими автоматически |
| Реальная приёмка | Physical controllers/audio/TV, owner review, разные сети остаются отдельными от source/unit/build/installed |

## Исторические точки продолжения

Записи ниже сохраняют состояние на свои даты. Они не меняют текущую очередь,
не возобновляют старые паузы и не удостоверяют сегодняшнюю доступность устройств.

**Подготовка библиотек, 15 октября — выполнена:** по отдельному поручению владельца
на Flip/Odin установлен [общий набор 77 игр / 27 платформ](MULTIPLAYER_TEST_LIBRARY_RU.md).
Сейвы/история сохранены; каталоги и SHA256 проверены. По следующему поручению
владельца архивы лишних ROM-файлов окончательно удалены. Очистка старых сборок
и тестовых артефактов на Odin завершена; на Flip потеряна связь до получения
отчёта об этом этапе. После возвращения Flip сверить план и уже удалённые файлы,
завершить остаток и проверить каталог. Отдельные ПК-игры Odin пока оставлены.
Это не возобновление реализации Apps и не повторная приёмка мультиплеера.
В подготовке выявлены ограничения автоматического импорта WUA с `DLC` в имени
и сохранения каталожной идентичности Pokémon; известные записи перенесены по
одинаковым образам, общий импортёр этим не исправлен.

**Apps — активный блок, 14 октября 2026:** владелец разрешил составить план
и начать реализацию. [План AP01–AP04](APPS_EXECUTION_PLAN_RU.md) заменяет
прежнюю паузу обсуждения Apps: сначала каталог, избранное/скрытие и полный
оконный цикл; затем Файлы, браузер/Downloads и общая приёмка. Выбран
полноэкранный вариант с компактными Home/сенсорными кнопками, без нового WM.
Odin доступен для проверки; Flip и системная работа Iris остаются на паузе.
AP01 разрабатывается отдельно от незавершённого кандидата Remote Play L07.
Каталог установлен на Odin: пять колонок, компактный скруглённый док в рамке,
квадратные клавиши прежнего размера. AP01 открыт до исправления видимости
управления поверх приложения; [проверки](APPS_AP01_EVIDENCE.md) не подменяют приёмку.
Следующие датированные блоки сохранены как история и не запрещают Apps.
**Браузер Apps — дополнение 14 октября, только требования:**
В выбор L2/R2 добавлен [встроенный браузер](APPS_WORKSPACE_RU.md), сохраняющий
загрузки при переходах. Общие Downloads, минимальный UI, вкладки/закладки/история,
легковесное полноценное ядро без Google-сервисов. Многопоточность желательна.
Выбор движка и реализацию пока не начинать; обсуждение владельца продолжается.

**Новые требования Apps — 14 октября, без начала реализации:**
Записана [основная вкладка Apps](APPS_WORKSPACE_RU.md), правая раскрывающаяся
панель избранных/запущенных, файловый менеджер по L2/R2 на готовой легковесной
основе и почти полноэкранные окна приложений с рамкой TrainerOS. Продолжить
обсуждение остальных пунктов владельца; реализацию и устройства не запускать.

**Пауза реализации / уточнение первой беты — 14 октября:**
Владелец задал [установщик поверх Armada](EARLY_BETA_INSTALLER_RU.md) вместо
собственного образа для первого выпуска. Запуск ярлыком, автоматическая
подготовка и настройка, определение ОС на microSD/внутренней памяти, отзывчивый
интерфейс; целевой охват всех Armada-портативок. Сейчас записать требования
и выслушать следующие пункты владельца. Разработку, сборки и проверки устройств
не возобновлять. Точка продолжения L07 ниже сохранена; её старые сведения
о доступности устройств не являются свежей проверкой. Образ/SteamOS/Bazzite
не блокируют первую бету.

**L07, незавершённый кандидат — 14 октября:**
[общий host/P2–P4, проверки и точка продолжения](REMOTE_PLAY_GROUPS.md).
Ветка `codex/remote-play-shared-host`; установленная сборка прежняя.
ARM 8/8, транспорт 10, media 4/4; локальный первый клиент получил 285/285 кадров.
Поздний второй гость упирается в отсутствие новых IDR у штатного Iris Flip.
Следующий шаг: восстановить Odin после зависания SSH, завершить безопасную
системную поставку уже подготовленного Iris-патча, затем принять независимые
подключения/выходы и жизнь Gamescope. L07 и поставку нескольких гостей не закрывать.

**Активный план — 14 октября 2026, доставка L06:**
[полный цикл, карта входов, очередь и критерии окончания](LOBBY_EXECUTION_PLAN_RU.md).
L01–L05 сохраняют ранее установленную реализацию и отдельную приёмку.
[L06: вход в работающую игру](LOBBY_LIVE_JOIN.md) реализован: разрешение хозяина
в Lobby options, Join game now для допущенного зрителя, подготовка через общий
контроль текущей игры и свободный порт без нового запуска хозяина.
Обычная сборка установлена и запущена на Flip/Odin; SHA и проверка первого кадра — в документе L06.

Через живой Fluxer и upstream RetroArch relay проверен настоящий NES Pong/FCEUmm:
одиночный запуск дольше прежних 75 секунд, первый P2, независимый ввод гостя,
его выход и повторный вход при неизменном PID хозяина, запрет новых подключений
и возврат в ту же комнату. Проверочный клиент использовал настоящие игровые
службы и эмулятор; это не проверка всех ручных входов производственного интерфейса.
Две портативки находились в одной внешней сети. Доказательства других платформ
и разных сетей не подменяются этим результатом.

Проверки L06: Windows/ARM — 30 Lobby + 31 GameParty + 45 RuntimeMultiplayer;
интерфейс — 50 проверок, 0 QML warnings, касание и контроллер в модели UI.
Точные установленные сборки NES/FCEUmm, SNES/Snes9x, Mega Drive/Genesis Plus GX
ограничивают доступность. Настоящий первый P2 в этом проходе проверен только на NES.

**Текущий L07:** [допуск лобби и устойчивый P2](REMOTE_PLAY_TRANSPORT.md#допуск-лобби-и-устойчивый-p2--14-октября-2026)
реализованы как общие службы и рецепт RetroArch. Роль/место/поколение ограничивают
локальное разрешение; уход гостя отзывает его. Порядок устройств больше не выбирает
игровой порт; SDL/bridge учитывают исходное имя ядра. Сборка установлена на обеих
портативках, Windows 2/2 и ARM 3/3; дополнительные IPC-проверки — 7/7.
[Падение при завершении захвата исправлено](GAMESCOPE_CAPTURE_LIFECYCLE.md):
обе установки прошли закрытие/повторный захват без потери игры и композитора.
Добавлен [ICE/DTLS-кандидат автоматического соединения](REMOTE_PLAY_TRANSPORT.md#автоматическое-соединение-и-шифрованный-путь--14-октября-2026):
семь сетевых проверок и парное сравнение Moonlight на Flip/Odin пройдены. Ключи
выдаются внутри шифрованного канала; отдельный helper доставлен на обе портативки.
Signaling теперь связан с доверенным контекстом лобби и общим организатором:
[первый запуск Remote Play, проверки и доставка](REMOTE_PLAY_LOBBY.md).
Через живой Fluxer проверены приглашение, подготовка, одна игра хозяина и клиент
без ROM у гостя, освобождение P2 без остановки хозяина и возврат комнаты к сбору.
**Уточнение владельца 14 октября:** Remote Play работает напрямую P2P;
зрительская трансляция остаётся через Fluxer. Собственный TURN, VPS или платный
relay не являются обязательной частью поставки; сервер владельца хранит бота,
а не игровой видеопоток. Публичный STUN нужен только для обнаружения адресов.
Сети, запрещающие прямое соединение, остаются явным ограничением.
**Цель владельца — публичная ранняя бета:** двойные/тройные роутеры и прочие
сложные NAT отложены и не блокируют её выпуск. Оставшиеся обязательные работы
сосредоточены на согласованном игровом цикле, защите прогресса и установке/
обновлении с восстановлением. Остальной принятый набор платформ сохраняется.
Прямое подключение со STUN/Trickle ICE установлено на обеих портативках;
9 транспортных проверок, ARM 7/7 и новый парный цикл через Fluxer пройдены.
**Повторное подключение доставлено:** через Lobby options / Join game now гость
возвращается после выхода и потери транспорта; игра хозяина не перезапускается.
На Flip/Odin через живой Fluxer проверены оба возврата, запрет новых входов и
неизменные PID/start-time/ticket хозяина. ARM 7/7; обычная сборка установлена на
обеих. [Доказательства и пределы](REMOTE_PLAY_LOBBY.md#повторный-вход-без-перезапуска-хозяина--14-октября-2026).
**Следующее конкретное действие:** дополнительные одновременные места Remote Play
через общий допуск и привязки портов; затем сохранённая приёмка.
L07 целиком не завершён: разные сети, системная поставка Iris и физическая приёмка открыты.
Прежние результаты не повторять без причины.
[Парная проверка P2, игрового звука и отзыва ввода пройдена](REMOTE_PLAY_TRANSPORT.md#восстановление-и-парная-игровая-проверка--14-октября-2026).
Гость на Odin двигает P2 в Pong на Flip; Sonic передаёт настоящий звук. Показ
оболочки хозяина отзывает ввод, возврат возобновляет его после нейтрали; выход
гостя сохраняет игру. Это диагностический цикл с автоматическим SDL-вводом,
а не полная физическая/сетевая приёмка или готовая кнопка в лобби.
Обе портативки восстановлены, защита оболочки и audio selector установлены
и проверены на обеих. Финальный ARM CTest — 3/3, процессы/временные ключи убраны.
[Геометрия кадра и IDR исправлены](ARMADA_IRIS_ENCODER.md): временные модули
прошли проверки Flip/Odin; при потере кадра 180 клиент получил IDR 182 и продолжил
поток. Изображение NES Pong на Odin проверено без полосы и сжатия.
Это диагностический стенд, не установленный Remote Play. Постоянная поставка
системного исправления остаётся необходимой частью L07. На Flip штатный драйвер
возвращён; Odin восстановлен перезапуском после зависания выгрузки в IRQ.
Штатные модули подтверждены на обеих. Горячую замену Odin не повторять; сохранить этот
дефект в системной приёмке. Прежняя обязательная передача игрового видео через
L05 отменена владельцем 14 октября; голос/зрители сохраняются.
Затем L08 — сквозная приёмка. Просмотр сам по себе управления не выдаёт.
Не повторять ранее доказанный gameplay без изменившегося кода или конкретного риска.

Сохраняются: остальные полные игровые пути L01/L02, физическое управление/HUD,
человеческая речь/гарнитуры, разные сети, тяжёлые сцены, несколько издателей,
обычный клиент Fluxer, публичный Live-каталог и новая временная дружба через API.
[L05](LOBBY_MEDIA.md) и [L04](LOBBY_RECOVERY.md) хранят прежние доказательства;
этот проход их не повторяет и не закрывает весь мультиплеер.
Старые записи очереди ниже — история; следующим этапом является L07.

**13 October Social follow-up:** Recently together is implemented as the fifth
Social face after Lobby, with L2/R2 and direct touch selection, account-scoped
local encounter history and explicit profile/friend actions. Automatic temporary
friendship creation/acceptance/cleanup and Keep-as-friend are now implemented
with an account-local ownership journal, bilateral release and late-reply guards.
Existing friends/blocks/requests are preserved. Paired live acceptance and
handheld delivery remain pending. The owner now explicitly authorized existing
runtime bindings and the full entry/overlay cycle before those devices return.
See [current behavior and target](LOBBY.md#недавно-вместе-и-временная-дружба--уточнение-13-октября-2026).
Device delivery and live provider acceptance remain pending.

**13 October source/server checkpoint:** the directory bot is running under the
owner's main Fluxer community; live publish/move/restart/withdraw/republish/close
passed. Shared lobby coordination, Social catalogue/inbox/Home/DM entry, seats and
QWERTY/Russian keyboard are implemented and checked on PC. Runtime preparation
and launch now have a trusted local service interface with per-player launch
acknowledgements. The later L01/L02 source increment binds RuntimeMultiplayer
through the existing GameParty services; handheld acceptance remains pending. The shared conversation
pane is embedded in Lobby, with library/live-Home context entry. Handheld and
paired provider acceptance remain open. The owner will
turn both handhelds on in the evening; do not poll SSH or repeat gameplay before
then. [Evidence and next acceptance](LOBBY.md#реализация-и-проверка--13-октября-2026).

**Owner lobby/core priority, 13 October 2026 (latest):** deliver the shared
lobby foundation and keyboard before resuming platform multiplayer integration.
Use a dedicated Social Lobby tab (L2/R2 and touch), game-free creation, stable
membership across games, per-game seats/readiness and all-Ready automatic launch.
Fluxer community categories/channels group genres/topics; each lobby is an
announcement, not its own category/channel set. The owner authorized initial bot
hosting on their server. Preserve existing transports and prior checks. Read
[the accepted contract](LOBBY.md); implementation, deployment and gameplay proof
remain separate. This supersedes older multiplayer-first scheduling below.

**Owner PS2 decision, 12 October 2026 (latest):** implement native game online
through ordinary ARMSX2 networking first: installed DEV9 capability, shared
invitation/launch bindings and bounded actual online evidence. Native LAN alone
does not prove game-server online. PS2 delivery remains unverified.

"Hotseat" was the owner's term for couch multiplayer with separate pads on one
virtual console, each player using their own handheld; use "Play on one console"
going forward. Compare ready-made PS2 solutions separately, with no architecture
selected. P1 handoff is not the requested mode. LRPS2 debugging/promotion remains
stopped; older PS2 both-mode requirements below are superseded. Keep prior checks
as historical evidence, without replaying them or counting handoff as couch play.

**Owner priority, 12 October 2026:** missing universal platform connections and
shared invitation/launch/input/Home/exit bindings are next; PS2 with both input
modes remains required. Ordinary PSP is sufficient. Defer private PSP parties,
the complete Hotseat catalogue and fine per-game work. Preserve completed runtime
evidence and the Wii U/Vita scope; PS3/Switch excluded. Older PSP-private-next
instructions below are superseded.

**12 October PS2 universal preparation:** source bindings now reuse the shared
invitation/launch/input/temporary-save lifecycle, including both controller modes.
The separate native candidate reduced history memory but still failed the minimum
speed/guest-unload gate; no playable PS2 bundle is admitted or installed.
[Exact evidence and next gate](emulators/lrps2.md#universal-integration-preparation--12-october-2026).
PS2 remains required. Preserve this source preparation and fix/replace the runtime
before promotion; do not repeat unchanged probes or resume deferred private PSP,
full catalogue or fine per-title work. Wii U retains its external account/console
prerequisites; PS3/Switch remain excluded.

**12 October N64 private Online delivery:** the previously installed Nearby
route now also has automatic Online friend launch through private admitted
channels. Both handhelds passed ordinary consent/save-leave/Home/minimize/Exit;
separate forced-relay Mario Kart 64 gameplay passed. Personal baseline hashes
are unchanged. [Evidence](emulators/RMG.md#private-online-delivery--12-october-2026).
Continue PSP private admission and retained native/guest/Hotseat gates; wider
games, physical/distinct-network and production service acceptance remain open.
PS3/Switch excluded. This supersedes N64's earlier operator-service blocker.

**12 October Dreamcast delivery:** separate Flycast Dojo bundles now run on
Flip/Odin. Soul Calibur reached independent-input fights through native Nearby
and the upstream public Finnish relay. Ordinary Online friend invitation,
fresh-progress consent, save/leave, Home/minimize and repeated launch/guest Exit
passed; the final bundle automatically closes the peer too. Both temporary
sessions were cleaned and bounded ordinary-save hashes are unchanged. The
launched game now takes priority over a pending compatibility scan. Physical,
sustained/wider-game, distinct-network and public-relay release dependency gates
remain open. [Evidence](emulators/flycast.md#handheld-runtime-continuation---12-october-2026).
Next: actual PS2 candidate feasibility and ordered Hotseat; retain DS/Wii/3DS/
Wii U/Vita and other accepted gates, excluding PS3/Switch. No issue is closed.

**12 October N64 Nearby delivery:** the portable native RMG bundle is installed
on Flip/Odin. Mario Kart 64 reached a paired race with independent virtual-pad
input; ordinary Nearby invitation, save/leave barrier, Home, minimize and guest
Exit also passed, closing both copies. Profiles now distinguish Nearby and Online
availability, including library badges and reverse Join. N64 sessions disclose
fresh temporary progress and preserve ordinary saves. Vulkan paraLLEl-RDP lacks
required SSBO support on Flip; this matched bundle uses upstream OpenGL/HLE.
The operator internet service, larger groups and physical/distinct-network gates
remain open. [Evidence](emulators/RMG.md#handheld-runtime-continuation---12-october-2026).
Continue the remaining accepted routes through Wii U/Vita, excluding PS3/Switch;
DS's retained gates and both Hotseat modes remain unfinished. No issue is closed.

**12 October ordinary DS delivery:** two-player Nearby and Online friend
invitations now launch the installed native DS runtime on Flip/Odin, with explicit
save-copy consent, normal Home/minimize/exit and protected own-save return.
Mixed fresh/existing and two different ordinary save paths passed; the earlier
60,017-frame paired race/soak remains separate gameplay evidence. Reverse Join
pins the reviewed game/party/account, and the common prepared-command launcher
now validates its final executable after preparation. Both devices are quiet.
[Evidence and exact builds](emulators/melonds.md#ordinary-handheld-delivery--12-october-2026).
This supersedes the earlier staged/no-profile statements below. Four-player DS,
broader saves/games, final frontend presentation and distinct-network/physical
acceptance remain open. Continue the accepted multiplayer scope through Wii U
and Vita, excluding PS3/Switch; no issue or whole multiplayer block is closed.

**Owner continuous multiplayer work, 11 October 2026:** continue the accepted
scope through Wii U and Vita, excluding PS3/Switch, while the owner is asleep.
Keep devices quiet and preserve personal saves. The earlier-family recheck is
recorded; DS now has a local player frontend and actual two-console Mario Kart DS
wireless gameplay on Flip/Odin, plus matching 23,500-frame recorded replays.
This supersedes the frontend-not-started status below,
not the remaining owned-save, GameParty, Internet or physical acceptance gates.
The ongoing evidence lives in [melonDS maintenance](emulators/melonds.md);
PROJECT_STATUS.md and CHANGELOG.md track user-facing results. No DS profile or
issue is declared complete by this local development increment.

**Owner recheck before DS, 11 October 2026:** verify earlier PS1/N64/Dreamcast/
GameCube/PSP work on the updated handhelds before resuming DS. Current ARM64
client is installed on both; PS1 CTR and the existing Melee profile reach paired
gameplay with independent controls. Lumines reaches a paired arena on the second
native challenge. Full RMG/Dojo native builds pass, while their
installed runtime/operator-service and gameplay gates remain open. See the
[bounded device record](MULTIPLAYER_DEVICE_RECHECK_20261011.md) for exact evidence,
PSP results and remaining acceptance. This scheduling correction supersedes the
immediate DS continuation below; no issue is closed.

**DS native workers delivered for verification, 11 October 2026:** ARM64
core/JIT, bounded input queues, per-machine threads and joined teardown are built.
Two/four local machines pass short Mario Kart DS startup replay checks on both
updated handhelds; see [exact evidence](emulators/melonds.md). These are headless
development probes, not an enabled DS invitation or wireless match. Next are
the native frontend, owned-save startup/state parity and secure GameParty
integration. Retain actual wireless, network/physical and sustained-load gates.
This supersedes the earlier workers/ARM64 deferral below; no issue closes.

**Devices available and OS prerequisite complete, 11 October 2026:** the owner
powered on both handhelds and requested Armada updates before emulator tests.
Both now boot `20261010.94f52f8` on the retained testing channel; TrainerOS starts
automatically with its unchanged installed binary, and system/input services pass
bounded checks. [Maintenance evidence](ARMADA_UPDATE_20261011.md) retains rollback
and acceptance limits. Resume DS native workers/ARM64 verification; physical,
actual wireless gameplay and distinct-network acceptance are still separate.

**DS native core preparation, 11 October 2026:** the bounded
[melonDS DetMP port](emulators/melonds.md) reuses the existing deterministic
wireless implementation on pinned desktop upstream, with cancellation and
queue/reply bounds. Windows static core/JIT build and synthetic lifecycle checks
pass. MP-06 remains incomplete: next are mirror workers and #164 safe state parity,
then existing GameParty/secure transport integration. Linux/ARM64, actual games
and handheld acceptance remain deferred; no DS invitation capability is enabled.
Retain PS2/Hotseat, Wii/3DS/Wii U/Vita and all earlier acceptance. No issue closes.

**PSP source continuation and PS2/Hotseat gaps, 11 October 2026:** generic raw
UMD ISO invitation/launch preparation now reuses PPSSPP's native Ad Hoc relay,
with preserved exact Lumines policy and ordinary PSP storage. Windows client
build and three scoped suites pass; native Linux/device checks are deferred. Wider games,
private native rooms, device delivery and network acceptance remain open;
see [maintenance](emulators/ppsspp.md). The [bounded mechanism review](HOTSEAT_RUNTIME_RESEARCH.md)
does not deliver PS2 or Hotseat: LRPS2 is a candidate; DEV9 and Dolphin Golf Mode
are not exclusive shared-pad netplay. These retained gaps do not block the
owner's independent offline preparation. Next is DS route selection/integration,
then the remaining Wii/3DS/Wii U/Vita routes; PS3/Switch remain excluded. No issue
or MP block closes, and the comprehensive Hotseat catalogue remains unfinished.

**Dreamcast source preparation, 11 October 2026:** [Flycast Dojo](emulators/flycast.md)
now has two-player CHD invitation/launch integration using native GGPO/relay and
private VMU/firmware copies. Local adapter/lifecycle and native relay-client
harness checks are separate from a full emulator build or game. Linux/ARM64
Dojo build, operator relay and handheld/gameplay acceptance remain open. Next
independent preparation is the retained PS2/GameCube/Hotseat block; keep DS/PSP,
3DS, Wii U and Vita in scope, PS3/Switch excluded. No MP block or issue closes.

**N64 source preparation, 11 October 2026:** [RMG native netplay](emulators/RMG.md)
now has automatic private rooms, roster-gated start and an isolated runtime launch
through the existing invitation/save lifecycle. Four lobby clients (including
TLS) are protocol evidence only. Full Linux/ARM64 RMG build, operator internet
service and device/gameplay checks remain open. Dreamcast is the next independent
family preparation; preserve MP-02 acceptance, Hotseat and the full Wii U/Vita
scope. No multiplayer block or issue is closed by this source increment.

**Owner Hotseat clarification, 11 October 2026:** deliver both simultaneous
independent controllers and ordered one-pad handoff on PS1/PS2/GameCube. Compile
the comprehensive [game/mode catalogue](HOTSEAT_GAME_CATALOG.md), beginning with
those systems and covering the accepted platform scope. The catalogue, live
handoff and remaining runtime implementations are separate unfinished outcomes;
the source-delivered shared-console profiles do not complete them.

**Owner continuation, 11 October 2026:** prepare/implement the remaining emulator
multiplayer families through Wii U and Vita, including PS1/PS2/GameCube Hotseat.
PS3/Switch are excluded. While both handhelds are off, use bounded relevant local
checks and defer deep gameplay/device/separate-network acceptance. Remaining
MP-02 acceptance does not block independent later-family implementation in this
pass. [Preparation register](MULTIPLAYER_PREPARATION.md) separates working code,
upstream candidates and remaining acceptance; no entire multiplayer block closes.

**Offline source delivery, 11 October 2026:** MP-03 adds PS1 shared-console
netplay through PCSX ReARMed/RetroArch; the existing MP-05 Dolphin bridge now also
admits generic raw GameCube discs. Existing invitations, independent controller
ports and private temporary cards are reused. The Windows client builds and
`retroarch`, `standalone`, `runtime_multiplayer` tests pass (3/3); source exports
and the adapter registry validate. ARM build/installation/gameplay are deferred.
Hotseat #126, PS2 and remaining families through Wii U/Vita are still open; next
independent work is MP-03's N64 runtime selection and retained family integrations.
See [evidence and limitations](MULTIPLAYER_PREPARATION.md#local-verification--11-october-2026).

**Universal invitation handoff, 10 October 2026:** an incoming multiplayer
invitation can stay as a corner badge while its recipient keeps playing and
saves. It survives an independent ordinary-game exit; explicit "Saved — join"
closes the current game through the protected exit/finalization path before
accepting. The shared flow reuses existing emulator networking. A trusted local
native hot-join contract is separate from prepared emulator launches; no native
provider is added here. [Implementation, delivery and limits](INVITATION_HANDOFF.md)
records the handheld checks. MP-02/#107 remain open and next; #77 stays queued.

**MP-02 GBA Wireless Adapter, 10 October 2026:** bounded Mario Golf/Mega Man
profiles reuse unchanged gpSP netpacket through the existing invitation flow.
Mario Golf reached paired RFU shots over the public relay on Flip/Odin; distinct
own profiles returned and were read by ordinary mGBA with diagnostic input.
The final installed build also fixes host departure stranding a guest's Exit
confirmation, with a real Nearby regression. [Evidence and limits](emulators/gpsp.md#wireless-adapter-integration---10-october-2026)
retain four-client/wider-game/separate-network/physical acceptance, ordinary input
and the existing requirement to close a guest's current game before joining.
MP-02 remains open and next; do not advance to MP-03 or close #107.

**MP-02 GBA parties, 10 October 2026:** generic GBA Link now prepares 2-4
accepted players automatically, with private per-member battery exchanges,
frozen seats and each player's own screen/controls/protected save return.
The existing mGBA Splitscreen and RetroArch mechanisms are reused. Four linked
machines pass actual Kirby match/rollback checks; the app and core are installed
on Flip/Odin. [Evidence and limits](emulators/mgba-splitscreen.md#two-to-four-player-delivery---10-october-2026)
keep real four-client, separate-network and physical acceptance open. Continue
MP-02's remaining mechanisms/transactions before MP-03; this is not universal
game compatibility or completion of MP-02.

**MP-02 generic GBA pairs, 10 October 2026:** mGBA Splitscreen now reuses the
existing invitation, automatic opaque SRAM preparation and RetroArch network
session. Kirby reached linked two-player gameplay with independent screens,
controls and normal own-save return on Flip/Odin. Ordinary solo mGBA and the
reviewed gpSP protocols remain. [Evidence and exact builds](emulators/mgba-splitscreen.md)
distinguish relay, final-build Nearby and deferred separate-network/physical
acceptance. Generic GBA no longer depends on a franchise whitelist. Four-player
preparation, special mechanisms and retained MP-02 transactions remain open;
continue MP-02 before advancing to MP-03.

**Adapter encapsulation implemented, 9 October 2026:** Pokemon and generic
experiences now live in `src/adapters/<id>/` behind the shared host/provider
contracts. Independent modules run through the actual host and rendered UI;
compatibility manifests support alternative identity sources and scoped art/sprite
packs. [Evidence and remaining acceptance](UX_FRAMEWORK.md) separates the final
handheld delivery from the initial UX build. The module-boundary correction is
resolved; MP-02 is next, with its timer WIP preserved and excluded from this build.

**Owner deferred follow-up, 9 October 2026:** investigate Odin GPU hangs and
consider a maintained patch once the responsible component is established.
[Observed fence wait, recovery and investigation scope](UX_FRAMEWORK.md#owner-follow-up-odin-gpu-hangs-9-october-2026).
This is later work, not a claim that the driver hang was fixed or a new priority
ahead of the accepted MP-02 queue.

**MP-02 Red journey delivered, 10 October 2026:** the save-prompt timer,
failed-finalization/cancelled-launch guards and progressive game availability
are installed on Flip/Odin. The real Red invitation, Cable Club trade, independent
SRAM return and ordinary Gambatte readback passed on both. Test save overrides
were removed after verification; original registrations and quiet device state
are restored. [Evidence and remaining acceptance](HANDHELD_MULTIPLAYER.md) keeps
MP-02 open for Gen II/TCG transactions, Gen III battle/cross-edition and Advance
Wars proof, plus the retained external gates. The timer work is now delivered;
older instructions to preserve it as uncommitted describe the previous state.

**MP-02 universal-runtime continuation, 10 October 2026:** an additional GB/GBC
route now reuses DoubleCherryGB's existing linked pair and RetroArch rollback/relay.
Ordinary online invitations led to Into the Blue gameplay with separate screens
and controls on Flip/Odin. Eligibility is based on non-battery cartridge hardware,
not franchise names. General independent battery-save integration remains the
next missing capability; retain the existing per-core mechanisms and all earlier
transaction/network acceptance. [Evidence](HANDHELD_MULTIPLAYER.md) does not mark
MP-02 complete. Reuse existing solutions throughout the remaining emulator queue.

**MP-02 battery-pair delivery, 10 October 2026:** supported generic GB/GBC battery
cartridges now use SameBoy's native linked subsystem and RetroArch rollback/relay,
with automatic opaque SRAM preparation after invitation consent. From Below
Pocket completed a two-device match through the ordinary UI, then both assigned
saves returned and were read back in ordinary Gambatte. Original preimage backup
and separate player settings were verified. [Evidence and boundaries](HANDHELD_MULTIPLAYER.md)
supersede the preceding battery-integration gap, not the rest of MP-02. Generic GBA
and retained transaction, capacity, distinct-network and physical acceptance stay
open. Known failed Hnefatafl ROM/core compatibility is excluded, not generalized
into a franchise whitelist. Reuse existing solutions throughout the emulator queue.

**UX-02 implementation delivered, 9 October 2026:** adaptive Home/game slots,
universal profile, Options/Select, live Game Options/minimize, contextual Together
and passive shell notifications/shared inbox are installed and running on Flip/Odin.
[Implementation, checks and limits](UX_FRAMEWORK.md) records real solo and paired runtime
handoffs, the gameplay inbox fallback and deferred physical/TV/future-route acceptance.
Odin now boots directly into TrainerOS; immediate activation and a real reboot
were verified. Resume MP-02; preserve its separate uncommitted timer work.
The dated planning-only statements below are superseded; no issue is closed by
this delivery and README screenshots remain unchanged.

**Latest owner scope, 9 October 2026:** document adaptive Home as part of the
[#174–178 experience architecture](EXPANSION_174_178.md), then complete the entire
shared framework/UI/control block before MP-02 and other queued expansion.
This follow-up edits documentation only. Adaptive Home and both game-oriented
slots are now first-class UX-02 requirements; Options-only delivery is insufficient.

**Current owner request, 9 October 2026 — UX-02 planning only:** think through
the complete interaction/session map and reconcile documents before implementation.
[Precise map](UX_OPTIONS_MAP_RU.md) and [#161–173 register](EXPANSION_161_173.md)
define the next work. No UI/runtime build, deployment or issue closure in this pass.
After the next continuation: UX-02A–D, then MP-02; preserve its uncommitted timer work.
Older dated "next" notes below are evidence, not the active scheduling decision.

**Owner library correction, 9 October 2026:** Collections replaces Worlds;
Pokemon opens games directly. Personal manual/dynamic and automatic collections
share the library, while Home/Y uses global recent games without L2/R2 collection
cycling. [Contract and delivery](SERIES_COLLECTIONS.md). This requested increment
precedes resuming MP-02; preserve its timer work. The common UI is now specified
by UX-02 above. Older scoped Home/Y navigation below is superseded.

**Owner download regression correction, 8 October 2026:** unresolved ScreenScraper
editions wait independently while other jobs continue. Downloads sorts active
work first and successful items beneath Downloaded; background scraping no longer
captures shell touch/controller navigation. [Evidence and device application
boundary](SCREENSCRAPER.md) records this bounded correction. MP-02 remains next.

**Owner quick scraping follow-up, 8 October 2026:** Start → Scraping now selects
several systems or all populated supported systems before Download, using the
existing settings and Downloads queue. [Evidence](SCREENSCRAPER.md) separates the
new delivery from the earlier integration. Wider menu/notification reorganisation
is deferred. Preserve the MP-02 timer work and return to MP-02 after this task.

**Owner alpha milestone, 8 October 2026:** MP-02 remains the next implementation
outcome. Finish all accepted multiplayer families, Hotseat, native online Link
and guest sessions, then REL-01 and the image/installer alpha for testers and
enthusiasts. [The active queue and alpha criteria](ROADMAP.md#alpha-milestone--2026-10-08)
supersede older all-product/Pack-Studio-before-distribution scheduling below.
Reviews, remaining product work, Pack Studio/ROM assets and full update/final
acceptance remain after alpha; multiplayer-critical dependencies stay before it.
Preserve timer work, existing evidence and owner-deferred tests. This planning
change delivers no runtime, installer or alpha artifact and closes no issue.

**Delivered owner detour, 8 October 2026 — #65 ScreenScraper:** developer access is
approved; explicit game/system/collection/library jobs, Settings for login and
downloads, and independent display choices are installed on Flip/Odin. Both
completed a real one-game download and rescan. [Evidence and remaining
checks](SCREENSCRAPER.md). Resume MP-02 with its timer changes preserved.
Earlier access-pause and next-task statements below are historical.

Owner-added Downloads popup is included: Start quick controls, thumbnails and
platform badges, queue reorder, pause/resume/cancel and persistent-in-session
progress after closing the popup. ScreenScraper is its first connected provider;
future download sources and restart restoration are not claimed as implemented.

Owner clarification, 8 October: current display controls cover game pictures and
data only. Selectable library layouts (for example grid/list) remain a later
improvement; they are not part of #65 and do not replace the retained MP-02 queue.

**Product reconciliation, 8 October 2026:** the [complete product vision in
Russian](PRODUCT_VISION_RU.md) and [#136-160 register](EXPANSION_136_160.md) add
temporary guest content, OddCrate, universal reviews, independent I/H/S/G update
ownership and true handheld/TV layouts. These are target requirements, not new
installed capabilities. The owner's personal-content correction replaces the
per-file verified-rights gate with contractual responsibility; signed-policy,
consent, integrity and session safeguards remain. **MP-02 stays next**, with its
uncommitted runtime timer work preserved. Older dated checkpoints below retain
evidence; their superseded targets do not override this revision.


**Compact Social revision installed, 8 October 2026:** the first UX-01
composition was rejected for oversized controls and insufficient chat space.
The replacement compact interface and native Fluxer profiles are now installed
on Flip/Odin; see [current interface and evidence](SOCIAL_COMPACT.md). Resume
**MP-02**, preserving its uncommitted timer work. Earlier functional evidence
below remains valid; the old layout is superseded. Owner physical/visual
acceptance and the remaining #135 gates stay open.

**UX-01 delivered, 8 October 2026:** [installed interface, device walkthrough and
test evidence](SOCIAL_UX.md). Resume MP-02. Touch and controller are parallel
inputs; #135 and the earlier deferred acceptance remain open.

**8 October 2026 live issue review:** #118–135 are new relative to the previous
#117 record. #70's distribution boundary and comments on #101/#112 were also
reviewed. [Acceptance register](EXPANSION_118_135.md) maps all 18 additions;
[ROADMAP](ROADMAP.md#active-execution-queue--2026-10-08) is the updated single
execution queue. **Owner priority: UX-01 interface reconciliation first, then
resume MP-02.** Earlier dated resume instructions below retain their evidence
but no longer select the next task. No issue was closed and no runtime acceptance changed.

Owner-approved website detour, 2026-10-07: the mouse-driven interactive HTML tour
is prepared in [web-demo](../web-demo/README.md), with an external explanation
panel, fictional original content and optional animated examples. See
[scope/evidence](WEBSITE_DEMO.md). This is a browser artifact; native MP-02 remains
unfinished and its in-progress source changes are preserved. Resume MP-02 next.

Owner correction, 2026-10-07: the ten collection-card illustrations were replaced
with independent environments, installed on both handhelds and visually reviewed
on Flip. [Evidence](SERIES_COLLECTIONS.md#artwork-and-private-content). Names,
identities and existing README captures are retained. Resume MP-02.
The final queue is R18 Pack Studio/art packs -> R18a/R18b optional ROM-native
assets -> R15 installable image -> R16 OTA/rollback -> R17 final acceptance.
This supersedes earlier Studio-last ordering without dropping any scope.

Public-readiness maintenance, 2026-10-06: the owner requested a bounded repository
cleanup before continuation. [Delivered scope and open decisions](PUBLIC_READINESS.md)
track #115 (license), #116 (public assets), #117 (clean contributor build), and the
updated #62 documentation reconciliation. Runtime code and README screenshots
remain unchanged; GitHub Actions is disabled. The owner subsequently adopted
GPL-3.0-or-later; #115 retains full release redistribution acceptance. Resume MP-02
below after this pass.

Updated 2026-10-09. Execution order lives in [ROADMAP](ROADMAP.md#active-execution-queue--2026-10-09).
This register decomposes accepted work; it is not a second roadmap or a promise
that each subtask receives a separate turn. Issues and existing evidence retain
their full acceptance. A checkbox means the stated outcome is delivered, not
merely that code exists. Do not close a parent block from partial child evidence.

## Delivered checkpoint: #114 — series collections

The original 4 October series delivery is retained as evidence, superseded for
navigation by 9 October Collections: personal manual/dynamic and automatic views,
direct Pokémon games, global Home/Y recents, no Home L2/R2 collection cycling.
Eight additional series, ten original illustrations and a bounded server-ROM selection remain.
No ROM relocation or identity reset. Remaining #90 semantic adapters stay open.
Acceptance/evidence: [Series collections](SERIES_COLLECTIONS.md).

## Delivered: UX-01 — integrated Social and multiplayer interface

Deliver #127–134 as one player-visible journey using current providers/runtimes:

- [x] Unified DM/group Messages, Communities and Discover; preserve remembered
  conversation, drafts/history/unread and migrate old Groups/Search routes.
- [x] Predictable A/B/X/Y/Select grammar, explicit Send and parallel touch controls.
- [x] One compact Together area, independent call state and shared party surface
  from Home/Social with roster/readiness/access/Start/leave/rejoin.
- [x] People-first invitations with deduplicated proven identity and automatic
  route selection; clear group details and contextual options.
- [x] Consistent incoming requests/missed notifications with existing consent,
  expiry, protected-operation and action-focus safety.
- [x] Applicable #135 actual invitation/gameplay/Home/rejoin and native-activity
  walkthroughs on Flip/Odin, necessary regression checks, real screenshots and
  complete paired delivery/commit/push. Future runtime/Hotseat and owner-deferred
  gates stay open; no mock-only UI completion or new networking framework.

[Detailed scope](EXPANSION_118_135.md#ux-01-one-social--together--party-journey).
Preserve the unfinished MP-02 source change. Its underlying timer regression
may be validated if the changed UI exercises it; no cable-research detour before
this outcome. Then resume the full MP-02 block below.

## Delivered implementation: UX-02 — adaptive framework, UI and controls

Implementation is installed on both handhelds. [Evidence](UX_FRAMEWORK.md) separates
source, automated checks, actual runtime input and remaining owner/TV/audio acceptance:

- [x] UX-02 foundation: experience descriptors/resolver and host boundaries;
  adaptive Home and slots A/B; extract verified Pokémon flows; separate universal
  identity/profile; useful generic fallback and two nonproduction descriptor/routing fixtures.
  Preserve semantic route migration, exact-build capabilities and stale-result guards.
- [x] UX-02A: shared Shell Options from every primary and focused-object Select,
  experience-supplied contextual actions, stable target/focus, global profile entry,
  parallel touch/controller and the new panels' Handheld/TV layout preference.
  Physical TV/remote/CEC and owner visual acceptance remain open.
- [x] UX-02B: Game Options for the actual process, safe minimize → Social → same
  game return; runtime capability gates, input/save/audio/time protection and #49 Exit.
- [x] UX-02C: contextual Together entries plus passive shell notifications, gameplay inbox fallback and
  common inbox, preserving provider/party/call ownership and clean capture.
- [x] UX-02D: integrated existing-route acceptance and control documentation;
  retain future-runtime, real TV, human audio and owner-deferred gates explicitly.

Each functional outcome includes required integration, bounded checks, both
available handheld deliveries and commit/push. Full details and failure cases:
[UX map](UX_OPTIONS_MAP_RU.md), [Options register](EXPANSION_161_173.md),
[experience/Home register](EXPANSION_174_178.md). These are dependencies inside the
whole accepted block; finish the framework/UI/controls before returning to MP-02.
Do not replay passed tests to fill an externally blocked acceptance turn.

<a id="current-task-mp-02--independent-handheld-link"></a>

## Retained next runtime task: MP-02 — independent handheld link

The owner resumed the interrupted handheld work on 4 October. Complete the
existing invitation/launch/own-save/exit journey for reviewed GB/GBC/GBA routes.
Implementation, actual cable exchange and readback evidence are recorded in
[Handheld multiplayer](HANDHELD_MULTIPLAYER.md). Do not substitute shared controls
for independent machines, or call the whole family complete from a handshake.
MP-01's distinct-network and real-user gates below remain open and owner-deferred;
no classic-system implementation or passed test loop is restarted.

### MP-02 delivery checkpoint

- [x] Pinned ARM64 gpSP/DoubleCherryGB installed on both, ordinary cores retained.
- [x] Own-save staging, guest `.netplay` handling, atomic normal return and recovery.
- [x] Emerald cable trade on copies; ordinary mGBA readback on both.
- [x] Installed GBA Home invitation, accepted online party, automatic linked
  launch, own Continue, peer departure and normal Home exit on both.
- [x] Red relay pair, existing guest save, changed guest SRAM finalization and
  ordinary Gambatte readback; reusable adapter and per-emulator update records.
- [x] Gen II own-clock bridge, recoverable SRAM/RTC return and Gold normal-game
  readback on both handhelds; per-core patch/rollback and reusable adapter export.
- [ ] Gen I/II/TCG cable transactions and GB ordinary UI journey. Gold clock
  continuity is not a trade or full family acceptance; do not repeat it as a pass.
- [ ] GBA battle/cross-edition and Advance Wars gameplay; four-client activity.
- [ ] Owner-deferred distinct-network/multi-user and human voice acceptance.

Resume here after UX-01; do not restart passed Emerald/Red checks without a concrete change.

7 October continuation: the clock/save-return outcome is delivered; next finish
the GB invitation-to-cable-transaction-to-ordinary-play journey. Keep the three
baseline `persistence_process` subprocess-start failures recorded in
[the evidence](HANDHELD_MULTIPLAYER.md) for the contributor-build follow-up (#117);
the affected adapter/lifecycle suites passed. Do not turn that separate baseline
into repeated multiplayer checks or mark the broader suite green.

8 October handoff: preserve the existing uncommitted connection-timer/queued-launch
fix in `RuntimeMultiplayer.cpp`. It prevents networking timeout/polling while
the player is still saving/exiting the ordinary game. Before continuing, inspect
the interrupted session and validate wait/cancel/restart behavior against that
actual change, then complete the GB cable transaction and ordinary readback.
The issue/plan review is not build, deployment or transaction evidence.

## MP-01 — retained classic RetroArch and shared online-party acceptance

**State:** existing NES/Mega Drive/SNES exact-game routes are implemented and
have paired gameplay evidence. Shared multi-user and distinct-network acceptance
is incomplete; implemented profiles are now available in ordinary builds under
the owner's 4 October correction. Missing acceptance must not prevent the owner
from running those checks. No missing NES transport
implementation was identified in the 4 October continuation. Do not describe
MP-01 as implementing NES again. The shared core/platform expansion is now
implemented; its broader coverage is not universal game-level runtime proof.
See [profiles and evidence](RETROARCH_MULTIPLAYER_PROFILES.md).
**Parent:** communication block 1, [#107](https://github.com/EriArk/TrainerOS/issues/107),
expanded by the owner's company, capacity, reverse-request and oldest-first rules.
**User outcome:** invite an online friend or request to join their game; play
with the supported number of people; leave/rejoin without damaging ordinary
play or interrupting the group's call. A company can run independent parties.
No player IP entry, router setup or VPN administration.

### Implemented baseline — retain, do not restart

- [x] Existing exact NES, Mega Drive and SNES profiles with paired invitation,
  independent gameplay and explicit loss/exit/rejoin evidence.
- [x] Shared coordinator for invitations/reverse requests, capacities/seats,
  company access and independent parties. Automated and bounded two-user UI
  evidence exists; it is not full four-user acceptance.
- [x] NES Four Score port assignment and independent P3/P4 emulator input.
- [x] Concurrent runtime room lifetimes and ordinary core-option isolation.

- [x] Replace the test-title-only boundary with reviewed classic core/platform
  profiles using the existing invitation/relay flow; retain exact profiles,
  private settings/saves and controller exceptions. Generic baseline is two;
  metadata-selected three/four seats now exist for Snes9x and PCE-family layouts.
  Established exact NES Four Score remains four. See the profile record for
  source-backed coverage versus actual device evidence.
- [x] Three/four-seat controller preparation and three-seat admission/rejoin
  regression coverage. European Battle Circuit now prepares a private four-player
  cabinet: four relay clients reached the same level with four active characters.
  Other FBNeo cabinets remain capped at two. No four-user acceptance implied.
- [x] Replace unrelated whole-FBNeo-BIOS comparison with pinned title/parent/BIOS
  dependencies, retaining core/content identity and ordinary files. The paired
  cabinet probe used ordinary device BIOS paths successfully. Source disk inspected;
  no BIOS copy needed. [Evidence](RETROARCH_MULTIPLAYER_PROFILES.md#title-dependencies-and-arcade-cabinet-checkpoint--2026-10-04).

### Remaining acceptance and release work

The following items are acceptance gaps, not evidence that those mechanisms are
absent. Fix concrete failures found while completing them; do not invent a new
implementation task when an external condition is all that is missing.

- [ ] Close the actual multi-user TrainerOS acceptance: invitations, accepting and
  declining, reverse join requests, capacity/readiness, seat assignment and
  authorized group entry. Reuse the existing coordinator/UI; fix demonstrated
  gaps instead of building another lobby or generic session framework.
- [ ] Close parallel-party acceptance through actual TrainerOS accounts and
  normal UI: members select the intended party; capacity, permissions, starts,
  exits and rejoining are independent. Company size is not game capacity. One
  shared group call survives game/party changes without new microphone consent.
- [ ] Complete the first exact route across separate internet connections using
  Invite/Accept/Join, real independent gameplay and clean Home return. Verify
  bounded peer departure/loss/rejoin on that route; fix failures before enabling
  it. A public relay reached from one home network is not this acceptance.
- [ ] Reconcile concrete route differences across the reviewed classic profiles:
  effective capacity, compatibility, saves/session settings, installed profile
  availability and cleanup. Reuse passed paired gameplay and isolation evidence;
  only exercise differences or changed behavior. Avoid testing every title.
  The concrete FBNeo unused-BIOS mismatch and European Battle Circuit cabinet
  defect are now resolved. Other cabinets/peripherals require their actual mode;
  do not promote them from counts alone or turn remaining scope into every-ROM
  tuning. Non-FBNeo optional-firmware matching remains conservative.
- [x] Remove the global developer opt-in so installed supported profiles can be
  tested through ordinary invitations. Preserve per-game/runtime eligibility,
  invitation consent, capacity and save/session protection; unavailable runtimes
  do not become supported simply because the blanket switch is gone.
- [ ] Resolve remaining acceptance and release claims from actual evidence,
  without hiding implemented functionality until the owner can test it. Keep
  the matrix, maintenance record and both-device deliveries current.

**Owner deferral, 2026-10-04:** separate-network testing waits until the owner is
home. Do not request another network switch or repeat same-router tests while
waiting. Multi-user acceptance and release enablement remain open separately.
The owner subsequently clarified that other classic RetroArch systems come
before handheld link, grouped by their common mechanism. That generic expansion
is implemented in this delivery. Before MP-02, resolve concrete classic-family
gaps and the shared release decision; do not schedule a separate transport
project for every console or repeat passed scenarios. The latest FBNeo probe
established four active players after independent coin/start input through the
public relay; it does not replace full TrainerOS multi-user/company acceptance.

**Completion:** all applicable items above are fulfilled and remaining scope is
explicitly resolved. An unavailable external condition keeps MP-01 open; it is
not a new feature called "test multiplayer." Do not claim all of block 1 complete
while later accepted emulator families remain unfinished.

### Evidence already available — do not repeat as new stages

| Available evidence | Source / limit |
| --- | --- |
| NES two-player online invitations, reverse request/group entry, independent controls and loss/exit/rejoin | [NES](NES_MULTIPLAYER.md); same-router devices, not distinct internet connections |
| SNES Contra and Mega Drive Streets of Rage 2 independent P2 and loss/exit/rejoin | [Runtime evidence](EMULATOR_MULTIPLAYER.md); reuse for unchanged paths |
| Four Score seats, P3/P4 independent input and reversed join order | [NES](NES_MULTIPLAYER.md), `a2b5d12`; four emulator clients, not four real TrainerOS users |
| Concurrent public-relay rooms, independent lifetimes, private core options and ordinary settings preservation | [RetroArch record](emulators/retroarch.md), `b83d4bc`; temporary fixture clients, not complete company UI acceptance |
| Generic classic profiles, content/BIOS/track compatibility and renamed-title invitations | [Profiles](RETROARCH_MULTIPLAYER_PROFILES.md); automated coverage plus bounded FBNeo connection/launch, not all-title gameplay |
| Shared group-call synthetic continuity through gameplay/loss/exit/rejoin | [Game parties](GAME_PARTIES.md); human speech/listening still separate |

### External acceptance conditions

| Missing condition | What it closes | What it does not justify |
| --- | --- | --- |
| Actual compatible TrainerOS clients/accounts for the claimed larger and concurrent sessions | End-to-end admission, UI and lifecycle beyond two players | Calling fixture processes extra users; redoing already passed emulator seats |
| Two independent internet connections, with target devices reachable for recovery; owner deferred until home on 4 October | Actual internet route, admission and recovery | Treating two devices behind one router as separate-network proof, or repeatedly requesting a switch during the deferral |
| Owner available to listen/speak, with usable microphone input | Remaining block 3 human audio quality/interoperability acceptance | Repeating synthetic audio checks or blocking unrelated permitted implementation |

Verify availability when needed; these are requirements, not claims that today's
devices are unreachable. Do not create accounts, send messages or alter unrelated
network infrastructure without applicable authorization. If no independent work
remains, state the prerequisite and wait instead of looping.

## Subsequent block 1 tasks — oldest first

Every row includes automatic invitation/compatibility/preparation, actual game
input, graceful exit/recovery, save/config protection, clear unsupported states,
both-device delivery and a maintained emulator record. Internet is primary;
LAN and the accepted router-free nearby route remain acceptance, not substitutes.
Use [standard runtimes](EMULATOR_STANDARD.md), the [route matrix](EMULATOR_MULTIPLAYER_MATRIX.md)
and [accepted experience](MULTIPLAYER_EXPERIENCE.md). No new general transport
survey unless a concrete route requires it; virtual LAN remains conditional.

| ID | Whole outcome | Dependency / retained work |
| --- | --- | --- |
| MP-02 | GB/GBC/GBA supported internet link between separate emulated handhelds | MP-01. Resolve actual link support per runtime/game; shared-controller netplay does not meet this requirement. |
| MP-03 | Selected PS1/N64 multiplayer routes integrated into the same invitation experience | MP-02. Mechanism and capacity follow the runtime; unsupported link/game modes remain explicit. |
| MP-04 | Dreamcast supported online/LAN mechanism with automatic connection | MP-03. Reuse Flycast package/maintenance work; installation is not multiplayer. |
| MP-05 | PS2/GameCube viable multiplayer routes | MP-04. Preserve existing ARMSX2; do not imply every PS2 game supports online play. Reuse Dolphin paired proof; finish its remaining internet/capacity/recovery gates. |
| MP-06 | DS/PSP viable online/link routes | MP-05. #162–166 native Linux deterministic mirrored DS Local Wireless over internet: core/privacy → GameParty adapter → real gameplay/recovery, 2-player before proven 3/4-player budgets. Reuse PPSSPP proof and retain its loss/admission/isolation/distinct-network gates. |
| MP-07 | 3DS and eligible newer families, then reconcile runtime-family coverage | MP-06. Source-backed support decisions and representative complete routes, not every ROM or obscure platform. Block 1 also retains HS-01 and external gates. |
| HS-01 | Live Hotseat Relay through the existing party/Home journey, #126 | After MP-07. One logical pad, exclusive ordered control token, disconnect/leave/rejoin and safe persistence. Actual title/device handoffs; no offline turns. [Acceptance](EXPANSION_118_135.md#hs-01-live-hotseat-relay). |

These are task families inside the accepted block, not permission to split a
promised complete delivery into a succession of handshake-only sessions. At each
start state the actual outcome being completed; do not promise every platform
until the remaining support and prerequisites are understood.

## Retained communication tasks

| ID | State / complete outcome | Acceptance source |
| --- | --- | --- |
| COM-02 | Delivered everyday messenger baseline; retain residual accepted expansion and fix real regressions without restarting the baseline | [Social](SOCIAL.md), #100/#101 and queued expansion |
| COM-03 | Open: remaining notifications/calls acceptance, including real speech and background conversation; human checks deferred by owner | [Media/voice](SOCIAL_MEDIA_VOICE.md), #102/#103 |
| LINK-04 | Queued after block 1: native online battle/trade/sale/gift with consent and protected two-endpoint settlement/recovery | [Emerald Link](EMERALD_LINK.md), #104/#105/#109/#110, #93/#94 trust prerequisites |
| UX-01 | Current owner priority, before MP-02: whole Social/Together/invitation/party/group/notification reconciliation, #127–134 | [New UX scope](EXPANSION_118_135.md#ux-01-one-social--together--party-journey); preserve providers, independent calls and save consent |
| REVIEW-05 | After runtime/Hotseat and LINK-04, reusing UX-01 evidence: complete #113 reviews inside Properties, residual accepted messenger expansion and #135 whole-journey acceptance | [Reviews](ADVENTURE_REVIEWS.md), [Social](SOCIAL.md), [new UX scope](EXPANSION_118_135.md#ux-01-one-social--together--party-journey). Replies/quotes, reactions, mentions, person card and persistent composer remain; no history search |

## Distribution outcomes added by #118–125

These are queued after the preserved product/Pack Studio/ROM-asset stages; they
do not displace MP-02. [Detailed acceptance](EXPANSION_118_135.md#distribution-one-release-two-delivery-paths).

| ID | Complete outcome | Dependency |
| --- | --- | --- |
| REL-01 | #119 machine-readable release and ownership state shared by every delivery operation | Stable R1–R14/R18/R18a/R18b inputs and maintained emulator recipes |
| R15 + INSTALL-01 | #70 prepared image; #120–122 session/runtime/preflight/convergence and #124 safe uninstall under #118 | REL-01; verified Armada, SteamOS and Bazzite host/device capabilities; actual fresh install/use/recovery/uninstall proof |
| R16 + MAINT-01 | #71 compatible update/rollback on both paths; #123 health/repair after host changes | Image/installer baselines; shared migration/ownership state; recoverable host update and owned-drift checks |
| R17 | #125 equivalent image/installed outcomes, upgrade proof and all existing final acceptance | Prior delivery paths; #115 redistribution, #116 assets and #117 clean contributor build gates |

SteamOS/Bazzite physical target access is not established by the current two
Armada handhelds. Record unavailable host acceptance precisely; do not claim
support from a manifest, package installation or synthetic preflight alone.

## Preserved project backlog

**Historical issue review, 2026-10-04:** #114 was the only new issue since #113; it had no
comments at review time. Record it under R7/#90: Worlds collection entry,
collection-specific organization and context, capability-driven experience,
lossless classification and handheld Back/A navigation. Acceptance is retained in
[the franchise register](EXPANSION_69_90.md#exact-integrations-and-franchise-experiences--7476-82-8990).
It explicitly does not reorder implementation. MP-02 stays current; no new
runtime or broad visual rewrite starts from this planning review.

The [R1–R18 queue and preservation map](ROADMAP.md#unified-execution-order--existing-work-and-new-issues)
remain intact. After communication, perform remaining work in that order, never
repeat delivered foundations: shop/adapter/navigation residuals; companion/save
features and RA; varied-game adapters; runtime readiness and Steam; device/input,
atmosphere and portable Trainer; maintenance; first boot/help; artwork/sprite packs
and Pack Studio; conditional ROM extraction/integration; shared release state;
image and installer; updates/rollback and repair; final equivalence/acceptance.
R7a/R18a/R18b and older U/P acceptance remain included.
ScreenScraper waits for owner access. Suspend stays deferred. Preserve Playroom
and current artwork. This register removes no issue or acceptance requirement.

## Updating this register

At each delivery update the affected task's state and remaining checkboxes, with
a link to evidence and source revision. Record actual implementation, actual
device behavior and external acceptance separately. Keep detailed logs in their
existing feature documents; do not prepend another competing queue. Before the
next pass ask: what new user outcome remains, and why would any passed check need
repeating? If the answer is only unavailable acceptance, report that honestly.
