# Возможности Fluxer: справочник для TrainerOS

## Discovery, люди и удаление комьюнити — 17 октября 2026

Native discovery проверен по официальному upstream и read-only запросами двух
тестовых аккаунтов: `tag=traineros-v1`, `sort_by=member_count`, HTTP 200, сейчас
ноль опубликованных результатов. Публичный tag и pinned-маркер joined guild —
разные механизмы; свой каталог не нужен. Поиск человека: известные identities,
GET по user ID; полный тег позволяет отправить явную заявку, публичного lookup
профиля по тегу не найдено. Leave — DELETE membership, Delete — POST guild/delete
с проверкой владельца и sudo (пароль/MFA). WebAuthn-only остаётся в Fluxer.
[Точные контракты, upstream revision и ограничения](SOCIAL67_DISCOVER.md#контракты-fluxer-и-фактическая-доступность).

## Уточнение области чата — 17 октября 2026

Наличие медиафункций у Fluxer не означает, что они нужны в текущем Lobby:
владелец выбрал текст, смайлы и файлы; исключил отдельные GIF, картинки и
голосовые сообщения **только для Lobby**. Social сохраняет свои требования;
его актуальные issues перечитать после завершения GameHub, перед началом работы.
Живой голос сохраняется. Вложения — через popup picker
на `+`, без дополнительного ограничения размера со стороны TrainerOS.
Старый медиа-лимит 16 MiB не переносить на обычные файлы. Перед реализацией
проверить upload-контракт и фактические лимиты контекста Fluxer, передавать
оригинальные файлы потоково. Это план, не новая возможность установленного
клиента. [Решения и источники LOBBY-64](LOBBY64_ENTRIES_HUD.md).

## Новые требования к проверке провайдера — 16 октября 2026

Этот проход не обновляет upstream evidence от 13 октября ниже.
Перед SOCIAL-16/LOBBY-16 проверить реальные права/контракты: friend-only game
activity (#220), eligibility/tagging native community discovery (#219), polls для
room vote (#210), voice channels, несколько publishers/viewer demand и реальный
локальный track mute (#181–182/#221). Документированный API не равен разрешению
аккаунта или установленной работе. При отсутствии capability не публиковать
приватные данные, не обещать discovery и не подменять сигналинг видимым DM-спамом.
Повторно использовать подтверждённые Fluxer механизмы; не строить собственный
media server. [Текущий контракт](PRODUCT_SPEC.md) различает spectator Fluxer
и interactive P2P, сохраняет один согласованный call и общие Downloads.

## Историческая редакция и доказательства

Ниже сохранён прежний текст с его датами. Конфликтующие цели и указания
очерёдности заменены текущим разделом выше; доказательства не переносятся на новый UI.

Проверено **13 октября 2026**. Назначение: перед новой социальной или медиафункцией
проверять готовую возможность провайдера и нашу существующую интеграцию.
Это инвентаризация, а не обещание добавить весь клиент Fluxer в TrainerOS.

Источники сверены с upstream
[`8cac3b35c9144ca2ea9dfb2a6e6561ea252123b3`](https://github.com/fluxerapp/fluxer/tree/8cac3b35c9144ca2ea9dfb2a6e6561ea252123b3).
Ниже «есть» означает документированный механизм/исходники Fluxer. Его наличие
не доказывает доступность конкретному аккаунту, включение на экземпляре или
проверку установленного TrainerOS. «Подключено» описывает нашу реализацию;
физическая и сетевая приёмка остаётся в документах соответствующих функций.

## Голос, видео и зрители — сначала использовать это

| Возможность Fluxer | Что использовать в TrainerOS | Наше состояние |
| --- | --- | --- |
| Голосовые звонки в DM и группе | Существующий звонок и его участники | Подключено; отдельный процесс сохраняет звонок при смене игры |
| Голосовые каналы комьюнити | Штатные каналы, допуск и права | Провайдер поддерживает; наш `voice-join` пока принимает только DM/группу |
| Кто говорит | События медиасессии, привязанные к ID участника | L05: индивидуальные SDK-события связаны с аккаунтами и кольцами игроков; [проверки](LOBBY_MEDIA.md) |
| Микрофон, mute/deafen, устройства, громкость | Существующий worker и настройки общения | Подключено; реальная речь/гарнитуры требуют отдельной приёмки |
| Go Live / демонстрация экрана и её звук | Экранные дорожки существующего подключения | Подключены публикация игры/звука и выбор зрителя; [границы захвата и приёмка](LOBBY_MEDIA.md) |
| Несколько демонстраций, выбор просмотра | Штатные публикации дорожек и подписки | L05; не нужен свой видеосервер или новый транспорт |
| Камера | Отдельный источник видеодорожки | Есть у провайдера; не входит автоматически в L05 |
| Превью трансляции и регион | Штатные stream API | Есть; превью само по себе не доказывает живую трансляцию |
| Зритель без своей игры | Приём медиа с обычным допуском к звонку | Не требует ROM, эмулятора или места контроллера; UI лобби — наша работа |

Контракты: [голос](https://docs.fluxer.app/voice/),
[звонки](https://docs.fluxer.app/http-api/calls/),
[трансляции](https://docs.fluxer.app/http-api/streams/),
[команды Gateway](https://docs.fluxer.app/gateway/commands/).
Текущий Go Live использует то же LiveKit-подключение: `self_stream` объявляет
публикацию, `viewer_stream_keys` — выбор зрителя. HTTP streams управляет
метаданными/превью, а не передаёт кадры. Stream API — user-only: бот каталога
не становится медиаклиентом пользователя.

Для речи готовый SDK предоставляет `active_speakers_changed`. Аккаунт определяется
из подтверждённой identity `user_<id>_<connection>`; один человек может иметь
несколько подключений. Нельзя определять говорящего по имени, наличию микрофона
или общему числу участников. Источники:
[события Python SDK](https://docs.livekit.io/reference/python/livekit/rtc/room.html),
[разбор identity в Fluxer](https://github.com/fluxerapp/fluxer/blob/8cac3b35c9144ca2ea9dfb2a6e6561ea252123b3/fluxer_app/src/features/voice/utils/VoiceParticipantIdentity.ts).

Наша особая задача для трансляции — захватывать **игру и её звук**, сохраняя
личные сообщения, клавиатуру и оболочку вне потока. Готовая доставка видео не
доказывает эту границу захвата. Проверить её на Gamescope/ARM до включения кнопки.
Remote Play дополнительно требует передачи управления; Go Live этого не обещает.
Уточнение владельца 14 октября: игровой Remote Play не привязывается к доставке
Go Live. Для него отдельно выбираются минимальные готовые части игрового
транспорта с измерением полной задержки; [решение и проверка](REMOTE_PLAY_TRANSPORT.md).

## Общение, контакты и сообщества

| Возможность | Fluxer / источник | TrainerOS |
| --- | --- | --- |
| DM, групповые DM, состав и название группы | [Private channels](https://docs.fluxer.app/http-api/users/private-channels/), [Channels](https://docs.fluxer.app/http-api/channels/) | Подключено в Social; разговор лобби использует ту же службу |
| Отправка, история, редактирование и удаление | [Messages](https://docs.fluxer.app/http-api/messages/) | Подключено; ошибки/повтор отправки сохраняют пользовательский текст |
| Ответы, упоминания, реакции | [Messages](https://docs.fluxer.app/http-api/messages/) | Подключены обычные действия Social; не создавать отдельную историю для мини-чата |
| Вложения и голосовые сообщения | [Messages](https://docs.fluxer.app/http-api/messages/) | Картинки и записанные голосовые сообщения подключены; это отдельная функция от живого звонка |
| Закрепления, embeds, расширенные вложения | [Messages](https://docs.fluxer.app/http-api/messages/) | Возможности провайдера шире нашего текущего UI |
| Непрочитанные и подтверждение чтения | [Read states](https://docs.fluxer.app/http-api/read-states/) | Подключено; HUD использует тот же счётчик |
| Поиск по сообщениям | [Search](https://docs.fluxer.app/http-api/search/) | Есть у Fluxer; владелец исключил из текущего объёма, не реализовывать свой индекс |
| Друзья, заявки, принятие, удаление, блокировки | [Relationships](https://docs.fluxer.app/http-api/users/relationships/) | Подключено; временная дружба использует те же операции |
| Профиль, имя, аватар | [Current user](https://docs.fluxer.app/http-api/users/current-user/) | Подключено; не смешивать профиль общения с локальным Trainer |
| Presence, пользовательский статус, настройки уведомлений | [Settings](https://docs.fluxer.app/http-api/users/settings/) | QUICK-34 подключает доступность и текстовый статус в карточке профиля; локальная DND сохраняется отдельно |
| Комьюнити, категории, каналы | [Guilds](https://docs.fluxer.app/http-api/guilds/), [Guild channels](https://docs.fluxer.app/http-api/guild-channels/) | Просмотр/участие подключены; бот каталога использует тематические каналы |
| Роли, участники, права и ограничения каналов | [Members](https://docs.fluxer.app/http-api/guild-members/), [Permissions](https://docs.fluxer.app/http-api/permissions/) | Допуск обязан учитывать Fluxer; отдельную систему прав не дублировать |
| Приглашения в комьюнити/группу | [Invites](https://docs.fluxer.app/http-api/invites/) | Подключено; приглашение в игру имеет собственное явное согласие |
| Поиск публичных комьюнити | [Discovery](https://docs.fluxer.app/http-api/discovery/) | Подключено в Discover; это не каталог игровых лобби |
| Модерация, баны и журнал действий | [Moderation](https://docs.fluxer.app/http-api/guild-moderation/), [Audit logs](https://docs.fluxer.app/http-api/guild-audit-logs/) | API есть; полный админ-клиент TrainerOS не заявлен |
| GIF, сохранённые медиа, emoji и stickers комьюнити | [GIFs](https://docs.fluxer.app/http-api/gifs/), [Memes](https://docs.fluxer.app/http-api/memes/), [Expressions](https://docs.fluxer.app/http-api/expressions/) | Использовать готовые каталоги/объекты при расширении, не собственный сервер медиа |
| Заметки о пользователях, внешние подключения профиля | [Notes](https://docs.fluxer.app/http-api/users/notes/), [Connections](https://docs.fluxer.app/http-api/connections/) | Есть у провайдера; не подключено как отдельные функции TrainerOS |
| Threads, forum/media posts | [Threads](https://docs.fluxer.app/http-api/threads/), [Forums](https://docs.fluxer.app/http-api/forums/) | В текущих исходниках/API есть; пользовательский клиент требует capability `channel_threads`. Не считать развёрнутым в TrainerOS |

## Приложения, инфраструктура и границы

| Возможность | Готовая основа | Что учитывать |
| --- | --- | --- |
| Приложения и bot-account | [Applications](https://docs.fluxer.app/http-api/applications/) | Бот каталога уже отдельный; пользовательская авторизация и bot token не взаимозаменяемы |
| OAuth2, scopes, выдача/отзыв доступа | [OAuth2](https://docs.fluxer.app/http-api/oauth2/) | Наличие OAuth2 не означает доступ ко всем user-only маршрутам |
| Webhooks | [Webhooks](https://docs.fluxer.app/http-api/webhooks/) | Готовый путь внешних публикаций; не замена звонку или членству пользователя |
| Реальные события, reconnect/resume, presence | [Gateway](https://docs.fluxer.app/gateway/overview/), [Events](https://docs.fluxer.app/gateway/events/) | Уже основа Social; учитывать rate limits, не заменять постоянным polling |
| Discovery экземпляра, включённые функции и лимиты | [Instance](https://docs.fluxer.app/http-api/instance/), [Deployment availability](https://docs.fluxer.app/http-api/deployment-availability/) | Проверять флаги и реальные ответы; документация не снимает ограничения конкретного сервера |
| MFA/passkeys, управление аккаунтом | [MFA](https://docs.fluxer.app/http-api/users/mfa/) | Не изобретать свою безопасность аккаунта; полнота UI у нас не заявлена |
| Push и экспорт пользовательских данных | [Push](https://docs.fluxer.app/http-api/users/push-notifications/), [Data harvest](https://docs.fluxer.app/http-api/users/data-harvest/) | Отдельные возможности; не считать подключёнными автоматически |
| Self-hosting | [Документация](https://docs.fluxer.app/self-hosting/) | Возможен; текущий бот на сервере владельца не означает собственный Fluxer-инстанс |

### Не путать текущую реализацию с анонсом

[Roadmap Fluxer от 12 октября 2026](https://fluxer.app/blog/roadmap-2026)
описывает будущую замену LiveKit собственным стеком, обязательное шифрование
звонков DAVE и отделение потоков от звонков. Действующие документы выше всё ещё
описывают LiveKit и общую media connection. Проверять контракт перед миграцией,
держать транспорт внутри интеграции; не писать новый SFU, кодек или шифрование.

Federation, обязательное E2EE личных сообщений и новые bot interactions также
не становятся доступными в TrainerOS из-за присутствия в roadmap. Threads/forums
уже описаны в свежем API, но требуют capability и проверки целевого развёртывания.
Дата и точная ревизия справочника нужны именно для таких переходных состояний.

### Что остаётся собственным механизмом TrainerOS

- Лобби и каталог игровых возможностей, места P1–Pn, Ready и разрешение запуска.
- Приглашение поверх игры, безопасная замена/выход, защита сохранений и runtime-службы.
- «Недавно вместе» и владение временной дружбой; сами friend API — Fluxer.
- Захват игрового окна/звука, исключение личного HUD, управление зрительским экраном.
- NetPlay/Link эмуляторов и Remote Play input; голос и Go Live их не заменяют.

Карта нашего кода: [FluxerSession](../src/integrations/social/FluxerSession.cpp),
[voice worker](../packaging/integrations/fluxer-voice.py),
[SocialMedia](../src/features/social/SocialMedia.cpp),
[SocialPlayers](../src/features/social/SocialPlayers.cpp),
[лобби](LOBBY.md), [голос: доказательства и приёмка](SOCIAL_MEDIA_VOICE.md).

Перед новой функцией: найти строку здесь → проверить актуальный upstream/экземпляр
→ найти существующий адаптер TrainerOS → подключить нужную возможность → записать
реальную проверку. Если готового механизма нет, сначала зафиксировать конкретный
пробел и сравнить готовые решения. Этот справочник обновлять вместе с интеграцией.
