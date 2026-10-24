# Первая ранняя бета: установщик поверх Armada

**24 октября 2026:** владелец разрешил начать визард, установщик Armada и OTA
TrainerOS в отдельной работе. Прежняя просьба ждать дополнительных требований
ниже историческая. Портативки сейчас заняты ремонтом мультиплеера владельцем:
не подключаться и не выполнять установку/проверки на них. Текущий first-run
consumer и остаток — [FIRST_RUN](FIRST_RUN.md).

Установщик и OTA должны использовать общий учёт принадлежащих TrainerOS
компонентов, совместимых версий и миграций. Нужны проверенные подпись/хеши,
продолжение загрузки, подготовка до активации, ожидание безопасного завершения
игры/save transaction, восстановление предыдущей совместимой версии и данных.
Обновление самого TrainerOS отдельно от штатного обновления Armada и игр Crates.
Наличие обновлений Crates не считается готовностью Software OTA.

Принято владельцем 14 октября, подтверждено последующими решениями. Публичная
бета не выпущена; это требования, не результат чистой установки. Текущая работа
в [CURRENT_TASKS](CURRENT_TASKS.md); реализация общей подготовки пакета описана ниже.

- Первая бета распространяется установщиком поверх существующей Armada, не
  образом ОС. Понятный `.desktop` или аналогичный ярлык; окончательный пакет ещё
  предстоит выбрать. Минимум действий, понятные инструкции, автоматические
  подготовка окружения, установка зависимостей/TrainerOS и настройка.
- Цель — совместимые Armada-портативки, не только Flip/Odin; это не утверждение
  проверки всех моделей. Самостоятельно определить расположение **ОС** на microSD
  или внутренней памяти. Путь библиотеки игр не является расположением ОС.
- Не форматировать и не переносить ОС. Сохранить игры/firmware, saves, профили,
  настройки и сторонние приложения. Ручная настройка разработчиком по SSH не
  заменяет пользовательский сценарий.
- Во время загрузок/распаковки/долгой установки UI отзывчив, показывает реальный
  этап/прогресс, понятные ошибки и восстановление. Не выдумывать точный процент.
- Повторная установка, прерывание, repair и безопасное удаление только своих
  компонентов требуют проверок. Пользовательские данные не откатываются вместе
  с программой. Каналы Armada OS, TrainerOS Software и Crates имеют разных владельцев.
- Требуемые системные исправления, включая Iris при выбранном Remote Play scope,
  должны поставляться восстанавливаемо поверх Armada. Не повторять горячую
  выгрузку зависавшего драйвера Odin ради обхода; собственный образ не навязывать.

## Managed installer и OTA — продолжение 24 октября 2026

Это **исходники и локальные проверки**, не выпущенная бета и не проверенная
установка на Armada. Описания прежних этапов ниже сохраняют датированную
историю; их фразы «ещё нет activation/host binding» заменены этим разделом.

### Установка и владение

- `host.py` читает ОС, ARM64 и реальные Armada capabilities; `findmnt /usr` и
  inverse `lsblk` определяют физический ОС-диск. Для MMC учитывается SD/MMC из
  sysfs, потому что removable flag не всегда различает карту и eMMC. Неизвестная
  или неоднозначная топология не выдаётся за поддержанную. Библиотека игр для
  определения ОС-диска не используется; форматирования/переноса ОС нет.
- `installer.py --gui` использует штатные KDE kdialog/qdbus: канал, версия,
  download size, место/ОС-диск, подтверждение, прогресс/отмена и повтор. Проверяются
  существующие system prerequisites и зависимости signed executable через ldd.
  **Автоматической установки недостающих host packages пока нет**: отсутствующая
  зависимость останавливает путь до активации с ошибкой. Qt plugins, runtime-набор,
  ABI и лицензии полного ARM64 payload требуют release-проверки.
- Личный root `~/.local/share/traineros-software`: доверенные config/key,
  фиксированный `bootstrap/packaging`, `versions/<signed-digest>`, cache/channels
  и один атомарный `receipt.json` с active/previous/health. Пользовательская БД,
  first-run state, аккаунты, ROMs и saves находятся **вне** этого root.
