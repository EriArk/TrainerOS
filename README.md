# TrainerOS

TrainerOS is a controller-first **Linux handheld shell/session** for a Pokémon-focused portable system.

The initial target is a **Retroid Flip-class device running ArmadaOS**. TrainerOS is software-only: no physical modification of the handheld is required.

**Adventure exit:** the verified RetroArch route captures gameplay before a compact Home-button question, keeps the game alive on B and closes gracefully on A. All save policies ask for permission. A confirmed clean exit supplies a durable, owner/build-bound picture for Home and Choose Adventure; crashes never count as save confirmation. [Implementation and remaining migration gates](docs/ADVENTURE_EXIT.md#durable-exit-media-and-ordinary-home-selection).

**Target reconciliation — 2026-09-19 (#62).** The accepted [#42–62 specification](docs/EXPANSION_42_62.md) supersedes older product direction. Planned behavior below is not a claim that the deployed build has changed; see the [working baseline](docs/ROADMAP.md#working-baseline) and dated module evidence.

> Status: native C++20 / Qt Quick shell installed as the main Flip 2 session, with a persistent [Worlds collection](docs/COLLECTION_CATALOGUE.md), controller file attachment and verified launch/return through RetroArch and selected [standalone adapters](docs/STANDALONE_ADAPTERS.md). The owner has confirmed physical navigation, profile creation and Adventure launch. Fresh installations show missing catalogue editions without invented ownership/progress; `--ephemeral` retains the sample experience. [Session recovery](docs/SESSION_PROTOTYPE.md) is exercised; sleep remains disabled. The [current roadmap](docs/ROADMAP.md) separates delivered features from the next modules and platform changes.

Start → Controller provides live input/display checks and a local diagnostic report for the first handheld run. These software observations help validate the actual device; they do not certify its physical mapping or ArmadaOS session behavior. See [device diagnostics](docs/DEVICE_DIAGNOSTICS.md).

Legacy [GBA state-resume code](docs/RETROARCH_RESUME.md) is isolated to its historical tests. Production uses ordinary startup and state-independent save backups; Home/Y show the clean exit picture with embedded black bars trimmed for display. The current build includes a personal [Hall of Fame archive](docs/HALL_OF_FAME.md), the [offline Pokédex and manual field journal](docs/POKEDEX.md), a [local Trainer overview](docs/TRAINER_OVERVIEW.md), and [Pokémon Center save backups](docs/SAVE_BACKUPS.md). These have been exercised with controller input on Flip 2. [English FireRed/Emerald save progress](docs/GAME_PROGRESS.md) now supplies Home's credited, game-set badge artwork and National Dex counts. A real [RetroAchievements account/read provider](docs/RETROACHIEVEMENTS.md) exists; global account management and verified emulator earning are the next integration step, not proof of current-save progress.

Start provides [quick volume/brightness controls and a confirmed Power menu](docs/DEVICE_CONTROLS.md#start-quick-controls-and-power---2026-09-20). Physical volume-key handling belongs to the dedicated session and remains active during Adventures; Switch Player still awaits separate Trainer ownership.

A read-only [battery gauge](docs/POWER_STATUS.md) shows the handheld's charge in the fixed lower panel.

Optional private [Pokédex illustrations](docs/ARTWORK_BOOTSTRAP.md) now appear in
the list, detail and favorite-species picker. Up on detail opens exact-form
alternatives and source credits; missing art stays explicit. The real Flip
bootstrap supplies the measured profiles for the later generic pack contract.

The [preserved plan for issues #19–41](docs/EXPANSION_PLAN.md) and [accepted #42–62 extensions](docs/EXPANSION_42_62.md) add separate Trainer profiles, a non-Pokémon **Multiverse** inside Worlds and a second Home context, alongside shared visual, media, feedback and Help work. These are planned extensions; [the roadmap](docs/ROADMAP.md) retains all earlier unfinished and future commitments and records changed catalogue/session decisions.

## Product idea

TrainerOS is not a ROM list with a Pokémon skin and it is not a launcher opened from another desktop. It is the **primary user interface of the device**: a cohesive full-screen trainer terminal organized around Adventures, Worlds, a Pokédex, the Trainer profile, and the history of completed journeys.

Normal lifecycle:

**Power on / wake → TrainerOS → choose and launch an Adventure → emulator/application → return to TrainerOS**

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
   │  └─ Start / Settings services; Center is paired with Pokedex
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
- **Worlds, not Games.** Pokémon Adventures are organized by region first; the planned Multiverse context groups non-Pokémon titles by system within the same primary page.
- **Full-screen sections.** Home is one page among peers; it is not permanently visible behind everything.
- **L1/R1 are sacred navigation controls.** They switch primary full-screen sections and are not reused by local features.
- **Choose, then launch.** Recent Adventure cards change shared context; the separate launch action starts the game normally using its ordinary save. #49 removes the normal state-resume dependency.
- **Progress becomes history.** TrainerOS should grow into a personal archive of regions visited, Pokédex progress, completed Adventures, teams, and Hall of Fame entries.
- **Modular integrations.** Emulator/process/filesystem quirks stay behind adapters.
- **Mock visuals are disposable.** Product behavior matters more than preserving early compositions; visual redesign is expected throughout development.
- **No bundled copyrighted game content.** No ROMs, BIOS files, keys, ripped art/audio, or commercial saves in the repository.

## Primary navigation

The five full-screen peer pages remain **Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**, selected with **L1/R1**. Home is not a permanent background shell.

**Planned #43:** L2/R2 switches paired faces within Worlds ⇄ Multiverse, Pokédex ⇄ Pokémon Center, and Hall/Journey ⇄ RetroAchievements. Preserve each face's route/focus/filter and the launch return route. B unwinds local detail; it does not flip the pair. Use a compact existing-header/chassis indicator, not a sixth page or large second tab row. Home retains its separate **X** Pokémon/Multiverse toggle (#31).

D-pad/left stick navigates lists; A confirms, B cancels/backs out, Start opens system services. Shared Y selects an Adventure on supported Pokémon pages; Worlds retains local controls. See [navigation](docs/UX_NAVIGATION.md).

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

### Choose Adventure drawer

**P1 delivered:** the shared selector, Pokedex/Center pair and Hall/RetroAchievements pair now work on the single-Trainer baseline. [Implementation and remaining boundaries](docs/SHARED_ADVENTURE.md). The full ownership/provider contract below remains the target.

**Planned #9/#49:** one shell-owned **Y · Choose Adventure** drawer selects the active Trainer's shared `CurrentPokemonAdventureContext`: Adventure ID, resolved exact build and ordinary-save identity/revision when available. Pokémon Home, Pokédex, Center, Hall, RA and Adventure-aware Trainer consume this same context. Multiverse Home remembers its own independent game selection. Before an explicit choice, use the latest actual launch in that Trainer/domain; unrelated launches do not overwrite an explicit choice.

A on a card commits the context and closes without launching; B cancels and restores the opener unchanged. L1/R1 closes the drawer without committing and switches primary page. Modal/keyboard/Start/recovery flows suppress shared Y. Worlds intentionally keeps its browser-local search/filter controls. Resolve existing local-Y conflicts explicitly when implementing the shared route. Unsupported save features never silently select another title.

Cards show the latest clean TrainerOS exit image, title, World and honest session/progress metadata. They are recent Adventure choices, not emulator-state slots. No extra persistent Current Adventure capsule/chip or independent per-feature selector: headers/content may show identity naturally.

Unobstructed Home A immediately invokes its large physical launch button regardless of prior directional input. Launch uses normal game startup and the game's ordinary save/autosave; Worlds also has an explicit launch action. Unconfigured/empty selections offer setup or Worlds exploration. The bottom-frame drawer remains compact, expands before rising inside the fixed viewport, and preserves controller focus. See [shared selection acceptance](docs/EXPANSION_42_62.md#shared-adventure-and-paired-navigation).

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

Planned #56 gives larger World cards low-contrast schematic environmental motifs and intentional diagonal pairings for compatible small groups. Region identities and strong golden controller focus remain distinct; large important Worlds stay standalone.

## Pokédex

Pokédex remains useful offline and controller-operated: bounded lists/detail, combined regional-collection/type/status filters, name/number search, sorting and clear empty/reset states. Search uses the shared keyboard with digits in a separate right block. Reference regional membership is not proof of a personal encounter.

**Planned #46** separates five layers:

1. Offline species/form/reference facts.
2. The selected Adventure's verified ordinary-save Seen/Caught as primary current progression, with only proven regional/National/form semantics.
3. Preserved Trainer-owned manual journal, Caught collection (#14), favorites and history, explicitly sourced and secondary to the current-save view.
4. Optional installed classic illustration artwork (#13) for the primary long list.
5. Optional PMDCollab animated sprites/portraits (#51) for detail and living-party scenes, not the main list artwork.

Unknown is distinct from false/not-caught. Aggregate counts, species flags and individual Pokémon are separate evidence levels; none fabricates catch dates/forms. Failed reads retain a labeled complete last-good snapshot for the same source. Save rollback does not erase manual/history records. Shared Y refreshes the same Adventure across features; L2/R2 preserves each Dex/Center route.

Art providers are separate from reference facts and game media. No official artwork is bundled; missing/partial packs use honest fallback. The runtime artwork prerequisites are **#58 raw corpus -> #60 real Flip profiles -> #57 required contract/validation**. Per the owner's 2026-09-20 clarification, **#59 Pack Studio comes last**, after UI and asset structure stabilize; Settings does not wait for Studio. No resident downloader. [Detailed acceptance](docs/EXPANSION_42_62.md#artwork-sequence).

## Trainer

Trainer is the persistent profile and progression view: trainer identity, featured Pokémon, active Adventure, total playtime, Worlds visited/completed, badges, Pokédex totals, championships, and milestones.

It should feel like an in-universe trainer device, not account settings.

The Trainer can be created locally and edited later, including name, avatar/emblem, and favorite Pokémon. Controller-operated text entry and persisted Save/Cancel behavior are part of the functional scope.

## Hall of Fame

**Planned #47/#48/#64:** Hall's first face is a live save-backed **Journey Record**, useful before completion, plus preserved Champion/completed-run history. Show only proven title-specific badges, milestones, playtime and Dex totals, with larger game-accurate badge assets or deliberate neutral mounts; no universal eight-badge/percentage/date assumptions. See [exact badge identity and artwork acceptance](docs/EXPANSION_63_64.md#accurate-league-badges-and-neutral-fallback).

Champion snapshots preserve exact build/playthrough/source revision and verified historical team/progress. Current Party is not the historical winning team; older saves do not erase the archive. Observation time is not victory time. Manual memories/editing remain valid and explicitly sourced.

RetroAchievements is Hall's **L2/R2 companion**, following the shared Adventure through verified content/set matching. It remains an external account source independent from current-save or manual completion truth. Settings owns the active Trainer's account; unsupported mapping never substitutes another game. Same-account complete offline caches retain earned/unknown distinctions. Earning, earned-state UI and verified notification have separate gates (#12/#24/#25/U7).

Acceptance includes controller list/detail/Back, paired-face restoration, Y changes, unsupported/corrupt/rollback states, preserved manual/Champion history, two-Trainer isolation and offline/wrong-account rejection. [Projection acceptance](docs/EXPANSION_42_62.md#pokédex-journey-and-achievements).

## Pokémon Center / system services

**Planned #44/#53:** Pokémon Center is a first-class L2/R2 companion to Pokédex, using the shared Adventure context. It is not a new primary page or merely system-maintenance branding.

- Practical Party cards/list and details expose verified slots, level, HP/status, moves/PP and held items.
- Storage exposes the actual title's boxes/slots. Read browsing precedes separately proven reorder, Party/Storage moves and release; destructive actions require fresh explicit confirmation and the shared safe transaction.
- Heal/Backup/Restore services retain ordinary-save protection. Paid healing requires proven party/money fields, exact fee/balance and in-game verification; backup-only fallback remains useful.
- Link Counter trade/transfer/sale is later exact-pair, recoverable two-device work; sale means in-game currency only.
- Party remains a stable management UI. A separate Party Playroom provides optional interactions; Practice Battle copies real Party data into a read-only sandbox and never mutates saves or grants rewards.

Device/account/integration settings remain in Start/Settings. No second Center save picker. [Center acceptance](docs/EXPANSION_42_62.md#pokémon-center-and-practical-party), [Link Counter](docs/EXPANSION_42_62.md#link-counter), [Playroom/battle](docs/EXPANSION_42_62.md#sprites-living-party-playroom-and-practice-battle).

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
- capability-based Adventure adapters for normal launch, clean exit capture, ordinary saves and exact-build metadata
- a dedicated TrainerOS session entry for production use on ArmadaOS

QML is preferred because TrainerOS is a GPU-accelerated, controller-driven embedded-style shell with frequent visual iteration. UI code must remain independent from emulator-specific implementation details.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Development model

The original first build was a **full controller-navigable mock**. Its foundations now support the real modules above; the mock remains available for isolated UX/testing work.

The mock is **not a final visual design**.

Development proceeds **bottom-up in dependency order**:

**native project skeleton → shared interface/controller skeleton → shared backend and persistence → individual functional modules → validated integrations and optional progress enrichment**

The [unified queue](docs/ROADMAP.md#unified-execution-order--existing-work-and-new-issues) combines existing unfinished work with the new issues. First reconcile device, DS backup and RA acceptance gaps; retain archive completeness, current launch fixes, broader adapters/save readers, ordinary-backup maintenance and #49 legacy-state retirement, derived progress, install/update rollback and controller polish. Add collection cleanup, Adventure media, global accounts, Pokédex artwork/Caught, healing and reversible system cleanup at their dependency points. The [carried-forward register](docs/ROADMAP.md#existing-work-carried-forward) records each remaining result and its evidence; new issues do not reset or displace it. Sleep remains explicitly deferred. Only verified capabilities become product promises, and independent work can proceed around a blocked device gate.

See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Repository map

- [`CMakeLists.txt`](CMakeLists.txt), `src/`, `tests/` — native application and automated checks
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — build, run, input mappings and verification limits
- [`AGENTS.md`](AGENTS.md) — working contract for Codex/agents
- [`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md) — product behavior
- [`docs/EXPANSION_42_62.md`](docs/EXPANSION_42_62.md) — accepted save/context/Center/artwork contracts and supersessions
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
5. provide a working compact Choose Adventure drawer using mock resume data
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
