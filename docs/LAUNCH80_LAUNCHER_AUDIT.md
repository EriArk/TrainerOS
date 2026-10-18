# LAUNCH-80 — сверка готовых механизмов запуска

18 октября 2026. Проверено покрытие всего локального каталога; поставка и первый
игровой цикл записаны отдельно ниже. Наличие файла не заменяет совместимость. Порядок работы —
[общая интеграция, затем игровые проверки](LAUNCH80_PLATFORM_REGISTER.md).

## Результат общего прохода

- 106 ID: 100 имеют кандидат Libretro, четыре уже подключены отдельным runtime;
  PS3/Switch включены владельцем позднее в этот же день для homebrew на Odin,
  их общая интеграция доставлена, ограниченные проверки записаны ниже. Все **72 различных ядра** из каталога найдены на обоих устройствах;
  для всех 72 найдены официальные `.info` Libretro. Это проверка наличия и
  контракта, не загрузки зависимостей/BIOS и не игровой совместимости.
- У всех 100 core-маршрутов уже общий запуск RetroArch с отдельными аргументами
  executable/core/content, а не 100 разных реализаций. Существующие настройки,
  save directories и Home/exit используются повторно.
- Четыре платформы без ядра в каталоге используют уже подключённые Dolphin,
  melonDS и управляемый Vita3K launcher. PPSSPP, ARMSX2 и Azahar также уже есть
  как standalone-альтернативы. Добавлены Cemu/DuckStation в общий адаптер,
  discovery, обновление готовности и передачу запуска существующему контроллеру.
- **Исправление предыдущего аудита:** Cemu/ARMSX2 имеют настоящие установленные
  ARM64 Libretro-библиотеки. Их привязки не признаны ошибочными. Автоматическое
  переключение имеющейся PS1/PS2/Wii U-библиотеки и перенос сейвов не выполняются.
- На каждом устройстве существуют файлы библиотеки для 28 платформенных ID
  (включая `ports` вне ROM-каталога). Отсутствие файла для остальных — пробел
  игрового прогона, а не повод вычеркнуть платформу.
- У Odin Sega CD/Neo Geo CD не настроены; текущий явный профиль RetroArch не
  содержит `discFirmware`, необходимого существующему проверенному CD-маршруту.
  Наличие BIOS и правильность ресурса надо проверить, не отключать защиту.

## Готовые механизмы и источники

