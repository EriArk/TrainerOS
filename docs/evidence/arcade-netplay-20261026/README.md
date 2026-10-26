# Arcade NetPlay — 26 октября 2026

Реализован общий Flycast Dojo GGPO route для Naomi / Naomi 2 / Atomiswave.
[Контракт, воспроизводимый package recipe, обновление и откат](../../emulators/flycast.md#naomi--naomi-2--atomiswave--26-october-2026).

Production ARM64 shell SHA-256:
`e873820f9b6f4c2cb44daeb1f4134a6625f85a0a7704c1145c03503fd73e6174`.
Перед сборкой сверены 991 source/test/package inputs; `BUILD_TESTING=OFF`.
Предыдущая SDK/Crates/content-format база `59e39f20` сохранена.

- 13 Python/libarchive helper tests прошли на каждой портативке.
- ARM64 CTest: `flycast_netplay`, `runtime_multiplayer`, `local_lobby` — 3/3.
  Затем добавлены наблюдение package dependencies и общие тексты ошибок;
  окончательная production сборка прошла. Native binary Dojo не менялся:
  `e81da3bbbdc14473c095478b969d4bf47b34bee83a58ea35a070d36e0eff5b77`.
- Реальный Dolphin Blue через compiled identity/command: обе стороны достигли
  GGPO running/session start в одной сети. Ограниченный probe завершил native
  процессы (exit 0); stage удалены, проверенные личные файлы неизменны.
  Это не новая приёмка матча, полного UI-пути или разных сетей.
- Adapter/runtime knowledge export и consistency check прошли.

## Установка завершена

Исходники [`f8b90298`](https://github.com/EriArk/TrainerOS/commit/f8b90298725127b52786f97036c837ee2ec72a7e)
установлены на **Flip и Odin**. Обе работающие оболочки и Flycast package сверены;
ошибок QML нет. История сеансов сохранена (Flip 411, Odin 243), проверенные личные
файлы неизменны (75/1283). Остальные 16 emulator package manifests на каждой
машине не изменились. Громкость устройств сохранена, сна/перезагрузки не было.

[Машиночитаемая запись поставки](delivery.json) содержит только технические
хеши и результаты. Личные логи, пути библиотек, сетевые адреса и игровые файлы
оставлены вне репозитория. Предыдущая оболочка `45b97fc1…` и прежний Flycast package
сохранены в отдельном delivery backup на каждой портативке; для отката вернуть
оба компонента вместе при закрытом сеансе, затем перезапустить оболочку.
