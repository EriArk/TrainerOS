# TrainerOS Architecture

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

## Deployment modes

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
    PreviousPage,
    NextPage
};
```

The concrete source may evolve if Qt's high-level gamepad support is insufficient for the target device. That substitution must not change feature APIs.

## Persistence

TrainerOS-owned metadata belongs in a local repository layer backed initially by SQLite.

Persist data such as:

- World configuration/state
- Adventure metadata and hidden adapter configuration
- recent/resumable-point cache
- Trainer profile
- Pokédex personal progress
- Hall of Fame entries
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
    virtual AdventureCapabilities capabilities() const = 0;

    virtual IntegrationStatus validate(const Adventure& adventure) = 0;
    virtual LaunchResult launch(const Adventure& adventure) = 0;
    virtual QList<ResumePoint> listResumePoints(const Adventure& adventure) = 0;
    virtual LaunchResult resume(const Adventure& adventure,
                                const ResumePoint& resumePoint) = 0;
};
```

Actual async APIs may use `QFuture`, signals, coroutines, worker services, or another well-contained pattern. Do not block the UI thread on filesystem scans/process waits.

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

## First adapter

Build `MockAdventureAdapter` first.

It provides fake Adventures/resume points/screenshots and simulates launch/resume success/failure so the entire controller UX can be proven without depending on emulator setup.

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

Keep reference/canonical Pokédex data separate from personal progress.

Suggested split:

- `PokedexReferenceProvider` — species/types/evolution/reference data
- `PokedexProgressRepository` — Seen/Caught/Favorite/history owned by TrainerOS
- optional `GameProgressProvider` implementations — enrich personal data from supported saves

Reference providers may use appropriately licensed bundled data, local imports, or cached network data. The UI is not hard-wired to one API.

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
