# TrainerOS

**The system is not a menu between games. It is the place your games come back to.**

![TrainerOS Home](docs/images/readme/01-home.webp)

TrainerOS started from a simple annoyance: on a normal handheld, the system knows which game file you launched, while the game knows everything that actually matters — your party, your progress, where you have been, what you collected, what you won. The moment the emulator closes, those two worlds stop talking to each other.

TrainerOS is an attempt to erase that line.

It is a controller-first handheld system experience built on Linux and ArmadaOS. You turn the device on as **your Trainer**, leave TrainerOS to play an **Adventure**, then come back with part of that Adventure still alive in the system around you.

Pokémon is where this idea starts, because it makes the connection wonderfully literal. Leave Pokémon Emerald and your team does not have to disappear with the emulator window. TrainerOS can read the real save, bring that party onto Home, show the same Pokémon in the Center and Playroom, heal them outside the game, buy an item outside the game, then launch Emerald again and let the game itself see the result.

The long-term idea is bigger than Pokémon. Games in the **Multiverse** can gradually gain their own ways to flow back into the system too. Some may only provide a beautiful launch/return experience and history. Others may eventually expose progress, collections, characters, milestones or game-specific companion features. Pokémon is simply the first place where TrainerOS is going deep.

> You do not open a collection of disconnected games.  
> **You continue the story of the person playing them.**

---

## One continuous loop

TrainerOS is designed around a loop that crosses the boundary between the system and the game:

~~~text
Power on
   ↓
Your Trainer
   ↓
Home / Worlds / Multiverse
   ↓
Choose an Adventure
   ↓
Play the actual game
   ↓
Home / Guide
   ↓
Return to TrainerOS
   ↓
The system refreshes what it can really know
   ↓
Party · Pokédex · Center · Journey · history · people nearby
   ↓
Go back into the same Adventure — or start another one
~~~

The game remains the game. TrainerOS does not replace Emerald's battle system, Steam, RetroArch, Dolphin or melonDS. It gives those games a place to return to and, where an integration is proven safe, a way to leave something meaningful behind.

Ordinary game saves remain authoritative. TrainerOS does not build its normal experience around emulator savestates. A guarded exit can capture the live game before the shell asks to close it, cancel back into the same still-running process, or return cleanly to the exact TrainerOS context you left.

That is the basic rhythm of the whole project: **system → game → system → game**, without feeling like you keep dropping in and out of unrelated applications.

---

## Pokémon: where the boundary already starts to disappear

The deepest working integration today is an exact English Pokémon Emerald build.

TrainerOS can already read the real ordinary save and turn it into shared system state: National Pokédex Seen/Caught progress, the six-member Party, all fourteen Storage boxes, nicknames, levels, types, nature, ability, held items, stats, moves and PP. The same verified Party can quietly live on Home, appear as practical data in the Pokémon Center, and wander around a small Playroom.

The important part is that these are not decorative copies typed into TrainerOS. They are the Pokémon that are actually in that save.

The connection also goes in the other direction. The first protected Emerald writers are live: the Center can restore Party HP, status and PP, and Shops & Traders can make real purchases with the money and inventories stored by the game. TrainerOS currently understands dozens of Emerald money counters, including ordinary Poké Marts, Lilycove departments, TMs, vending, herbs, plants and Secret Base decorations.

Those writes are tested by launching the normal game afterwards and checking the result there. Buy Fire Blast in TrainerOS, launch Emerald, and Fire Blast is in the Bag. Buy a Pichu Doll, walk to the in-game PC, and the decoration is there.

This is the part of TrainerOS that best explains the project: **the Pokémon Center is no longer only a place inside one ROM. It can become part of the device itself.**

[Emerald Party and Storage](docs/EMERALD_PARTY.md) · [Save-backed Pokédex](docs/SAVE_POKEDEX.md) · [Center healing](docs/EMERALD_HEALING.md) · [Shops & Traders](docs/EMERALD_SHOPS.md)

---

## A Center that exists between Adventures

The Pokémon Center is meant to become the shared companion space around supported Pokémon games.

Today it already has real Party and Storage views, protected save backup/restore, a working Emerald treatment room, Shops & Traders, and a living Party Playroom. The Playroom is intentionally separate from practical management: it is the place where your actual team can wander, sleep, greet one another, react to you and simply exist outside the battle screen.

