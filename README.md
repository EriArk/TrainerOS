# TrainerOS

TrainerOS is a controller-first handheld shell built on Linux/ArmadaOS.

The idea is simple: **games should not completely disappear the moment you exit them.**

TrainerOS keeps the system and the games connected. You launch an Adventure, play normally, return to TrainerOS, and — when that game has a verified integration — parts of it can continue to exist in the system around you.

Pokémon is the first and deepest example.

![TrainerOS Home](docs/images/readme/01-home.webp)

## What that means

With Pokémon Emerald, TrainerOS can already read the real save and use it outside the game.

Your actual Party can appear on Home, in the Pokémon Center and in the Playroom. The Pokédex can show real Seen/Caught progress. Party and Storage come from the same ordinary save.

TrainerOS can also write back safely where support has been verified. Right now Emerald can be healed from the Center, and Shops & Traders can make real purchases using the money and inventories from the save.

Buy an item in TrainerOS, launch Emerald again, and the item is there.

That is the basic idea of the project: the game is still the game, but some of its world can continue into the system.

## The loop

~~~text
Boot TrainerOS
   ↓
Choose an Adventure
   ↓
Play the actual game
   ↓
Press Home / leave the Adventure
   ↓
Return to TrainerOS
   ↓
Party / Pokédex / Center / history update
   ↓
Continue the same Adventure or choose another one
~~~

TrainerOS does not use emulator savestates as the main product model. Games launch normally and use their own saves.

A guarded exit can capture the current game frame before closing, let you cancel back into the same running game, or return to the same place in TrainerOS.

## One Trainer, many Adventures

Pokémon games are organized as **Worlds** and **Adventures**, not as emulator folders.

FireRed belongs to Kanto. Emerald belongs to Hoenn. Remakes, spin-offs and ROM hacks can keep their own identity.

Your **Trainer** sits above individual games.

TrainerOS already supports multiple local Trainers, profile switching, optional PINs and separate TrainerOS history. The first per-Trainer save route is working for GBA/mGBA, so two Trainers do not have to share the same Emerald playthrough.

The long-term goal is for one Trainer to build a history across many Adventures: badges, Pokédex progress, playtime, milestones, Champion records and old teams.

![TrainerOS Worlds](docs/images/readme/02-worlds.webp)

## Pokémon Center

The Center is where game data becomes useful outside the game.

Current Emerald support includes:

- real Party and Storage
- save-backed Pokédex progress
- protected save backup/restore
- healing HP, status and PP
- Shops & Traders with real money/inventory writes
- a Party Playroom using the real current team

Future game integrations can add their own supported Center features instead of pretending every Pokémon title has the same save format.

Planned social features also live here: local TrainerOS-to-TrainerOS trading, transfers and eventually other nearby interactions between real players.

## Multiverse

TrainerOS is not limited to Pokémon.

Non-Pokémon games live in the **Multiverse**.

![TrainerOS Multiverse](docs/images/readme/03-multiverse.webp)

Today it already has a real persistent library, media/video presentation and validated launch routes across several emulator families.

The same idea applies here too, just more gradually.

Some games may only need:

~~~text
TrainerOS → launch → play → return → history/media
~~~

Others can eventually get deeper adapters of their own: progress, collections, characters, milestones or other game-specific companion features.

Steam is planned as another Multiverse source. Steam itself stays installed for downloads, store, Proton settings and everything else; TrainerOS simply becomes another way to launch installed games and return to the shell.

## The shell

Normal use is controller-only.

The main sections are:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

Paired views keep related things close:

- Worlds / Multiverse
- Pokédex / Pokémon Center
- Hall / RetroAchievements

Linux, emulator cores, save paths and desktop tools stay out of the way during normal use.

<table>
<tr>
<td width="50%"><img src="docs/images/readme/04-pokedex.webp" alt="TrainerOS Pokedex"></td>
<td width="50%"><img src="docs/images/readme/05-hall-of-fame.webp" alt="TrainerOS Hall of Fame"></td>
</tr>
</table>

## What already works

TrainerOS is still in active development, but it is already running as the main session on a real Retroid Flip 2.

Working pieces include:

- dedicated TrainerOS session on ArmadaOS
- controller-first navigation and settings
- multiple local Trainers
- Batocera-style ROM folder discovery and gamelist media
- contextual library rename/move/delete
- RetroArch, melonDS and Dolphin launch routes
- guarded game exit and return to TrainerOS
- Pokémon Emerald save-backed Pokédex, Party and Storage
- living Party on Home and in Playroom
- protected Emerald healing and purchases
- RetroAchievements account/read/cache integration
- local game video previews
- ScreenScraper backend groundwork

Support is deliberately verified game-by-game and runtime-by-runtime. Similar games do not automatically inherit save-writing support.

## Library format

TrainerOS follows Batocera-style folders and `gamelist.xml` instead of inventing a private ROM format.

~~~text
Emulation/
  bios/
  roms/
    gba/
    snes/
    psx/
    gamecube/
    ...
~~~

Drop a supported game into the appropriate folder and TrainerOS can discover it. Existing artwork and metadata remain useful.

## Where it is going

The finished system is intended to ship as a TrainerOS image based on ArmadaOS, with first-run setup, OTA updates, rollback, portable Trainer backups and support for more handhelds through explicit device profiles.

Pokémon is going first because it gives TrainerOS the clearest version of the idea.

Multiverse games can follow one by one.

## Development

TrainerOS is native C++20 + Qt 6/QML with SDL2, SQLite and CMake.

~~~sh
git clone https://github.com/EriArk/TrainerOS.git
cd TrainerOS

cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure

./build/native/traineros --windowed --ephemeral
~~~

More detail:

- [Roadmap](docs/ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Batocera library](docs/BATOCERA_LIBRARY.md)
- [Emerald Party / Storage](docs/EMERALD_PARTY.md)
- [Emerald healing](docs/EMERALD_HEALING.md)
- [Emerald Shops & Traders](docs/EMERALD_SHOPS.md)

TrainerOS does not ship ROMs, BIOS files, commercial saves or private ripped media.

The project is unofficial and is not affiliated with Nintendo, The Pokémon Company, Retroid, ArmadaOS, Valve, ScreenScraper, RetroAchievements, Libretro or the emulator projects it can integrate with.

No software license has been selected yet.
