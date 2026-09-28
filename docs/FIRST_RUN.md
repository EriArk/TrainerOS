# First-run setup

The native development build now has a device setup flow before first Trainer
creation. It is a building block for the Armada-based image, not delivery of
the image, bundled emulators or their updater.

## User path

Welcome → directions/A/B check → optional Connections → library storage →
Trainer card → optional Trainer PIN/family reset code → Home.

Connections reuses the existing Wi-Fi/Bluetooth pane and masked controller
keyboard. B returns to the setup choice; a connection operation or question
retains its own cancellation behavior first. Continuing offline is supported.
Physical navigation remains trapped in setup until a real Trainer is open.

Storage offers the existing library, internal storage and mounted removable
locations. It recognizes `Emulation/roms`, `roms` and `batocera/roms`, preserves
existing aliases, and creates missing platform directories from the shared
ARM64 registry. It never formats a disk, moves games or rewrites gamelists.
Folder preparation runs off the UI thread, checks the selected mounted device
again and reports write failures without advancing. A disconnected card must
not be replaced by an empty directory on the underlying internal filesystem.

The Trainer card uses the existing real registration transaction. At completion,
optional PIN setup reuses the existing family-code and Trainer-access controls;
a missing family code can be created inline before the personal PIN. Accounts,
including RetroAchievements, remain available later in Settings.

## Persistence and existing installations

`first-run.json` in the application data directory stores version 1, the current
step, selected ROM root and a nonsecret Trainer-card draft. Writes use atomic
replacement. Passwords and PIN drafts are never written there. Corrupt/unreadable
progress stays intact and produces Retry, rather than silently restarting setup.
Completed profile creation is detected after a restart so the wizard does not
create a duplicate Trainer. Finishing removes the temporary card draft.

Existing installations with Trainers and no setup marker retain their current
entry behavior. They are not forced through setup by an upgrade. A completed
new installation enters Home after normal profile/PIN entry on subsequent boots;
personal Adventure selection and browsing records remain stored. Removing the
last Trainer retains device setup and uses the existing Trainer creation gate.

The persisted ROM root feeds the folder scanner and file-picker starting point.
An explicit `--roms-dir` remains a development override. No SQLite migration,
emulator configuration replacement or session-default change is involved.

## Verification — 2026-09-28

- Native build and all 45 CTest targets passed. After the final registration-row
  correction, the affected first-run and Trainer-profile tests passed again.
- Flip ARM64 build installed with SHA-256
  `bb8c9a6cebf2bf8f6b57b4925c0a55cc20730992f25dda7077a6723e76d9f808`.
- On Flip, isolated application data exercised real controller events through
  welcome, controls, the live Connections pane, offline continuation, storage,
  controller text entry, real Trainer creation and Home. Setup and the card draft
  resumed after process restarts. Family-code creation followed by personal PIN
  creation worked; the next launch required that PIN.
- Actual Gamescope captures verified welcome, controls, storage and completion.
  Tests used a separate library/data directory on the configured device; they
  did not reset the owner's installation or test a fresh OS image.
- Production replacement preserved all 3 Trainers and 830 Adventure records,
  passed SQLite integrity checks, restarted normally and did not create a setup
  marker for existing owners. Binary/database backups were retained privately.
- Odin was unreachable over SSH and was not updated. New-network authentication,
  physical card removal and emulator/image update retention were not exercised.

## Remaining first-run acceptance

- Real translations/language selection and time-zone/date controls.
- Optional account setup within the wizard; active-Trainer RA is still Settings.
- Reusable storage selection in Settings, nonstandard folder browsing and stable
  removable-device identity across changed mount paths.
- General scanner classification/idempotence fixes identified in the startup
  audit; broad emulator readiness/repair and reproducible image installation.
- Full fresh-image → copied ROM → play/save/return and emulator-update retention
  acceptance. A clean app data directory on an already configured handheld does
  not prove these image-level requirements.

See [the full journey](STARTUP_EXPERIENCE_AUDIT.md) and [execution order](ROADMAP.md).
