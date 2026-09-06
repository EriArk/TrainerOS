# TrainerOS on ArmadaOS

Preparation for the first physical visit is in [FIRST_DEVICE_RUN.md](FIRST_DEVICE_RUN.md), with a read-only baseline report script. No target findings have been filled in from desktop assumptions. The current application passes Windows checks and Ubuntu Linux CI, includes local persistence and makes no session changes. CI uses offscreen/software rendering; the actual ArmadaOS device/session gate remains open.

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

The machine should retain three deliberate graphical modes: TrainerOS as the intended main/default experience, Steam Gaming Mode as an alternative, and KDE Plasma as Desktop / Maintenance Mode. This is a product requirement, not an assumption that the current system exposes three display-manager entries.

### TrainerOS session — default everyday mode

Purpose:

- boot/wake into the Pokémon-focused shell
- browse Home, Worlds, Pokédex, Trainer, Hall of Fame
- launch/resume Adventures
- return to TrainerOS after Adventure exit
- access normal handheld/system actions without desktop UI

No Plasma panels, taskbars, desktop windows, or unrelated app-launcher UI should appear during normal use.

### Steam Gaming Mode — retained alternative

Keep ArmadaOS's Steam experience available. TrainerOS does not replace Steam's files, controller configuration or startup scripts during development. The eventual mode-selection/return path must follow the actual ArmadaOS session arrangement and remain recoverable.

### KDE Plasma — Desktop / Maintenance Mode

Purpose:

- Wi-Fi/network troubleshooting
- Dolphin/file management
- package management
- terminal/development
- recovery
- manual emulator configuration
- advanced system tasks

Do **not** uninstall Plasma. It is valuable recovery infrastructure and prevents TrainerOS from needing to reinvent a full desktop.

Entering Desktop Mode must be explicit from the TrainerOS system menu. Leaving Desktop Mode should make returning to TrainerOS simple.

## Safe integration order

Do not start by replacing the graphical session.

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

Steam Gaming Mode and Plasma remain available as alternative modes.

### Current implementation boundary

The first native skeleton runs only in safe application mode. `DevelopmentPlatformService` reports session switching as unavailable. System-menu entries for Desktop / Maintenance Mode and Steam Gaming Mode explain this state; neither runs commands or changes session configuration. There are no session installers or default-session changes in this milestone.

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
