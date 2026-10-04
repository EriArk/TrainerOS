# TrainerOS

**A controller-first handheld system where games do not have to end at the emulator window.**

<img src="screenshots/site-2026-10-04/01-home.png" alt="TrainerOS Home on AYN Odin 2">

TrainerOS is a native Linux handheld shell built on top of ArmadaOS.

For an ordinary game, it can simply be the part of the device you live in: browse the library, press A, play the real game, press Home, and come back. Emulator paths, cores, room codes and desktop windows stay underneath.

When TrainerOS has a verified adapter for a game, it can know more. Progress, characters, collections, history and carefully bounded game-specific actions can become part of the handheld itself instead of disappearing when the emulator closes.

Pokémon is the first place where that idea goes deep. Everything else still works through the **Multiverse**, and other series can grow their own first-class experiences when there is something useful to build around them.

[See the current full-resolution handheld gallery](screenshots/site-2026-10-04/README.md).

## The game can continue outside the game

Pokémon Emerald is the clearest example of what TrainerOS is trying to become.

TrainerOS reads the real ordinary save. The same party that exists inside Emerald can appear on Home, in the Field Guide, in Party, in Boxes and in the Playroom. Journey can follow real badges and milestones. The Care Center can work with the same save instead of maintaining a second fake copy of your progress.

<table>
<tr>
<td width="50%"><img src="screenshots/site-2026-10-04/06-field-guide.png" alt="TrainerOS Field Guide"></td>
<td width="50%"><img src="screenshots/site-2026-10-04/09-care-center.png" alt="TrainerOS Care Center"></td>
</tr>
</table>

The connection also goes back into the game. On verified Emerald saves, TrainerOS can already heal the party, move Pokémon between Party and Boxes, manage held items, rename Boxes, release with safeguards, and make supported shop or service purchases using the game's real currencies and inventory.

Those changes are protected with backup, validation and read-back checks. Launch Emerald afterwards and Emerald itself sees the result.

<img src="screenshots/site-2026-10-04/10-playroom.png" alt="TrainerOS Playroom">

The Playroom is the less practical side of the same idea: your actual party can simply exist on the device outside battle. Practice, shops, storage and the rest of Companions are separate faces of that shared Adventure rather than separate little apps.

The save remains the authority. TrainerOS is not trying to replace the game with a save editor or a collection of savestates.

## It is still a normal game system

A game does not need a deep adapter to belong in TrainerOS.

Non-Pokémon games live in the **Multiverse**. The library follows Batocera-style folders and metadata, keeps artwork and local video useful, shows game and player information where it is known, launches supported runtimes directly, and returns to the same shell when play ends.

<table>
<tr>
<td width="50%"><img src="screenshots/site-2026-10-04/04-multiverse.png" alt="TrainerOS Multiverse"></td>
<td width="50%"><img src="screenshots/site-2026-10-04/05-game-library.png" alt="TrainerOS game library"></td>
</tr>
</table>

The important distinction is that TrainerOS does not pretend every game has the same structure.

Some games may never need more than a good library, media, launch/return and history. A larger series can eventually become a **franchise pack** with shared concepts that actually fit it — a Final Fantasy, Sonic, Metal Slug or Need for Speed experience does not have to imitate Pokémon. Games without a pack simply remain good Multiverse games.

That is also where installed Steam games are intended to live. Steam itself stays Steam; TrainerOS does not need to rebuild its store, downloads or compatibility tools just to make an installed game part of the everyday library.

## Playing together belongs to the handheld, not the emulator menu

Multiplayer is being built around the same rule: the person using TrainerOS should not have to know how a particular emulator creates rooms.

On a supported game, Home can invite a nearby Trainer or an online friend. A friend's running game can appear with the existing conversation, where they can ask to join. Groups can keep persistent membership while individual game parties come and go, and selected members can be allowed into a game without another organizer prompt. The organizer still decides when the match starts.

TrainerOS prepares the runtime-specific networking underneath that flow. Current work includes real paired gameplay through classic RetroArch routes, PPSSPP and a native Dolphin NetPlay bridge. Exact support is still tied to compatible games, runtimes and controller layouts rather than a blanket claim that every ROM can play online. Some reviewed profiles already prepare three- and four-seat games; broader real-user and separate-network acceptance is still being completed.

A group voice call is independent of a particular match. Friends can stay in the same call while playing different games, forming different parties, leaving a game or joining it again.

**TrainerOS Link is a different thing from emulator multiplayer.** Emerald already has protected local TrainerOS-to-TrainerOS trades, gifts, sales and full-team battles that operate on the real saves. The online version is being completed separately because moving persistent game data between two Trainers needs stricter recovery and trust rules than starting a temporary netplay session.

## One Trainer, many Adventures

A **Trainer** is a person on the device, not a Pokémon save and not an emulator profile.

TrainerOS already supports multiple local Trainers, optional controller-entered PINs, isolated shell history and account state, and the first verified per-Trainer ordinary-save route for GBA/mGBA. The library itself can stay shared.

