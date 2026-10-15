# Сверка новых и изменённых issues — 16 октября 2026

Владелец признал последние новые и изменённые issues источником истины.
Полные тела и комментарии перечитаны при аудите; метаданные повторно сверены
через GitHub CLI перед записью этого реестра. Проверены все 232 существующих
issues (включая закрытые); после границы 2026-10-13T00:00:00Z изменены **116**:
**47 новых #188–234 и 69 прежних**. Максимальный updatedAt —
**2026-10-15T22:03:12Z** (16 октября в часовом поясе владельца).
Повторная сверка не обнаружила изменений относительно прочитанных тел.

Это снимок требований, не заменитель перечитывания live issue перед работой.
[Канонический UX-контракт](UX_CONTRACT_20261016_RU.md) содержит решения и замены;
[ROADMAP](ROADMAP.md#current-execution-queue) — единственная очередь.
Номера ниже сгруппированы по владельцу основной работы, cross-cutting требования
применяются ко всем соответствующим блокам. OPEN/CLOSED — статус GitHub,
не наша оценка source/installed/device acceptance.

## Конфликты, которые разрешены явно

- #206 содержит прежнюю формулировку четырёх primaries; более поздние #111/#224/#232 добавляют Apps и требуют пять.
- #201 CLOSED: промежуточный hide-first-slot больше не нужен; optionality и migration входят #206/#207.
- #129 CLOSED: отдельный Together/Y menu заменён единым Lobby и global X/Y. Существующие social/party backends сохраняются.
- #39 CLOSED исторически, но добавлены новые требования optional Steam install/restore. Старое закрытие не подтверждает новый UI; перед этим подблоком согласовать актуальность статуса. Issues данным проходом не изменяются.
- Start/widgets ранее deferred в #224, затем приняты #233; Settings приняты #234; последние #65/#233/#234 добавляют полный scraper и default local video. Старые deferred фразы не отменяют дополнений.
- #171/#195 явно требуют solo pause для Minimize/QuickChat. Общие ранние фразы #134/#173 без pause относятся к пассивной индикации.
- #187 заменяет строгий oldest-first; новые вопросы не возобновляют private PSP, полный handoff catalogue или PS3/Switch.
- #147 допускает release pipeline, но GitHub Actions остаётся выключен по отдельному решению владельца. Включение требует отдельного указания.

## Покрытие

### Каркас, навигация, адаптеры и общие controls

**Рабочая область:** CORE-16. Пять primaries, optional GameHub Adapter, legacy migration, responsive density и общий input. #92 external package gate сохраняется отдельно. #194 — документация, не готовность кода.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#92 — Adapters: define a versioned installable game-adapter package format](https://github.com/EriArk/TrainerOS/issues/92) | 2026-10-15T15:25:34Z | OPEN |
| [#111 — Navigation: five primaries Home / Games / GameHub / Social / Apps, with legacy migration](https://github.com/EriArk/TrainerOS/issues/111) | 2026-10-15T16:52:00Z | OPEN |
| [#156 — TV UI foundation: introduce a real ten-foot responsive layout system, not 960x540 stretching](https://github.com/EriArk/TrainerOS/issues/156) | 2026-10-15T03:21:53Z | OPEN |
| [#157 — TV UX: redesign Home, libraries, Crates, Social and game overlays for couch viewing](https://github.com/EriArk/TrainerOS/issues/157) | 2026-10-15T16:51:50Z | OPEN |
| [#158 — TV input UX: reliable D-pad/remote focus, text entry and accessibility at ten feet](https://github.com/EriArk/TrainerOS/issues/158) | 2026-10-15T03:22:28Z | OPEN |
| [#159 — TV display profiles: external screen, docked mode and safe transition without host takeover](https://github.com/EriArk/TrainerOS/issues/159) | 2026-10-15T03:23:02Z | OPEN |
| [#167 — UX foundation: universal Options, contextual Select and active-game multitasking](https://github.com/EriArk/TrainerOS/issues/167) | 2026-10-15T03:21:16Z | OPEN |
| [#169 — Select UX: context actions for the focused object and current TrainerOS screen](https://github.com/EriArk/TrainerOS/issues/169) | 2026-10-15T15:24:57Z | OPEN |
| [#174 — Experience architecture: adapter-owned GameHub screens, Home composition and verified capabilities](https://github.com/EriArk/TrainerOS/issues/174) | 2026-10-15T08:55:05Z | OPEN |
| [#175 — Experience navigation: optional GameHub Adapter menu, views and safe contextual actions](https://github.com/EriArk/TrainerOS/issues/175) | 2026-10-15T08:54:59Z | OPEN |
| [#176 — Pokémon experience extraction: encapsulate Companions, Center and game progress outside the core shell](https://github.com/EriArk/TrainerOS/issues/176) | 2026-10-15T11:06:56Z | OPEN |
| [#177 — Generic Experience fallback: honest Home, GameHub Overview/RA and cross-adapter QA](https://github.com/EriArk/TrainerOS/issues/177) | 2026-10-15T08:47:44Z | OPEN |
| [#178 — Player identity vs game progression: decouple universal Trainer profile from adapter-specific history](https://github.com/EriArk/TrainerOS/issues/178) | 2026-10-15T08:47:46Z | OPEN |
| [#192 — Controller UX foundation: spatial focus on every screen and right-stick scrolling](https://github.com/EriArk/TrainerOS/issues/192) | 2026-10-15T15:27:50Z | OPEN |
| [#193 — Display foundation: render TrainerOS at native output resolution with responsive density](https://github.com/EriArk/TrainerOS/issues/193) | 2026-10-15T03:20:14Z | OPEN |
| [#194 — UX docs reconciliation: Home v2, Games, GameHub, Social, Lobby and lifecycle](https://github.com/EriArk/TrainerOS/issues/194) | 2026-10-15T16:51:19Z | OPEN |
| [#206 — GameHub v3: replace two game primaries with one hub and conditional L2/R2 faces](https://github.com/EriArk/TrainerOS/issues/206) | 2026-10-15T16:52:04Z | OPEN |
| [#207 — Experience Adapter v3: redesign adapter contract, integration points and Pokémon implementation](https://github.com/EriArk/TrainerOS/issues/207) | 2026-10-15T15:25:46Z | OPEN |
| [#224 — Epic: Apps v2 — five primaries, four Apps faces and modular TrainerOS workspace](https://github.com/EriArk/TrainerOS/issues/224) | 2026-10-15T21:23:25Z | OPEN |
| [#227 — TrainerOS global controls: X Quick Apps, Y game drawer and safe focus-loss popup behavior](https://github.com/EriArk/TrainerOS/issues/227) | 2026-10-15T17:07:39Z | OPEN |
| [#232 — Apps v2 integration & acceptance: merge prototype with GameHub/Social, real-device UX and regressions](https://github.com/EriArk/TrainerOS/issues/232) | 2026-10-15T17:07:48Z | OPEN |

### Home, игра, Options и клавиатура

**Рабочая область:** HOME-16. Три зоны, точная focus/session identity, preview без consent, pause lease для solo, protected Exit и QWERTY. Старое Home/Y/D-pad поведение заменено.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#49 — Launch lifecycle: keep ordinary saves and confirm saving only after explicit Exit game](https://github.com/EriArk/TrainerOS/issues/49) | 2026-10-15T03:22:26Z | OPEN |
| [#112 — Home button: add an adaptive activity overlay with game appearance presets, without duplicating Start](https://github.com/EriArk/TrainerOS/issues/112) | 2026-10-15T19:30:47Z | OPEN |
| [#168 — Shell Options: replace the compact Home menu with a large universal activity surface](https://github.com/EriArk/TrainerOS/issues/168) | 2026-10-15T19:50:51Z | OPEN |
| [#170 — Game Options: large artwork-led overlay for the actual running game session](https://github.com/EriArk/TrainerOS/issues/170) | 2026-10-15T19:47:22Z | OPEN |
| [#171 — Game multitasking: minimize a live game, safely pause when supported, and return](https://github.com/EriArk/TrainerOS/issues/171) | 2026-10-15T15:27:25Z | OPEN |
| [#173 — Notifications UI: shared passive toasts during gameplay and in shell, with Options inbox](https://github.com/EriArk/TrainerOS/issues/173) | 2026-10-15T13:47:56Z | OPEN |
| [#185 — System keyboard: preserve TrainerOS styling while replacing A-Z grid with QWERTY and language picker](https://github.com/EriArk/TrainerOS/issues/185) | 2026-10-15T05:07:57Z | OPEN |
| [#188 — Home v2: three-zone console dashboard with an adapter-owned game showcase](https://github.com/EriArk/TrainerOS/issues/188) | 2026-10-15T03:23:04Z | OPEN |
| [#189 — Home v2: unread activity inbox and quick Social chat avatars](https://github.com/EriArk/TrainerOS/issues/189) | 2026-10-15T03:18:52Z | OPEN |
| [#190 — Home v2: verified recent-achievements icon strip and details](https://github.com/EriArk/TrainerOS/issues/190) | 2026-10-15T03:18:54Z | OPEN |
| [#191 — Launch/Home context: follow the actually started game and keep global Y recents honest](https://github.com/EriArk/TrainerOS/issues/191) | 2026-10-15T08:49:28Z | OPEN |
| [#195 — In-game Quick Chat: tiny speech-bubble replies from any Social conversation](https://github.com/EriArk/TrainerOS/issues/195) | 2026-10-15T13:48:08Z | OPEN |

### Games, коллекции, Overview, Screens и RA

**Рабочая область:** GAMES-16. Единая библиотека/ID, четыре вида, rules, Recent/Favorites, media provenance и только доказанные live RA captures. #201 закрыт как переходное superseded решение.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#24 — RetroAchievements: clearly distinguish earned achievements from locked/unknown records](https://github.com/EriArk/TrainerOS/issues/24) | 2026-10-15T08:01:39Z | OPEN |
| [#25 — RetroAchievements: show an in-universe unlock banner and play a short TrainerOS jingle](https://github.com/EriArk/TrainerOS/issues/25) | 2026-10-15T08:01:23Z | OPEN |
| [#48 — RetroAchievements: match GameHub awards to the selected game and verified account](https://github.com/EriArk/TrainerOS/issues/48) | 2026-10-15T08:51:13Z | OPEN |
| [#114 — UX: make Worlds the entry point for game-series collections](https://github.com/EriArk/TrainerOS/issues/114) | 2026-10-15T06:48:11Z | OPEN |
| [#197 — Games v2: one root with collection ribbon and horizontally scrolling platform grid](https://github.com/EriArk/TrainerOS/issues/197) | 2026-10-15T06:44:43Z | OPEN |
| [#198 — Games browser: Wheel, List, Shelves and Grid views with shared game identity](https://github.com/EriArk/TrainerOS/issues/198) | 2026-10-15T06:45:36Z | OPEN |
| [#199 — Games collections: expressive rule builder, preview and manual overrides](https://github.com/EriArk/TrainerOS/issues/199) | 2026-10-15T06:46:23Z | OPEN |
| [#200 — Y / Recent games drawer: add Favorites as a second source, keep the existing chassis panel](https://github.com/EriArk/TrainerOS/issues/200) | 2026-10-15T15:25:06Z | OPEN |
| [#201 — Experience UX: hide the first game-specific primary tab when no matching game adapter provides it](https://github.com/EriArk/TrainerOS/issues/201) | 2026-10-15T08:47:49Z | CLOSED |
| [#202 — GameHub Overview: per-game dashboard with Timeline and Screens inside one face](https://github.com/EriArk/TrainerOS/issues/202) | 2026-10-15T08:45:48Z | OPEN |
| [#203 — GameHub Overview Screens: screenshot gallery and fullscreen viewer](https://github.com/EriArk/TrainerOS/issues/203) | 2026-10-15T08:46:18Z | OPEN |
| [#204 — RetroAchievements: automatic clean gameplay screenshot on verified live unlock](https://github.com/EriArk/TrainerOS/issues/204) | 2026-10-15T07:58:56Z | OPEN |
| [#205 — GameHub RetroAchievements: collectible trophy-card gallery and rich details](https://github.com/EriArk/TrainerOS/issues/205) | 2026-10-15T08:50:18Z | OPEN |

### Social v3

**Рабочая область:** SOCIAL-16. Messages/Communities/Discover, общий rich chat/call, приватность на delivery layer и native discovery по реальной capability. #129 закрыт как superseded, backends сохраняются.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#98 — Epic: add a lightweight Fluxer messenger, TrainerOS Link activities and in-game multiplayer](https://github.com/EriArk/TrainerOS/issues/98) | 2026-10-15T13:45:09Z | OPEN |
| [#101 — Social UI v3: compact Messages, Communities and Discover with simple profiles](https://github.com/EriArk/TrainerOS/issues/101) | 2026-10-15T13:43:43Z | OPEN |
| [#102 — Social media: support safe image attachments and native recorded voice messages](https://github.com/EriArk/TrainerOS/issues/102) | 2026-10-15T13:45:12Z | OPEN |
| [#103 — Social voice: add simple private voice rooms with native audio and safe lifecycle handling](https://github.com/EriArk/TrainerOS/issues/103) | 2026-10-15T13:45:06Z | OPEN |
| [#127 — UX: reconcile Social, multiplayer, calls and Link into one coherent user journey](https://github.com/EriArk/TrainerOS/issues/127) | 2026-10-15T13:45:20Z | OPEN |
| [#128 — Epic: Social v3 — Messages, Communities, Discover; simplify Fluxer UX](https://github.com/EriArk/TrainerOS/issues/128) | 2026-10-15T13:43:40Z | OPEN |
| [#129 — Superseded: standalone Social Together menu and conversation party gateway](https://github.com/EriArk/TrainerOS/issues/129) | 2026-10-15T13:49:16Z | CLOSED |
| [#130 — Social v3 navigation: three L2/R2 faces, deep route restoration, global Y everywhere](https://github.com/EriArk/TrainerOS/issues/130) | 2026-10-15T15:25:22Z | OPEN |
| [#133 — Social v3 group details: lean member controls, group calls and no GameParty](https://github.com/EriArk/TrainerOS/issues/133) | 2026-10-15T13:43:32Z | OPEN |
| [#134 — Notifications UX: unify calls, game invitations and TrainerOS activities under one incoming-activity model](https://github.com/EriArk/TrainerOS/issues/134) | 2026-10-15T13:43:36Z | OPEN |
| [#217 — Social v3 / Messages: one person = one DM, all friends and requests, pins, rich chat](https://github.com/EriArk/TrainerOS/issues/217) | 2026-10-15T13:39:04Z | OPEN |
| [#218 — Social v3 / Communities: groups plus communities, drill-down channels, simple creation and pins](https://github.com/EriArk/TrainerOS/issues/218) | 2026-10-15T13:39:07Z | OPEN |
| [#219 — Social v3 / Discover: people search, actual co-players and native popular TrainerOS communities](https://github.com/EriArk/TrainerOS/issues/219) | 2026-10-15T13:39:10Z | OPEN |
| [#220 — Social v3 / Profiles: compact cards, presence dots and PRIVATE friend-only game activity (research + integration)](https://github.com/EriArk/TrainerOS/issues/220) | 2026-10-15T13:41:19Z | OPEN |
| [#221 — Social v3 / Calls: persistent one-call voice across messages, groups, communities and gameplay](https://github.com/EriArk/TrainerOS/issues/221) | 2026-10-15T13:41:22Z | OPEN |
| [#222 — Social v3 / Notifications: quiet defaults, private text previews, no game-overlay duplicates](https://github.com/EriArk/TrainerOS/issues/222) | 2026-10-15T13:41:26Z | OPEN |
| [#223 — Social v3 end-to-end acceptance: three faces, legacy-state migration, real Fluxer and handheld UX](https://github.com/EriArk/TrainerOS/issues/223) | 2026-10-15T13:46:39Z | OPEN |

### Lobby и его полный цикл

**Рабочая область:** LOBBY-16. GameHub room, Online/Local, vote, auto-prep/Ready, safe import/consent/return, единый HUD. Подключить существующие runtime, не объявлять все новые media/network возможности готовыми.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#131 — Multiplayer UX: replace transport-first invites with one people-first Play Together flow](https://github.com/EriArk/TrainerOS/issues/131) | 2026-10-15T13:45:24Z | OPEN |
| [#132 — Lobby players: roster, owner-approved seat requests, automatic readiness and rejoin](https://github.com/EriArk/TrainerOS/issues/132) | 2026-10-15T11:07:30Z | OPEN |
| [#135 — Lobby end-to-end acceptance: online/local room, vote, Ready, gameplay and return](https://github.com/EriArk/TrainerOS/issues/135) | 2026-10-15T11:07:28Z | OPEN |
| [#172 — Play Together context entries: invite from Home game card, library, chat or live game](https://github.com/EriArk/TrainerOS/issues/172) | 2026-10-15T11:04:41Z | OPEN |
| [#179 — Epic: GameHub Lobby v2 — rooms, picker, auto-prep, spectators and in-game HUD](https://github.com/EriArk/TrainerOS/issues/179) | 2026-10-15T11:07:22Z | OPEN |
| [#180 — Game Lounge: public browser, solo room publishing and access controls](https://github.com/EriArk/TrainerOS/issues/180) | 2026-10-15T11:02:38Z | OPEN |
| [#181 — Lobby spectators: viewer-demand multi-stream video, player requests and stream permissions](https://github.com/EriArk/TrainerOS/issues/181) | 2026-10-15T11:07:24Z | OPEN |
| [#182 — Game Lounge: one audible conversation for stream viewers and players](https://github.com/EriArk/TrainerOS/issues/182) | 2026-10-15T11:02:44Z | OPEN |
| [#183 — In-game HUD: one translucent draggable panel with player avatars, voice mute and Quick Chat](https://github.com/EriArk/TrainerOS/issues/183) | 2026-10-15T11:07:26Z | OPEN |
| [#184 — Game Lounge: optional live seat joining into an already running emulator](https://github.com/EriArk/TrainerOS/issues/184) | 2026-10-15T11:02:52Z | OPEN |
| [#196 — In-game multiplayer HUD: short message speech bubbles tied to player avatars](https://github.com/EriArk/TrainerOS/issues/196) | 2026-10-15T11:05:02Z | OPEN |
| [#208 — GameHub Lobby: move universal Lounge browser from Social to a permanent L2/R2 hub face](https://github.com/EriArk/TrainerOS/issues/208) | 2026-10-15T11:04:44Z | OPEN |
| [#209 — GameHub Lobby room UX: game-like player slots, voice controls and full-featured side chat](https://github.com/EriArk/TrainerOS/issues/209) | 2026-10-15T10:54:29Z | OPEN |
| [#210 — Lobby Game Picker: Games-style platforms/collections/search, verified play modes and 4-option voting](https://github.com/EriArk/TrainerOS/issues/210) | 2026-10-15T10:55:20Z | OPEN |
| [#211 — Lobby auto-preparation: remove manual Prepare, show per-player readiness and safe auto-launch](https://github.com/EriArk/TrainerOS/issues/211) | 2026-10-15T10:56:04Z | OPEN |
| [#212 — Lobby missing-game recovery: import locally owned ROM via existing file picker and revalidate](https://github.com/EriArk/TrainerOS/issues/212) | 2026-10-15T10:56:44Z | OPEN |
| [#213 — Lobby chat media: GIFs, general file attachments and Downloads integration via existing Fluxer conversation](https://github.com/EriArk/TrainerOS/issues/213) | 2026-10-15T17:07:33Z | OPEN |
| [#214 — Lobby Local mode: offline rooms without Fluxer login, discover nearby peers and lock creation type](https://github.com/EriArk/TrainerOS/issues/214) | 2026-10-15T10:58:33Z | OPEN |
| [#215 — In-game spectator comments: short opt-out overlay for all players in same Online Lobby](https://github.com/EriArk/TrainerOS/issues/215) | 2026-10-15T10:59:17Z | OPEN |
| [#216 — Multiplayer UX ownership: route ALL ordinary network play through GameHub Lobby; keep adapter-specific Link](https://github.com/EriArk/TrainerOS/issues/216) | 2026-10-15T11:00:16Z | OPEN |

### Оставшиеся механизмы multiplayer

**Рабочая область:** MP-16. Прямой interactive P2P отдельно от Fluxer spectator; приоритетная популярная волна, сохранённый широкий охват. В текущую работу над каркасом доводка всех режимов не входит.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#186 — Multiplayer: universal host-stream Remote Play for titles without working native netplay](https://github.com/EriArk/TrainerOS/issues/186) | 2026-10-15T11:04:54Z | OPEN |
| [#187 — Multiplayer phase 1: finish NES, SNES, Mega Drive, PS1, PS2, GameCube, Wii and ready NetPlay cores](https://github.com/EriArk/TrainerOS/issues/187) | 2026-10-15T11:06:42Z | OPEN |

### Apps, Files и Browser

**Рабочая область:** APPS-16 / FILES-16 / BROWSER-16. Сохранить прототип, исправить реальную Gamescope видимость, folder/search/dock; встроенные готовые основы и общие task/download службы.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#225 — Apps v2: desktop-app grid, iOS-style folders, search, Steam-game exclusion and hidden items](https://github.com/EriArk/TrainerOS/issues/225) | 2026-10-15T15:18:02Z | OPEN |
| [#226 — Quick Apps v2: physical bottom-right dock with macOS behavior, pinned/running split and drag](https://github.com/EriArk/TrainerOS/issues/226) | 2026-10-15T15:18:08Z | OPEN |
| [#228 — Apps AP01 completion: real external app lifecycle and visible Home/touch controls over Gamescope](https://github.com/EriArk/TrainerOS/issues/228) | 2026-10-15T15:21:03Z | OPEN |
| [#229 — Apps / Files: embedded lightweight file manager with Browser-style controls and unified tasks](https://github.com/EriArk/TrainerOS/issues/229) | 2026-10-15T17:16:57Z | OPEN |
| [#230 — Apps / Browser: compact auto-hiding chrome, controller web input and shared segmented Downloads](https://github.com/EriArk/TrainerOS/issues/230) | 2026-10-15T17:07:17Z | OPEN |

### Crates и системные модули

**Рабочая область:** CRATES-16. Ручной каталог, GitHub Releases, explicit install/update, shared installer/reviews и capability-limited SPI; отдельный game-adapter contract. #147 не включает запрещённые workflows.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#113 — Adventure Reviews: shared Fluxer stars/comments and Crates install-based eligibility](https://github.com/EriArk/TrainerOS/issues/113) | 2026-10-15T16:39:32Z | OPEN |
| [#144 — Crates: handpicked physical crates for games, apps and TrainerOS modules](https://github.com/EriArk/TrainerOS/issues/144) | 2026-10-15T16:34:51Z | OPEN |
| [#145 — Crates UX: physical crate showcase, Workshop/What's New and project popups](https://github.com/EriArk/TrainerOS/issues/145) | 2026-10-15T16:34:55Z | OPEN |
| [#146 — Crates catalogue: owner-server delta sync and verified GitHub Releases](https://github.com/EriArk/TrainerOS/issues/146) | 2026-10-15T16:34:59Z | OPEN |
| [#147 — Crates release pipeline: adapted ARM64/x86-64 builds via GitHub Releases](https://github.com/EriArk/TrainerOS/issues/147) | 2026-10-15T16:35:04Z | OPEN |
| [#148 — Crates installer: manual Download/Update/Update All, versions and shared Downloads queue](https://github.com/EriArk/TrainerOS/issues/148) | 2026-10-15T16:37:11Z | OPEN |
| [#149 — Crates curation: owner/Codex manual crate layout, source rights and release inclusion](https://github.com/EriArk/TrainerOS/issues/149) | 2026-10-15T16:37:26Z | OPEN |
| [#150 — Crates donations: verified creator link via QR or embedded Browser](https://github.com/EriArk/TrainerOS/issues/150) | 2026-10-15T16:37:30Z | OPEN |
| [#151 — Crates Top Rated: one simple list with shared Fluxer ratings for all project types](https://github.com/EriArk/TrainerOS/issues/151) | 2026-10-15T16:37:34Z | OPEN |
| [#152 — Crates acceptance: physical crate discovery, GitHub installs, Downloads, public ratings](https://github.com/EriArk/TrainerOS/issues/152) | 2026-10-15T16:37:39Z | OPEN |
| [#231 — TrainerOS system modules: versioned capability-limited extensions without new primaries](https://github.com/EriArk/TrainerOS/issues/231) | 2026-10-15T21:23:21Z | OPEN |

### Start, Settings, ScreenScraper и update ownership

**Рабочая область:** SYSTEM-16. Поздние дополнения #65/#233/#234 заменяют прежнее deferred и старые индексы Settings. Видимые настройки используют реальные providers и нужный scope. #39 CLOSED не доказывает новый restore UI.

| Issue | Редакция UTC | GitHub |
| --- | --- | --- |
| [#26 — Power menu: replace Leave TrainerOS with Power Off, Restart and Switch Player](https://github.com/EriArk/TrainerOS/issues/26) | 2026-10-15T19:30:53Z | OPEN |
| [#29 — Media: add delayed ScreenScraper video previews inside selected-game panels](https://github.com/EriArk/TrainerOS/issues/29) | 2026-10-15T22:03:12Z | OPEN |
| [#35 — Start menu: add quick volume and screen-brightness sliders](https://github.com/EriArk/TrainerOS/issues/35) | 2026-10-15T19:30:56Z | OPEN |
| [#36 — Settings: add selectable ambient music and UI sound themes](https://github.com/EriArk/TrainerOS/issues/36) | 2026-10-15T21:23:12Z | OPEN |
| [#37 — Settings: add charger-connect haptics with safe power-state debouncing](https://github.com/EriArk/TrainerOS/issues/37) | 2026-10-15T21:23:14Z | OPEN |
| [#38 — Settings: expose RGB lighting through Armada capability integration](https://github.com/EriArk/TrainerOS/issues/38) | 2026-10-15T21:23:16Z | OPEN |
| [#39 — Settings: add optional Steam Gaming Mode install/restore entry backed by the reversible platform helper](https://github.com/EriArk/TrainerOS/issues/39) | 2026-10-15T21:23:19Z | CLOSED |
| [#65 — Add native ScreenScraper integration for all supported platforms](https://github.com/EriArk/TrainerOS/issues/65) | 2026-10-15T22:02:34Z | OPEN |
| [#71 — Prepared-image OS updates: base Linux OTA independent of TrainerOS Software](https://github.com/EriArk/TrainerOS/issues/71) | 2026-10-15T21:23:10Z | OPEN |
| [#86 — UX: simplify Start into a system-only quick menu and remove Pokémon Center from it](https://github.com/EriArk/TrainerOS/issues/86) | 2026-10-15T19:30:38Z | OPEN |
| [#153 — Release architecture: separate Linux image, TrainerOS Software and Crates project updates](https://github.com/EriArk/TrainerOS/issues/153) | 2026-10-15T16:51:35Z | OPEN |
| [#154 — TrainerOS Software: signed cross-distro updater independent of system image](https://github.com/EriArk/TrainerOS/issues/154) | 2026-10-15T21:23:08Z | OPEN |
| [#155 — Settings UX: show separate System Image, TrainerOS Software and game updates](https://github.com/EriArk/TrainerOS/issues/155) | 2026-10-15T21:23:06Z | OPEN |
| [#233 — Start v2: full-screen widget dashboard for QuickActions and Extensions](https://github.com/EriArk/TrainerOS/issues/233) | 2026-10-15T22:03:07Z | OPEN |
| [#234 — Settings v2: six-section controller-first UI, Light/Dark, software management and update ownership](https://github.com/EriArk/TrainerOS/issues/234) | 2026-10-15T22:03:10Z | OPEN |

## Что этим согласованием не закрыто

Новый код/сборки/устройства не проверялись документационным проходом. Apps AP01
имеет собственную evidence и открытый Gamescope overlay; shared-host Remote Play
остаётся отдельным кандидатом. Provider research, права, save/trust и реальные
network/audio/physical/TV проверки сохранены. Первая beta — installer на Armada;
окончательный набор beta gates не выведен автоматически из числа новых issues.
Правило владельца: сейчас реализуется **основной каркас**, затем по возможности
каркас остальных разделов в согласованном порядке; не доводить все виды multiplayer
и не подменять такой доводкой эту работу. Глубокая реализация функций имеет отдельные gates.
