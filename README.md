# TrainerOS

**A controller-first handheld system built around your games, your progress and the people you play with — not emulator menus.**

<img src="screenshots/site-2026-10-04/01-home.png" alt="TrainerOS Home on AYN Odin 2">

TrainerOS is the interface you boot into, browse your library from, talk to friends in, launch games from and come back to when you are done.

For an ordinary game, that can be as simple as **pick it → press A → play → press Home → return**.

For games TrainerOS understands more deeply, the boundary starts to disappear. Your party, progress, collections and history can live in the system around the game. Friends can see that you are playing, ask to join, and enter a supported multiplayer game without opening an emulator lobby or typing an IP address.

The first supported Adventure connects a creature-collecting RPG to the OS beyond launching it. The Games library also supports ordinary games without a semantic adapter.

TrainerOS is still in active development, but the current build already runs as a dedicated handheld session on **Retroid Pocket Flip 2** and **AYN Odin 2**.

[Full-resolution handheld screenshots — 4 October 2026](screenshots/site-2026-10-04/README.md)

[Interactive website demo](web-demo/README.md) — a mouse-driven interface tour
with original fictional content and optional animated examples. The static bundle
can be served independently or embedded in a website; it does not run emulators
or connect to real accounts.

Current product direction: [Product architecture (Russian)](docs/PRODUCT_ARCHITECTURE_RU.md); [accepted screen decisions](docs/PRODUCT_SPEC.md).
It describes accepted behaviour; [project status](PROJECT_STATUS.md) distinguishes delivery and remaining acceptance.

## Development status and getting started

This repository currently provides **source and development deployment tooling**,
not a ready-to-flash public TrainerOS image. Prepared Flip 2 and Odin 2 installations
have real device evidence; that does not establish a fresh-install experience on
every handheld. Selected game artwork and names are blurred in the public screenshots; the surrounding interface is unchanged.

- [Build and run from source](docs/DEVELOPMENT.md)
- [Project status (Russian)](PROJECT_STATUS.md) · [Changelog (Russian)](CHANGELOG.md) — current capabilities, evidence and dated changes for posts and the website.
- [Documentation](docs/README.md) · [Contributing](CONTRIBUTING.md)
- [Current tasks and remaining acceptance](docs/CURRENT_TASKS.md) · [Roadmap](docs/ROADMAP.md)
- [Security reporting](SECURITY.md) · [Licensing and third-party materials](THIRD_PARTY_NOTICES.md)

| Area | Current boundary |
| --- | --- |
| Library, series and handheld shell | Installed device evidence; compatibility depends on available runtimes/content |
| Semantic game integration | Exact supported game/build adapters, not all games on a platform |
| Social and multiplayer | Implemented routes with bounded evidence; distinct-network, larger-group and human audio acceptance remain open |
| Public distribution and updates | First beta: installer over existing Armada; clean installation, recovery and release verification remain open. Images and SteamOS/Bazzite come later |
| Code license and public artwork | Original code: GPL-3.0-or-later; third-party terms remain separate and artwork review is ongoing |

## Pick a game and play

Games opens collections and platforms, with wheel, list, shelves and screenshot grid views. Home adapts to the selected game; Y opens Recent / ★ without launching a game merely by selecting it.

<table>
<tr>
<td width="50%"><img src="screenshots/site-2026-10-04/02-worlds.png" alt="TrainerOS Worlds"></td>
<td width="50%"><img src="screenshots/site-2026-10-04/04-multiverse.png" alt="TrainerOS Multiverse"></td>
</tr>
</table>

There is no normal setup screen between the library and the game. If a game is ready, **A launches it**.

TrainerOS keeps emulator paths, cores, command lines and room settings out of the everyday interface. Press Home while playing to open Game Options: continue, minimize and browse TrainerOS, manage the current lobby when available, or exit cleanly. Returning to a minimized game restores the same running process; a universal pause is not promised.

Your ordinary game save stays the ordinary game save. TrainerOS does not make savestates the center of the experience.

<img src="screenshots/site-2026-10-04/05-game-library.png" alt="TrainerOS game library with media and preview">

