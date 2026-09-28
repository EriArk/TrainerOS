# Date and time — 2026-09-28

Settings has a Date & time category in its existing two-pane overlay. The same
controller/pane is used between Connections and Games during first setup.
Continue is initially selected; keeping the current system time takes one A.
Existing completed installations and Trainers do not repeat onboarding.

- Automatic time reflects and changes the actual system synchronization service.
  Enabled and synchronized are separate states; offline use can continue.
- Time zone opens an inline city list. Left/right changes the region, up/down
  chooses a city and A applies it. B cancels without changing the system.
- With automatic time off, the inline date/time editor uses left/right to adjust
  fields. A advances to the next field; Save date & time applies the draft.
  B discards it. A changed system zone or newly enabled automatic time invalidates
  a manual draft instead of silently using stale settings.
- Legends stay in the chassis footer. No keyboard, desktop dialog or per-game
  setup is required. Errors retain actual system state, and unavailable clock
  management does not prevent proceeding through first setup.

## System boundary

`ClockService` uses bounded `timedatectl --no-ask-password` processes on a worker.
No shell interpolation is used. Readback after each operation is authoritative.
The UI uses an explicit `QTimeZone` rather than relying on a cached local zone.
Manual values are converted to UTC before setting the system clock; changing a
zone never changes the current instant. RTC local/UTC policy remains untouched.

The systemd [timedate1 interface](https://github.com/systemd/systemd/blob/main/man/org.freedesktop.timedate1.xml)
owns time, zone and NTP changes. Armada's existing chronyd is reused; no second
time daemon is installed. The normal session installer includes a narrow polkit
rule for these three operations, limited to the named active local user.
`packaging/session/install.py --user armada --clock-only` can add only that rule
to an existing prepared device, preserving its first prior copy for recovery.
The shell stays unprivileged. SSH-only processes do not receive this permission.

The first-run file retains schema version 1 with an additional named `clock`
stage; old stages remain valid. Device clock settings stay in the OS, not in
Trainer profiles or SQLite. A completed library/profile is not migrated.

## Remaining boundary

This is native first-run/Settings delivery, not a fresh image, OTA implementation,
location-based automatic zone detection or translated interface. Broader system
installation/update/recovery acceptance remains in ROADMAP.

## Verification

Windows builds and affected clock, first-run, core, interaction, storage and
rendered shell/Settings checks passed. Clock regressions cover automatic/manual
gating, cancellation, rejected writes with authoritative readback, busy navigation,
stale manual drafts and unavailable-service setup continuation. First-run checks
include resuming the clock step without duplicating a Trainer. ARM64 build and
clock/first-run Linux tests also passed.

On the actual Flip, controller events opened Settings, disabled automatic time,
edited/cancelled a draft, changed Asia/Jerusalem to Asia/Kabul and restored it,
then re-enabled NTP. System readback confirmed the changes and restored values.
No manual wall-clock jump was performed on the owner's device; that write route
is covered by the service implementation and injected operation tests, not a
claimed physical time-jump test. Device screenshots cover Settings, zone picker,
manual editor and a separate isolated first-run instance. Odin was not tested.
The isolated clock step survived process restart, then one A advanced to storage.

Rapid intentional test/deployment SIGTERM restarts exhausted the existing
supervisor's three-short-exit budget. Recovery left the TrainerOS session and
Gamescope also reported a shutdown assertion. The normal session was restored
through its installed session-control boundary; this was not a clock-operation
failure. Future device probes should avoid spending the production restart budget.

Deployment preserves the existing 3 Trainers and 830 library registrations,
with a checked database/binary backup and SQLite quick_check. It does not reset
the installed profiles or send them through onboarding.
Final Flip binary SHA-256:
`88a56402efdb52af16cdce5d193cb6962d09fb5656168b842cae5c32d8010544`.
After recovery the normal session is active, its first frame is logged, and
Asia/Jerusalem, enabled NTP and synchronized status are verified.
