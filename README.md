# TrainerOS

TrainerOS is a controller-first **Linux handheld shell/session** for a Pokémon-focused portable system.

The initial target is a **Retroid Flip-class device running ArmadaOS**. TrainerOS is software-only: no physical modification of the handheld is required.

> Status: native C++20 / Qt Quick shell installed as the main Flip 2 session, with a persistent [Worlds collection](docs/COLLECTION_CATALOGUE.md), controller file attachment and verified launch/return through RetroArch and selected [standalone adapters](docs/STANDALONE_ADAPTERS.md). The owner has confirmed physical navigation, profile creation and Adventure launch. Fresh installations show missing catalogue editions without invented ownership/progress; `--ephemeral` retains the sample experience. [Session recovery](docs/SESSION_PROTOTYPE.md) is exercised; sleep remains disabled. The [current roadmap](docs/ROADMAP.md) separates delivered features from the next modules and platform changes.

Start → Controller provides live input/display checks and a local diagnostic report for the first handheld run. These software observations help validate the actual device; they do not certify its physical mapping or ArmadaOS session behavior. See [device diagnostics](docs/DEVICE_DIAGNOSTICS.md).

The current build also includes verified [GBA saved-moment resume](docs/RETROARCH_RESUME.md), a personal [Hall of Fame archive](docs/HALL_OF_FAME.md), the [offline Pokédex and manual field journal](docs/POKEDEX.md), a [local Trainer overview](docs/TRAINER_OVERVIEW.md), and [Pokémon Center save backups](docs/SAVE_BACKUPS.md). These have been exercised with controller input on Flip 2. [English FireRed/Emerald save progress](docs/GAME_PROGRESS.md) now supplies Home's badge crystals and National Dex counts. A real [RetroAchievements account/read provider](docs/RETROACHIEVEMENTS.md) exists; global account management and verified emulator earning are the next integration step, not proof of current-save progress.

A read-only [battery gauge](docs/POWER_STATUS.md) shows the handheld's charge in the fixed lower panel.

## Product idea

TrainerOS is not a ROM list with a Pokémon skin and it is not a launcher opened from another desktop. It is the **primary user interface of the device**: a cohesive full-screen trainer terminal organized around Adventures, Worlds, a Pokédex, the Trainer profile, and the history of completed journeys.

Normal lifecycle:

**Power on / wake → TrainerOS → choose or continue an Adventure → emulator/application → return to TrainerOS**

The underlying Linux desktop should not appear during normal use.

## Platform model

TrainerOS uses **ArmadaOS as the system base** instead of rebuilding the low-level handheld Linux stack.

ArmadaOS provides the operating-system foundation, device support, graphics/audio/input stack, packages, and emulator environment. TrainerOS is the main everyday graphical mode. The accepted target is TrainerOS plus **Plasma Mobile for maintenance**, if validated on Flip; Steam Gaming Mode should be removable with a proven restore path. Current Steam and Plasma Desktop sessions remain available until that migration is verified.

The current stack is:

```text
Retroid hardware
└─ ArmadaOS / Linux
   ├─ TrainerOS session  ← normal/default experience
   │  ├─ Home
   │  ├─ Worlds
   │  ├─ Pokédex
   │  ├─ Trainer
   │  ├─ Hall of Fame
   │  └─ System / Pokémon Center services
   │
   ├─ Adventure integrations
   │  ├─ RetroArch
   │  ├─ melonDS
   │  ├─ Azahar  ← installed runtime; TrainerOS adapter planned
   │  ├─ Dolphin
   │  └─ future adapters
   │
   ├─ Steam Gaming Mode  ← retained until reversible removal is proven
   ├─ KDE Plasma Desktop  ← installed maintenance option
   └─ KDE Plasma Mobile  ← Armada maintenance choice; consolidation planned
```

The 2026-09-13 plan intentionally replaces the former permanent three-mode requirement. Plasma Mobile must prove networking, files, terminal/package tools and recovery before ordinary Desktop is retired. Steam removal must preserve personal data and include a tested restore launcher/CLI. Shared KDE components remain where required. These are planned system changes, not changes made by updating this document. See [session consolidation](docs/ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).

TrainerOS is not currently intended to become a Linux distribution of its own. If the project matures, a later deliverable may be a reproducible/custom ArmadaOS image with TrainerOS preconfigured as the default session.

See [`docs/ARMADA_PLATFORM.md`](docs/ARMADA_PLATFORM.md).

## Core principles

