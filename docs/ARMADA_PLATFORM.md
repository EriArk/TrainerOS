# TrainerOS on ArmadaOS

Preparation and acceptance steps are in [FIRST_DEVICE_RUN.md](FIRST_DEVICE_RUN.md). The first actual ARM64 build, runtime, display, storage and package findings are recorded in [ARMADA_DEVICE_BASELINE.md](ARMADA_DEVICE_BASELINE.md). The [dedicated session](SESSION_PROTOTYPE.md) records the later device checks: standalone controller/rendering, Adventure preservation across a shell crash, failed-start recovery and Steam/Plasma transitions. Offscreen CI does not replace those checks; sleep/wake remains deferred.

The native [controller/display diagnostic service](DEVICE_DIAGNOSTICS.md) is ready for that visit. It records Qt/SDL observations locally on request. Display coordinates, backend names and detected input signals must be checked against the real screen, control labels and session setup; they are not substituted for physical acceptance.

## Decision

TrainerOS targets **ArmadaOS as its system base** on the Retroid Flip-class handheld.

This replaces the earlier plan to implement TrainerOS as an Android APK/Home launcher.

The goal is not to fork ArmadaOS immediately. The goal is to use ArmadaOS for hardware/Linux support and install TrainerOS as a native graphical shell/session on top of it.

## Why this model

TrainerOS wants to behave like the device's operating interface rather than like a mobile app. A Linux base gives the project cleaner control over:

- process launch/termination
- emulator command lines and environment
- filesystem-backed save/state discovery
- screenshots/thumbnails
- process lifecycle and return-to-shell behavior
- local backups
- device/system status
- controller input
- desktop/session switching

Those capabilities still require careful implementation, but they can live behind normal Linux/platform boundaries rather than Android application sandbox and launcher semantics.

## Session philosophy

The installed baseline retains TrainerOS, Steam Gaming Mode and Plasma maintenance options. The accepted 2026-09-13 target is **TrainerOS + Plasma Mobile maintenance**, conditional on verified Mobile recovery and reversible Steam removal. This supersedes the former permanent three-mode requirement. Current working sessions stay available until the migration below passes.

### TrainerOS session — default everyday mode

Purpose:

- boot/wake into the Pokémon-focused shell
- browse Home, Worlds, Pokédex, Trainer, Hall of Fame
- launch/resume Adventures
- return to TrainerOS after Adventure exit
- access normal handheld/system actions without desktop UI

No Plasma panels, taskbars, desktop windows, or unrelated app-launcher UI should appear during normal use.

### Steam Gaming Mode — retained pending reversible removal

Keep ArmadaOS's Steam experience available until the accepted issue #11 migration has a tested restore route. Changes must follow the actual Armada session arrangement and preserve personal Steam libraries, compatdata and saves. Removal is planned, not completed.

### KDE Plasma — Desktop / Maintenance Mode

Purpose:

- Wi-Fi/network troubleshooting
- Dolphin/file management
- package management
- terminal/development
- recovery
- manual emulator configuration
- advanced system tasks

Never remove the last verified Plasma recovery environment. The owner now prefers Plasma Mobile; ordinary Desktop may be retired only after Mobile proves the same required maintenance/recovery functions. Retain shared KDE dependencies rather than treating Desktop and Mobile as completely independent stacks.

Entering Desktop Mode must be explicit from the TrainerOS system menu. Leaving Desktop Mode should make returning to TrainerOS simple.

## Safe integration order

The original application → isolated session → default-session sequence below describes the bootstrap order already followed. Current evidence is in [SESSION_PROTOTYPE.md](SESSION_PROTOTYPE.md); the next platform work is the separate consolidation plan below.

### Phase A — application mode

Run TrainerOS full-screen inside a normal desktop/development session.

Validate:

- build/runtime dependencies
- input events from the real Retroid controls
- rendering/performance
- launch/return from external applications
- persistence
- adapter behavior
- crash handling

### Phase B — isolated session prototype

Create an isolated TrainerOS session option, but preserve the existing Steam/Plasma startup and recovery paths while testing. Do not change the default until this phase passes.

Validate:

- session starts reliably
- failure/crash can be recovered from
- power/wake behavior
- controller availability
- external Adventure launch and return
- maintenance-mode transition

### Phase C — default experience

Once reliable, configure TrainerOS as the normal/default session for this dedicated handheld.

At this baseline Steam and Plasma remain available. Their later consolidation has separate acceptance and does not erase the original recovery evidence.

### Original skeleton boundary — historical

The first native skeleton runs only in safe application mode. `DevelopmentPlatformService` reports session switching as unavailable. System-menu entries for Desktop / Maintenance Mode and Steam Gaming Mode explain this state; neither runs commands or changes session configuration. There are no session installers or default-session changes in this milestone.