The general library can use Batocera-style folders and existing metadata, artwork and local video previews. Player-count information can be shown where it is known, and games with supported multiplayer can expose it directly from the library instead of making you remember which emulator happens to support what.

## Chat, invite, play

**Social is built into TrainerOS.** It is not a link that throws you into another app.

Messages lists people and opens their direct conversation on the right. Groups and communities share the Communities section; Discover handles people, invitations and Recently Together. Context menus retain basic membership actions, calls and supported messages through Fluxer, with touch and controller operation.

The screenshots below are dated evidence of an earlier layout; current flows are described in the product contract.

<table>
<tr>
<td width="50%"><img src="screenshots/social-2026-10-08/messages.png" alt="TrainerOS Messages with touch and controller controls"></td>
<td width="50%"><img src="screenshots/social-2026-10-08/group-details.png" alt="TrainerOS group membership, game access and notification controls"></td>
</tr>
</table>

GameHub has the dedicated Lobby catalogue. Home offers the same access to your rooms or Create Lobby. Rooms can choose a game together, collect votes and readiness, and prepare a supported runtime. Once everyone is ready, the organizer explicitly presses **Start**. A shared launch window shows preparation and cancellation.

Scheduled meetings can create a local reminder after acceptance. Invitations wait in notifications; service groups created by Lobby stay out of ordinary Social. Supported connections use the existing emulator mechanisms rather than player-entered IP addresses. Each route retains its own compatibility and remaining network checks.

A group voice call is separate from a particular match, so the same friends can keep talking while somebody changes games, leaves a party or joins another one.

Real paired gameplay has already been exercised on handhelds with several classic console games, PSP Lumines and supported multiplayer titles. Some supported games can prepare three or four player seats as well. Compatibility is still being expanded and verified game by game; TrainerOS does not label every multiplayer ROM as online-capable just because an emulator can launch it.

## Adventures continue outside the game

The first supported Adventure shows how a playthrough can connect to the OS.

With a supported save, the creatures you see in TrainerOS are the same ones that are in your game.

<table>
<tr>
<td width="50%"><img src="screenshots/site-2026-10-04/06-field-guide.png" alt="TrainerOS Field Guide"></td>
<td width="50%"><img src="screenshots/site-2026-10-04/07-party.png" alt="TrainerOS Party"></td>
</tr>
</table>

The **Field Guide** can show real Seen/Caught progress. **Party** shows the actual six party members. **Boxes** reads the real storage. **Journey** can follow real badges and milestones.

The connection also works in the other direction.

From TrainerOS, supported saves can already be healed, creatures can be moved between Party and Boxes, occupied slots can be swapped, held items can be managed, Boxes can be renamed, and protected releases can be performed. Shops and services can spend the game's real currencies and put the purchased result into the real save.

Launch the game afterwards and the game sees the change.

<table>
<tr>
<td width="50%"><img src="screenshots/site-2026-10-04/09-care-center.png" alt="TrainerOS Care Center"></td>
<td width="50%"><img src="screenshots/site-2026-10-04/11-shops.png" alt="TrainerOS Shops"></td>
</tr>
</table>

The **Playroom** is deliberately less practical. It gives the real current party somewhere to exist on the handheld outside battles — moving around, reacting and simply being present instead of turning every party screen into another spreadsheet.

<img src="screenshots/site-2026-10-04/10-playroom.png" alt="TrainerOS Playroom">

TrainerOS also has its own **Link** activities. Two nearby TrainerOS handhelds have already completed protected trades, gifts, creature sales, native trade evolutions and full-team battles using the real saves. That is separate from ordinary emulator multiplayer: persistent creatures moving between two Trainers needs its own safeguards and recovery.

## One Trainer across many games

A **Trainer** belongs to the person using the handheld, not to one ROM.

Multiple local Trainers can have their own profiles, optional PINs, history, navigation state and accounts while sharing the same installed game library. The first isolated ordinary-save route is already working for one supported emulator runtime, with more runtimes planned to follow.

