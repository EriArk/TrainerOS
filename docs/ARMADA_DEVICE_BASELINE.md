# Flip 2 application baseline — 2026-09-11

The user confirmed comfortable physical controller navigation, profile creation and a real Adventure launch on 2026-09-11. Waydroid 1.6.3 and Armada's controller helpers are present but the Android image is not initialized; no Android game compatibility is claimed. Collection scope and reference limitations are recorded in [collection catalogue](COLLECTION_CATALOGUE.md).

The collection/search increment `e76b9badc9072075ab7d9a8022d172b87cbbba8f` passed all 18 checks on Windows (91.00 s), independent Ubuntu 24.04 (84.74 s), and ARM64 Fedora on the Flip (88.70 s). Its production build, with test entry points disabled, was installed and launched in Steam Gaming Mode. GitHub Actions [34601156001](https://github.com/EriArk/TrainerOS/actions/runs/34601156001) did not start its job because of account billing/spending limits; the independent Linux run is not GitHub CI success.

Before that update, the expanded personal library was browsed with InputPlumber controller chords. Pocket Monster (an unofficial NES release) reached a playable level through FCEUmm, and Gaia reached its title screen through mGBA; Start+Select returned both to TrainerOS. These are specific boot/input/return checks, not full-game compatibility claims.

After the new production build launched, live search validation was interrupted by a system-wide I/O stall during the remaining large-file transfer. SFTP could still read cached files and `/proc`, while ordinary SSH commands hung. Memory remained available, but I/O pressure approached 100%; both internal UFS and microSD had outstanding operations whose completion counters stopped advancing across repeated samples. Blocked tasks included filesystem workers, the journal, audio and shells. Stopping the sender did not clear the stall. This does not establish a root cause or a sleep diagnosis. At that point, suspend tests were deferred and post-update on-screen search acceptance and the final crossover imports remained open. The owner reopened the idle black-screen investigation on 2026-09-12; see the findings below.

These findings come from the owner's Snapdragon 865 Retroid Pocket Flip 2. They describe this installation, not every ArmadaOS release or Retroid model. Private device reports, addresses, keys and game inventories stay outside Git.

## System and display

- ArmadaOS Preview `20260910.07c9dcb`, signed `armada:testing` image, Fedora 44 ARM64, kernel 7.2.3. It boots from internal UFS. The previous Beta deployment remains available for rollback.
- SDDM offers Armada Plasma, Plasma Mobile and Steam Gamescope sessions. Gaming Mode uses `gamescope-wl`; its process name differs from the original baseline script's `gamescope` assumption.
- The official `/usr/libexec/armada/session-control` switches between desktop and gaming mode. Both modes remain installed. TrainerOS still runs as an ordinary application; no TrainerOS session or default-login change is shipped.
- KDE reports DSI-1 at native 1080×1920/60 Hz, rotated to landscape. Scale 1.5 produces a 1280×720 logical desktop over the 1920×1080 physical display. Advertised DRM modes alone would misleadingly suggest a portrait application viewport.
- The runtime contains Qt 6.11.2, QSQLITE, Qt Quick/Shapes/Wayland and SDL2 compatibility 2.32.72 over SDL3 3.4.16. Build tools and development headers are absent from the host.
- InputPlumber exposes a mapped Xbox controller. Physical button-label agreement, comfort, latency and hinge behavior need the owner at the device; synthetic input does not establish these results.
- The owner reopened power/idle diagnostics on 2026-09-12. Everyday operation still uses the no-sleep configuration. Native suspend, fake suspend and physical hinge/wake reliability have not been established.

## Idle black-screen investigation — 2026-09-12

After a physical reboot, SSH, the graphical session and storage were responsive. Kernel and system journals from the current and four previous boots were preserved privately. The preceding boot's Steam log contains `Issuing Suspend Command` about 15 minutes after Steam started. Its system journal contains no matching native/fake-suspend entry. The system sleep targets were masked, logind's idle/power/lid actions were ignored, and the user `armada-powerbuttond.service` was masked. KDE's separate dim/display-off/autosuspend settings were already disabled.

Steam Gaming Mode still had its own active idle policies. Disabling Linux suspend alone had left Steam able to start its sleep flow. This is a confirmed configuration gap and a plausible explanation for an idle black screen; it does not establish the cause of the earlier system-wide I/O stall.

The following Steam settings were changed through the running client's settings API, after preserving the previous configuration. Values are seconds; zero disables the timer.

| Persisted Steam setting | Before | After |
| --- | ---: | ---: |
| `IdleSuspendBatterySeconds` | 900 | 0 |
| `IdleSuspendACSeconds` | 3600 | 0 |
| `IdleBacklightDimBatterySeconds` | 300 | 0 |
| `IdleBacklightDimACSeconds` | 0 | 0 |
| `IdleScreensaverBatterySeconds` | 0 | 0 |
| `IdleScreensaverACSeconds` | 300 | 0 |

All six values were read back from the running client and its persisted `config.vdf`. Brightness recovered from the idle-dimmed value to the configured level. The Linux/KDE no-sleep settings remain in place. No firmware, kernel, GPU/UFS runtime-power policy, fan control or TrainerOS feature code was changed. Private helpers and configuration backups are device maintenance artifacts, not shipped application code.

Observed acceptance: more than six minutes without injected controller input after the change, on AC, passed the previous five-minute screensaver threshold. A fresh Gamescope capture showed the live Steam page and updated clock; an InputPlumber Guide chord opened its menu and a second chord closed it. Backlight stayed on, no session cgroup was frozen, no new Steam suspend request appeared, and no blocked tasks or kernel suspend/I/O-error entries were observed. Separate 64 KiB temporary-file write/fsync/read probes passed on internal storage and the correctly identified microSD, without touching game/save files. TrainerOS then launched through its existing Steam shortcut. No reboot or session restart was needed to apply the timer changes.

Idle acceptance must record the actual power source and observation interval, check the screen and controller response after that interval, and verify that no new Steam suspend request or kernel I/O failure appeared. A short AC check does not validate a full hour of idle, battery operation, physical lid events or suspend/resume. Re-enabling sleep requires a separate bounded wake/recovery check on the installed ArmadaOS version; the former blanket deferral no longer prohibits investigating it.

If a black screen recurs, distinguish the active paths before changing more settings: Steam's `logs/systemmanager.txt`, logind/suspend journals, `/run/armada/fake-suspend.active`, the session cgroups' frozen state, backlight state, and `/proc/pressure/io` plus disk completion counters. SSH/SFTP reachability alone does not prove the kernel or graphical session is healthy. The previous simultaneous UFS/microSD stall remains unresolved.

## Native build and installation

Use a rootless Fedora 44 Podman container on the ARM64 handheld, with only the source/build directory mounted. Build tools belong in the container; the executable uses the existing host Qt/SDL runtime. Do not use `dnf` to mutate the bootc host for this development build.

The tested build environment uses GCC 16.2, CMake 4.3, Ninja, `qt6-qtbase-devel`, `qt6-qtdeclarative-devel` and `sdl2-compat-devel`. Include Qt's SQL plugin and Qt Test for verification. The initial Fedora container image ID was `sha256:cc520476538d32d3a570918d061b7627bcf49432163520dd6168298db51c4fa1`; repository packages can change independently.

Configure with an explicit final prefix before building, for example:

```sh
cmake -S /work/source -B /work/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/home/armada/.local
cmake --build /work/build --parallel 3
ctest --test-dir /work/build --output-on-failure
```

The CMake install now includes the application and a desktop launcher. `CMAKE_INSTALL_PREFIX` is resolved into that launcher's executable path; configure it for the actual destination rather than overriding only `cmake --install --prefix`. Stage installation with `DESTDIR` when the final prefix is outside the container mount, then copy the staged files into the user's matching prefix. No administrator privileges or session installation are needed.

Run the built executable against the host's libraries as well as testing inside the container. An offscreen pass establishes ABI/QML behavior; a Wayland GPU run establishes rendering against the actual graphics stack. Neither is a substitute for physical controller and readability acceptance. Check a `BUILD_TESTING=OFF` build for the installed everyday application.

The desktop launcher's ID is `org.traineros.TrainerOS`, also declared by the Qt application for Wayland identification. Start → Exit Development App remains the explicit return path. The binary and launcher can be replaced independently of personal data.

First verification: all 15 CTest entries passed on the ARM64 Fedora container (75.35 seconds). The same executable passed shell and diagnostic SDL scenarios against ArmadaOS host libraries, then the Worlds rendered scenario in the actual KDE Wayland session with the software-renderer override removed. Captured layouts were inspected. A separate `BUILD_TESTING=OFF` configuration built and staged the executable/launcher successfully. These checks exercise synthetic controller events; physical controls and long play sessions remain unverified.

Follow-up: the [RetroArch adapter](RETROARCH_ADAPTER.md) passed the expanded 16-test suite and a real Ruby launch/menu/quit/return flow on the host. The personal library contains the user's imported metadata; ROMs and BIOS remain on the separate card. Plasma display dimming/idle-off was disabled in addition to the previously disabled suspend policy, at the user's request. A screenshot taken while DPMS is off can show a stale compositor frame; restore the display before judging rendered validation. No suspend/resume test was performed.

## Removable content layout

The dedicated microSD uses one ext4 partition, label `TRAINERDATA`, mounted by UUID at `/var/mnt/games`. The mount is optional at boot and uses systemd automount; a missing card must not prevent recovery into the existing OS. `~/Emulation` points to this mount. The mountpoint itself is not user-writable when the card is absent.

```text
Emulation/
  roms/{gb,gbc,gba,nds,n3ds,n64,gc,wii,wiiu,switch,pokemini,...}/
  bios/
  saves/{retroarch,melonds,dolphin,azahar,cemu}/
  states/
  screenshots/
  library/manifests/
  backups/
```

Platform directory names follow ES-DE/Armada conventions (`gc`, `n3ds`). Preserve edition, language, revision and hack names inside those directories. These are external content paths, not World IDs. TrainerOS registers each Adventure under its actual region(s), with a custom World when appropriate; a filename alone must not invent a region or progress.

TrainerOS's SQLite metadata, emulator installations and emulator configuration stay on internal storage. Game content, BIOS and external saves remain separate from the application. Source files are copied, never moved or deleted; a private manifest records original paths, destination paths, sizes and SHA-256 values. Do not commit that inventory or any copied game/BIOS data.

## Emulator packaging observed

Armada Store currently installs RetroArch and Dolphin as system Flatpaks. Its melonDS, Azahar Enhanced, Cemu and ES-DE entries install ARM64 AppImages under `~/Applications`. Store templates and desktop entries are used, preserving its update tracking. Some web documentation still describes older Flatpak packaging for melonDS; inspect the installed catalog rather than hard-coding that older path.

RetroArch's ARM64 core updater needs Armada's configured aarch64 buildbot URL. Core availability differs by build: the observed feed has `parallel_n64`, not `mupen64plus_next`. Installed emulators and copied content do not imply TrainerOS adapters, exact resume, save parsing, achievements or game compatibility.

Sources: [Armada Store](https://armadaos.dev/using-armada/armada-store/), [emulator conventions](https://armadaos.dev/emulation/emulators/), [ES-DE integration](https://armadaos.dev/emulation/es-de/), plus the installed catalog and live reports.

## Follow-up integration checks

The installed ES-DE build migrates settings into `~/ES-DE/settings/`; custom emulator definitions remain in `~/ES-DE/custom_systems/`. It found all copied content. A controller launch of Diamond through the configured melonDS command returned to the same ES-DE list with Start+Select. A local X11 helper supplies that normal close request for AppImages and an optional DS pointer; it is machine configuration, not a shipped TrainerOS adapter. Right-stick pointer comfort and touch-heavy gameplay remain unverified.

The seven native application shortcuts (TrainerOS, ES-DE and the five emulators) were read back from Steam's live app store after creation. Use the installed Armada Store `catalog.launch_spec` conventions, including `/usr/libexec/armada/armada-game-launch %command%`. On this installation, Steam's default controller remapping interfered with the native input path. Disabling Steam Input **for these shortcuts only** restored it; do not globally change Steam game controls. The resulting Steam desktop client -> TrainerOS -> Ruby -> same TrainerOS detail flow passed with InputPlumber gamepad chords. This does not validate the dedicated Gamescope session.

The expanded 16-test suite also passed against Ubuntu 24.04's Qt/SDL dependencies in an isolated x86_64 server container (71.88 seconds), using source commit `f9de1dbdd34200b15f050087af18ccda00f1b475`. The corresponding GitHub Actions run did not start: GitHub reported failed account payments or a spending-limit restriction. This independent run is useful verification, not a successful GitHub CI run. Resolve account billing before relying on Actions again.

Flatpak access must be tested from inside each sandbox, not inferred from host permissions. Dolphin initially exposed the custom card mount through `host:ro`, so Wii could not create SYSCONF. Explicit per-application write grants to `saves/<emulator>`, `states/<emulator>` and `screenshots/<emulator>` resolved it; ROM and BIOS paths are read-only. All six temporary create/read/remove probes passed in the RetroArch and Dolphin sandboxes. Dolphin subsequently created its Wii system data on the card and rendered PokePark's title screen. Its SDL Stop shortcut belongs to `General/Stop`, not `Keys/Stop`; Start+Select now exits batch emulation normally. Per-game INIs belong under the Dolphin **data** directory's `GameSettings`, while named controller profiles belong under its **config** directory's `Profiles/Wiimote`.
