# Social — поведение и технические границы

Текущий UI — [PRODUCT_SPEC](PRODUCT_SPEC.md#social): Messages показывает людей,
Communities — группы/комьюнити, Discover — поиск и Recently Together. Lobby отдельно
в GameHub. Global X/Y не заменяются локальными compose/Together shortcuts.

Fluxer — источник аккаунта, профиля, прав, друзей/членства, сообщений и звонков.
REST/Gateway и защищённые credential jobs выполняются worker, токены не входят
в QML. Использовать доступные provider operations, не изобретать собственный чат.
Одни drafts/actions обслуживают touch и controller. Не терять черновик при смене
разговора; async-результаты проверяют аккаунт, цель и поколение запроса.

Базовые меню компактны, но сохраняют remove friend, leave group/community и
delete own community с явным подтверждением/проверками Fluxer. По неизвестному
точному тегу отправляется явная заявка; не выдумывать профиль без API. Обычные
ошибки, MFA и ограничения не обходятся; security-key-only flow остаётся в Fluxer.

Служебные Lobby groups скрываются по устойчивой привязке, не имени. Закрытие
комнаты убирает членство/группу; host leave сначала передаёт управление, одинокий
host закрывает комнату. Личные/обычные группы не трогать. Историческое разрешение
очистить конкретные тестовые группы не является разрешением на новое удаление.

Голосовой call независим от игровой партии и microphone consent. Переключение
игры не перезапускает звонок. Screen capture/уведомления сохраняют privacy;
friend-only activity требует ограничения доставки, не только скрытия UI.
Lobby text/emoji/file ограничения не отменяют базовые медиа обычного Social.

## Доказательства и незакрытые проверки

- [Возможности Fluxer](FLUXER_CAPABILITIES_RU.md) — сверяться перед интеграциями.
- [Lifecycle и управление](SOCIAL66_LIFECYCLE_CONTROLS.md) — актуальное отделение
  служебных групп, controller journey, ограниченная тестовая очистка.
- [Discover](SOCIAL67_DISCOVER.md) — реализация/установка и оставшиеся public Join,
  leave/delete+MFA, новая реальная сессия → Recently Together, owner acceptance.
- [Community contracts](SOCIAL_COMMUNITIES.md) — identity, tags и permissions.
- [Voice/media](SOCIAL_MEDIA_VOICE.md) — точные прежние проверки, отдельно живое
  прослушивание/headset/background audio. Синтетический звук их не заменяет.
- [Lobby](LOBBY.md), [media](LOBBY_MEDIA.md) — собственный состав сообщений/ролей.

Старое устройство экранов, тесты и результаты остаются в
[исторической записи](https://github.com/EriArk/TrainerOS/blob/c6aaac25ba4f55262f3f6418e8d29cac7cdc9aec/docs/SOCIAL.md).
Непройденная приёмка не превращается в пройденную от удаления дублирующего текста.
Текущая очередь только в [CURRENT_TASKS](CURRENT_TASKS.md), не в «далее» старых отчётов.
