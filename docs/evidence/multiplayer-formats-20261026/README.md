# Единый проход multiplayer-форматов — 26 октября 2026

Исходники и ARM64 production-артефакты готовы; установка на устройства будет записана отдельным подтверждением.
[Контракт и ограничения](../../MULTIPLAYER_CONTENT_FORMATS.md#единый-проход-оставшихся-форматов--26-октября-2026).

- Объединены multiplayer `6f06f26b` и установленная SDK/Crates-ветка `1c772932` в `a6c1518e`. Проверены все 978 production/test/SDK входных файлов.
- Оболочка Release, `BUILD_TESTING=OFF`, SHA-256 `45b97fc10b2db587fa7bb6718da62b670bcb2296984a8f9b1ab845f5d8cf5423`.
- ARM64 Account, SDK, Standalone, RetroArch и RuntimeMultiplayer прошли; после последних правок Dolphin/PS2 Standalone прошёл повторно. [Журнал](shell-tests.txt). Фокусированные Library checks: 8 passed, 0 failed (включая init/cleanup), отмена через настоящий Shell touch/controller path.
- Полный Library suite не заявляется прошедшим: прежняя ошибка `contextualEditsDeleteRomAndPreserveIdentity`/WorldEditing preference осталась вне этого изменения. В этом проходе проверены нужные операции составных файлов.
- Azahar: 11 filesystem-тестов, actual native CIA/ZCIA inspect/install, base/demo/update/DLC, идемпотентность, сохранность тестового сейва и четыре отказа malformed/encrypted/system/non-executable. [Native журнал](azahar-native-tests.txt). Обнаруженные границы общего CLI и отсутствующего storage обработаны явно.
- Dolphin: actual ARM64 reader через QEMU, 10 случаев; PS2 launcher: 14 тестов.
- [Свойства пакета](properties.png) и [установка с Cancel](installing.png): реальный LibraryToolsPanel QML, host offscreen render 960×540, fixture model. Это проверка интерфейса, не снимки с устройства и не свидетельство игрового матча.
- `export-game-adapters.py`, `check-adapter-knowledge.py`, Python compile, local Markdown targets и `git diff --check` прошли.

Личные настройки, адреса, история и полные приватные receipts в Git не копируются.
