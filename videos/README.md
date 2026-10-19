# Видео TrainerOS

## Скриншоты

В [screenshots](screenshots/) собраны 196 существующих снимков и проверочных
изображений со съёмки. [Галерея с миниатюрами](screenshots/index.html) открывается
локально в браузере; нажатие показывает исходное изображение.
Папки разделены по экранам: Home, Games, GameHub, Home/Start, Apps/Files/Browser,
Crates, Social, Lobby и gameplay. `preparation` содержит подготовку и возврат
из игры, `review` — контактные листы и кадры проверки монтажа.
Сами изображения локальные, исключены из Git. Оригиналы сохранены;
[контрольные суммы](screenshots/manifest.json) проверены после копирования.

## Ролики

Локальная подборка записей от 19 октября 2026.

Готовые ролики находятся здесь, исходные дубли и промежуточные нарезки — в `sources/`.
MP4 исключены из Git: видео доступны в этой рабочей копии, не после обычного
клонирования. Все ролики без звука. Отдельный файл подписей —
[русский SRT](traineros-overview-75s.ru.srt).

| Ролик | Содержание |
| --- | --- |
| [Обзор — 75 с](traineros-overview-75s.mp4) | Общий обзор без подписей |
| [Обзор с подписями — 75 с](traineros-overview-captioned-75s.mp4) | Отключаемые русские субтитры |
| [Home — 20 с](traineros-home-red-20s.mp4) | Emerald, Quick Apps и недавние игры |
| [Games — 35 с](traineros-games-turquoise-35s.mp4) | Коллекции, платформы и виды библиотеки |
| [GameHub — 40 с](traineros-gamehub-turquoise-final-40s.mp4) | История, галерея и достижения |
| [Home/Start — 21 с](traineros-home-start-red-final-21s.mp4) | Меню игры и системные виджеты |
| [Social — 27 с](traineros-social-two-devices-final-27s.mp4) | Переписка двух устройств |
| [Apps — 30 с](traineros-apps-turquoise-final-30s.mp4) | Приложения, файлы и браузер |
| [Crates — 22 с](traineros-crates-red-final-22s.mp4) | Каталог проектов |
| [Лобби и игра — 66 с](traineros-lobby-metal-slug-v2-66s.mp4) | Основной вариант: голосование, запуск, совместная игра |
| [Metal Slug — 22 с](traineros-metal-slug-both-players-22s.mp4) | Действия обоих игроков |
| [Лобби и игра — 67 с](traineros-lobby-metal-slug-final-67s.mp4) | Предыдущая версия монтажа |

[manifest.json](manifest.json) содержит размеры и SHA-256 всех 45 скопированных
MP4: 12 готовых вариантов и 33 исходных/промежуточных записи. Копии побайтно
проверены; оригиналы сохранены. Пустая неудачная проба `pipewire-test.mp4`
не включена.

[История съёмки и ограничения](../docs/DEMO_VIDEOS.md).
Новые записи и проверки устройств не проводились.
