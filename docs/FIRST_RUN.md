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
Settings → Library → Game storage reuses the mounted-location service and card
list. A selects and prepares a location; B returns to Library. The committed
choice updates the scanner and file-picker root and starts a scan immediately.
It does not move/delete existing ROMs, saves, installations or gamelists. The
previously registered library keeps its identities/history. Busy preparation
holds navigation and session exit until completion; errors retain the old root.

`library-location.json` stores the selected device-wide path with atomic writes.
It takes precedence over the original setup marker's root, allowing older setup
records to remain readable. Preparation reuses mounted-device checks; the GUI
thread checks scanner/database activity before committing and changing the root.
An explicit `--roms-dir` remains a development override. No SQLite migration,
emulator configuration replacement or session-default change is involved.

## Presentation refinement — 2026-09-28

The setup uses a compact six-stop route across the top, an original illustrated
handheld/travel kit on the left and a focused task area on the right. Controls
light up on the handheld drawing when checked. Mounted storage cards show media
type, real free space, capacity use and the current location; the selected path
stays in a separate detail area. Normal controller legends remain in the chassis
footer. The original illustration uses cached Canvas geometry and no ambient
paint timer, downloaded artwork or extra image pack.

Composition references: [Steam Gaming Mode setup in Bazzite's documentation](https://docs.bazzite.gg/General/Installation_Guide/post-installation/)
and [Nintendo's first-time setup guide](https://www.nintendo.com/en-gb/Support/Nintendo-Switch/Nintendo-Switch-First-Time-Setup-and-Connection-1295677.html).
These inform task hierarchy/controller simplicity, not copied artwork or a
replacement for the established TrainerOS chassis.

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

### Refinement and Settings storage verification

- Native and ARM64 builds passed. Eight affected CTest targets passed: first-run,
  Batocera discovery, core, interactions, storage, Trainer profiles and the general
  and Worlds QML smoke scenarios. Final presentation-only refinements were rebuilt
  and inspected on the handheld.
- Real Flip controller events completed the revised wizard in an isolated data
  directory, then opened Settings → Library → Game storage. Selecting internal
  storage prepared the folders and applied the path. A restart without the
  development ROM-root override retained that choice in Settings. The owner's
  production library location was not switched by this test.
- Final Flip binary SHA-256:
  `c75ee950afdcdc6b0c3234ba960fe749fac3b5a9ba21bf3410ab7db9ff788d0e`.
  Replacement preserved 3 Trainers / 830 Adventure records and SQLite integrity;
  private binary/database recovery copies were retained. Odin remained unreachable.

## Remaining first-run acceptance

- Real translations/language selection and time-zone/date controls.
- Optional account setup within the wizard; active-Trainer RA is still Settings.
- Nonstandard folder browsing and stable removable-device identity across
  changed mount paths. Mounted-location selection in Settings is implemented.
- General scanner classification/idempotence fixes identified in the startup
  audit; broad emulator readiness/repair and reproducible image installation.
- Full fresh-image → copied ROM → play/save/return and emulator-update retention
  acceptance. A clean app data directory on an already configured handheld does
  not prove these image-level requirements.

See [the full journey](STARTUP_EXPERIENCE_AUDIT.md) and [execution order](ROADMAP.md).