- [ES-DE Linux systems](https://gitlab.com/es-de/emulationstation-de/-/blob/master/resources/systems/linux/es_systems.xml)
  и [find rules](https://gitlab.com/es-de/emulationstation-de/-/blob/master/resources/systems/linux/es_find_rules.xml):
  выбор runtime, отдельные аргументы, core/content, standalone-варианты.
- [Libretro core info](https://github.com/libretro/libretro-core-info): форматы,
  firmware, дополнительные ресурсы, типы ввода и требования графики.
- [Batocera system register](https://github.com/batocera-linux/batocera.linux/blob/master/package/batocera/emulationstation/batocera-es-system/es_systems.yml):
  идентичность платформ/семейств. Этот файл сам по себе не доказывает команды запуска.
- На обоих устройствах прочитаны custom systems ES-DE; на Odin также существующие
  Armada wrappers. Они не копируются поверх TrainerOS: часть содержит фиксированные
  пути и профиль конкретного устройства. Повторно используются подтверждённые команды.
- [Cemu CLI](https://github.com/cemu-project/Cemu/blob/main/src/config/LaunchSettings.cpp):
  `-f -g FILE`; [DuckStation CLI](https://github.com/stenzek/duckstation/wiki/Command-Line-Arguments):
  `-batch -fullscreen -- FILE`. Настройки/карты памяти остаются у текущей установки.
  [Cemu close](https://github.com/cemu-project/Cemu/blob/main/src/gui/wxgui/MainWindow.cpp)
  и [DuckStation close](https://github.com/stenzek/duckstation/blob/master/src/duckstation-qt/mainwindow.cpp)
  сверены для штатного запроса закрытия; фактический цикл новых standalone ждёт устройства.

## Все платформы

«Есть» в столбцах устройств означает наличие файла ядра, а не игровую приёмку.
Standalone отмечен отдельно. ES-DE — имя сопоставленной платформы; «core info»
означает прямое подтверждение общего Libretro-механизма без выдуманного ES-DE ID.
В исходниках новые standalone-альтернативы выбираются для новой настройки;
старые привязки не мигрируются. Версия установлена на Flip; Odin ожидается.

| ID | Общий механизм | Источник сопоставления | Ядро Odin / Flip |
| --- | --- | --- | --- |
| `3do` | RetroArch / `opera` | ES-DE `3do` | есть / есть |
| `amiga1200` | RetroArch / `puae` | ES-DE `amiga1200` | есть / есть |
| `amiga500` | RetroArch / `puae` | ES-DE `amiga` | есть / есть |
| `amigacd32` | RetroArch / `puae` | ES-DE `amigacd32` | есть / есть |
| `amigacdtv` | RetroArch / `puae` | ES-DE `cdtv` | есть / есть |
| `amstradcpc` | RetroArch / `cap32` | ES-DE `amstradcpc` | есть / есть |
| `apple2` | RetroArch / `applewin` | ES-DE `apple2` | есть / есть |
| `arcadia` | RetroArch / `amiarcadia` | ES-DE `arcadia` | есть / есть |
| `arduboy` | RetroArch / `arduous` | ES-DE `arduboy` | есть / есть |
| `atari2600` | RetroArch / `stella` | ES-DE `atari2600` | есть / есть |
| `atari5200` | RetroArch / `a5200` | ES-DE `atari5200` | есть / есть |
| `atari7800` | RetroArch / `prosystem` | ES-DE `atari7800` | есть / есть |
| `atari800` | RetroArch / `atari800` | ES-DE `atari800` | есть / есть |
| `atarist` | RetroArch / `hatari` | ES-DE `atarist` | есть / есть |
| `atomiswave` | RetroArch / `flycast` | ES-DE `atomiswave` | есть / есть |
| `bbcmicro` | RetroArch / `b2` | ES-DE `bbcmicro` | есть / есть |
| `bk` | RetroArch / `bk` | Libretro core info | есть / есть |
| `c128` | RetroArch / `vice_x128` | Libretro core info | есть / есть |
| `c20` | RetroArch / `vice_xvic` | ES-DE `vic20` | есть / есть |
| `c64` | RetroArch / `vice_x64` | ES-DE `c64` | есть / есть |
| `cdi` | RetroArch / `same_cdi` | ES-DE `cdimono1` | есть / есть |
| `channelf` | RetroArch / `freechaf` | ES-DE `channelf` | есть / есть |
| `colecovision` | RetroArch / `gearcoleco` | ES-DE `colecovision` | есть / есть |
| `cplus4` | RetroArch / `vice_xplus4` | ES-DE `plus4` | есть / есть |
| `dos` | RetroArch / `dosbox_pure` | ES-DE `dos` | есть / есть |
| `dreamcast` | RetroArch / `flycast` | ES-DE `dreamcast` | есть / есть |
| `enterprise` | RetroArch / `ep128emu_core` | Libretro core info | есть / есть |
| `fbneo` | RetroArch / `fbneo` | ES-DE `fbneo` | есть / есть |
| `fds` | RetroArch / `fceumm` | ES-DE `fds` | есть / есть |
| `gameandwatch` | RetroArch / `gw` | ES-DE `gameandwatch` | есть / есть |
| `gamegear` | RetroArch / `genesis_plus_gx` | ES-DE `gamegear` | есть / есть |
| `gb` | RetroArch / `gambatte` | ES-DE `gb` | есть / есть |
| `gba` | RetroArch / `mgba` | ES-DE `gba` | есть / есть |
| `gbc` | RetroArch / `gambatte` | ES-DE `gbc` | есть / есть |
| `gc` | Standalone / `dolphin` | ES-DE `gc` | standalone |
| `gx4000` | RetroArch / `cap32` | ES-DE `gx4000` | есть / есть |
| `intellivision` | RetroArch / `freeintv` | ES-DE `intellivision` | есть / есть |
| `jaguar` | RetroArch / `virtualjaguar` | ES-DE `atarijaguar` | есть / есть |
| `lowresnx` | RetroArch / `lowresnx` | ES-DE `lowresnx` | есть / есть |
| `lynx` | RetroArch / `handy` | ES-DE `atarilynx` | есть / есть |
| `macintosh` | RetroArch / `minivmac` | ES-DE `macintosh` | есть / есть |
| `mame` | RetroArch / `mame` | ES-DE `mame` | есть / есть |
| `mastersystem` | RetroArch / `genesis_plus_gx` | ES-DE `mastersystem` | есть / есть |
| `megadrive` | RetroArch / `genesis_plus_gx` | ES-DE `megadrive` | есть / есть |
| `megaduck` | RetroArch / `sameduck` | ES-DE `megaduck` | есть / есть |
| `msx` | RetroArch / `bluemsx` | ES-DE `msx1` | есть / есть |
| `msx2` | RetroArch / `bluemsx` | ES-DE `msx2` | есть / есть |
| `msx2+` | RetroArch / `bluemsx` | Libretro core info | есть / есть |
| `msxturbor` | RetroArch / `bluemsx` | ES-DE `msxturbor` | есть / есть |
| `n3ds` | RetroArch / `citra` или `azahar` | ES-DE `n3ds` | есть / есть |
| `n64` | RetroArch / `parallel_n64` | ES-DE `n64` | есть / есть |
| `naomi` | RetroArch / `flycast` | ES-DE `naomi` | есть / есть |
| `naomi2` | RetroArch / `flycast` | ES-DE `naomi2` | есть / есть |
| `nds` | Standalone / `melonds` | ES-DE `nds` | standalone |
| `neogeo` | RetroArch / `fbneo` | ES-DE `neogeo` | есть / есть |
| `neogeocd` | RetroArch / `neocd` | ES-DE `neogeocd` | есть / есть |
| `nes` | RetroArch / `fceumm` | ES-DE `nes` | есть / есть |
| `ngp` | RetroArch / `mednafen_ngp` | ES-DE `ngp` | есть / есть |
| `ngpc` | RetroArch / `mednafen_ngp` | ES-DE `ngpc` | есть / есть |
| `odyssey2` | RetroArch / `o2em` | ES-DE `odyssey2` | есть / есть |
| `pc88` | RetroArch / `quasi88` | ES-DE `pc88` | есть / есть |
| `pc98` | RetroArch / `np2kai` | ES-DE `pc98` | есть / есть |
| `pcengine` | RetroArch / `mednafen_pce_fast` | ES-DE `pcengine` | есть / есть |
| `pcenginecd` | RetroArch / `mednafen_pce_fast` | ES-DE `pcenginecd` | есть / есть |
| `pcfx` | RetroArch / `mednafen_pcfx` | ES-DE `pcfx` | есть / есть |
| `pet` | RetroArch / `vice_xpet` | Libretro core info | есть / есть |
| `pico` | RetroArch / `picodrive` | Libretro core info | есть / есть |
| `pico8` | RetroArch / `retro8` | ES-DE `pico8` | есть / есть |
| `pokemini` | RetroArch / `pokemini` | ES-DE `pokemini` | есть / есть |
| `ps2` | RetroArch / `armsx2` или `armsx2` | ES-DE `ps2` | есть / есть |
| `ps3` | RPCS3; общий standalone | ES-DE `ps3`; homebrew на Odin | Odin: игровой цикл Ping Pong |
| `psp` | RetroArch / `ppsspp` или `ppsspp` | ES-DE `psp` | есть / есть |
| `psvita` | Standalone / `vita3k` | ES-DE `psvita` | standalone |
| `psx` | RetroArch / `pcsx_rearmed` или `duckstation` | ES-DE `psx` | есть / есть |
| `satellaview` | RetroArch / `snes9x` | ES-DE `satellaview` | есть / есть |
| `saturn` | RetroArch / `yabasanshiro` | ES-DE `saturn` | есть / есть |
| `scummvm` | RetroArch / `scummvm` | ES-DE `scummvm` | есть / есть |
| `scv` | RetroArch / `emuscv` | ES-DE `scv` | есть / есть |
| `sega32x` | RetroArch / `picodrive` | ES-DE `sega32x` | есть / есть |
| `segacd` | RetroArch / `genesis_plus_gx` | ES-DE `megacd` | есть / есть |
| `sg1000` | RetroArch / `genesis_plus_gx` | ES-DE `sg-1000` | есть / есть |
| `snes` | RetroArch / `snes9x` | ES-DE `snes` | есть / есть |
| `snes-msu1` | RetroArch / `snes9x` | Libretro core info | есть / есть |
| `spectravideo` | RetroArch / `bluemsx` | ES-DE `spectravideo` | есть / есть |
| `sufami` | RetroArch / `snes9x` | ES-DE `sufami` | есть / есть |
| `supergrafx` | RetroArch / `mednafen_supergrafx` | ES-DE `supergrafx` | есть / есть |
| `supervision` | RetroArch / `potator` | ES-DE `supervision` | есть / есть |
| `switch` | Eden; общий standalone | ES-DE `switch`; homebrew на Odin | Odin: 2048 save/reopen; UX-пробелы ниже |
| `systemsp` | RetroArch / `flycast` | Libretro core info | есть / есть |
| `thomson` | RetroArch / `theodore` | ES-DE `to8` | есть / есть |
| `tic80` | RetroArch / `tic80` | ES-DE `tic80` | есть / есть |
| `uzebox` | RetroArch / `uzem` | ES-DE `uzebox` | есть / есть |
| `vectrex` | RetroArch / `vecx` | ES-DE `vectrex` | есть / есть |
| `videopacplus` | RetroArch / `o2em` | ES-DE `videopac` | есть / есть |
| `vircon32` | RetroArch / `vircon32` | ES-DE `vircon32` | есть / есть |
| `virtualboy` | RetroArch / `mednafen_vb` | ES-DE `virtualboy` | есть / есть |
| `wasm4` | RetroArch / `wasm4` | ES-DE `wasm4` | есть / есть |
| `wii` | Standalone / `dolphin` | ES-DE `wii` | standalone |
| `wiiu` | RetroArch / `cemu` или `cemu` | ES-DE `wiiu` | есть / есть |
| `wonderswan` | RetroArch / `mednafen_wswan` | ES-DE `wonderswan` | есть / есть |
| `wonderswancolor` | RetroArch / `mednafen_wswan` | ES-DE `wonderswancolor` | есть / есть |
| `x1` | RetroArch / `x1` | ES-DE `x1` | есть / есть |
| `x68000` | RetroArch / `px68k` | ES-DE `x68000` | есть / есть |
| `xegs` | RetroArch / `atari800` | ES-DE `atarixe` | есть / есть |
| `zx81` | RetroArch / `81` | ES-DE `zx81` | есть / есть |
| `zxspectrum` | RetroArch / `fuse` | ES-DE `zxspectrum` | есть / есть |

## Поставка и первый игровой цикл — 18 октября

Историческая первая поставка; позднейшая установка на оба устройства записана
в разделе «Odin homebrew» ниже. Недоступность Odin больше не блокирует работу.

- Полный ARM64-снимок `a09cef0` собран в изолированном builder на Flip с
  Qt WebEngine и libfm-qt6. Тесты standalone/retroarch — **2/2**, 44,36 с.
  Затем собран production с `BUILD_TESTING=OFF`; отдельный offscreen-запуск
  не выявил QML-ошибок.
- На Flip установлен бинарник и `overlay_support.py`. Проверены запущенный хеш,
  резервная копия, SQLite `quick_check=ok`, неизменность истории при обновлении
  (304 сессии до/после), отсутствие QML-ошибок.
  Бинарник SHA-256 `d94485d8c989bfb1a7171ff948182932c90e6a772e7df82cab6e8a0234d2fedb`;
  helper `1d2a570140371fd5a9f9d6a6e555e8dfe5934df98ceed815ed9661cea8c9d597`.
- **NES Pong на Flip:** нажатие Start на карточке Home сразу открыло игру,
  событие Start через устройство ввода перевело титульный экран в игровой.
  Home открыл Now playing; Continue вернул игру; Exit показал подтверждение,
  Leave вернул Home с новым кадром. RetroArch завершён, сессия 125 с записана
  как `returned`, база целая. Физические кнопки владельцем, звук и сохранения
  этим не проверены; это один образец, не приёмка всей NES-библиотеки.
- Odin пока недоступен по SSH. На нём остаётся последняя подтверждённая
  Home/Start-79; готовый комплект ожидает включения. Cemu/DuckStation ещё
  требуют собственного игрового цикла, даже после установки их маршрутов.
- На личном сервере найдены образцы для 21 платформы; текущие библиотеки
  устройств покрывают 28 ID, включая `ports`. Ни то ни другое не даёт контент
  для всех 106 строк. Отсутствующие образцы/BIOS не считаются успешными проверками.

### Подготовленные homebrew для Odin

Файлы загружены напрямую из релизов авторов, размеры и SHA-256 проверены.
В репозиторий бинарники не включены; результаты последующего запуска — ниже.

| Платформа | Образец | SHA-256 |
| --- | --- | --- |
| PS3 | [Ping Pong v1.0.0, `pingpong.fake.self`](https://github.com/utkuhalis/ps3-homebrew/releases/tag/v1.0.0), 4 072 080 байт | `65a4d0168fafacbb4d6bde6a51313836925e4134ba3571807dc77bcd632116a6` |
| Switch | [2048 v1.1.0, `2048.nro`](https://github.com/BernardoGiordano/2048/releases/tag/1.1.0), 1 365 830 байт | `cd7e7f343830920196590d99c82a9f1ab8a375eeaeb943fa6c671aa68250a20d` |

Первоначальная задача после включения: проверить RPCS3/Eden и их команды,
подключить общий launch/Home/exit без обхода обычного интерфейса. Она выполнена
в указанном ниже объёме; незавершённые свойства Eden остаются открытыми.
У 2048 автор документирует сохранение при выходе через `+` и восстановление
при новом запуске — отдельный пригодный сценарий проверки сохранений.
Коммерческие игры и мультиплеер PS3/Switch этой подготовкой не проверены.

## Odin homebrew — 18 октября

После перезагрузки Odin доступен. Обе портативки используют Armada
`20261017.64207e6`; Odin — kernel 7.2.6. В общий standalone/discovery добавлены
RPCS3 `.self/.elf` и Eden `.nro/.nsp/.xci`, начальные SDL-настройки и Home helper.
Признанный формат не означает проверку всех его вариантов или наличие firmware.
Существующие настройки discovery не переписывает; явные миграции с backup/rollback
хранятся в [RPCS3](emulators/rpcs3.md) и [Eden](emulators/eden.md).

**Поставка:** одинаковый production-бинарник на Flip/Odin:
`ab61d36876734869173fc5664da49f9dae1d5310256f231128f85cfd9bcdc3c5`;
финальный helper `0bef6d7561bc57dfbdc0e2a23684955433a63ac1afc19a16e1753e7dc241365f`.
ARM standalone/RetroArch **2/2** (8,39 + 40,35 с), затем `BUILD_TESTING=OFF`;
финальный helper **23/23**. Запущенные хеши совпали, QML-ошибок нет,
SQLite quick_check ok; история при установке неизменна (Odin 145, Flip 305).
Резервные бинарники сохранены. Финальный helper заменяет промежуточную пробу CLI.

**PS3:** существующий RPCS3 `0.0.43-20291-ffba2bf0`, ARM64, Vulkan Turnip Adreno 740.
Ping Pong запущен обычным A из TrainerOS; выбор бота, матч и движение ракетки
подтверждены кадрами и вводом через устройство. Home/Continue/Exit прошли;
повторный запуск с уже готовым кэшем и одно подтверждение Exit — тоже (45 с, returned).
Первый удачный цикл — 285 с. Сохранение у этого образца не проверено.
Первоначальный shader-interpreter режим застрял на 6650 вариантах; после
неудачного штатного завершения остановлен только точный тестовый процесс,
до игрового прогресса. Этот цикл записан failed (549 с). Обычный Async Recompiler
дошёл до игры после первичной PPU-компиляции. Firmware и личные данные сохранены.

**Switch:** официальный Eden 0.2.1 ARM64 Clang PGO, source `58c1e20ee5`.
Исправлен выбор llvmpipe вместо Turnip через окружение пакета; проверен Dynarmic.
2048 открыл поле, принял направления, показал изменение плиток/счёта, прошёл
Home/Continue. Внутренний `+` создал 68-байтовый обычный save (цикл 292 с).
Повторный запуск на финальном production восстановил те же плитки и счёт 4;
Home/Exit вернул оболочку с одним подтверждением (253 с). Save SHA-256 до/после:
`de752aaa36af711d89f717aceb68139deb3dcffdc5e0324b519c2158bdee7a8a`.
Ключи/firmware не добавлялись. Наблюдение поля и чтение файла дополняют друг друга;
одна запись returned не считается доказательством сохранения.

**Открыто у Eden:** Qt спрашивает о ключах перед keyless homebrew; после No игра
работает. Внутренний выход игры оставляет библиотеку Eden, закрываемую через Home.
Пакет возвращает 0 даже после падения дочернего процесса — требуется точное
распространение ошибки. Встроенный `eden-cli` проверен, но падает SIGABRT
(`basic_string: construction from null is not valid`); его пробная привязка
откачена на рабочий Qt-вариант, save не изменён. Не считать неудачные пробы
успехом по одному статусу истории. Обычный пользовательский маршрут ещё не завершён.

Локальные кадры/логи: `work/launch80/odin-ps3-play.png`, `odin-ps3-home.png`,
`odin-ps3-relaunch.png`, `odin-switch-play.png`, `odin-switch-reopened-final.png`,
`odin-switch-exited-final.png`, receipts обоих устройств и `modern-final.log`.
Приватные кадры уведомлений и бинарники в Git не включены. Физический ввод,
звук, коммерческие игры и универсальная совместимость остаются непроверенными.
Следующий проход — ранние платформы по реестру; эти образцы их не заменяют.

## Предыдущие проверки и история блокировки сборки

**Уточнение после вопроса владельца:** Odin не является обязательным устройством
для сборки или одиночных игровых проверок. Flip доступен, TrainerOS работает,
активного эмулятора при подготовке не было. На нём создан отдельный ARM64-контейнер
на существующей Fedora 44 базе; добавлены `qt6-qtwebengine-devel`, `libfm-qt-devel`
и `glib2-devel`. Сборка полного снимка `a09cef0` уже запущена с браузером и
файловым менеджером. Затем выполняются standalone/retroarch и production-сборка
с `BUILD_TESTING=OFF`. Ожидание сборки завершено поставкой выше. Ниже сохранена история блокировки
первой попытки, а не действующее требование доступности Odin.

- Финальные исходники: сборка `trainer_standalone_tests`/`trainer_retroarch_tests`
  и `ctest -R '^(standalone|retroarch)$'` на Windows/MinGW — **2/2**, 54,62 с.
  Новые случаи проверяют Cemu/DuckStation, сохранение PS1/PS2/Wii U core-привязок,
  discovery и сохранение профилей, аргументы/ошибки/отмену общего запуска.
  Linux-специфичные проверки с условным пропуском требуют ARM-прогона.
  Отдельно успешно скомпилирован финальный `src/app/Main.cpp`; это не полный
  ARM-релиз и не проверка установленного приложения.
- На Flip выполнен `tests/OverlayHelperTests.py` с финальным helper: **23/23**.
  Проверено, что игровые Cemu/DuckStation распознаются по аргументам запуска,
  а обычные окна эмулятора не получают право закрытия по этому признаку.
- ARM-сборка на Odin остановилась на потере SSH-доступа. Прошедшие до этого
  профильные тесты относились к промежуточной версии и не заменяют проверку
  финальных исходников после отмены автоматической миграции.
- Ранняя блокировка из-за отсутствия полного builder на Flip снята созданием
  изолированного окружения с браузером/файловым менеджером. Установлен финальный
  снимок с восстановленным `AdapterRouter.h`, не промежуточная мигрирующая версия.
- Odin должен получить проверенный финальный комплект; повторять прежнюю
  незавершённую сборку или устанавливать её промежуточный бинарник нельзя.
- Следом — реальные запуски в порядке реестра, Home/возврат/штатный выход,
  необходимые ресурсы/BIOS и пользовательские сохранения. Новые Cemu/DuckStation
  также требуют этого цикла; для DuckStation отдельно проверить запрос
  подтверждения закрытия. Физический ввод/звук остаются приёмкой владельца.

## Идентичность снимка источников

- `upstream-es_systems.xml` SHA-256 `275bcd127fece2e467fd2e24976e05767c5d1308a1509f84d6733eff5abb7f89`.
- `upstream-es_find_rules.xml` SHA-256 `67af7c75f76733dac4de2dead1aa01d32e89071f2ed4b3791514d5abcdc8c6c9`.
- `batocera-es_systems.yml` SHA-256 `6257cff6c78e66f4aa75dbd5aaaaa48a6e3018e7a795d53bf2bf3e73109b031e`.
- `core-info.json` SHA-256 `8de610c772eaa719354f79b50ac16d8198504ab6394513db8176623be001c4ba`.
