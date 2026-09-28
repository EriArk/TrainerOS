# Standalone Home exit — 2026-09-28

Owned melonDS windows and Dolphin batch launches now use the existing guarded
Home/Guide overlay alongside RetroArch, PPSSPP and ARMSX2. No new launch screen
or per-game setup is introduced. B cancels to the same process; A requests
normal emulator closure, without asserting that gameplay was saved.

## Implementation

melonDS receives its normal Qt `WM_DELETE_WINDOW`. Dolphin gets a per-launch
`-C Dolphin.Interface.ConfirmStop=False` override plus batch mode. Its saved
preferences remain unchanged; the helper requires both arguments before accepting
Dolphin. Only one close request is sent per process identity, because a repeated
Wii stop can escalate to forced shutdown. XRes ownership, Adventure ancestry,
process-start identity, physical neutral gates and watchdog recovery remain.

In a dedicated session the stock controller/pointer bridge no longer closes games
through Start+Select, bypassing confirmation. Its fallback outside TrainerOS
remains. Custom bridges are preserved.

## Installation

`packaging/session/install.py --user ACCOUNT` includes the helpers. For existing
supported installations, `--emulator-support-only` updates just this support
without changing boot defaults or restarting InputPlumber/the session.

Before writes it checks Python D-Bus, SDL2, X11, XRes, Xtst, working ffmpeg,
setfacl and active InputPlumber. It installs helpers under
`/var/opt/traineros/integrations`, retaining original root-file backups through
the session installer, and installs the account-specific read-only input hook.
The ACL can be renewed on an already prepared controller. An absent built-in
pad does not prevent service startup.

An exact stock legacy user bridge is backed up and redirected to the canonical
helper, so updates need not edit existing emulator JSON. Custom files and
symlinks are preserved. The session client merges the raw-pad SDL exclusion
with existing exclusions; games continue using the virtual pad.

Dedicated sessions use the canonical helper when `overlay.json` is absent.
Explicit disabled, invalid, oversized or unreadable configurations do not enable
the default; custom paths remain supported. To disable the transport, retain
`{"version":1,"enabled":false}`. Removing the file now selects the default.
No emulator preferences, ROMs or saves are overwritten by installation.

## Evidence and limits

- Windows build and standalone/process/Adventure-exit/exit-presentation/QML-smoke
  suites passed. ARM64 production build and 15 standalone cases passed.
- Seven helper cases cover recognition, Dolphin qualification, no repeated close,
  ownership, neutral input and watchdog release. Two installer cases cover
  stock/custom preservation, repeatability and prerequisite failure before writes.
- Actual Flip, isolated library: A launched Kirby Super Star Ultra (melonDS
  AppImage) and Wind Waker (Dolphin Flatpak) from their wheels. Injected built-in
  controller-source Guide/B/Guide/A events opened confirmation, cancelled to the
  same recorded PID, then closed normally back to the wheel. No second Dolphin
  dialog appeared. Device captures were inspected. No `overlay.json` was present
  in the isolated library, proving the installed default.
- Repeated helper installation required no session/InputPlumber restart. Final
  production deployment retained 3 Trainers and 830 registrations; SQLite
  quick_check passed, with private binary/database backups retained.

Installed production SHA-256:
`406dded9d455687e6a8acb028c2ffdc05b874d55714d77ef42c3e119054ed0fa`.

This is DS/GameCube lifecycle evidence, not semantic-save proof, Wii shutdown
proof, every emulator's coverage or a clean-image test. Odin was not exercised
here. Firmware readiness, package provisioning, update/rollback and other routes
retain their gates. The installer targets a running supported Armada system;
offline image assembly still needs release-stage integration.

## Upstream basis

- [melonDS MainWindow::closeEvent](https://github.com/melonDS-emu/melonDS/blob/master/src/frontend/qt_sdl/Window.cpp): normal frontend window closure.
- [Dolphin RequestStop and OnStopComplete](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DolphinQt/MainWindow.cpp): confirmation, Wii power-down, repeated-stop escalation and batch exit.
- [Dolphin command-line config layer](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/UICommon/CommandLineParse.cpp): per-launch `-C System.Section.Key=Value` overrides.
