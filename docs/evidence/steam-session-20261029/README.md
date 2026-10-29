# Переход TrainerOS → Steam Gaming Mode, 29 октября 2026

Владелец сообщил о невозможности перейти в Steam Gaming Mode на Odin.
Владелец границы — S15: сохранённая оболочка S04 → root-owned session helper →
SDDM/Armada. Игры, аккаунты и пользовательские данные не переустанавливались.

## Причины и исправление

- До исправления обычный Start → Switch → Steam → Continue вызывал helper.
  Armada выбирала Steam, но `switch-gamemode` завершался после KDE logout:
  текущий TrainerOS compositor и его SDDM wrapper продолжали работать.
  D-Bus KDE Shutdown допускает активацию вне рабочего стола; успешный logout
  не является подтверждением завершения TrainerOS.
- TrainerOS helper теперь вызывает `default-gamemode` / `default-desktop`,
  проверяет результат и явно запрашивает restart SDDM. Неудачный выбор не
  перезапускает сеанс; ошибки restart возвращаются клиенту; marker очищается.
- После первого исправления Steam Gamescope падал с
  `Failed to connect to wayland socket: .`: в постоянном user manager
  оставался `WAYLAND_DISPLAY=`. Даже пустое значение выбирало nested backend.
  EXIT cleanup wrapper теперь останавливает свой compositor, очищает display
  значения D-Bus и затем удаляет их из systemd environment.
- При диагностике автоматический fallback Armada оставил живой Plasma-сеанс,
  удерживающий tty1; его графические targets были остановлены. Перезагрузка
  устройства не потребовалась. Это восстановление после неудачных попыток,
  а не новая постоянная настройка Plasma.

## Проверено

- На Odin и Flip прошли все **10 SessionSupervisorTests**: реальные Linux
  descendant/restart checks, boot default, marker, выбор обоих окружений,
  отказ selection/restart, teardown display environment и input guard.
- Оба root-owned helper установлены на Odin и Flip с Linux line endings;
  сохранены исходные файлы для отката. Оболочка и её бинарный файл не менялись.
- На Odin выполнен переход из свежего TrainerOS через обычное меню. Steam
  Gaming Mode действительно запустился: отдельный активный Steam unit,
  TrainerOS unit остановлен, главный экран Steam с библиотекой визуально проверен.
  Odin оставлен в Steam, громкость **0%**. Существующий Steam вход сохранился.
- Перед финальным переходом восстановленный TrainerOS показывал прежний профиль
  и 100 000 Искр; после перехода SQLite `quick_check` вернул `ok`.
- Flip получил исправление без переключения его текущего окружения. Реальный
  переход на Flip и физические кнопки этой проверкой не подтверждаются.

Установленные SHA-256 (LF):

| Файл | SHA-256 |
|---|---|
| control.py | e67e94dbcb3447d28f2d1dd0b397f055f51f3e49201144d0abf5e3954f00bfbb |
| start-session.sh | dbe02c99190af97d7f8a5a1b128f78fa8872cd8385e3c4d385e01da7e7901c69 |

## Открыто отдельно

**Дополнение того же дня:** описанный ниже сбой воспроизведён, исправлен и
проверен на установленной сборке; см. [завершение оболочки](../shell-teardown-20261029/README.md).
Ниже сохранено состояние на момент первой поставки session helper.

До исправления перехода обнаружен ABRT оболочки при обычном завершении:
`corrupted size vs. prev_size while consolidating`; supervisor возвращал TrainerOS.
Изменение session helper не объявляется исправлением этого повреждения памяти.
Нужны отдельное воспроизведение и разбор teardown оболочки. Также не заявлены
игровая совместимость Steam, смена Steam аккаунта и полный обратный UI-маршрут.
Приватные снимки библиотек/профилей и coredump в репозиторий не включены.
