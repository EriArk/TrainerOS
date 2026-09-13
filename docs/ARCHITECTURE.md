# TrainerOS Architecture

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

See `ROADMAP.md` for execution order. Feasibility research is input to later modules, not a reason to skip the foundations.

## Deployment modes

The implemented Armada boundary is documented in [dedicated session integration](SESSION_PROTOTYPE.md). A separate systemd/Gamescope lifecycle owns the compositor, a Linux supervisor preserves orphaned Adventures, and confirmed mode requests drain local persistence before invoking the fixed platform helper. Normal app mode remains available for development and recovery.

TrainerOS's eventual main/default session coexists with the retained Steam Gaming Mode and KDE Plasma. The initial application build performs no session mutation. A platform service owns future mode availability and transitions.

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

Library controllers read committed UI-thread snapshots after asynchronous store startup. Refresh preserves identities across reordering and falls back when records disappear. Before a launch/resume request, the controller rechecks the record's primary/additional World relationships. The ephemeral preview supplies a synchronous fake snapshot; normal composition uses `LocalStateStore` and an unconfigured adapter that advertises no launch capabilities. Browsing state is durable, while save-file support remains future integration work.

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
    ToggleContinue, // Y, scoped to Home
    PreviousPage,
    NextPage
};
```

The concrete source may evolve if Qt's high-level gamepad support is insufficient for the target device. That substitution must not change feature APIs.

The initial native source is SDL2 GameController, polled from the Qt event loop. It translates inputs into the semantic `Action` enum and centralizes dead zones, edge detection, directional repeat, hotplug and foreground gating. SDL handles controller mappings; the physical Flip 2 mapping outside Steam still needs device testing. SDL virtual controllers exercise the same source in automated tests. Keyboard shortcuts are development conveniences. See `DEVELOPMENT.md` for current mappings and limits.

`ToggleContinue` is separate from `Confirm`: Y opens/closes the Home drawer; A in the drawer selects Home's Adventure without launching. A on Home's main button requests the adapter launch/resume. `PlayHistoryController` observes identified lifecycle start/end signals and writes sessions through a repository; it does not parse game saves. `ContinueEntry` distinguishes recent process sessions from actual `ResumePoint` records. See [Home and play history](HOME_AND_HISTORY.md). Map semantic actions to the physical device's labeled controls through the input/device profile.

Controller text entry is shared shell infrastructure for Pokédex search and profile editing. It owns its draft text, key focus, Apply/Cancel behavior, and restoration to the requesting control. Keep text-entry and filter-picker focus above the underlying screen in Back precedence.

Profile creation/editing must use a repository/use-case boundary with stable identity, validated drafts, explicit save/cancel, and recoverable write failures. A conversational design preview's in-memory state does not satisfy the native application's persistence requirement.

The native prototype now uses `TextEntryController` for the reusable buffer, key geometry, spatial focus and Apply/Cancel lifecycle. It emits accepted text without knowing its consumer. `TrainerController` owns the profile form draft and validation, backed by `TrainerRepository` / `MockTrainerRepository`; profile identity is no longer supplied by the sample library repository. The shell coordinates layer precedence and records an explicit text consumer (Trainer name or Pokédex search), routing Apply only to that consumer and clearing it on global navigation. `KeyboardPanel.qml` and feature pages present these controllers without storage logic.

Transient layer priority is notice → system menu → keyboard → active service / profile form / Continue → primary page. Start preserves the lower layer. Global page changes cancel transient edits. The shell routes text to its explicit consumer, including library title/notes/custom World name, and restores the requesting control. The Trainer controller assigns a stable profile identity on creation. Fake repositories support isolated failure tests; normal profile and library writes use asynchronous SQLite storage.

## Persistence

TrainerOS-owned metadata belongs in a local repository layer backed initially by SQLite.

Persist data such as:

- World configuration/state
- Adventure metadata and hidden adapter configuration
- recent/resumable-point cache
- Trainer profile
- Pokédex personal progress
- Hall of Fame entries
- cached external achievement definitions/unlocks, separate from local completion and current-save progress
- integration configuration
- shell/settings/controller preferences

External emulator save/state data remains external source data. Store references, derived metadata, checksums, and backups where appropriate; never make the TrainerOS database the only copy of actual game progress.

## Adventure adapter layer

The UI must not know how a specific emulator starts, exits, or stores states.

Suggested interface shape:

```cpp
class AdventureAdapter {
public:
    virtual ~AdventureAdapter() = default;

    virtual QString id() const = 0;
    virtual AdventureCapabilities capabilities(const Adventure& adventure) const = 0;