<img src="screenshots/site-2026-10-04/12-journey.png" alt="TrainerOS Journey">

Journey is where current progress can turn into longer-lived history. A save can move into post-game or eventually be replaced without requiring every meaningful milestone to vanish with that exact file. Hall records and RetroAchievements stay separate sources instead of being blended into made-up progress.

The longer-term ownership model goes further: portable Trainer backup/restore, stronger save lineage for trust-sensitive interactions, and more verified per-Trainer save namespaces across supported runtimes.

## The handheld is the interface

TrainerOS is meant to be the normal graphical mode of the device, not an application launched from a Linux desktop.

The main shell is deliberately small:

**Home ⇄ Worlds ⇄ Companions ⇄ Trainer ⇄ Social**

Related views sit behind the shoulder buttons instead of turning the screen into a wall of tabs. Companions contains Field Guide, Party, Boxes, Care Center, Playroom and Shops. Trainer contains Profile, Journey, Hall and RetroAchievements. Social contains conversations, groups, communities and discovery.

First run already covers controls, optional Wi-Fi/Bluetooth, date and time, library storage, Trainer creation and optional PIN setup. Settings handles everyday device work such as connections, display/audio and storage without requiring a mouse or keyboard.

<table>
<tr>
<td width="50%"><img src="screenshots/site-2026-10-04/13-system-menu.png" alt="TrainerOS system menu"></td>
<td width="50%"><img src="screenshots/site-2026-10-04/14-settings.png" alt="TrainerOS settings"></td>
</tr>
</table>

Plasma remains an explicit maintenance/recovery environment. Steam remains installed. TrainerOS is trying to hide ordinary desktop plumbing when it is not useful, not remove the escape hatch when it is.

## What the finished product is supposed to be

The intended public release is a reproducible **TrainerOS image based on ArmadaOS** for supported handhelds.

The goal is not a README full of Linux setup steps. A new device should boot into a short controller-first setup, let you choose internal or removable game storage, create a Trainer, optionally connect accounts, then enter Home. Supported emulator/runtime baselines belong in the image. User ROMs, BIOS files, saves and private media remain user-owned data.

From there, updates are intended to be staged and recoverable, with a known-good image/data rollback path rather than "hope the package upgrade worked". A portable Trainer backup should make a reflash or move to another supported device much less dramatic.

Game integration is meant to become separately installable too. The current adapter work already keeps exact game knowledge and runtime integration isolated enough to export and build outside the main application; the eventual package format comes after enough very different adapters exist to design it from reality rather than freezing a plugin API around Emerald alone.

| Area | Current state | Direction |
| --- | --- | --- |
| Handheld shell | Running as a dedicated session on Retroid Pocket Flip 2 and AYN Odin 2 | Reproducible ArmadaOS-based image with device profiles |
| Library | Pokémon Worlds + general Multiverse, direct launch, media and return | Steam source, wider readiness/repair and franchise packs |
| Deep game integration | Emerald is the first mature semantic vertical | More exact games, then installable adapters |
| Social play | Messaging, groups, calls, game parties and several real netplay routes | Broader runtime coverage, separate-network acceptance and online TrainerOS Link |
| Ownership | Multiple Trainers, PINs, history and first isolated save routes | Portable Trainer recovery and stronger save/session provenance |
| Delivery | Development devices are maintained directly | First-boot image, OTA, compatible rollback and final recovery flow |

That end state is still being built. TrainerOS already does a lot on real handhelds, but the project deliberately keeps **available**, **verified** and **planned** separate. Similar games do not silently inherit save writers. An installed emulator does not automatically become a supported multiplayer route. A four-seat configuration is not called four-player acceptance until four actual users have gone through it.

For the detailed and intentionally pedantic version of what is done and what is still open, see the [Roadmap](docs/ROADMAP.md) and [Current tasks](docs/CURRENT_TASKS.md).

## Development

TrainerOS is native **C++20 + Qt 6/QML**, with SDL2, SQLite and CMake. The emulator and game-specific mess stay behind adapters instead of leaking into the UI.

~~~sh
git clone https://github.com/EriArk/TrainerOS.git
cd TrainerOS

cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure

./build/native/traineros --windowed --ephemeral
~~~

Useful technical starting points: [Architecture](docs/ARCHITECTURE.md), [game-adapter knowledge](docs/adapters/README.md), [runtime matrix](docs/EMULATOR_MULTIPLAYER_MATRIX.md), [Emerald Link](docs/EMERALD_LINK.md) and [Armada platform notes](docs/ARMADA_PLATFORM.md).

The repository does not ship commercial ROMs, BIOS files, commercial saves, private ripped media or account secrets. Current screenshots show development devices and their local test libraries; final distribution artwork has its own provenance and packaging requirements.

TrainerOS is an unofficial fan-made project and is not affiliated with Nintendo, The Pokémon Company, Retroid, AYN, ArmadaOS, Valve, ScreenScraper, RetroAchievements, Libretro or the emulator projects it integrates with.

No software license has been selected yet.
