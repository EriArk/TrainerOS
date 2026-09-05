# TrainerOS

TrainerOS is a controller-first Android frontend for a Pokémon-focused handheld.

The initial hardware target is the **Retroid Flip / Retroid Flip-class clamshell handheld**, but the product is intentionally software-only: no physical modification of the console is required. TrainerOS should make the device feel like a dedicated trainer terminal rather than an Android handheld running a generic emulator launcher.

> Status: product definition / pre-implementation.

## Product idea

TrainerOS is not a ROM list with a Pokémon skin. It is a cohesive, full-screen interface organized around **adventures, regions, a Pokédex, the trainer profile, and the history of completed journeys**.

TrainerOS is intended to be the **primary interface of the handheld**, not an app that the user manually opens from another launcher. Android and individual emulators stay underneath the experience. The normal user flow should be:

**Power on / wake → TrainerOS → choose or continue an adventure → game → return to TrainerOS**

The user should only need to see Android when deliberately opening system settings, maintenance tools, or an explicit exit-to-Android action.

Where the target firmware permits it, TrainerOS should register and operate as the device's default Android Home/Launcher. If a firmware limitation prevents reliable Home replacement, the fallback must still preserve the dedicated-device experience through full-screen operation and deliberate recovery back into TrainerOS.

## Core principles

- **Primary device interface.** TrainerOS is the normal shell the user lives in. Android's stock launcher is an escape hatch, not part of the everyday flow.
- **Software-only.** No case, button, screen, or other hardware modification is part of this project.
- **Controller-first.** Every normal action must be usable without touch.
- **Console-like.** Fast startup, predictable focus, large readable targets, minimal text entry, no desktop-style chrome.
- **Pokémon-native information architecture.** The frontend talks about *Worlds*, *Adventures*, *Trainer*, *Pokédex*, and *Hall of Fame* — not ROM folders, cores, emulator packages, or platforms.
- **Worlds, not Games.** The library is organized by Pokémon regions first.
- **Full-screen sections.** Home is one screen among several; it is not a permanent dashboard behind the rest of the UI.
- **L1/R1 are sacred navigation controls.** They switch between the primary full-screen sections and are not reused for local actions.
- **Fast resume.** Recent save states are surfaced as compact visual cards rather than one giant “Continue” hero tile.
- **Progress becomes history.** TrainerOS should gradually build a personal archive of regions visited, Pokédex progress, completed adventures, teams, and Hall of Fame entries.
- **No bundled copyrighted game content.** The repository must not ship ROMs, BIOS files, ripped game assets, or proprietary emulator data.

## Primary navigation

The top-level UI is a horizontal set of full-screen sections:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

- `L1` / `R1`: previous / next primary section
- D-pad / left stick: move focus inside the current section
- `A`: confirm / open
- `B`: back / close
- `Start`: system menu

Exact secondary shortcuts can evolve, but **L1/R1 must remain dedicated to top-level section switching**.

See [`docs/UX_NAVIGATION.md`](docs/UX_NAVIGATION.md).

## Screens

### Home

Home is a living trainer overview, not a game launcher.

It can show a restrained mix of:

- active world / current adventure
- region progress
- badge progress
- Pokédex progress
- recently caught / recently discovered Pokémon when data is available
- latest achievement or Hall of Fame activity
- time, battery, network and backup status
- a themed visual tied to the active world, trainer, or selected Pokémon

#### Continue Adventure drawer

Home contains a **slide-out Continue Adventure panel** with compact save-state cards.

Each card should be able to contain:

- save-state screenshot
- adventure title
- world / region
- location when known
- save-state timestamp
- small progress metadata (for example badges or playtime)

Selecting a card resumes that specific state through the configured emulator adapter. If an adventure has no compatible save state, TrainerOS can fall back to launching the normal in-game save.

The panel is deliberately compact and secondary: **Home itself is not the Continue screen**.

### Worlds

Worlds is the main adventure library, organized by region rather than hardware platform.

Initial world set:

- Kanto
- Johto
- Hoenn
- Sinnoh
- Unova
- Kalos
- Alola
- Galar
- Paldea

A world can contain multiple adventures / game versions. For example, Kanto can contain original adventures, remakes, and other Kanto-focused titles. Platform and emulator details are implementation metadata, not the user-facing hierarchy.

A world can expose status such as:

- Not visited
- In progress
- Completed
- Champion
- Pokédex completion
- total time
- last visit

### Pokédex

The Pokédex is both an encyclopedia and a record of the trainer's own history.

Long-term, entries may combine canonical Pokémon data with local progress such as:

- seen / caught
- first adventure where encountered
- worlds in which the trainer has owned the Pokémon
- party appearances
- favorite status
- shiny history
- notes / tags

The implementation must allow offline caching and must not require copyrighted artwork to be committed to this repository.

