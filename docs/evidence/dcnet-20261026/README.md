# Dreamcast native Online — поставка 26 октября 2026

Реализация: `62e9702209963f1f003e220100b3a593bfad776f`.
Включает параллельную поставку Crates `8354cc75` через merge `b5e1b720`.
[Контракт](../../emulators/flycast-dcnet.md), [машиночитаемые результаты](verification.json).

## Установлено

На **Flip и Odin** работает production shell (`BUILD_TESTING=OFF`):
`83cceb3374a711bd85d3ed8ae51355edc2b3d240b757710c1a8cb445607dbb8b`.
Native Flycast 2.7 с закреплённым patch:
`993dbf4dff9be92ba390e3772c6659c94debc68536398c5ee985f7f6985d8b67`.
Сверены 979 source inputs общего build directory и каждый manifest file нового пакета.
Live process SHA подтверждён отдельно на каждой портативке после замены.

Предыдущая оболочка `8321ec3e…`, её профили/настройки и SQLite backup сохранены
на каждом устройстве. После установки неизменны проверенные 1539 личных файлов
Flip / 2166 Odin, история, Crates data/active и 18 других package manifests.
QML errors не обнаружены. InputPlumber вернулся в mode 0; диагностических игр нет,
Odin оставлен с системной громкостью 0%. Рецепт rollback не удаляет native-online saves.

## Проверено

- Shared `flycast_netplay`, `local_lobby`, `runtime_multiplayer`: 3/3 suite pass.
  После исправления private directory/managed-close marker повторён затронутый
  Flycast suite: 6 passed, 0 failed, 1 retained skip. Cabinet namespace case
  пропущен в container; это ограничение builder, прежняя host-проверка сохранена.
- Adapter/runtime knowledge export проверен (32 adapter files / 237 runtime files).
- Native product codes прочитаны из собственных дисков; Quake III catalog alias
  исправлен. Имена пользовательских файлов не участвуют в выборе Online.
- На обоих устройствах native ChuChu Rocket US (`MK-51049`) запустился с Vulkan;
  в SDL записан виртуальный контроллер на порту A. Проверенный кадр — boot screen,
  не матч или доказательство физического управления.
- Финальный integration probe прошёл **общий Home request → capture → подтверждение
  Exit → обычный WM_DELETE_WINDOW → native exit 0 → защищённый save return** без
  внешнего закрытия и без timeout. Прочитанные перед запуском flash/A1/A2 совпали
  с предыдущим сохранённым archive; после выхода новый archive принят S08 без ошибки.
  Это проверка непрозрачного flash/VMU persistence, не регистрации игрового аккаунта.
- Native launch обнаружил неверную изоляцию XDG_DATA_DIRS, скрывавшую Vulkan ICD.
  Исправление изолирует fallback внутри Flycast; системный поиск драйвера сохранён.
  Ранние диагностические прогоны не засчитаны как Home/exit acceptance.

## Границы

Полный пользовательский путь двух реальных Lobby-клиентов до одного внешнего
игрового матча в этом проходе не выполнялся. Разные сети, доступность каждого
сервиса/редакции и физическое управление не выведены из unit tests/boot frame.
Online сейчас имеет два места координатора, ручной выбор комнаты и постоянные
собственные VMU; автоматического выбора игрового сервера/комнаты нет.
3–4 native кабинета и расширение подтверждённого каталога остаются открытыми.
Сон/крышка и ранее остановленные Sonic/Battlefront/Iris/Remote Play не возобновлялись.

Полные журналы, игровые изображения, исходные пути и сохранения остались приватными.