- `initialize` не заменяет существующую trust/data identity. Каждая установка
  проверяет подпись, inventory и текущие SQLite/first-run schemas, копирует в
  неактивную версию и fsync/rename публикует receipt. Повтор той же версии безопасен;
  другая версия не может переопределить тот же sequence. Прерывание до receipt
  оставляет старую активную версию; после него — новую с unconfirmed health.
- Собственные ярлыки и системные session-файлы имеют отдельные журналы хешей.
  Системная часть запрашивает обычную pkexec-авторизацию. Все её назначения
  проверяются до первой записи. `--managed` добавляет выбор сессии и сохраняет
  boot/sleep defaults; старые helper-only/default режимы остаются отдельными.
  Нельзя молча присвоить файлы существующей ручной установки или чужие изменения.
- Repair повторно получает **точный подписанный установленный manifest**, лечит
  повреждённые application/cache files и сохраняет карантин при прерывании.
  Rollback выбирает предыдущую собственную версию только при совместимости с
  текущей БД/настройками. Он не восстанавливает старые пользовательские данные.
- Uninstall проверяет ownership всех удаляемых ярлыков/системных файлов и удаляет
  собственные версии/кэш. Перед удалением сессии выбранный TrainerOS boot default
  нужно штатно сменить на Desktop/Steam. Изменённые чужим владельцем файлы
  сохраняются с отказом операции. Данные, trust и recovery bootstrap сохраняются;
  для повторной установки остаётся исходный архив Setup.
- `audit` проверяет **software files/data compatibility**, не сертифицирует весь
  host. Состояние session integration отражается отдельно как recorded/not recorded;
  полнота привилегированной установки должна проверяться на Armada.

### Применение из Settings

Обычная source-сборка остаётся без обновлений. Managed launcher передаёт свой
root; Settings асинхронно читает реальный receipt, путь текущего executable,
личную папку, target и схемы. Только совпавшая managed installation получает
источник/кнопки Apply, Previous version, Repair, Check installed files и Clear cache.

Apply/rollback/repair требуют отдельного подтверждения и повторной проверки
существующих runtime/save/file/service guards. Запрос содержит выбранную версию,
исходный active digest и время. Shell выполняет обычный защищённый выход;
только специальный exit 75 разрешает launcher выполнить запрос. Отмена выхода
возвращает UI, обычный выход/сбой не применяет оставленный запрос. Перед применением
снова проверяются канал/совместимость/подписи; старый запрос не переигрывается.