### Trainer

Trainer is the personal profile and progression screen.

Possible data:

- trainer name
- avatar
- favorite Pokémon
- current adventure
- total playtime
- worlds visited
- worlds completed
- badges earned
- Pokédex completion
- milestones / achievements

The visual language should resemble a polished in-universe trainer device, not an Android settings profile.

### Hall of Fame

Hall of Fame is a permanent archive of completed adventures.

An entry can store:

- world / region
- adventure
- completion date
- final playtime
- final team of six
- optional screenshot
- optional notes

Automatic extraction from save files is a later enhancement; manual confirmation must remain a valid fallback.

## Pokémon Center / system services

“Pokémon Center” is a good user-facing metaphor for maintenance tasks, but it is **not required to be a primary L1/R1 page**.

It can live in the system menu or as a Home utility and eventually provide:

- save backup / restore
- save-state management
- cloud sync status
- storage health
- emulator integration status

`Start` should open a system menu that can also expose TrainerOS settings, controller mapping, Android settings, and an **explicit exit-to-Android action**. Android access must be intentional; it should never be the expected destination after closing an Adventure.

## Device-shell behavior

TrainerOS owns the normal handheld experience.

Expected behavior:

- launching or waking the device should land in TrainerOS whenever the platform permits it
- exiting a supported emulator/adventure should return to TrainerOS
- Android status/navigation chrome should be hidden during normal use where platform APIs allow
- crashes/restarts should recover back into TrainerOS cleanly rather than strand the user in a desktop-like launcher
- system settings and stock Android remain reachable for maintenance through an explicit path
- TrainerOS must not require replacing the Android ROM or physically modifying the device

## Visual direction

The target is **modern trainer hardware**, not “Android with Pokémon wallpaper.”

Preferred characteristics:

- clean in-universe device UI
- strong typography and clear focus state
- restrained use of Pokémon type colors
- pixel / retro details only where they add character
- world-specific accents and atmosphere without rebuilding the entire layout for every region
- motion that feels like panels/pages of a handheld terminal
- excellent readability on a small landscape display
- no mouse-like interaction patterns

Avoid:

- huge launcher tiles everywhere
- Material Design defaults showing through unchanged
- permanent desktop-style navigation bars
- emulator/core/platform terminology in the normal UI
- overloading the screen with Pokémon art
- forcing touch for normal navigation

See [`docs/DESIGN_LANGUAGE.md`](docs/DESIGN_LANGUAGE.md) for composition, focus, motion, and visual anti-patterns.

## Technical direction

Recommended implementation:

- Kotlin
- Android
- Jetpack Compose
- controller/focus navigation as a first-class input system
- local database for library and history
- adapter layer for emulator launching, save states, screenshots, and optional save parsing
- Android Home/Launcher integration as the intended production mode where supported by the target firmware
- robust full-screen frontend fallback where Home replacement is restricted

The architecture should keep the UI independent from any single emulator. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Repository map

- [`AGENTS.md`](AGENTS.md) — rules and context for Codex/agents
- [`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md) — detailed product behavior
- [`docs/UX_NAVIGATION.md`](docs/UX_NAVIGATION.md) — controller and screen navigation rules
- [`docs/DESIGN_LANGUAGE.md`](docs/DESIGN_LANGUAGE.md) — visual direction and layout principles
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — proposed Android architecture
- [`docs/DATA_MODEL.md`](docs/DATA_MODEL.md) — core domain model
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — implementation stages and acceptance criteria
- [`docs/CODEX_START.md`](docs/CODEX_START.md) — suggested first implementation task for Codex

## First milestone

The first useful build should prove the feel of TrainerOS before deep emulator integration:

1. launch as a landscape full-screen Android app
2. controller navigation works without touch
3. `L1/R1` switches Home, Worlds, Pokédex, Trainer, and Hall of Fame
4. Home has a working compact Continue Adventure drawer using mock save-state data
5. Worlds shows region-first navigation
6. Start opens a system menu
7. UI state survives app restart
8. the codebase already uses emulator adapter interfaces, even if the first adapter is a mock

Only after that should the project spend time on emulator-specific save-state integration and save parsing. Primary-Home/Launcher integration becomes a production requirement once the core controller shell is stable enough to test safely on the target device.

## Legal / content policy for the repository

TrainerOS is a frontend. Keep the repository clean:

- no ROMs
- no BIOS files
- no keys or firmware dumps
- no copyrighted game dumps
- no ripped proprietary UI/audio assets
- no bundled saves copied from commercial games

Use placeholders, original UI assets, user-provided files, and data providers with appropriate licenses.

---

The goal is simple: when the Flip opens, it should feel like **a personal Pokémon trainer terminal with years of adventures inside it**, not like a generic Android emulation handheld.