<img src="screenshots/site-2026-10-04/12-journey.png" alt="TrainerOS Journey">

**Journey** keeps the larger story: current progress, milestones and eventually the pieces worth remembering after a particular save has moved on. Hall records and RetroAchievements remain their own sources instead of being mixed together into invented progress.

The intended end state is that your Trainer can survive the device too: export it, restore it after a reflash, or move it to another supported TrainerOS handheld without treating the whole system as disposable.

## Each series can have its own experience

The **Multiverse** is a full part of TrainerOS, not a waiting room for games that do not have special integration.

A game can simply have a good cover, video preview, play history, direct launch, Home menu and clean return. That is enough.

Series collections and adapter-owned Home/game sections are already available. The installed framework hosts the Pokémon experience and a general fallback; new game families still need their own verified adapters. A Sonic, Final Fantasy, Metal Slug or Need for Speed section should use concepts that fit those games. Test modules do not establish those commercial-game integrations.

Installed Steam games are intended to join the Multiverse in the same way. Steam itself remains available for the things Steam is good at; TrainerOS only needs to make the installed games feel like part of the same handheld.

## It behaves like the device, not an app on the device

The main navigation stays small:

**Home ⇄ Games ⇄ GameHub ⇄ Social ⇄ Apps**

L1/R1 switches sections and preserves their last place. L2/R2 switches related views or Home blocks. GameHub contains My Story, Screens, RetroAchievements, Lobby and supported adapter screens. Apps includes applications, Files, Browser and Crates.

Home opens current-game settings and activity controls. Start opens customizable widgets and quick actions, including Settings, Power and Switch. The profile is available from the avatar in the chassis.

First run already walks through controls, optional Wi-Fi/Bluetooth, date and time, game storage, Trainer creation and an optional PIN. It can continue offline and resume if setup is interrupted.

<table>
<tr>
<td width="50%"><img src="screenshots/site-2026-10-04/13-system-menu.png" alt="TrainerOS system menu"></td>
<td width="50%"><img src="screenshots/site-2026-10-04/14-settings.png" alt="TrainerOS Settings"></td>
</tr>
</table>

Start opens the widget and quick-action menu. Settings handles the things you normally expect from the handheld itself: connections, display and audio, storage, accounts and communication settings. Plasma stays available as an explicit maintenance/recovery environment, and Steam remains installed.

The point is not to remove Linux. The point is that you should not have to use a Linux desktop just to use your game console.

## Where it is going

The first public beta is planned as an **installer over existing ArmadaOS**. It should prepare the environment automatically, identify whether the OS is on microSD or internal storage, show responsive progress and preserve personal data. A simple launcher and minimal steps are the target; a finished public installer is not available yet.

Images and other host installers remain later work. Current priorities include the complete ordinary-launch catalogue (including PS3/Switch homebrew checks on Odin), locked compatible emulator configurations, recoverable installation and the named outstanding user/network checks. Your ROMs, firmware, saves and personal media remain yours.

What already exists is deliberately kept separate from what is merely planned. TrainerOS may know that a game launches without claiming it understands the save. It may know that a game has four players without claiming four real TrainerOS users have completed a session. Unsupported things stay unsupported instead of becoming optimistic buttons.

For the current implementation status and the long version of what remains, see the [Roadmap](docs/ROADMAP.md). For the much nerdier development evidence, the [project docs](https://github.com/EriArk/TrainerOS/tree/main/docs) are there on purpose so this README does not have to become one.

---

TrainerOS does not ship commercial ROMs, BIOS files, commercial saves or private ripped media.

TrainerOS is an unofficial fan-made project and is not affiliated with game publishers, Retroid, AYN, ArmadaOS, Valve, Fluxer, ScreenScraper, RetroAchievements, Libretro or the emulator projects it can work with.

Original TrainerOS code is licensed under **GNU GPL version 3 or later
(GPL-3.0-or-later)**. See [LICENSE](LICENSE), [scope and redistribution](LICENSING.md)
and [third-party notices](THIRD_PARTY_NOTICES.md). This software license does not
grant rights to game artwork, media or trademarks.
