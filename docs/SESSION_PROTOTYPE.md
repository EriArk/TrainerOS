# Dedicated TrainerOS session on ArmadaOS

**Target change — #49:** the following session install, supervisor/recovery and dated acceptance remain implemented evidence; any resume check refers to legacy behavior. The next lifecycle increment must prove clean capture and a controller confirmation while the game stays alive, then cancellation back to the same process or graceful exit. The current hidden-shell/stop path does not prove this overlay. Preserve orphaned-game recovery, return route, maintenance and no-sleep policy. [Exit contract](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit).

This package targets the ArmadaOS testing build inspected on the Flip 2 on 2026-09-13. It preserves Steam and Plasma and does not modify the immutable `/usr` image. Sleep remains disabled by the owner's decision; wake/hinge testing is a separate gate.

## Install a session choice

First install and validate the native application at `~/.local/bin/traineros`. From a reviewed checkout:

```sh
sudo python3 -I packaging/session/install.py --user armada
install -Dm644 packaging/session/org.traineros.Session.desktop \
  ~/.local/share/applications/org.traineros.Session.desktop
```

The account is an explicit installer argument. Root-owned launchers live in `/var/opt/traineros/session`; the user service is `/etc/systemd/user/traineros-session.service`. The SDDM entry lives under `/etc/traineros/wayland-sessions`, added without removing existing session search paths. Installation preserves the current autologin choice. Original files and the original autologin override are retained in `/var/lib/traineros/session-backup`.

`ArmadaPlatformService` exposes fixed mode choices. The root helper accepts only session/power verbs; Python runs in isolated mode and a narrow sudoers rule allows those actions without a password. No arbitrary root command or file path enters this interface.

The existing Armada selector owns Desktop and Steam transitions. On this build its Desktop choice is Armada Plasma Mobile; the other installed Plasma entries remain available. TrainerOS writes the same autologin override when selected, so subsequent official mode choices remain authoritative.

After validating the session, select TrainerOS as the boot default explicitly:

```sh
sudo python3 -I packaging/session/install.py --user armada --default
```

Armada's `armada-session-default.service` otherwise resets autologin to Steam at every boot. The optional root-owned drop-in under `/etc/systemd/system/armada-session-default.service.d/traineros.conf` replaces only its fixed default-selection command, retaining the distro's ordering before the display manager. Steam/Desktop transitions still work during the current boot; the next boot returns to TrainerOS. The installer checks the observed Armada service before enabling this integration.

## Graphics and process ownership

SDDM's wrapper starts and waits for a dedicated systemd user service. That service owns Gamescope and binds to `graphical-session.target`, following the installed Armada convention. Existing Plasma workspace targets are stopped first: stopping SDDM alone can leave a compositor and its sockets alive. The shared graphical target must be started through a dependent service, not directly.

Gamescope consumes Armada's `device-env` connector, panel type and orientation. It runs a separate client without sourcing Steam's mutable profile or using Steam's shared short-session counter. QML contains no display numbers or hardware paths.

The client publishes the actual display to the activation environment and uses X11 for Qt applications. `WAYLAND_DISPLAY` is unset; the explicit Gamescope Wayland name remains available separately. RetroArch's Flatpak `fallback-x11` permission otherwise hides `DISPLAY` when a Wayland socket is available, causing its Qt companion to abort. A process ID or initial compositor frame does not prove a game launched: inspect it after startup.

## Recovery and mode changes

- Three client crashes within two minutes fall back to Plasma. A startup without a first rendered frame has a 45-second deadline, acknowledged over a private inherited pipe.
- A Linux subreaper waits for orphaned Adventures before restarting a crashed shell. A shell crash does not justify terminating a game that may be saving.
- Mode changes flush TrainerOS persistence and wait for its services. They are unavailable during an active Adventure. Failure keeps the app open with an error.
- A/B confirm/cancel; primary page changes cancel the confirmation. Normal app mode offers entry into the dedicated session, which offers departure to Desktop or Steam.
- A root-only marker belongs to the old wrapper PID and expires after 30 seconds. Its recovery callback cannot overwrite an intentional mode switch; a new session can still recover from startup failure.
- Client logs rotate in `~/.local/state/traineros/session.log`. Compositor/service output uses the journal identifier `traineros-session`.

Prepare a graphics-independent timed fallback before a first trial:

```sh
sudo systemd-run --unit=traineros-trial-recovery --on-active=3min \
  /var/opt/traineros/session/control.py desktop
sudo /var/opt/traineros/session/control.py traineros
```

Use a new unit name for another trial. Cancel the timer after validation; do not leave it armed during ordinary Adventures. Desktop's **Return to TrainerOS** entry selects the dedicated session again. Cross-boot behavior requires the explicit default integration above.

## Rollback

From SSH or a terminal, `sudo /var/opt/traineros/session/control.py desktop` selects the known-good desktop. Armada's own `sudo /usr/libexec/armada/session-control switch-desktop` works independently of TrainerOS; `switch-gamemode` selects Steam.

For removal, first switch to Desktop. Restore installed files from the backup directory, or remove only files marked originally absent there. Remove the dedicated user desktop entry and reload the systemd user manager. Preserve Armada's original entries and review any later user changes before restoring old configuration.

To restore Armada's original Steam boot preference, remove only the optional `traineros.conf` drop-in described above (or restore its recorded predecessor), reload the system manager with `sudo systemctl daemon-reload`, then use Armada's `default-gamemode` command. Merely selecting Desktop for the current session does not remove TrainerOS's boot preference.

## Acceptance

- [x] Dedicated landscape shell, physical controls, page navigation and Continue selection.
- [x] Real FireRed/mGBA resume rendered after display isolation was fixed.
- [x] Killing TrainerOS left that Adventure alive; it exited normally and the shell restarted afterward.
- [x] Three deliberately failing starts returned to Plasma; the production binary was restored and verified.
- [x] Controller confirmation and transition into Plasma from normal app mode.
- [x] Final production first-frame acknowledgement and controller round trip through Steam.
- [x] TrainerOS selected after those checks; trial recovery timers cancelled.
- [x] Physical restart from the handheld panel boots directly into TrainerOS and restores browsing state, after integrating Armada's boot-default service.

The initial 2026-09-13 session suites passed on Windows (25 tests), Ubuntu/Qt 6.4 (26) and the Flip ARM64 build (26). The following handheld-control increment passes 26/27/27 respectively; seven Linux subprocess checks cover the supervisor and default selection. The installed release build disables test-only flows. Sleep/wake remains deferred.

Native suites cover controller confirmation, persistence and lifecycle. Linux subprocess tests cover crash limits, orphan preservation, first-frame deadlines and transition ownership. They do not replace the device gates above. See [platform model](ARMADA_PLATFORM.md), [device baseline](ARMADA_DEVICE_BASELINE.md) and [roadmap](ROADMAP.md).
