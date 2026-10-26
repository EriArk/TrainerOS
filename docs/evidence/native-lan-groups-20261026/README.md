# Группы PS2 LAN / Dolphin BBA — 26 октября 2026

Реализация `8751ae05`, включая объединённую Crates recovery `f8244d75`.
Production `BUILD_TESTING=OFF`; проверены 981 входной файл.

- Оболочка: `102adeb3bc0f939f2585191b41c2c26e10ecd4903043c43673b5f8e915e2e603`.
- Общий endpoint: `6a6a945b740b13c64c98b69dca465b01641cfb39e23d5202f083f0f6fa3da90a`.
- Settings PS2: `eclx1-group-own-cards-v2`; Dolphin: `dolphin-bba-group-isolated-v2`.
- На **Flip и Odin** установлены и запущены одинаковые shell и LAN-пакеты;
  точные hashes — [Flip](flip-verified.json), [Odin](odin-verified.json).
- История не изменилась, 1539 / 2166 проверенных личных файлов не изменились,
  остальные 17 emulator packages сохранены. Ошибок QML нет. Громкость сохранена:
  Flip 0.20, Odin 0.00. Предыдущие shell/packages оставлены для отката.

## Проверки

- [Shell](shell-checks.log): standalone, lobby с реальными каналами 3/4 участников,
  game_party и runtime_multiplayer прошли. Проверены назначение каждого слота,
  Ready/Start, отзыв старого соединения и сохранение живого процесса.
- [Дополнительные проверки](production-checks.log): приватный групповой контракт,
  Dolphin BBA launch, 7 целевых library cases, Crates showcase и production build.
- [Endpoint](endpoint-tests.log): 8 PS2 config, 14 launcher, private namespace
  boundary, PS2/BBA × 2/3/4 через реальные Unix/UDP/ICE/DTLS процессы.
- [Установленные helpers](installed-groups.log): те же 6 комбинаций повторены с
  точными **установленными** PS2 и BBA transport binaries. Они имеют разные hashes;
  тест использует каждый отдельно. Endpoint обеих платформ идентичен. Проверены
  все отправители, broadcast, отсутствие эха, отказ чужого MAC и сохранение
  дочернего процесса после закрытия каналов.
- Export/knowledge проверен: 243 runtime files, включая BBA header и общий
  transport test harness. Исправлена полнота переносимой копии.

## Известные незакрытые результаты

Полный library suite: 23 passed / 3 failed. Три сбоя повторены отдельно в
[журнале](library-existing-failures.log): контекстный вход в Settings,
world-edit preference и ожидание turquoise вместо текущей light theme.
Эти тесты и соответствующие Settings/Shell/storage пути не менялись данной
реализацией; они не удалены и проверки не ослаблены. Общий прогон не объявляется
успешным. Целевые сохранность/форматы/Crates проверки прошли отдельно.

Протокольные fixtures не являются запущенными играми; новых коммерческих матчей
этот проход не утверждает. Сохраняются native ограничения режимов, прямого
соединения и ранее отложенные владельцем игровые исследования.

[Контракт и обслуживание](../../emulators/native-lan-groups.md).