- **Primary device shell.** TrainerOS is where the user lives after boot/wake and where supported Adventures return after exit.
- **ArmadaOS underneath, TrainerOS on top.** Reuse the proven Linux/device stack instead of imitating an OS from inside Android.
- **Keep Plasma.** KDE Plasma is an intentional maintenance/recovery mode, not the normal interface.
- **Controller-first.** Every normal action must work without touch, mouse, or keyboard.
- **Console-like.** Fast startup, predictable focus, minimal text entry, no desktop chrome in normal use.
- **Pokémon-native information architecture.** The UI talks about *Worlds*, *Adventures*, *Trainer*, *Pokédex*, and *Hall of Fame*. Small platform badges identify editions; ROM folders, cores and emulator commands stay in maintenance/integration layers.
- **Worlds, not Games.** The playable library is organized by Pokémon regions first.
- **Full-screen sections.** Home is one page among peers; it is not permanently visible behind everything.
- **L1/R1 are sacred navigation controls.** They switch primary full-screen sections and are not reused by local features.
- **Fast resume.** Recent resumable points appear as compact visual cards, not one giant Continue hero screen.
- **Progress becomes history.** TrainerOS should grow into a personal archive of regions visited, Pokédex progress, completed Adventures, teams, and Hall of Fame entries.
- **Modular integrations.** Emulator/process/filesystem quirks stay behind adapters.
- **Mock visuals are disposable.** Product behavior matters more than preserving early compositions; visual redesign is expected throughout development.
- **No bundled copyrighted game content.** No ROMs, BIOS files, keys, ripped art/audio, or commercial saves in the repository.

## Primary navigation

The top-level UI is a horizontal set of full-screen sections:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

- `L1` / `R1`: previous / next primary section
- D-pad / left stick: move focus inside the current section
- `A`: confirm / open
- `B`: back / close
- `Start`: system menu
- `Y`: open/close Continue Adventure on Home

Exact secondary shortcuts can evolve, but **L1/R1 remain globally reserved**.

See [`docs/UX_NAVIGATION.md`](docs/UX_NAVIGATION.md).

## Home

Home is a living trainer overview, not a game launcher and not a permanent shell layer.

It may show a restrained mix of:

- active World / current Adventure
- region and badge progress
- Pokédex progress
- recently discovered/caught Pokémon when available
- latest milestone / Hall of Fame activity
- time, battery, network, storage, sync/backup state
- a themed visual tied to the active World, Trainer, or featured Pokémon

### Continue Adventure drawer

Home contains a compact **slide-out Continue Adventure panel** with recent resumable points.

A card may contain:

- save-state/session screenshot
- Adventure title
- World / region
- location when known
- timestamp
- tiny progress metadata such as badges or playtime

Selecting a card rebuilds Home around that Adventure without launching it. A large frame-mounted button on Home starts the chosen Adventure, or resumes its selected state when supported. The explicit Home choice and real launch history survive restart; without an explicit choice, Home uses the most recently launched Adventure.

The panel is secondary: **Home itself is not the Continue screen**.

## Worlds

Worlds is the Adventure library, organized by region rather than emulator/platform.

Initial set:

- Kanto
- Johto
- Hoenn
- Sinnoh
- Unova
- Kalos
- Alola
- Galar
- Paldea

A World can contain multiple Adventures and expose status such as Not Visited, In Progress, Completed, Champion, Pokédex completion, total time, and last visit. Emulator/process details remain hidden implementation metadata.

The native shell supports region → Adventure list → Adventure detail, with personal records managed through Start → Manage Adventures. An Adventure can belong to several regions or a custom World. Controller focus reveals rows inside bounded lists; Back, page switching and restart preserve useful selections. Unconfigured records show Needs setup and unknown progress. The explicit sample preview demonstrates richer adapter capabilities without running an emulator.

## Pokédex

The Pokédex combines reference information with the Trainer's personal history. Long-term data may include Seen/Caught, first encounter, Worlds encountered, party appearances, favorite status, shiny history, and notes/tags.

The implementation should support offline/local data and provider adapters. It must not require proprietary artwork committed to this repository.

The offline field guide contains 1025 species and 1579 named forms, with regional lists, type/status filters, name/number search and form-specific reference facts. Favorites, manual Seen/Caught marks and field notes persist locally. Controller input covers browsing and editing; the reference remains separate from personal records and game saves. See [Pokédex](docs/POKEDEX.md).

## Trainer

Trainer is the persistent profile and progression view: trainer identity, featured Pokémon, active Adventure, total playtime, Worlds visited/completed, badges, Pokédex totals, championships, and milestones.

It should feel like an in-universe trainer device, not account settings.

The Trainer can be created locally and edited later, including name, avatar/emblem, and favorite Pokémon. Controller-operated text entry and persisted Save/Cancel behavior are part of the functional scope.

## Hall of Fame

Hall of Fame combines a permanent archive of completed Adventures with RetroAchievements achievements. Archive entries may store World, Adventure, completion date, final playtime, final team of six, screenshot, notes, and whether the data was imported or manually confirmed. External achievement unlocks retain their own source and remain separate from current-save progress.

The local archive supports controller-operated creation and editing of completed-Adventure memories, including team, date, manual time and notes. Records survive restart and remain independent of current saves. RetroAchievements stays in this section, with controller account entry, verified cartridge-file matching and private offline records. Normal runs never show fictional unlocks. See [Hall of Fame](docs/HALL_OF_FAME.md) and [achievement integration boundaries](docs/RETROACHIEVEMENTS.md).

## Pokémon Center / system services

“Pokémon Center” is an optional user-facing metaphor for maintenance services such as:

- save backup / restore
- save-state/session management
- sync status
- storage health
- Adventure integration status
- missing-content relinking

It does not need to occupy a primary L1/R1 page.

