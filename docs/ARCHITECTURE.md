# TrainerOS Architecture

## Recommended baseline

TrainerOS should start as a normal Android application that can also act as the device's default Home/launcher where the firmware permits it.

Recommended stack:

- Kotlin
- Jetpack Compose
- AndroidX ViewModel
- Kotlin coroutines / Flow
- Room for structured local data
- DataStore for small settings/preferences
- Android Storage Access Framework / persisted URI permissions for user-selected content where practical
- a small explicit emulator/integration adapter layer

The current Retroid Pocket Flip 2 reference hardware runs Android 13 with a 5.5-inch 1080p 60 Hz display. Do **not** hard-code that resolution; the UI should remain usable on other landscape Retroid-class devices and on desktop/emulator previews.

## Application modes

### 1. Default launcher mode

Preferred on a dedicated device if the firmware lets the user select TrainerOS as the Home app.

Benefits:

- Android launcher stays out of the normal flow
- Home button can naturally return to TrainerOS
- strongest dedicated-console feel

Implementation may expose an activity with the appropriate HOME/DEFAULT intent categories when safe and supported.

### 2. Full-screen frontend mode

Fallback when default-launcher behavior is unavailable or undesirable.

- immersive landscape activity
- explicit option to start TrainerOS automatically if the platform allows it
- normal Android exit path remains available through the TrainerOS system menu

Do not require privileged/root APIs for the core product.

## Module shape

Start simple, but keep domain boundaries clear. A multi-module structure can evolve toward:

```text
app/
core/model/
core/data/
core/ui/
feature/home/
feature/worlds/
feature/pokedex/
feature/trainer/
feature/halloffame/
feature/system/
integration/emulators/
integration/pokedex/
```

For the very first prototype, fewer Gradle modules are acceptable if packages follow the same boundaries. Avoid creating architecture ceremony that slows the UI proof-of-concept.

## UI state

Use a single top-level `TrainerOsState`/navigation controller for:

- active primary page
- remembered per-page focus/navigation state
- system-menu visibility
- global theme/world accent

Each feature owns its internal state and exposes events upward only when they are truly cross-feature actions.

Do not model primary pages as ordinary Android destinations that destroy/recreate all local state every time `L1/R1` is pressed. The experience should feel like adjacent console pages with stable state.

## Controller input layer

Controller input is infrastructure, not a screen-specific afterthought.

Suggested responsibilities:

- normalize Android `KeyEvent` / gamepad key codes into TrainerOS actions
- support D-pad and left-stick focus movement
- map `A/B/Start/L1/R1`
- apply dead zones and key-repeat policy
- allow future remapping
- prevent features from consuming reserved global actions accidentally

Example domain actions:

```kotlin
sealed interface TrainerInput {
    data object Up : TrainerInput
    data object Down : TrainerInput
    data object Left : TrainerInput
    data object Right : TrainerInput
    data object Confirm : TrainerInput
    data object Back : TrainerInput
    data object SystemMenu : TrainerInput
    data object PreviousPage : TrainerInput
    data object NextPage : TrainerInput
}
```

The exact implementation may use Compose focus APIs plus a global key dispatcher, but global actions must remain explicit and testable.

## Primary navigation model

Represent primary pages with a stable enum/sealed type:

```text
HOME
WORLDS
POKEDEX
TRAINER
HALL_OF_FAME
```

`PreviousPage/NextPage` change only this level.

Nested feature routes (World detail, Pokédex entry, Hall of Fame entry) are local to their primary page. `B` unwinds local detail before anything else.

## Data layer

Room is appropriate for TrainerOS-owned metadata and history:

- World configuration/state
- Adventure metadata
- Continue entries
- trainer profile
- Pokédex personal progress
- Hall of Fame entries
- integration configuration

Data from external game applications remains external. Store references, derived metadata, checksums/identifiers, and backups where appropriate; do not make the Room database the sole copy of game progress.