    virtual IntegrationStatus validate(const Adventure& adventure) = 0;
    virtual LaunchResult launch(const Adventure& adventure) = 0;
    virtual QList<ResumePoint> listResumePoints(const Adventure& adventure) = 0;
    virtual LaunchResult resume(const Adventure& adventure,
                                const ResumePoint& resumePoint) = 0;
};
```

Actual async APIs may use `QFuture`, signals, coroutines, worker services, or another well-contained pattern. Do not block the UI thread on filesystem scans/process waits.

Capabilities describe the configured Adventure, not just the adapter type: two records using the same adapter may differ in launch/resume availability. The prototype implements only ID, per-Adventure capabilities and simulated launch/resume results. A missing capability remains an ordinary UI state; registry/validation/enumeration and async lifecycle APIs are later integration work.

Possible capabilities:

```text
LAUNCH
PROCESS_LIFECYCLE
ENUMERATE_RESUME_POINTS
DIRECT_RESUME
STATE_SCREENSHOT
SAVE_BACKUP
SAVE_METADATA
PROGRESS_METADATA
```

Capability detection allows graceful degradation:

- `LAUNCH` only → Adventure remains playable
- resumable point enumeration → Continue drawer becomes richer
- direct resume → selecting a card loads the exact point
- screenshot support → visual cards
- metadata support → Home/Trainer/Pokédex enrichment

## Process lifecycle

External Adventures are first-class lifecycle transitions, not random subprocesses.

Launch flow:

1. persist current shell page/focus and relevant session state
2. validate the adapter configuration
3. hide/suspend shell presentation as appropriate
4. launch external emulator/application through the adapter/process service
5. monitor the child/application lifecycle where reliable
6. when Adventure exits, foreground/restore TrainerOS
7. restore the previous shell context immediately
8. refresh metadata/resume points asynchronously

The platform layer should decide whether the shell stays resident, is hidden, pauses rendering, or participates in a compositor/session-specific handoff. Feature UI must not depend on that choice.

The current device-independent implementation separates `AdventureLaunchController` (checkpoint, lifecycle state and exactly-once context restoration) from `ProcessService` (one owned asynchronous QProcess, literal arguments, exit/error/stop handling). A test-only adapter connects these to the actual QML window and input foreground gate. Normal user records remain unconfigured until a real adapter and its target environment are validated. See [LIBRARY_AND_LAUNCH.md](LIBRARY_AND_LAUNCH.md) for limits and acceptance.

## First adapter

Build `MockAdventureAdapter` first.

The current mock adapter simulates launch/resume responses without starting any process. `MockLibraryRepository` supplies the sample Adventures and resume points; screenshots remain original geometric placeholders and the adapter declares screenshot support unavailable. Per-record capability overrides cover direct resume, launch-only and setup-needed states. Launch failure can be injected for retry checks. This proves controller interaction without depending on emulator setup.

After the full mock, implement real adapters one at a time. Likely early candidates are RetroArch, melonDS, Azahar, and Dolphin, but the actual order should follow the friend's desired Pokémon library and what is reliable on the target ArmadaOS build.

## Continue model

The Continue drawer consumes domain `ResumePoint` objects, never raw filesystem scans from QML.

A resume point can carry:

- stable ID
- Adventure ID
- adapter ID
- timestamp
- screenshot reference
- location text when known
- playtime/progress summary
- direct-resume availability
- external source reference

When exact resume is unavailable, the card may still launch the Adventure normally.

## Pokédex providers

The normal composition now injects `OfflinePokedex` for bundled, validated species/form facts and `LocalStateStore` for personal marks. `PokedexJournalEditor` owns transient manual Seen/Caught/note drafts; `SqlitePokedexJournal` owns schema/transaction mapping on the existing storage worker. Feature UI never reads ROMs or interprets the reference as current game progress. See [Pokédex](POKEDEX.md).

Keep reference/canonical Pokédex data separate from personal progress.

Suggested split:

- `PokedexReferenceProvider` — species/types/evolution/reference data
- `PokedexProgressRepository` — Seen/Caught/Favorite/history owned by TrainerOS
- optional `GameProgressProvider` implementations — enrich personal data from supported saves

Reference providers may use appropriately licensed bundled data, local imports, or cached network data. The UI is not hard-wired to one API.

The native mock implements `PokedexReferenceProvider::load()` and `PokedexProgressRepository::progress()/setFavorite()` as separate injected boundaries. A single fixture supplies their independent reference/progress data in memory. `PokedexController` owns the applied query/filters/order, stable selected entry ID, list/detail/rail/choice focus and recovery behavior. QML owns layout and revealing the focused row, not filtering or storage. Collection membership is reference data; personal encounter history is not inferred from it.

Filtering intersects all active dimensions. Favorite changes rebuild the result set without altering Seen/Caught or reference records. Removing the selected result returns to a valid list row or recovery state. Failed writes retain the old mark; failed reference refreshes retain the last successful snapshot. The synchronous fixture establishes interaction only: async loading, provider/version provenance, full regional catalogs, persistence and game-progress enrichment remain later work.

## Hall of Fame achievements provider

Hall of Fame combines local completion archives with RetroAchievements data through a separate achievement-provider/repository boundary. Introduce the contract/fake at the foundation stage; implement the real provider when the Hall of Fame module is reached.

Keep external account/game/achievement identities and unlock modes/dates distinct from local Adventure runs and HallOfFameEntry records. A provider refresh updates a cache asynchronously and must not block local archive browsing. Disconnected, unsupported, cached/offline, loading and failed-refresh states are explicit. Manual archive changes do not award RA achievements, and imported unlocks do not imply that the current save has corresponding progress.

The native mock now injects `HallOfFameRepository` and `AchievementProvider` into `HallOfFameController`. The controller owns Archive/list/detail and achievement set/list/detail routes, stable selected IDs and per-set cursor memory. `HallOfFamePage.qml` presents those projections; the reusable `ControllerList.qml` bounds scrolling and reasserts actual Qt focus after delegates are rebuilt. Shell-level navigation and overlays retain priority.

Archive loading is currently a synchronous fake read with last-good-data recovery. The achievement provider is a QObject boundary that exposes context, linked sets, snapshots and asynchronous refresh notifications. Snapshots scope definitions/unlocks to a provider/account/set; the controller rejects mismatched identities. The mock completes refresh on the Qt event loop, supports held requests for tests and invalidates pending work/cache when the account changes. Disconnected/unsupported results hide old records, while same-context loading/offline/error results may retain a clearly labeled snapshot. QML never handles account credentials, cache keys or network calls.

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

`AdapterRouter` is the application composition boundary for installed Adventure integrations. It delegates file attachment and capability/launch/resume requests without exposing emulator-specific logic to QML. Standalone launch profiles share literal argument construction and worker preflight while retaining explicit per-installation platform validation; see `STANDALONE_ADAPTERS.md`.