`Start` opens the system menu. That menu may expose TrainerOS settings, controller mapping, Pokémon Center, Adventure management, restart/power actions, and an explicit **Desktop / Maintenance Mode** that leaves TrainerOS for KDE Plasma.

## Device-shell behavior

TrainerOS owns the normal handheld lifecycle:

- boot/login/session startup should enter TrainerOS by default once integration is stable
- wake should restore TrainerOS or the active Adventure appropriately
- exiting a supported Adventure should return to TrainerOS
- TrainerOS should restore prior page/focus quickly after external applications close
- desktop panels/window chrome should never appear in the normal shell
- Plasma remains reachable through an explicit maintenance action
- a TrainerOS crash should recover to a safe session/maintenance path rather than leave the user with a dead black screen

Exact ArmadaOS session/compositor integration must be validated on the target device instead of assuming one desktop/session implementation.

## Technical direction

Recommended application stack:

- C++20 (or later where justified)
- Qt 6
- QML / Qt Quick for the shell UI
- CMake
- controller/input service as first-class infrastructure
- SQLite-backed local persistence behind repository interfaces
- Linux process/service/filesystem boundaries for system behavior
- capability-based Adventure adapters for emulator launch, resume points, screenshots, saves, and metadata
- a dedicated TrainerOS session entry for production use on ArmadaOS

QML is preferred because TrainerOS is a GPU-accelerated, controller-driven embedded-style shell with frequent visual iteration. UI code must remain independent from emulator-specific implementation details.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Development model

The original first build was a **full controller-navigable mock**. Its foundations now support the real modules above; the mock remains available for isolated UX/testing work.

The mock is **not a final visual design**.

Development proceeds **bottom-up in dependency order**:

**native project skeleton → shared interface/controller skeleton → shared backend and persistence → individual functional modules → validated integrations and optional progress enrichment**

The [unified queue](docs/ROADMAP.md#unified-execution-order--existing-work-and-new-issues) combines existing unfinished work with the new issues. First reconcile device, DS backup and RA acceptance gaps; retain archive completeness, current launch fixes, broader adapters/save readers, moment maintenance, derived progress, install/update rollback and controller polish. Add collection cleanup, Adventure media, global accounts, Pokédex artwork/Caught, healing and reversible system cleanup at their dependency points. The [carried-forward register](docs/ROADMAP.md#existing-work-carried-forward) records each remaining result and its evidence; new issues do not reset or displace it. Sleep remains explicitly deferred. Only verified capabilities become product promises, and independent work can proceed around a blocked device gate.

See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Repository map

- [`CMakeLists.txt`](CMakeLists.txt), `src/`, `tests/` — native application and automated checks
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — build, run, input mappings and verification limits
- [`AGENTS.md`](AGENTS.md) — working contract for Codex/agents
- [`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md) — product behavior
- [`docs/UX_NAVIGATION.md`](docs/UX_NAVIGATION.md) — controller/navigation rules
- [`docs/DESIGN_LANGUAGE.md`](docs/DESIGN_LANGUAGE.md) — visual direction and anti-patterns
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Qt/Linux architecture
- [`docs/ARMADA_PLATFORM.md`](docs/ARMADA_PLATFORM.md) — ArmadaOS/session integration plan
- [`docs/DATA_MODEL.md`](docs/DATA_MODEL.md) — domain model
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — foundation and module implementation order
- [`docs/CODEX_START.md`](docs/CODEX_START.md) — first implementation brief

## Original first milestone — historical

The bootstrap acceptance below is retained for context; use the roadmap's working baseline and next queue for current work:

1. compile as a native Linux/Qt application
2. run full-screen in a normal development desktop session and on the target ArmadaOS device
3. be fully controller-navigable
4. use `L1/R1` to switch Home, Worlds, Pokédex, Trainer, and Hall of Fame
5. provide a working compact Continue Adventure drawer using mock resume data
6. show region-first Worlds navigation
7. open a TrainerOS system menu with a stubbed Maintenance/Desktop action
8. preserve useful UI state across restart once the shared backend milestone is complete
9. route mock launching/resume through `MockAdventureAdapter`

Do **not** begin by replacing the ArmadaOS session or integrating real emulators. Prove the shell safely as a normal application first; then add a dedicated TrainerOS session once crash/recovery behavior is understood.

## Deliverables

The final project is **not an APK**.

Expected mature outputs are:

- native ARM64 Linux TrainerOS executable and resources
- installable package for the ArmadaOS base (packaging format chosen after validating the current base system)
- TrainerOS session/launcher configuration
- optional installer/setup tooling
- later, potentially a reproducible/custom ArmadaOS image with TrainerOS preconfigured

## Legal / content policy

Keep the repository clean:

- no ROMs
- no BIOS/firmware dumps
- no encryption keys
- no commercial game dumps
- no ripped proprietary UI/audio assets
- no bundled commercial-game save files

Use original placeholders, user-provided content, and appropriately licensed data/assets.

---

The goal is simple: opening the Flip should feel like powering on **a personal Pokémon trainer terminal with years of Adventures inside it**, while Linux, Plasma, emulator binaries, and filesystem details quietly do their jobs underneath.
