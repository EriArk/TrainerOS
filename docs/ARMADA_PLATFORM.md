# TrainerOS on ArmadaOS

**Current target — 2026-09-26:** #11/#39 are closed as superseded, not delivered.
Steam Gaming Mode stays installed; #69 exposes installed Steam games through
Multiverse without replacing Steam. #70 makes a reproducible Armada-based image
the supported distribution boundary and #71 adds compatible OTA rollback.
Native package/session development remains the foundation. #73 Trainer recovery,
#77/#78 profiles, #80/#81 readiness/health and #72 first boot precede release.
#79 schedules dedicated sleep proof; current suspend remains disabled.
[Acceptance](EXPANSION_69_90.md), [execution queue](ROADMAP.md#unified-execution-order--existing-work-and-new-issues).

**Planned lifecycle amendment #49:** ordinary saves/autosaves replace normal state-based resume. Prove capture-before-prompt, controller focus and cancellation without terminating the game through this platform/session boundary, not QML commands. Preserve current crash/recovery behavior and no-sleep policy. [Exit and migration contract](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit).

Preparation and acceptance steps are in [FIRST_DEVICE_RUN.md](FIRST_DEVICE_RUN.md). The first actual ARM64 build, runtime, display, storage and package findings are recorded in [ARMADA_DEVICE_BASELINE.md](ARMADA_DEVICE_BASELINE.md). The [dedicated session](SESSION_PROTOTYPE.md) records the later device checks: standalone controller/rendering, Adventure preservation across a shell crash, failed-start recovery and Steam/Plasma transitions. Offscreen CI does not replace those checks; sleep/wake remains deferred.

The native [controller/display diagnostic service](DEVICE_DIAGNOSTICS.md) is ready for that visit. It records Qt/SDL observations locally on request. Display coordinates, backend names and detected input signals must be checked against the real screen, control labels and session setup; they are not substituted for physical acceptance.

## Decision

TrainerOS targets **ArmadaOS as its system base** on the Retroid Flip-class handheld.

This replaces the earlier plan to implement TrainerOS as an Android APK/Home launcher.

ArmadaOS owns hardware/Linux support. TrainerOS remains a native graphical shell/session, delivered for consumers through a reproducible prepared Armada-based image (#70). This does not authorize an independent low-level OS fork.

## Why this model

TrainerOS wants to behave like the device's operating interface rather than like a mobile app. A Linux base gives the project cleaner control over:

- process launch/termination
- emulator command lines and environment
- filesystem-backed ordinary-save resolution
- screenshots/thumbnails
- process lifecycle and return-to-shell behavior
- local backups
- device/system status
- controller input
- desktop/session switching

Those capabilities still require careful implementation, but they can live behind normal Linux/platform boundaries rather than Android application sandbox and launcher semantics.

## Session philosophy

Retain TrainerOS as default, Steam Gaming Mode for its full environment, and verified Plasma maintenance. Mobile may replace ordinary Desktop only after its recovery gates pass; Steam removal was cancelled by the closure of #11/#39. The three modes do not make non-Steam launch depend on Steam.

### TrainerOS session — default everyday mode

Purpose:

- boot/wake into the Pokémon-focused shell
- browse Home, Worlds, Pokédex, Trainer, Hall of Fame
- launch Adventures through normal game startup/save loading
- return to TrainerOS after Adventure exit
- access normal handheld/system actions without desktop UI

No Plasma panels, taskbars, desktop windows, or unrelated app-launcher UI should appear during normal use.

### Steam Gaming Mode — retained

Keep ArmadaOS's Steam experience available. #69 discovers installed app manifests/local media and uses supported Steam launch routes with stable app/library identities. Store, downloads, Proton and advanced Steam management remain in Steam; personal libraries, compatdata and saves are protected. No Steam removal/restore work remains scheduled.

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

**Reconciled 2026-09-26:** the stable heading preserves links to the original #11 plan. That issue and #39 were closed as superseded; Steam stays installed. Only the owner's Mobile-only-if-viable maintenance request and measured software cleanup remain active here, at R13. The observations below are dated evidence, not a current package inventory.

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

**Cancelled historical requirement.** #11/#39 removal, restore manifest/helper,
Plasma restore launcher and remove/restore trials must not be implemented merely
because older documents retain this heading. Their closure comments and #69
retain Steam. Normal platform rollback still protects every configuration change.

### C. Physical acceptance before declaring consolidation complete

Prove TrainerOS → chosen maintenance shell → TrainerOS, restart into TrainerOS,
failed-shell recovery, retained Steam access and representative non-Steam
launch/input/return. Keep bounded manifests/rollback for actual Mobile or cleanup
changes. Preserve personal Steam libraries/compatdata/saves and shared KDE services.
Do not perform the cancelled remove/restore round trip. No package inventory alone
closes physical recovery. Dedicated #79 sleep testing is a separate gate; neither
Mobile selection nor cleanup may enable suspend incidentally.

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

Native ARM64 packages, the session definition, input/supervisor helpers and
versioned migration/recovery tooling remain development and image building blocks.
The supported consumer deliverable is **#70's reproducible Armada-based image**,
not a manual installer for arbitrary distributions. Pin/record upstream base and
TrainerOS layers, publish manifest/hashes, and include the validated device/runtime
baseline and maintenance path. Never bake in private ROMs/BIOS/art/saves/secrets.

Build first-boot #72 and portable Trainer recovery #73 before clean-flash release
acceptance. #71 OTA follows the image mechanism: trusted staged activation,
space/power/schema checks and a last-known-good **image plus compatible data**
rollback. An old binary must never run against a migrated unsupported database.
Update/reboot/rollback/update-again and clean-flash setup/recovery need physical
proof. No specific partition/bootc/OTA mechanism is chosen by this plan before
inspecting supported Armada facilities.

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
- clean gameplay capture before user-exit UI where verified
- title/integration save policy and still-running-game exit/cancel coordination
- ordinary-save paths
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
- system modes and runtime/BIOS readiness (Center remains Pokémon content)
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

## Planned device controls, Help and startup extension

The [#19–41 acceptance register](EXPANSION_PLAN.md) adds capabilities to the same Armada boundary, not an alternative system stack:

- #35 Start quick controls reuse actual volume/brightness services and safe minimums. #37 power-cable feedback requires a real supported haptic path and debounced external-power edges; charging/full updates and startup must not replay pulses. No gameplay rumble reconfiguration or resumption of sleep testing.
- #38 probes installed Armada RGB/tool/config and the actual Flip hardware, then exposes only supported modes. The [generic UART proposal](https://github.com/armada-os/armada-packages/pull/73) and [Pocket S2 integration](https://github.com/armada-os/armada/pull/419) are references for the backend direction, not proof of Flip compatibility. No duplicated low-level protocol, experimental firmware change or continuous setting reassertion.
- #39 Steam install/remove/restore UI is cancelled. #69 retains Steam and adds its installed library; #86 groups Steam and Desktop under System modes.
- #40 Help obtains active library/backup/BIOS locations and capabilities through semantic non-secret facts. Verify BIOS requirements against actual adapters/official documentation, never hard-code guessed universal locations or proprietary download sources.
- #41 audits firmware/vendor, bootloader/initramfs/kernel, any userspace splash, display manager, Gamescope/supervisor and first frame separately. Brand only supported userspace/session layers with managed versioned settings, drift protection and rollback. Profile creation/chooser/Home determine the final destination; show real coarse readiness, no fake checks or cinematic delay. Preserve logs, failure fallback and explicit diagnostic access. Record unavoidable earlier branding instead of patching unknown immutable assets.

U11 installation/update/rollback and the selected maintenance arrangement precede physical boot-branding acceptance. Measure complete startup and verify failed-start recovery plus customization removal on Flip. These are planned capabilities; this documentation change does not install/remove software, enable sleep, modify RGB or replace boot assets.

## Long-term option: TrainerOS image

**Promoted by #70, 2026-09-24; reconciled 2026-09-26.** This heading remains for
older links, but the image is now a required distribution deliverable, not an
option conditional on package deployment failing. R15 owns the reproducible image,
R16 compatible OTA, R17 startup/release validation. Retain development app mode,
Steam and verified maintenance/recovery. No physical hardware modification or
replacement of Armada low-level device support belongs to this work.