## Planned session consolidation — 2026-09-13

Sources of product direction: [issue #11](https://github.com/EriArk/TrainerOS/issues/11), then the owner's request to keep only Plasma Mobile if possible. This section is the future migration contract, not a record of removal. The current roadmap schedules it as one bounded platform track independent of artwork/save features.

### Feasibility observed, usability still to verify

[Armada's Desktop Mode documentation](https://armadaos.dev/using-armada/desktop-mode/) explicitly supports selecting Plasma Mobile through Armada Control. A read-only SSH check on the owner's Flip on 2026-09-13 found:

- `plasma-mobile-6.7.4-1.fc44.armada.aarch64` installed;
- `plasma-workspace-6.7.5-1.fc44.aarch64` and `plasma-desktop-6.7.5-1.fc44.aarch64` installed;
- Armada and upstream Mobile/Desktop Wayland session entries, plus the Steam Gamescope entry.

No package, session selection or power policy changed during this planning check. Mobile is available to trial without installing a different distribution. Its presence does not establish landscape usability, controller navigation, maintenance-tool completeness or successful recovery. KDE's [Plasma Mobile source documentation](https://github.com/KDE/plasma-mobile) lists shared Plasma components, including `plasma-workspace`; removing everything named Plasma/Desktop would be unsafe and would not describe a Mobile-only shell accurately.

### A. Prove Mobile, then replace the maintenance destination

1. Reinspect the installed Armada image/session mechanism and preserve the exact managed configuration needed to undo the trial. This host uses Fedora bootc; do not assume mutable-host package removal is supported or run broad `dnf remove` operations.
2. Select Mobile through the supported platform mechanism while retaining the known-good Desktop recovery route. Check the physical landscape orientation/scaling, input and keyboard access, Wi-Fi, files, terminal, package/app tools and privileged launcher behavior. Normal TrainerOS still requires the controller; do not claim Mobile is controller-first merely because it is mobile-oriented.
3. Prove TrainerOS → Mobile → TrainerOS, physical reboot into TrainerOS and failed-shell recovery into Mobile. Keep SSH/terminal rollback available. Mobile must not re-enable dimming/locking/suspend contrary to the current no-sleep policy.
4. Only after success, change the maintenance/fallback capability to Mobile and retire ordinary Desktop session selection and removable Desktop-only components. Preserve shared KWin/Qt/Plasma services needed by Mobile, TrainerOS and adapters. Record packages kept for dependencies or immutable-image limits; hiding an entry is not uninstalling its bytes.

If Mobile or supported package removal cannot meet these gates, retain the working Desktop route and report the specific limit. Do not fork ArmadaOS just to hide the distinction. Runtime failure must still leave a recoverable environment.

### B. Prepare Steam restoration before removal

Capture a bounded, versioned restore manifest for only the packages/Flatpaks, exact refs/versions where practical, session entries, managed services/drop-ins and relevant autologin/configuration that the migration changes. Record whether each managed path existed and its prior content; never archive arbitrary home directories or game libraries. Inspect dependencies and the actual update mechanism before choosing removal operations.

Provide a narrow root-owned helper and a visible **Restore Steam Gaming Mode** application launcher in the selected Plasma environment (Mobile if accepted, otherwise retained Desktop), plus a documented SSH/terminal command. It reads only the versioned manifest, presents the proposed restoration, and accepts no arbitrary command/path/package text from QML. Restore only compatible managed state; distinguish unchanged files, later edits, missing package versions and changed Armada session mechanisms. Stop on unsupported drift without overwriting newer configuration.

Prove removal/restore idempotence, interrupted recovery, unknown-state refusal and preservation of unrelated files in a non-destructive fixture root. Change mode UI/supervisor availability together with the platform migration. Plasma remains the recovery destination; direct Adventure adapters must not acquire a Steam dependency.

### C. Physical acceptance before declaring consolidation complete

Perform **remove Steam → reboot/use TrainerOS and maintenance → restore Steam → boot Steam → remove again**. Verify direct RetroArch/melonDS/Dolphin launch/input/return, persistence and shell-failure recovery. Exercise the restore launcher from the actual selected Plasma shell and retain CLI recovery if that shell fails. Record precise source/system versions and the final available sessions; protect personal Steam libraries, compatdata, saves and unrelated configuration throughout.

Mobile replacement and Steam removal are separate mutations with separate rollback boundaries. Keep the last verified setup if either fails. A package inventory or fixture pass alone closes neither gate. Sleep/hinge/wake testing remains deferred and outside this migration.

### D. Carefully reduce installed software and background activity

The owner's next 2026-09-13 instruction adds a separate system-cleanup pass; see [roadmap step 10](ROADMAP.md#10-remove-unnecessary-software-and-background-work). Audit package/Flatpak/AppImage ownership and dependencies, development leftovers, autostarts and user/system services before changing them. Optional apps may be removed and unnecessary background entries disabled in reviewed reversible batches. Do not infer that a runtime is unused merely because its planned TrainerOS adapter has not shipped.

Preserve shared KDE/Qt, compositor/input/audio/network/authentication and recovery components, the required emulator stack and all personal game/save/media/history data. Use Armada's supported image/package mechanisms; do not blindly autoremove or prune shared runtimes, development containers or the verified rollback deployment. Record immutable-image/dependency limits explicitly.

Verify reboot, maintenance/SSH access, TrainerOS input and representative Adventure launch/return after each relevant batch. Record actual disk free space, idle RAM/CPU, active services/processes and startup behavior before/after in comparable conditions. Distinguish package removal, cache cleanup and disabling autostart: each affects space/load differently. Keep bounded manifests and restore steps; do not change sleep policy to improve idle measurements.

## Do not assume ArmadaOS internals

ArmadaOS is an evolving project. Before implementing session switching or system integration, inspect the current target installation.

Discover/validate:

- current desktop/session arrangement
- compositor(s)
- display manager/login flow, if any
- Gamescope usage
- package format and package manager
- emulator installation paths and launch conventions
- user data locations
- controller/input presentation
- system services for brightness/audio/power/network
- suspend/hinge behavior

Document discoveries in this file or a device-specific note before encoding them into production platform code.

Feature/domain code must never depend directly on those details.

## Packaging strategy

The mature project should produce a native ARM64 build and an installable package suitable for the current ArmadaOS base.

The exact package/distribution mechanism is intentionally deferred until the target ArmadaOS environment is inspected.

Possible mature deliverables:

1. native TrainerOS package
2. session definition/configuration
3. setup/installer script or package hooks
4. configuration migration/update tooling
5. optional later custom ArmadaOS image with TrainerOS preconfigured

Do not build a custom OS image merely because packaging is inconvenient during development.

## External application model

TrainerOS launches Adventures through adapters rather than exposing emulator binaries directly.

Likely early integration candidates:

- RetroArch
- melonDS
- Azahar
- Dolphin

These names are implementation details and normally remain hidden from the user.

Each adapter discovers/declares its own capabilities and owns:

- validation
- launch arguments/environment
- process lifecycle where possible
- resume-state discovery
- direct resume when supported
- state screenshots when available
- save/state paths
- metadata parsing
- backup rules

Do not assume equal capabilities across emulators.

## Return-to-TrainerOS behavior

The core interaction is:

```text
TrainerOS
  ↓ launch
Adventure
  ↓ exit
TrainerOS restored to prior context
```

Before launch, persist enough shell state to restore:

- primary page
- detail route
- selected/focused item
- scroll position where useful
- active Continue drawer state only if restoring it is not confusing

After return, restore UI first and refresh external metadata asynchronously.

## System UI ownership

The initial read-only [battery status service](POWER_STATUS.md) supplies a permanent chassis gauge from the current Linux power-supply interface. Power policy and session transitions remain separate device-validation steps.

TrainerOS should expose normal handheld actions itself through its system menu and service surfaces where practical:

- audio
- brightness
- network summary/setup entry
- battery/power state
- sleep/restart/shutdown
- storage summary
- controller configuration
- backups / Pokémon Center
- Desktop / Maintenance Mode

This does not mean reimplementing every desktop settings panel. Complex/rare configuration can intentionally hand off to Plasma maintenance mode.

## Recovery principle

A dedicated shell must never make the device harder to recover.

Requirements before becoming the default session:

- known way to select/enter Plasma if TrainerOS fails
- logs available from maintenance mode
- startup failure does not create an endless black-screen loop without escape
- session configuration can be reverted without reflashing the device
- user game saves remain outside fragile shell configuration

## Upgrade principle

ArmadaOS updates may change emulator packages, paths, session behavior, or system services.

Keep those dependencies concentrated in:

- platform/armada
- integration adapters
- packaging/session files

A system update should not require rewriting Home, Worlds, Pokédex, Trainer, or Hall of Fame.

## Long-term option: TrainerOS image

Only after the package/session approach is mature should the project consider producing a reproducible image based on ArmadaOS.

Such an image would primarily provide:

- TrainerOS preinstalled
- TrainerOS default session configured
- known emulator/integration baseline
- controlled visual boot/session experience
- easier restore/reinstall for the dedicated device

It should still preserve an accessible maintenance/recovery environment unless there is a strong reason not to.