The direction goes further. Verified games can gain safe Party/Storage operations, title-specific services and a read-only Practice Battle sandbox built from the real team without writing battle damage back to the save.

And when another TrainerOS user is sitting next to you, the same idea becomes social. The planned **Link Counter** is a local, offline meeting point for nearby TrainerOS devices: discover another Trainer, compare exact game capabilities, trade or transfer real Pokémon through verified providers, and recover safely if the connection drops. The goal is the feeling of a modern link cable rather than another internet account.

That local connection is also where future nearby battles and other Trainer-to-Trainer interactions naturally belong. TrainerOS is personal first, but it should not have to be lonely.

[Pokémon Center model](docs/EXPANSION_42_62.md) · [Party Playroom](docs/PARTY_PLAYROOM.md) · [Link Counter issue](https://github.com/EriArk/TrainerOS/issues/45)

---

## One Trainer, many Worlds, years of history

A Pokémon game is an **Adventure**. Kanto, Johto, Hoenn, Sinnoh and the other regions are **Worlds**.

The distinction matters because TrainerOS is not trying to make the emulator or console generation the center of your library. FireRed belongs to Kanto because that is where the Adventure lives. Emerald belongs to Hoenn. Remakes, spin-offs and meaningful ROM hacks can keep their own identity without turning the interface into a directory of cores and extensions.

Your **Trainer** exists above those individual Adventures.

Multiple local Trainers already work on one device, with isolated TrainerOS history, navigation, media and account state, optional controller-entered PINs and a family recovery code. The first private ordinary-save route is also live for GBA/mGBA, so a second Trainer can start Emerald from New Game without inheriting somebody else's playthrough.

The larger goal is a personal history that keeps growing across games: Worlds visited, badges, Pokédex progress, playtime, milestones and preserved Champion records. The planned Journey/Hall model separates the live state of the current save from historical moments, so beating a game can become part of your Trainer's history even after that save moves into post-game or is replaced by another run.

A future portable Trainer backup is intended to make that identity survive a reflash or a move to another supported device. The system can be replaced. **The Trainer should remain yours.**

![TrainerOS Worlds](docs/images/readme/02-worlds.webp)

---

## Multiverse: Pokémon first, not Pokémon only

TrainerOS is still a game system.

Everything outside Pokémon lives in the **Multiverse**: a system-oriented library for retro games and, in the planned direction, locally installed Steam games as well.

The working Multiverse already uses the same controller-first shell, history, selection, media and launch/return model as Pokémon. TrainerOS understands Batocera-style folders and gamelist metadata, can discover games without a private ROM manifest, and has real validated launch routes across several cartridge, disc and standalone emulator families.

Over time, Multiverse games can get deeper adapters of their own.

A simple game may only need:

~~~text
TrainerOS → launch → play → Home → clean return → history/media
~~~

A richer adapter may eventually let a game leave more behind: progress, a collection, a character state, milestones, recent context or a companion screen that makes sense for that particular title. TrainerOS does not pretend every game exposes the same semantics. Depth is added **game by game, when it is actually useful and verifiable**.

Steam follows the same philosophy. TrainerOS is not trying to rebuild the Steam Store, downloader, Proton settings or Steam Input. Steam stays installed. The planned integration simply makes installed Steam games another Multiverse source for everyday play, while the full Steam environment remains available when you actually need it.

![TrainerOS Multiverse](docs/images/readme/03-multiverse.webp)

---

## The interface is part of the fiction

TrainerOS is meant to feel like a device, not a desktop application stretched across a handheld screen.

The normal shell has five peer spaces navigated with the shoulders:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

Paired faces keep related spaces close without turning the interface into a forest of tabs: Worlds pairs with Multiverse, Pokédex with Pokémon Center, and Hall/Journey with RetroAchievements. Home can switch between Pokémon and Multiverse context.

There is no normal reason to choose a RetroArch core, edit a save path, open a file manager or see a Linux desktop. Those details still exist, but they live underneath the experience or in explicit maintenance tools.

<table>
<tr>
<td width="50%"><img src="docs/images/readme/04-pokedex.webp" alt="TrainerOS Pokedex"></td>
<td width="50%"><img src="docs/images/readme/05-hall-of-fame.webp" alt="TrainerOS Hall of Fame"></td>
</tr>
<tr>
<td align="center"><b>Pokédex</b><br><sub>Offline reference plus real save-backed progress where a provider is verified.</sub></td>
<td align="center"><b>Hall / Journey</b><br><sub>Current progress, memories, Champion history and the RetroAchievements companion.</sub></td>
</tr>
</table>

Artwork, animated companions, environmental World cards, local game video previews and later audio/haptic/lighting layers are there to make the handheld feel alive, but the UI tries to keep the information itself calm and readable.

---

## What is real today?

TrainerOS is still in active development, but this is no longer only a UI prototype. The current code has been installed and exercised as the main session on a real Retroid Flip 2 running ArmadaOS.

| Area | Working state |
| --- | --- |
| Dedicated handheld shell | TrainerOS can boot as the normal graphical session, launch games and return without exposing a desktop during ordinary use |
| Controller-first UI | Full shell navigation, text entry, settings, diagnostics, profiles and game management work without touch/mouse |
| Multiple Trainers | Real profile creation/switching, PIN protection, family recovery and owner-scoped TrainerOS state |
| Library | Batocera-style folder discovery, gamelist metadata/media, contextual rename/move/delete, stable Adventure identity |
| Launch / return | Real RetroArch and standalone adapter routes, clean exit media and guarded return into the shell |
| Pokémon Emerald | Save-backed Pokédex, Party, Storage, living Party, healing and protected real purchases |
| Save safety | Exact-build validation, protection backups, stale-source checks, candidate verification, atomic replacement and read-back verification |
| Multiverse | Real persistent non-Pokémon library, shared game wheel, media/video presentation and multiple validated runtime routes |
| RetroAchievements | Real Trainer-scoped sign-in/read/cache foundation; deeper earning/notification integration is still being expanded |
| ScreenScraper | Native client, hashing, platform map and safe gamelist/media writer are prepared; the complete end-user scraping flow is not finished yet |

The project is deliberately strict about the difference between **implemented**, **verified**, and merely **planned**. A working emulator process does not automatically mean a platform is supported. A similar ROM does not inherit another game's save writer. A pretty screen does not claim data that no provider can prove.

The continuously updated [ROADMAP](docs/ROADMAP.md) and module evidence documents record those boundaries in much more detail.

---

## Why the save integration is intentionally picky

TrainerOS does not say “Gen III saves are supported” because one Emerald file worked.

Deep integration is tied to an **exact game/build and a verified save format**. Read and write capabilities are separate. Unknown information stays unknown. A ROM hack, translation or revision is its own case until proven otherwise.

For a protected write, the intended pattern is conservative:

~~~text
resolve the exact current save
        ↓
prove the game is not writing it
        ↓
stable read + validation
        ↓
verified protection backup
        ↓
prepare a separate candidate
        ↓
validate the exact allowed change
        ↓
re-check the source revision
        ↓
atomic replacement
        ↓
read it back and verify again
~~~

That is slower to implement than a generic save editor, but it is the only direction that makes sense for a system meant to live with somebody's years-old games.

A global read-only integration mode is also planned for users who want all of the save-aware presentation with **zero TrainerOS-initiated save mutation**.

[Save capability model](https://github.com/EriArk/TrainerOS/issues/42)

---

## The library belongs to the user

TrainerOS follows familiar Batocera-style filesystem and gamelist conventions instead of making the internal database the only way to own a collection:

~~~text
Emulation/
  bios/
  roms/
    gba/
      game.gba
      gamelist.xml
      images/
      videos/
    snes/
    psx/
    gamecube/
    ...
~~~

Copy a supported game into the right system folder and TrainerOS can discover it. Existing metadata and media remain useful. Moving a game through TrainerOS preserves its Adventure identity and personal history instead of turning it into a new game because the path changed.

The planned native ScreenScraper flow builds on that same structure: explicit scraping writes compatible metadata/media back into the library and then lets the ordinary offline scanner pick it up. Normal browsing never needs a network request.

[Batocera library contract](docs/BATOCERA_LIBRARY.md) · [ScreenScraper integration](docs/SCREENSCRAPER.md)

---

## From a project to an actual handheld system

The current target is the **Retroid Flip 2**. ArmadaOS supplies the underlying Linux, graphics, hardware and emulator ecosystem; TrainerOS owns the everyday device experience above it.

The intended public distribution is a reproducible **TrainerOS image based on ArmadaOS**, not a pile of manual shell commands for the user. The finished path is meant to look like:

~~~text
flash image
   ↓
boot TrainerOS
   ↓
first-run device setup
   ↓
create / restore Trainer
   ↓
find library and check runtimes
   ↓
play
~~~

OTA updates with rollback, device profiles for additional handhelds, portable Trainer backups, runtime/BIOS health, reliable sleep/wake and privacy-safe support bundles are all part of that productization path.

The Flip 2 is the reference device, not a promise that every Linux handheld is already supported. New hardware should earn support through a real device profile and physical validation rather than a model-name check.

[System image](https://github.com/EriArk/TrainerOS/issues/70) · [OTA](https://github.com/EriArk/TrainerOS/issues/71) · [Device profiles](https://github.com/EriArk/TrainerOS/issues/78)

---

## Under the hood

TrainerOS is a native **C++20 + Qt 6 / QML** application with SDL2 input, SQLite persistence and narrow platform/integration services. The dedicated session runs on the ArmadaOS/Linux stack with Gamescope.

The architecture intentionally keeps the interesting boundaries boring:

~~~text
Qt Quick presentation
        ↓
feature controllers / semantic models
        ↓
Trainer + library + history + progress repositories
        ↓
game-specific providers and Adventure adapters
        ↓
RetroArch / melonDS / Dolphin / Steam / platform services
        ↓
ArmadaOS / Linux / hardware
~~~

QML does not parse save offsets, rewrite XML by hand or talk directly to emulator processes. Game-specific binary knowledge stays inside exact providers. Slow filesystem, database, hashing and network work belongs on worker/service boundaries.

That separation is what lets a real Emerald Party feed Home, Center and Playroom without each screen learning how Emerald stores a Pokémon.

[Architecture](docs/ARCHITECTURE.md) · [Data model](docs/DATA_MODEL.md) · [UX/navigation](docs/UX_NAVIGATION.md)

---

## Building it today

There is no finished consumer image release yet. The repository is the active development tree.

For a normal Linux development build:

~~~sh
git clone https://github.com/EriArk/TrainerOS.git
cd TrainerOS

cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure

# Safe desktop preview:
./build/native/traineros --windowed --ephemeral
~~~

The current development baseline uses CMake 3.24+, C++20, Qt 6.4+ (Core, Gui, Qml, Quick, Sql, Network, Xml and Multimedia), SDL2, OpenSSL 3, SQLite and ffmpeg for the video test fixture.

The dedicated ArmadaOS session is device-specific work. Do not treat the desktop build instructions as a safe installation recipe for an arbitrary handheld. See [Native development](docs/DEVELOPMENT.md), [device baseline](docs/ARMADA_DEVICE_BASELINE.md) and [session integration](docs/SESSION_PROTOTYPE.md).

---

## Local first, by design

TrainerOS does not need a TrainerOS cloud account to make the basic idea work. Your local Trainer, games, saves and history are the center of the system. RetroAchievements and ScreenScraper are optional external services, not prerequisites for having a usable device.

There is also no plan to turn TrainerOS into a subscription, paid feature ladder or store. The project exists because this kind of device should be fun to have. If donations are accepted, the intended use is practical: buying additional handhelds so new device profiles can be developed and physically verified instead of guessed.

TrainerOS also does not ship ROMs, BIOS/firmware dumps, commercial saves, account credentials or private ripped media. User game content stays on the user's device.

---

## Project status and licensing

TrainerOS is an unofficial fan-made project and is not affiliated with Nintendo, The Pokémon Company, Retroid, ArmadaOS, Valve, ScreenScraper, RetroAchievements, Libretro or the emulator projects it can integrate with. Their names and trademarks belong to their respective owners.

The repository is public, but **no software license has been selected yet**. Until a license is added, normal copyright rules apply to the source code. Artwork, fonts, sprites and other assets may also have their own recorded provenance and terms.

---

## Where this is heading

The end goal is not “a better ROM launcher.”

It is a handheld where the boundary between the operating system and the games on it becomes increasingly thin.

A place where you can leave an Adventure and still see your team. Where your old saves become part of a longer personal history instead of files hidden in emulator folders. Where a Pokémon Center can live outside one game. Where the person sitting next to you can become another Trainer instead of another anonymous online account. Where a non-Pokémon game can eventually teach the system something meaningful about what you did there too.

**The game can close. The Adventure does not have to.**