## Integration adapter layer

The UI must not know how a specific emulator/application launches or stores its states.

Suggested interface shape:

```kotlin
interface AdventureAdapter {
    val id: String
    val capabilities: Set<AdventureCapability>

    suspend fun validate(adventure: Adventure): IntegrationStatus
    suspend fun launch(adventure: Adventure): LaunchResult
    suspend fun listResumePoints(adventure: Adventure): List<ResumePoint>
    suspend fun resume(adventure: Adventure, resumePoint: ResumePoint): LaunchResult
}
```

Possible capabilities:

```text
LAUNCH
ENUMERATE_RESUME_POINTS
DIRECT_RESUME
STATE_SCREENSHOT
SAVE_BACKUP
SAVE_METADATA
```

A capability-based model lets TrainerOS degrade gracefully:

- adapter with only `LAUNCH` → Adventure still works
- adapter with resume-point enumeration → Continue drawer gets richer
- adapter with direct resume → selecting a mini card resumes exact state
- adapter with metadata support → Home/Pokédex/Trainer can gain automatic progress later

## First adapter

Build a `MockAdventureAdapter` first.

It should provide several fake Adventures and fake resume points with placeholder screenshots so the entire UI can be tested before emulator-specific work begins.

After the controller UX is stable, add real adapters one at a time. Keep per-app quirks inside those adapters.

## Continue model

The Continue drawer should consume domain `ResumePoint` data, not scan files directly from Compose.

A resume point can carry:

- stable ID
- Adventure ID
- adapter ID
- timestamp
- screenshot reference
- location text if known
- playtime/progress summary
- direct-resume capability

When direct resume is unavailable, the card can still launch the Adventure and explain the fallback unobtrusively.

## Pokédex providers

Keep reference/canonical Pokédex data separate from personal progress.

Suggested split:

- `PokedexReferenceProvider` — species/types/evolution/reference data
- `PokedexProgressRepository` — seen/caught/favorite/history owned by TrainerOS

A provider may use bundled appropriately licensed data, cached network data, or user-imported data. The UI should not be hard-wired to one remote API.

## Theme system

Use a stable base visual system plus World accents.

Suggested tokens:

- background layers
- surface layers
- primary text / secondary text
- focus outline/marker
- accent
- semantic status colors
- type colors
- spacing/radius scale
- typography scale
- motion durations

World themes should override a small set of atmosphere/accent tokens rather than replacing component structure.

## Persistence and return-from-game behavior

When an Adventure is launched:

1. persist current primary page and relevant UI state
2. record launch timestamp
3. launch through the adapter
4. when TrainerOS resumes, refresh integration metadata asynchronously
5. restore previous screen/focus quickly

Avoid blocking UI return on expensive scans.

## Safety for user data

Any save-management code must follow these rules:

- never overwrite/delete external save data silently
- prefer copy/backup then replace
- display source/destination clearly for destructive restore operations
- verify writes where practical
- keep adapter failures isolated from the rest of TrainerOS

## Performance target

The frontend should feel instantaneous on handheld hardware:

- no network dependency for primary navigation
- lazy-load heavy images
- keep primary page switches smooth at 60 Hz
- cache parsed metadata
- do file/database work off the main thread
- avoid rebuilding large Pokédex lists unnecessarily

## Testing

At minimum add tests for:

- primary page next/previous logic
- global button mapping
- Back precedence
- adapter capability fallback
- repository/database mapping
- Continue ordering

UI/device tests should verify:

- D-pad focus on every primary page
- `L1/R1` page switching
- modal/drawer focus trap and restore
- system menu open/close
- behavior after returning from an external Adventure

## Device-specific tuning

Keep a `DeviceProfile` concept available for later tuning of:

- safe areas
- overscan/insets
- controller key-code quirks
- preferred density/text scale
- sleep/hinge behavior if reliably exposed

Do not introduce Retroid-specific hacks into feature UI code.