Linux launcher удерживает межпроцессную runtime lease и становится
[subreaper](https://www.man7.org/linux/man-pages/man2/PR_SET_CHILD_SUBREAPER.2const.html).
Он ждёт завершения дочерних игр даже после выхода оболочки, не посылая им kill.
Первый реальный QML frame подтверждается через READY_FD и записывается отдельно
от факта установки. Падение shell/launcher оставляет boot fence: без доказательства
освобождения runtime обслуживание до нового boot запрещено. Автоматического
отката или watchdog crash-loop recovery нет; ручной Recovery остаётся отдельным
путём. Реальные внешние service-launched процессы требуют проверки на Armada.

Downloads хранит локальный job (`channels/ui-job.json`), выбранный канал и дату
проверки. После перезапуска задача возвращается **Paused**, даже если раньше была
Prepared: явный Resume заново проверяет канал и кэш. Поиск обновления не качает
payload, фон не возобновляет пользовательскую задачу. Очистка download cache не
удаляет installed versions/данные; неизвестные файлы кэша сохраняются. Исторические
неактивные версии пока не удаляются автоматически.

KDE progress/cancel использует [официальный DBus интерфейс kdialog](https://develop.kde.org/docs/administration/kdialog/).
Пропавшее окно считается отменой загрузки; в фазе commit Cancel скрыт. На длинных
проверках/системной авторизации показан этап без выдуманного процента. Фактический
KDE/dialog/Polkit путь здесь не запускался.

### Подготовка публикации GitHub

`publisher.py build` создаёт **локальные** assets: signed `release.json`,
`traineros.tar.gz`, channel JSON, `TrainerOS-Setup.tar.gz` с публичным ключом,
corresponding source, notices/LICENSE, provenance и `publication.json` с хешами.
Нет автопубликации, генерации production key или GitHub Actions. Закрытый ключ
не входит в Setup. Entry script имеет Unix line endings и executable bit,
в том числе при сборке архива на Windows.

Издатель передаёт reviewed ARM64 ELF payload и provenance с точным source commit,
командой сборки, версиями зависимостей, путями corresponding-source archive/notices
и `redistributionReviewed: true`. Это явное утверждение издателя; инструмент не
может сам доказать лицензии/полноту исходников или пригодность бинарника на железе.

```sh
python3 packaging/software/publisher.py build \
  --payload /reviewed/payload --output /new/release-assets \
  --private-key /private/signing.pem --public-key /reviewed/trusted-public.pem \
  --version "$VERSION" --sequence "$SEQUENCE" --source "$COMMIT" \
  --repository EriArk/TrainerOS --tag "$TAG" --channel beta \
  --channel-sequence "$CHANNEL_SEQUENCE" --issued "$ISSUED" --expires "$EXPIRES" \
  --sqlite-min 0 --sqlite-max "$SQLITE_SCHEMA" --first-run-min 0 --first-run-max 2 \
  --provenance /reviewed/provenance.json --notes /reviewed/notes.txt
```

После проверки contents/хешей издатель загружает immutable assets в GitHub Release,
проверяет анонимные скачивания через `publisher.py verify-remote --directory ...`,
и **только затем** обновляет соответствующий signed channel в ветке `updates`.
Зеркало JSON в Release само по себе не обновляет канал. Verify-remote проверяет
assets, отдельно сообщает, что channel branch ещё не проверена. Не публиковать
обещание работающей беты до аппаратной приёмки. Большие файлы и маленькие feeds
размещаются на GitHub, сервер владельца не используется для их раздачи.

### Локальная проверка текущего продолжения

Windows/UCRT64 Qt 6.11.1: native build; 19 updater QtTest checks (включая
init/cleanup) и QML-сценарий Settings/Downloads/подтверждения Apply прошли.
17 first-run checks и QML до Home подтверждают новый офлайн-путь. Сборка
`BUILD_TESTING=OFF` прошла: first-run/OTA smoke options отклоняются, тестовые
строки отсутствуют в executable. Две целевые
проверки ScreenScraper identity/credentials прошли; прежние несвязанные failures
расширенного набора сохранены в FIRST_RUN, весь проект не объявляется зелёным.

19 release + 29 channel tests переиспользуют настоящие OpenSSL ключи, tar,
файлы, межпроцессные locks и локальный HTTPS. Новый `SoftwareInstallationTests.py`:
22 passed, 2 Linux-only skipped. Покрыты interrupted copy/receipt/retry, schemas/WAL,
compatible rollback, tampering/repair cache, ownership/drift/uninstall, реальный
запрет CLI maintenance при занятой lease, boot fence, stale request, публикационный
архив без закрытого ключа, отмена progress и ОС-диск SD/eMMC. Linux fsync/power loss,
subreaper, root install и настоящий KDE не выводятся из этих Windows результатов.

### Что ещё не принято

- Production public key/rotation, реальные Beta/Preview feeds и public Release.
- Полный ARM64 payload с совместимыми Qt/runtime dependencies и redistribution
  acceptance. Неавтоматизированные недостающие host packages и миграция ручных
  установок. Bootstrap/root helpers не обновляются через Software OTA: новая
  версия bootstrap требует отдельно проверенного установщика/миграции ownership.
- Linux subreaper/flock/first-frame и отказ по service-launched runtime; две
  настоящие subprocess-проверки добавлены, но на Windows пропускаются явно.
  Локальный Linux runtime в этом проходе недоступен; это не зелёная Linux-приёмка.
- Чистая microSD/internal Armada, KDE/Polkit, реальные игры/защищённые saves,
  обрыв питания/носителя, повтор/repair/uninstall/rollback и физическое управление.
  Никаких подключений к портативкам в этом проходе не выполняется.

## Общая подготовка Software-пакета — 24 октября 2026

[packaging/software/releases.py](../packaging/software/releases.py) реализует
общую основу installer и OTA: `build`, `verify`, `prepare`. Это рабочий CLI
подготовки, **не готовый пользовательский установщик/обновлятор**.

- Из явно выбранного application tree собирается tar.gz и подписанный Ed25519
  manifest: продукт, версия, последовательность, исходный commit, target
  `linux-aarch64`, диапазоны SQLite/first-run, размеры, хеши и разрешения файлов.
  Пакет не содержит установочных скриптов, исполняемых этим механизмом.
  Порядок файлов и метаданные tar/gzip нормализованы; повторная упаковка того же
  payload/контракта/ключа даёт одинаковые байты в локальной проверке. Это ещё не
  доказательство воспроизводимой сборки самого ARM64 executable из исходников.
- Повторно используются canonical JSON, OpenSSL Ed25519 и atomic/fsync из
  `packaging/crates/catalogue.py`; каталоги, ключи, активные receipts и жизненный
  цикл Crates не меняются. Для запуска CLI оба исходных файла должны быть рядом
  в их текущем дереве `packaging/`. Это не отдельный скопированный crypto-stack.
- Доверенный публичный ключ передаётся bootstrap-стороной. Подпись и структура
  проверяются **до** скачивания; ключ из скачанного manifest не принимается.
  Production key, его ротация и публикация release feed ещё не настроены. Тестовые ключи
  создаются только во временных каталогах теста и не публикуются.
- Загрузка HTTPS, включая проверку redirect, продолжает `.part` через Range.
  Ответ 200 вместо 206 начинает файл заново; неверный диапазон, превышение
  размера и несовпадение SHA-256 отклоняются. Байты/этапы идут JSONL в stdout,
  исключения/отмена не выдают `prepared`. Это данные для будущего UI/Downloads.
- Распаковка идёт в собственный временный каталог без `extractall`: только
  перечисленные обычные файлы, без symlink/hardlink/devices, path traversal,
  дубликатов и конфликтов file/directory. После проверки каждого файла каталог
  публикуется атомарным rename под digest подписанного manifest. Повторное
  использование снова проверяет полный inventory/хеши, на Linux — права.
- Один процесс подготовки на cache; проверяется свободное место. Обычная отмена
  удаляет временную распаковку и сохраняет частичную загрузку для повторения.
  После аварийного завершения процесса может остаться неактивный `.extract-*`;
  автоматическая очистка таких каталогов и repair повреждённого cache ещё нужны.
  Низкоуровневый `releases.py prepare` получает **локальный** подписанный manifest;
  получение через удалённый канал и проверка выбора теперь описаны ниже.

Пример для разработчика (пути/ключ — локальные параметры, не URL выпуска):

```sh
python3 packaging/software/releases.py verify --manifest /path/release.json --key /path/trusted-public.pem
python3 packaging/software/releases.py prepare --manifest /path/release.json --key /path/trusted-public.pem --cache /path/software-cache
```

`prepared` означает только целостный кандидат. Здесь ещё нет active pointer,
активации, rollback, миграции данных, root-helper или смены загрузочной сессии.
Поле target не является проверкой текущего host; SQLite/first-run ranges —
подписанный контракт издателя. Новый channel consumer сравнивает их с переданными
bootstrap значениями; чтение фактического host/БД и lifecycle binding ещё нужны.
Инсталлятор должен определить реальную Armada/ОС-диск, использовать существующий
`packaging/session/install.py` для системной интеграции и получить разрешение
на безопасную активацию у владельцев runtime/save. Игра может пережить падение
оболочки: отсутствующий GUI PID не доказывает завершение игры/транзакций.

Проверки: `tests/SoftwareReleaseTests.py`, 19 локальных тестов Windows/UCRT64
с настоящими OpenSSL-подписями, файлами, tar и межпроцессной блокировкой прошли.
Проверены tampering/wrong key, duplicate JSON, interrupted/Range/ignored Range,
неверные размер/хеш, опасные архивы, неполный inventory, недостаток места,
повторная проверка изменённого cache, отмена распаковки/повтор, запрет HTTP redirect
и encoded body, CLI, идентичная упаковка при другом системном времени.
HTTP-ответы — fixtures; живой CDN,
Linux durability/power loss, ARM64, настоящая установка и runtime пока не проверены.

## Каналы GitHub и получение кандидата — 24 октября 2026

По решению владельца тяжёлые Software-пакеты/установщики хранятся в **GitHub
Releases**, скачивание идёт напрямую оттуда. Сервер владельца не раздаёт OTA.
Небольшие подписанные `beta.json`/`preview.json` также хранятся на GitHub;
для изменяемого указателя предусмотрена отдельная ветка `updates` с raw HTTPS URL.
Ветка/публичные endpoints пока **не созданы**, ключ и release assets не опубликованы.
Это отделяет обновляемый указатель от неизменяемых пакетов конкретного тега.
Прямые ссылки поддерживаются [GitHub](https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases).
GitHub `latest`, порядок commits/main и наличие prerelease сами не выбирают версию.
Ни GitHub-аккаунт игрока, ни API token, ни GitHub Actions этому пути не требуются.
Actions по прежнему решению владельца остаётся выключен.

[channels.py](../packaging/software/channels.py) — общий CLI consumer для будущего
установщика и Settings → System → Updates, в том же S13, что и `releases.py`:

- `build` создаёт локальный подписанный файл канала из проверенного release
  manifest. Канал содержит Beta/Preview, отдельный возрастающий номер публикации,
  время выпуска/истечения, текст изменений, точную версию/sequence/архитектуру,
  минимальную версию протокола updater, URL/размер/SHA-256 manifest. Сам manifest
  отдельно подписан и закрепляет архив. Встроенного production key нет.
- `check` получает только небольшие метаданные; проверяет обе подписи,
  точное совпадение manifest и канала, target, minimum updater и диапазоны данных.
  Результаты: `available`, `up-to-date`, `blocked`; при сетевой/проверочной ошибке
  возвращается ошибка, а не ложное «обновлений нет». Архив не скачивается.
- Только явный `prepare` после свежей повторной проверки канала вызывает общий
  HTTPS/Range/download/extract механизм. Получаем `prepared`, всегда
  `installed: false`. Ошибка, отмена или несовместимость не активируют программу.
  Артефакт из прежнего `check` не разрешает обойти истёкший/изменённый канал.
- Подписанное состояние канала атомарно сохраняется отдельно от cache пакетов,
  под межпроцессной блокировкой. Более старый номер канала, изменение содержимого
  под тем же номером, снижение номера Software или замена manifest той же версии
  отвергаются. Для Beta/Preview история независима; установленная более новая
  версия при переходе Preview → Beta остаётся установленной без загрузки старой.
  Повреждение состояния — ошибка, не молчаливый сброс защиты.
- Срок действия канала ограничен 31 днём: издатель должен обновить подпись/номер
  даже без нового пакета. Истечение блокирует новое получение OTA, **не запуск
  установленного TrainerOS**. Неверные часы объясняются ошибкой; свежесть без
  доверенных часов не гарантируется. Bootstrap может задать минимальные feed/release
  sequences; удаление локального состояния теряет локальную историю защиты.
- HTTPS redirects допускаются (включая GitHub CDN), downgrade к HTTP, credentials
  в URL, неожиданные статусы/кодирование, oversized/truncated metadata запрещены.
  JSON с duplicate keys отвергается. Доверие остаётся у bootstrap-ключа, не CDN.

Действующий #154 разрешает фоновые проверки, но **скачивание и установка только
по действию пользователя**. Ранее обсуждавшееся автоматическое скачивание не
включено. Сам CLI не создаёт таймеров; локальный Settings consumer добавлен ниже.

Пример для разработчика (environment содержит **явно выбранные доверенные**
параметры; это не готовая команда установки публичной версии):

```sh
python3 packaging/software/channels.py check \
  --feed-url "$FEED_URL" --key "$PUBLIC_KEY" --channel beta \
  --state "$CHANNEL_STATE" --installed-sequence "$INSTALLED_SEQUENCE" \
  --target linux-aarch64 --sqlite "$SQLITE_SCHEMA" --first-run "$FIRST_RUN_SCHEMA"

# Только по явному Download; все проверки канала повторяются.
python3 packaging/software/channels.py prepare \
  --feed-url "$FEED_URL" --key "$PUBLIC_KEY" --channel beta \
  --state "$CHANNEL_STATE" --installed-sequence "$INSTALLED_SEQUENCE" \
  --target linux-aarch64 --sqlite "$SQLITE_SCHEMA" --first-run "$FIRST_RUN_SCHEMA" \
  --cache "$SOFTWARE_CACHE"
```

`installed-sequence=0` означает отсутствие Software-установки, не разрешение
игнорировать существующую. Target/schema/installed sequence пока явно передаёт
caller; будущий bootstrap обязан читать их из проверенного host/receipt/данных.
Cache и channel state — разные каталоги. Издатель использует `channels.py build
--help`: отдельные public/signing key, уникальный output, UTC issued/expires,
номер канала, manifest URL и optional UTF-8 notes. Команда ничего не загружает в GitHub.

Порядок будущей публикации: собрать/проверить точный ARM64 payload и notices/source,
подписать manifest с URL конкретного release tag, загрузить архив/manifest в Releases,
проверить анонимное чтение и целостность, затем опубликовать подписанный канал.
Не менять байты опубликованного пакета под тем же release sequence. Новый hotfix
получает новый sequence; обновление срока канала увеличивает только channel sequence.
Ключ подписи остаётся вне GitHub/устройства; bootstrap содержит только доверенный
публичный ключ и источник. Ротация/отзыв, снятие плохого кандидата без замены,
производственный bootstrap и проверенная процедура публикации ещё нужны.

**Проверено локально:** 28 `SoftwareChannelTests` + 19 `SoftwareReleaseTests`
прошли на Windows/UCRT64. Кроме подписей, файлов, persistence/lock и HTTP fixtures,
новый тест запускает настоящий loopback HTTPS server с временным TLS certificate,
проверяет redirect и отдельные CLI subprocess `check` → `prepare` без Authorization;
архив отсутствует среди запросов `check`. Это не проверка GitHub CDN или портативок.

**Остаток:** публичный ключ/bootstrap/feed и реальные ARM64 release assets,
host/storage/receipt probe, компонентные/инкрементальные пакеты, production binding UI,
безопасная активация/health check/repair/совместимый rollback и ARM64 приёмка.
Сейчас schema 1 содержит один application bundle; она не выдаёт поддержку всех
будущих runtime/policy/adapter-компонентов. `minimumUpdater` позволяет явно
отказать старому клиенту, но self-update этого клиента ещё не реализован.
Перед активацией потребуются повторная актуальность/совместимость и S03/S08 guard;
подготовленный cache сам по себе не даёт права менять работающую установку.

## Settings → System → Updates — подготовленный интерфейс

[SoftwareUpdateController](../src/features/settings/SoftwareUpdateController.h)
принадлежит S13, живёт внутри Settings и вызывает существующий `channels.py`
через асинхронный `QProcess`. [UpdatesPane](../src/qml/UpdatesPane.qml) встроена
в System; прежние аппаратные действия сохраняют номера, Updates добавлено четвёртым.
Новых менеджеров установки/скачивания или копий crypto-проверок нет.

- Страница показывает версию установленного Software, Beta/Preview, результат
  проверки, размер и отдельное прокручиваемое описание релиза как plain text.
  Текст подписанного канала не интерпретируется как HTML/QML.
- Проверка запускается отдельно от Download. Для настроенного источника есть
  переключаемая проверка раз в шесть часов; она не скачивает архив и сообщает о
  новой версии через общий inbox. Повторное уведомление той же версии в текущем
  процессе подавляется. Crates проверяется только при явном переходе в Crates.
  Download передаёт `--expected-sequence` показанного кандидата: если свежий канал
  уже указывает другой релиз, пакет не скачивается, требуется повторная проверка
  и выбор пользователем. Фоновая проверка не стирает существующее задание загрузки.
- Download публикуется в общем Downloads под `traineros-software`; Pause доступна
  на этапе передачи, Resume повторяет свежий channel check и использует `.part`.
  Cancel останавливает worker, не удаляя установленную версию/личные данные.
  Возможен сохранённый частичный файл для повторной загрузки, не скрытый autostart.
- Settings и Downloads управляют одним состоянием. Закрытие страницы не завершает
  процесс; Back возвращает на строку Updates в System. Канал нельзя менять во время
  процесса. Смена канала сбрасывает выбранного кандидата и требует новой проверки.
- Процент показывается только по реальным byte events текущего этапа; отсутствие
  байтов — неопределённый прогресс. Успех требует конечного JSON result, правильного
  exit code и `installed: false`: одно сообщение `phase=prepared` недостаточно.
  Ошибки, неожиданный вывод и таймаут не становятся «обновлений нет».
- Operating system и Crates показаны отдельно. Кнопок Apply/Rollback/Reboot нет:
  текущий механизм умеет только безопасно подготовить candidate. Нет управления
  процессами игры, сейвами, загрузочной сессией или host package manager.

**Граница подключения:** доверенный installer/composition должен вызвать
`configure(SoftwareUpdateSource)` с Python/helper, public key, URLs двух каналов,
раздельными state/cache, доказанной installed version/sequence, host target и
фактическими схемами данных. Они не задаются через QML, скачанный manifest или
пользовательскую форму. Production bootstrap/receipt probe ещё не готов, поэтому
обычная сборка **намеренно не вызывает configure**, показывает `Development build`
и «Updates are not available for this build yet», не выполняет сетевых запросов.
Тестовый fixture подключается исключительно под `TRAINEROS_UI_TESTS` в отдельном
smoke-входе; настоящая публичная загрузка из обычной сборки ещё не доступна.

В этом проходе не сохраняются candidate/job и last-check между запусками оболочки;
сохраняются только канал и предпочтение фоновых проверок. Проверенный Python cache
и replay state сохраняются своим владельцем, повторный Download использует их.
Production binding, восстановление представления job после перезапуска, явное
применение с S03/S08 guard и совместимый rollback остаются следующими частями.

**Локальная проверка интерфейса, 24 октября:** 29 `SoftwareChannelTests`
прошли, включая остановку при смене показанного release sequence и настоящий
локальный HTTPS/CLI check → prepare. `SoftwareUpdateTests`: 16 passed, 0 failed
(включая init/cleanup); проверены отсутствие auto-download, пауза/продолжение,
отмена, фоновые уведомления, несовместимость, ошибки запуска/протокола/места,
отсутствие ложного installed-success. QML smoke в полной оболочке прошёл без
QML warnings: SDL virtual controller + pointer, System/Back/notes, Download →
Pause → выход/возврат → Resume из общего Downloads → prepared, ошибка сети.
Кадры 960×540 и 1920×1080 просмотрены локально. UI/process-проверки используют
изолированный fixture, не GitHub CDN и не реальные аккаунты/портативки;
криптография и HTTPS проверяются отдельно Python-набором. Портативки по указанию
владельца не подключались и не изменялись.

Сборки Windows/UCRT64 с `BUILD_TESTING=ON` и `OFF` проходят. В обычном `OFF`
бинарнике нет OTA fixture/тестового входа; запуск с `--updates-smoke-test`
проверенно отклоняется как unknown option с exit 1. Это проверка production
границы сборки, не установочный ARM64 артефакт и не доказательство GitHub delivery.

## Приёмка перед выпуском

1. Ярлык на обычной Armada без частного окружения разработчика → рабочий first run.
2. Установка при ОС на microSD и во внутренней памяти, корректная идентификация дисков.
3. Отзывчивость на длинных этапах, ошибки/повтор/прерывание без ложного успеха.
4. Повторная установка/восстановление и доказательство сохранности личных данных.
5. Проверки на заявленных моделях/версиях, notices/corresponding source/материалы
   точного артефакта; [#115/116/117](PUBLIC_READINESS.md) остаются открытыми.

Образ Armada и SteamOS/Bazzite — будущие направления, не blockers первой беты.
Pack Studio, SPI и весь full-product backlog не входят автоматически в её состав.
Окончательный релизный список согласуется отдельно; защиту данных и recovery не снимать.
