# LAUNCH-80 — сверка готовых механизмов запуска

18 октября 2026. Проверено покрытие всего локального каталога, без запуска игр
и без подмены проверки совместимости наличием файла. Порядок работы —
[общая интеграция, затем игровые проверки](LAUNCH80_PLATFORM_REGISTER.md).

## Результат общего прохода

- 106 ID: 100 имеют кандидат Libretro, четыре — отдельный runtime; PS3/Switch
  отложены. Все **72 различных ядра** из каталога найдены на обоих устройствах;
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
старые привязки не мигрируются. Установка этой версии ещё не выполнена.

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
| `ps3` | Отложено владельцем | ES-DE `ps3` | — |
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
| `switch` | Отложено владельцем | ES-DE `switch` | — |
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

## Проверки и границы доставки

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
- Новая версия не установлена ни на Odin, ни на Flip. На Flip нет текущего
  полного сборочного окружения с зависимостями браузера/файлового менеджера;
  старым окружением рабочую версию не заменяли.
- После восстановления Odin повторно передать финальные исходники, обязательно
  включая восстановленный `AdapterRouter.h`, собрать и проверить, затем установить.
  Нельзя установить оставшийся промежуточный бинарник.
- Следом — реальные запуски в порядке реестра, Home/возврат/штатный выход,
  необходимые ресурсы/BIOS и пользовательские сохранения. Новые Cemu/DuckStation
  также требуют этого цикла; для DuckStation отдельно проверить запрос
  подтверждения закрытия. Физический ввод/звук остаются приёмкой владельца.

## Идентичность снимка источников

- `upstream-es_systems.xml` SHA-256 `275bcd127fece2e467fd2e24976e05767c5d1308a1509f84d6733eff5abb7f89`.
- `upstream-es_find_rules.xml` SHA-256 `67af7c75f76733dac4de2dead1aa01d32e89071f2ed4b3791514d5abcdc8c6c9`.
- `batocera-es_systems.yml` SHA-256 `6257cff6c78e66f4aa75dbd5aaaaa48a6e3018e7a795d53bf2bf3e73109b031e`.
- `core-info.json` SHA-256 `8de610c772eaa719354f79b50ac16d8198504ab6394513db8176623be001c4ba`.
