# TrainerOS Architecture

Sections describing the initial mock are implementation history; their memory-only or legacy state fields do not override the accepted targets or the Current local persistence boundary. Existing module/test evidence is retained without claiming planned APIs already exist.

**Target reconciliation — 2026-09-19 (#62).** The accepted [#42–62 specification](EXPANSION_42_62.md) supersedes older product direction. Planned behavior below is not a claim that the deployed build has changed; see the [working baseline](ROADMAP.md#working-baseline) and dated module evidence.

The read-only `CollectionRepository` composition overlays bundled catalogue editions and World references on the personal library. It creates no personal ownership or progress until file attachment is committed. Emulator installation discovery remains outside this composition; the selected adapter prepares supported registrations through a callback, keeping paths/commands outside feature UI. See [collection catalogue](COLLECTION_CATALOGUE.md).

## Recommended baseline

TrainerOS is a native Linux shell application/session running on top of ArmadaOS.

Recommended stack:

- C++20
- Qt 6
- QML / Qt Quick
- CMake
- SQLite for TrainerOS-owned structured data
- repository/service abstractions around persistence
- Linux process/service/filesystem integration behind platform interfaces
- capability-based Adventure adapter layer

The first prototype must run as an ordinary full-screen Qt application. Do not make it the default session until controller behavior, external process launch/return, persistence, crash handling, and recovery are understood on the actual device.

## Construction order

The first [game-progress reader](GAME_PROGRESS.md) uses the verified save-resolution boundary to enrich Home. A worker validates an exact content build and stable ordinary-save bytes, then publishes a versioned observation through `GameProgressProvider`. This remains separate from emulator transport, the manual Pokédex and external achievements.

Build bottom-up: native project structure, shared interface/controller components, shared backend and persistence, then functional modules and real integrations in dependency order. The interface proof uses mock data and fake boundaries; the next backend milestone makes core profile/shell state durable. It does not depend on emulator integration, external accounts or save parsing. Keep the foundation small enough to serve actual upcoming modules.

**Current extension order - owner, 2026-09-19:** the native/shared persistence foundation already exists. Finish the handheld screen and controller flows on minimal presentation contracts; preserve real consumers and use labelled development fixtures for future ones. Early art/sprite/badge providers need no new save reader. Bind owner-scoped storage and other real feature services afterwards; defer new exact-save/battle/link research. No universal all-games model or speculative save writer is needed to validate UI.

Planned [#63/#64 boundaries](EXPANSION_63_64.md): platform chronology lives in shared catalogue metadata; exact badge-set/earned identities come from verified progress evidence and resolve through a separate credited asset provider. Neither save offsets nor SVG paths belong in feature QML. Existing schema/provider support is not implied by these planned contracts.

See `ROADMAP.md` for execution order. Feasibility research is input to later modules, not a reason to skip the foundations.

## Deployment modes

The implemented Armada boundary is documented in [dedicated session integration](SESSION_PROTOTYPE.md). A separate systemd/Gamescope lifecycle owns the compositor, a Linux supervisor preserves orphaned Adventures, and confirmed mode requests drain local persistence before invoking the fixed platform helper. Normal app mode remains available for development and recovery.

The installed main/default session currently coexists with Steam and Plasma. The accepted target now prefers Plasma Mobile maintenance and reversible Steam removal; the platform service owns capability changes only after the [migration/recovery gates](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13) pass. Ordinary development app mode remains non-mutating.

### 1. Development / safe app mode

Run TrainerOS from a normal ArmadaOS desktop/development session.

Use this mode first for:

- rapid QML iteration
- debugging
- controller testing
- adapter development
- crash recovery without risking an unusable graphical session

### 2. Dedicated TrainerOS session

Production goal once the shell is stable.

TrainerOS should have its own graphical session entry that starts the required compositor/session environment and TrainerOS without exposing Plasma desktop chrome.

Desired lifecycle:

```text
boot/login
  ↓
TrainerOS session
  ↓
TrainerOS shell
  ↓ launch Adventure
external emulator/application
  ↓ exit
TrainerOS shell
```

The exact session/compositor/display-manager arrangement must be discovered from the current ArmadaOS build and isolated inside platform integration. Do not assume a particular display manager or Gamescope nesting strategy in feature code.

### 3. Desktop / Maintenance Mode

KDE Plasma remains installed and usable.

TrainerOS system menu exposes a deliberate action to leave the dedicated session and enter a normal Plasma desktop for:

- network troubleshooting
- file management
- package management
- terminal/development
- recovery
- advanced configuration

Returning from maintenance mode should make it easy to re-enter TrainerOS.

## High-level module shape

Start simple but preserve clear boundaries:

```text
src/
  app/
    Main.cpp
    TrainerApplication.*
  core/
    model/
    usecase/
    repository/
    input/
    navigation/
  features/
    home/
    worlds/
    pokedex/
    trainer/
    halloffame/
    system/
  integrations/
    adventure/
      mock/
      retroarch/
      melonds/
      azahar/
      dolphin/
    pokedex/
  platform/
    linux/
    armada/
    process/
    power/
    network/
    storage/
    session/
  persistence/
    sqlite/
  qml/
    shell/
    components/
    pages/
    overlays/
    themes/
```

A single executable is acceptable initially. Separate libraries/modules only when they improve replacement/testing boundaries; avoid architecture ceremony before the UI proof exists.

## Accepted next-module boundaries — planned

The first #49 media consumer is now concrete: `PlayHistoryController` binds a
launch-time `ExitMediaSource`; the store worker verifies the content and commits
the bounded JPEG and successful session in one transaction. `LibraryRepository`
projects the latest scoped `ExitMedia` to Home/Y through an opaque image handle.
It is independent of the legacy ResumePoint provider. This uses the existing
single Trainer/Pokemon context and does not preempt #20 multi-owner migration or
the later scalable media cache. [Storage and limits](ADVENTURE_EXIT.md#durable-exit-media-and-ordinary-home-selection).

The following target boundaries extend existing repositories/workers; [#42–62 acceptance](EXPANSION_42_62.md) and the [roadmap](ROADMAP.md) define implementation order. Do not build a framework before its first consumer.

- A shell-owned `CurrentPokemonAdventureContext` publishes committed Trainer/Adventure/exact-build/ordinary-save identity and revision. Shared Y changes it; features never own a second current save. Multiverse selection remains independent. Capture initiating owner/context/revision on async work and reject stale results after selection or Trainer/account switches.
- Navigation owns five primary pages and the L2/R2 paired faces, with per-face route/focus/filter state. Home X changes domain. Drawer/modal priorities and Worlds local Y are explicit; no duplicate persistent context capsule.
- #49 replaces state-based launch with ordinary startup/save loading. The lifecycle service coordinates capture-before-prompt, still-running-game cancellation, verified save policy, graceful exit, interruption and provenance-bound media. Prove actual compositor/input handoff before generic overlay claims.
- Adventure media owns title/build images and clean exit captures by Trainer/domain/Adventure/session/revision. #13 illustration art and #51 sprite/portrait providers use separate species/form identities and source/credit rules. They may share bounded cache utilities, never ownership or compatibility assumptions.
- #42 exact-save resolution/providers extend the current reader/backup layer with independent read/write capabilities and immutable semantic snapshots. Reads feed Dex/Party/Storage/Journey; mutation is through a shared protected transaction, never QML offsets or a read provider implicitly promoted to editor.
- Owner-scoped repositories serve #19–20 onboarding/chooser/PIN and one active-Trainer RA account service. Shared installations/library/media stay device-wide. Legacy shared saves retain explicit lineage; account creation cannot imply private playthroughs.
- #46 current-save Pokédex and #47 Journey/Champion projections preserve #14 manual collection/history and existing Hall memories separately. #48 RA is an external account/set projection, not proof of current-save progress.
- #44 practical Center, #53 Party and separate #54 Playroom consume the same semantic Party/Storage. #52 uses bounded native FSM/steering, one update tick and hidden/reduced-motion controls. #55 battle owns disposable copies behind an ARM64-verified engine adapter; no external-save write channel.
- #45 Link Counter follows proven exact-pair import/export/money writers and durable two-endpoint transaction recovery. Discovery, transport and transaction coordination are separate; no raw save transfer or success on one-sided commit.
- Existing media/audio/haptics/RGB/Steam/boot helpers and offline Help retain [#19–41 gates](EXPANSION_PLAN.md). Help consumes allowlisted non-secret runtime facts, never a second config store. Shared chassis/header primitives remain reusable.

The artwork track is specifically #58 one-shot raw corpus → #60 temporary Flip importer and measured canonical profiles → #57 contract/shared validator → #59 separate Qt desktop Pack Studio → polished Settings manager. No resident downloader, speculative plugin system or UI-specific filesystem logic.

## QML / C++ boundary

QML owns presentation and local visual interaction.

C++ owns:

- domain/use-case logic
- persistence repositories
- controller normalization
- process lifecycle
- filesystem access
- emulator adapters
- ArmadaOS/platform integration
- long-running/background work

Do not put shell commands, hard-coded emulator paths, save-file parsing, or direct SQLite logic in QML.

Expose narrow view models/controllers to QML rather than the entire service graph.

## Top-level shell state

Maintain one stable shell state/navigation controller for:

- active primary page
- remembered per-page focus/navigation state
- system-menu visibility
- active World/theme accent
- currently running/last Adventure
- shell/session lifecycle events

Primary pages are persistent conceptual peers rather than repeatedly destroyed desktop windows.

Primary pages:

```text
HOME
WORLDS
POKEDEX
TRAINER
HALL_OF_FAME
```

`L1/R1` change only this top-level page state.

Nested routes remain owned by their feature page. `B` unwinds local detail before any higher-level behavior.

The native `WorldsController` owns region/list/detail routes, the selected World ID, a remembered Adventure ID per World and detail-action focus. It projects `LibraryRepository` domain records into QML-facing rows/details and asks the injected Adventure adapter for each record's capabilities. `WorldsPage.qml` owns layout and revealing the focused row inside its bounded list; it does not select integrations or launch processes. The shell handles global actions and overlays before forwarding local input. Worlds navigation remains in memory when switching primary pages.

Library controllers read committed UI-thread snapshots after asynchronous store startup. Refresh preserves identities across reordering and falls back when records disappear. Before a normal launch request, the controller rechecks the record's primary/additional World relationships. The ephemeral preview supplies a synchronous fake snapshot; original unconfigured composition used `LocalStateStore` and an adapter with no launch capabilities; current configured adapters are documented in the module records. Browsing state is durable; ordinary-save support now has the per-build limits in GAME_PROGRESS and SAVE_BACKUPS.

## Input architecture

Controller input is infrastructure.

Create a central input service that:

- reads Qt gamepad/key events or the selected lower-level input source
- normalizes device-specific codes
- applies dead zones and repeat policy
- maps `A/B/Start/L1/R1`
- emits semantic TrainerOS actions
- supports future remapping
- prevents features from consuming reserved global actions

Conceptual actions:

```cpp
enum class TrainerInput {
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    SystemMenu,
    ToggleContinue, // legacy enum name; target shared Choose Adventure with modal/Worlds rules
    PreviousCompanion, // planned L2
    NextCompanion, // planned R2
    PreviousPage,
    NextPage
};
```

The concrete source may evolve if Qt's high-level gamepad support is insufficient for the target device. That substitution must not change feature APIs.

The initial native source is SDL2 GameController, polled from the Qt event loop. It translates inputs into the semantic `Action` enum and centralizes dead zones, edge detection, directional repeat, hotplug and foreground gating. SDL handles controller mappings; the physical Flip 2 mapping outside Steam still needs device testing. SDL virtual controllers exercise the same source in automated tests. Keyboard shortcuts are development conveniences. See `DEVELOPMENT.md` for current mappings and limits.

`ToggleContinue` is the legacy action name, separate from Confirm. Target Y selects the shared Adventure without launching; Home A requests normal launch. `PlayHistoryController` records actual process sessions without parsing saves. The existing `ContinueEntry`/`ResumePoint` composition is legacy code to migrate under #49, not an active design requirement. See [Home and play history](HOME_AND_HISTORY.md). Map semantic actions to the physical device's labeled controls through the input/device profile.

Controller text entry is shared shell infrastructure for Pokédex search and profile editing. It owns its draft text, key focus, Apply/Cancel behavior, and restoration to the requesting control. Keep text-entry and filter-picker focus above the underlying screen in Back precedence.

Profile creation/editing must use a repository/use-case boundary with stable identity, validated drafts, explicit save/cancel, and recoverable write failures. A conversational design preview's in-memory state does not satisfy the native application's persistence requirement.

The native prototype now uses `TextEntryController` for the reusable buffer, key geometry, spatial focus and Apply/Cancel lifecycle. It emits accepted text without knowing its consumer. `TrainerController` owns the profile form draft and validation, backed by `TrainerRepository` / `MockTrainerRepository`; profile identity is no longer supplied by the sample library repository. The shell coordinates layer precedence and records an explicit text consumer (Trainer name or Pokédex search), routing Apply only to that consumer and clearing it on global navigation. `KeyboardPanel.qml` and feature pages present these controllers without storage logic.

Transient layer priority is notice → system menu → keyboard → active service / profile form / Continue → primary page. Start preserves the lower layer. Global page changes cancel transient edits. The shell routes text to its explicit consumer, including library title/notes/custom World name, and restores the requesting control. The Trainer controller assigns a stable profile identity on creation. Fake repositories support isolated failure tests; normal profile and library writes use asynchronous SQLite storage.

## Persistence

TrainerOS-owned metadata belongs in a local repository layer backed initially by SQLite.

Persist data such as:

- World configuration/state
- Adventure metadata and hidden adapter configuration
- recent play history, source-aware observations and exit-media cache
- Trainer profile
- Pokédex personal progress
- Hall of Fame entries
- cached external achievement definitions/unlocks, separate from local completion and current-save progress
- integration configuration
- shell/settings/controller preferences

Ordinary saves remain external source data; legacy emulator-state references are retained only as required for safe migration. Store references, derived metadata, checksums, and backups where appropriate; never make the TrainerOS database the only copy of actual game progress.

## Adventure adapter layer

Adapters own runtime validation, ordinary launch, actual process lifecycle, save resolution, clean gameplay capture and title-specific save policy where proven. The UI never interprets paths, runtime arguments or raw save bytes. Keep async I/O and process waits off the UI thread.

Target capabilities are independently advertised for the **exact configured Adventure/build**, not an entire emulator or generation. Launch and lifecycle can work without semantic save reading. `manualConfirm | autosave | unknown` is verified per title/integration, with unknown asking before exit.

Save providers supply independent read flags for Trainer summary, money/playtime, badges, Pokédex, Party, Storage, Pokémon records, Journey and Champion. Separate write flags cover heal, reorder, moves/release, money and import/export. Common provenance includes ROM revision/hash, runtime/configuration, format/resolver and stable save fingerprint. See [domain contract](DATA_MODEL.md#exact-save-snapshots-and-transactions--planned).

Protected writes require stable source/no active writer → verified protection backup → separate candidate → checksums and allowed semantic delta validation → source revision recheck → atomic replacement where supported → independent readback and retained recovery. Multi-file formats require their own recovery protocol. Refuse unsupported/corrupt/changed sources; preserve unknown format-specific bytes needed for round trips.

**Historical implementation:** the current C++ adapter still exposes enumeration/direct resume/state screenshot methods. These are migration inputs, not the target interface; [legacy resume evidence](RETROARCH_RESUME.md) is retained. No new adapter should expand them. #49 must decouple ordinary-save resolution from any legacy resume configuration before enabling safe writers.

## Process lifecycle

**Accepted target #49, not yet implemented:** normal TrainerOS creates, manages and resumes no emulator savestates/ResumePoints, in either Pokémon or Multiverse. Ordinary game saves/autosaves are authoritative. Relaunch starts the game normally; the game loads its own save.

On user-requested exit, capture a clean gameplay screenshot **before** the overlay. Resolve exact title/integration policy `manualConfirm | autosave | unknown`; do not infer it from platform. Manual/unknown asks “Have you saved?” while the game remains alive. B returns to the same process; A confirms graceful exit. Per the owner's 2026-09-19 clarification, verified autosave also asks "Close this game?"; it never exits without A. The target trigger is the physical Home/Guide button instead of Start+Select, subject to the Flip routing gate. Start retains the shell system menu. Confirmation is a user assertion, not automatic proof of saving.

Exit images feed Home/Y/history with Trainer/domain/Adventure/session provenance. Cancelled attempts and crash/kill/battery loss cannot fabricate a confirmed exit or replace valid history with a false capture. Preserve prior valid media where appropriate and mark interrupted outcomes honestly. Capture failure leaves a usable exit/cancel path, never a state-thumbnail substitute.

Checkpoint the launching page/paired face/route/focus, restore it promptly on return, and refresh ordinary-save observations asynchronously. First prove capture, overlay/input ownership and cancellation with the still-running emulator on Flip. Migration retires only verified TrainerOS-owned obsolete state artifacts safely, preserving ordinary saves, histories and independent images. [Full lifecycle/migration acceptance](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit).

The existing `AdventureLaunchController` owns checkpoints/exactly-once restoration; `ProcessService` owns asynchronous QProcess/literal arguments. Its `AdventureExitController` now supplies the explicit pending-capture/confirmation/close protocol, disabled until a platform provider opts in for that running game. Cancellation does not stop the process; only an actual clean exit after a graceful-close request can complete the pending image. Generic stop is not a graceful-close implementation and no longer masks failed history. The shell still hides on Adventure start; the platform capture/overlay/input route is not yet connected or proven. Preserve orphaned-Adventure recovery and platform-owned compositor handoff. [Exit protocol and remaining gates](ADVENTURE_EXIT.md), [implemented launch lifecycle](LIBRARY_AND_LAUNCH.md), [session evidence](SESSION_PROTOTYPE.md).

## First adapter

The original mock demonstrated controller navigation with fake library/capability/failure records. Its state-resume samples are historical under #49. Reuse isolated fakes to exercise normal launch, capture/confirmation/cancellation and unavailable capabilities without coupling UI work to an emulator.

Real RetroArch, melonDS and Dolphin paths now exist with per-title limits. Additional adapters remain one complete verified runtime at a time; installed Azahar/Cemu alone is not integration proof. See [working baseline](ROADMAP.md#working-baseline).

## Continue model

The target Choose Adventure drawer projects recent Adventure choices and clean exit media from repositories; it never scans files from QML. Stable Adventure IDs, Trainer/domain scope, session/media revision, optional ordinary-save observations and honest availability replace the legacy state-point model.

Selection commits the shared Pokémon context (or independent Multiverse Home choice), closes and never starts a process. Home/Worlds normal launch is separate. Current selection may have no readable save or screenshot; do not invent progress or choose a different title to make a feature appear supported. Preserve useful legacy history and independently sourced images during #49 migration.

## Pokédex providers

The normal composition now injects `OfflinePokedex` for bundled, validated species/form facts and `LocalStateStore` for personal marks. `PokedexJournalEditor` owns transient manual Seen/Caught/note drafts; `SqlitePokedexJournal` owns schema/transaction mapping on the existing storage worker. Feature UI never reads ROMs or interprets the reference as current game progress. See [Pokédex](POKEDEX.md).

Keep reference/canonical Pokédex data separate from personal progress.

Suggested split:

- `PokedexReferenceProvider` — species/types/evolution/reference data
- `PokedexProgressRepository` — Seen/Caught/Favorite/history owned by TrainerOS
- exact-build `GameProgressProvider` implementations — planned primary current-save projections (#42/#46), independent from manual history

Reference providers may use appropriately licensed bundled data, local imports, or cached network data. The UI is not hard-wired to one API.

The native mock implements `PokedexReferenceProvider::load()` and `PokedexProgressRepository::progress()/setFavorite()` as separate injected boundaries. A single fixture supplies their independent reference/progress data in memory. `PokedexController` owns the applied query/filters/order, stable selected entry ID, list/detail/rail/choice focus and recovery behavior. QML owns layout and revealing the focused row, not filtering or storage. Collection membership is reference data; personal encounter history is not inferred from it.

Filtering intersects all active dimensions. Favorite changes rebuild the result set without altering Seen/Caught or reference records. Removing the selected result returns to a valid list row or recovery state. Failed writes retain the old mark; failed reference refreshes retain the last successful snapshot. The synchronous fixture establishes interaction only: async loading, provider/version provenance, full regional catalogs, persistence and game-progress enrichment remain later work.

## Hall of Fame achievements provider

Hall of Fame combines local completion archives with RetroAchievements data through a separate achievement-provider/repository boundary. Introduce the contract/fake at the foundation stage; implement the real provider when the Hall of Fame module is reached.

Keep external account/game/achievement identities and unlock modes/dates distinct from local Adventure runs and HallOfFameEntry records. A provider refresh updates a cache asynchronously and must not block local archive browsing. Disconnected, unsupported, cached/offline, loading and failed-refresh states are explicit. Manual archive changes do not award RA achievements, and imported unlocks do not imply that the current save has corresponding progress.

The native mock now injects `HallOfFameRepository` and `AchievementProvider` into `HallOfFameController`. The controller owns Archive/list/detail and achievement set/list/detail routes, stable selected IDs and per-set cursor memory. `HallOfFamePage.qml` presents those projections; the reusable `ControllerList.qml` bounds scrolling and reasserts actual Qt focus after delegates are rebuilt. Shell-level navigation and overlays retain priority.

The original mock used synchronous fake archive loading with last-good-data recovery; normal manual records now use the persistent repository. The achievement provider is a QObject boundary that exposes context, linked sets, snapshots and asynchronous refresh notifications. Snapshots scope definitions/unlocks to a provider/account/set; the controller rejects mismatched identities. The mock completes refresh on the Qt event loop, supports held requests for tests and invalidates pending work/cache when the account changes. Disconnected/unsupported results hide old records, while same-context loading/offline/error results may retain a clearly labeled snapshot. QML never handles account credentials, cache keys or network calls.

The normal composition now injects `RetroAchievementsProvider`; the fake remains isolated to sample/test runs. `AchievementAccountController` owns controller form drafts, while the provider owns authentication, verified file association, request validation and account-scoped caches. A dedicated worker handles hashing, bounded HTTPS requests and cache I/O. UI projections contain masked password text and account status, never tokens or network commands. Real support is deliberately limited to verified hash formats and core definitions; timestamps absent from the read API remain unknown. See [RetroAchievements](RETROACHIEVEMENTS.md) for protocol and earning boundaries.

## Platform services

Use interfaces for system-level behavior such as:

```text
SessionService
ProcessService
PowerService
NetworkService
AudioService
BrightnessService
StorageService
DesktopModeService
DeviceProfileService
```

The `armada` implementation may call system services/commands/APIs as appropriate, but those details stay out of feature code.

For shell-critical actions such as session switching and power management, prefer established system mechanisms over ad-hoc shell scripts once the mechanism is known.

`ControllerInput` also exposes SDL-mapped snapshots before dead zones/foreground gating and a controller-versus-keyboard origin for observed semantic actions. `DiagnosticsController` consumes these without intercepting input or changing its mapping; observations stay in memory and do not trigger shell navigation persistence. `DiagnosticsService` reads selected QWindow/QScreen/QSysInfo fields on the GUI thread and writes requested snapshots atomically on its worker. QML presents the data and controller action rail, with no OS commands, filesystem scanning or SQL. See `DEVICE_DIAGNOSTICS.md` for report ownership and interpretation limits.

## Desktop mode boundary

`DesktopModeService` owns the transition between TrainerOS and Plasma maintenance mode.

The feature UI should only request an intentional transition and show confirmation when appropriate. It should not know display-manager commands, session names, or login internals.

## Theme system

Use a stable base visual language plus World-specific atmosphere/accent tokens.

Suggested tokens:

- background/surface layers
- primary/secondary text
- focus outline/marker
- accent
- semantic states
- Pokémon type accents
- spacing/radius
- typography scale
- motion durations

World themes should primarily override atmosphere/accent data rather than require duplicated page implementations.

## Current local persistence boundary

`LocalStateStore` implements Trainer, Pokédex progress, Library and Preferences repositories with UI-thread committed projections and a dedicated SQLite worker. Asynchronous write callbacks run on the caller's UI thread while its QObject context exists. Feature controllers manage pending/error/draft state; QML has no SQL or filesystem operations. The app chooses normal persistent or explicit ephemeral composition.

`SessionState` coordinates asynchronous startup, versioned navigation restore, a 300 ms browsing debounce and draining writes on normal exit. Stable record IDs and applied filters are restored; transient overlays and unsubmitted drafts are excluded. Startup failures have controller-accessible recovery. L1/R1 remains available during writes and browsing-state errors after startup. Full schema/ownership, failure semantics and acceptance criteria are in [LOCAL_PERSISTENCE.md](LOCAL_PERSISTENCE.md); this supersedes earlier descriptions of profile/favorites/navigation as memory-only.

`LibraryManagementController` owns an unsaved registration draft and optimistic revision; one commit writes the Adventure, optional custom World and all World relationships. `FilePickerController` consumes the asynchronous `FileCatalog` boundary; `LocalFileCatalog` enumerates/sorts directories on its own worker and returns bounded batches. Stale/cancelled results are ignored. `SettingsController` applies theme/motion changes only after the preferences repository commits. These are services opened from Start, not additional primary pages.

## Visual iteration rule

Do not confuse QML component reuse with design lock-in.

The full mock is disposable visually. During each vertical slice, entire layouts/components may be redesigned based on real data and handheld testing. Keep domain/adapters stable enough that visual replacement is cheap.

## Safety for user data

Any save-management code must:

- never overwrite/delete external save data silently
- prefer copy/backup then replace
- clearly identify destructive restore actions
- verify writes/checksums where practical
- keep adapter failures isolated
- preserve a recovery path when integrating with unknown emulator versions

## Performance targets

TrainerOS should feel instantaneous:

- no network dependency for primary navigation
- smooth target-refresh-rate animations on the handheld
- lazy image loading and thumbnail caching
- file/database work off the render/UI thread
- cached parsed metadata
- no repeated full-ROM/save scans during simple page switching
- quick return to shell after an Adventure exits

## Testing

At minimum test:

- primary page next/previous logic
- global button mapping
- Back precedence
- adapter capability fallback
- Continue ordering
- persistence mapping
- process lifecycle state machine
- recovery after failed external launch

Device/integration tests should verify:

- D-pad/stick focus on every primary page
- `L1/R1` switching
- modal/drawer focus trap and restore
- system menu behavior
- launch and return from a mock/real external process
- safe transition to/from Plasma maintenance mode
- startup/restart behavior in the dedicated session once enabled

## Device profile

Keep device-specific quirks behind `DeviceProfile` / platform code:

- display size/density/safe areas
- controller mappings
- input device identifiers
- sleep/hinge behavior if reliably exposed
- brightness ranges
- power/session quirks

Do not scatter Retroid-specific constants through QML.

`DeviceService` owns asynchronous sound, backlight, network and storage operations behind an injectable backend. `DeviceController` exposes bounded rows and actions to the shared service panel. Power requests use the same confirmed, journal-draining platform transition as session changes; device and backup work jointly hold that exit gate. See `DEVICE_CONTROLS.md`.

`AdapterRouter` is the application composition boundary for installed Adventure integrations. It delegates file attachment and capability/launch requests (and legacy resume dispatch pending #49) without exposing emulator-specific logic to QML. Standalone launch profiles share literal argument construction and worker preflight while retaining explicit per-installation platform validation; see `STANDALONE_ADAPTERS.md`.

## Ordinary-save integration boundary ? 2026-09-19

The deployed RetroArch/mGBA launch and save resolver now share configuration verification independently of legacy state preparation. Production composes the personal/collection repository directly and starts no state-scanning thread. State scan/entry preparation remains linked only to the historical regression executable. Existing ordinary-backup bundles retain their content identity; a refreshed operation token accounts for the new SRAM context fingerprint. Home/Y crop the same decoded exit image for presentation without altering stored media provenance. [Current behavior and retirement evidence](ADVENTURE_EXIT.md#ordinary-save-decoupling-and-legacy-retirement).

## Shared Adventure routing increment

P1 publishes one committed `currentAdventureId` and gives the protected-save Center its first shared-selection consumer. Global/modal routing, paired triggers, asynchronous generation protection and the single-Trainer boundary are documented in [Shared Adventure](SHARED_ADVENTURE.md). P2 ownership and later exact-build/save projections retain separate gates.

Hall paired routing keeps view state in `HallOfFameController`, with independent archive/achievement routes and action focus plus existing stable row identities. Shell owns modal precedence and dispatches L2/R2 without coupling QML to providers. Navigation JSON retains the old active-route fields and adds optional per-face snapshots; old rail focus normalizes to visible content. Provider changes reconcile unavailable content without selecting a different face. This is P1 navigation, not the later selected-save Journey/RA projection.
