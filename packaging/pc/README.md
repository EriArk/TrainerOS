# Local PC runtime binding

`launch.py` belongs at `/var/opt/traineros/pc/launch.py`, readable by the ordinary
session user. It is launched through the existing `ProcessService` and
`AdventureLaunchController`, never through a shell command string or Steam.
The Linux subreaper waits for adopted children after launcher exit; cancellation
signals only this session's descendants. There is no global `wineserver -k`.

PC registrations use `adapterId=pc-local`, `platformId=pc`, `domain=multiverse`
and stable `pc:<UUID>` IDs. This is an execution binding, not an Experience adapter.
`contentPath` is the selected executable. `integrationConfig` holds `runner`,
`installRoot`, `workingDirectory`, the owner-specific `prefix`, pinned `proton`
and `umu` paths, native `dataRoot`, and `ownership=copied`. Native games receive
owner/game-specific XDG config, data and cache paths; programs ignoring XDG may
still write beside their executable or under HOME. Files, prefixes and personal progress
are not deleted when importing another game. No ROM extension registry is used.

Current import accepts self-contained game **folders**, copied using libfm-qt
through the shared Files task service. Symlinks and special files are rejected;
there is no implicit link-only import or automatic permission change. ELF
architecture and Windows PE signatures are inspected before confirmation.
The earlier folder-only checkpoint below remains historical. The source now also
accepts ready-game ZIP/7Z/RAR archives and an executable's containing folder.

## PC import and installer preparation — 26 October 2026

Games → PC → Add has two entries: **Install a game** and **Add a ready-to-play
game**. Both choose internal/removable storage and end with explicit executable,
title and working-directory confirmation. An installer exit never creates a card.

- Folder copies stay with libfm/GIO. Archive decoding uses the required system
  **libarchive** dependency through `FilesService::start("extract", ...)`, with
  shared Downloads progress/cancel. No second archive/download service is added.
  Extraction uses a new empty destination; links, special/absolute/traversing
  paths, case-folded duplicate names, encrypted data and overwrites are refused.
  Entry count and actual expanded bytes are bounded; disk space keeps a 64 MiB
  reserve and extraction has a 512 GiB ceiling. Unsupported/split archives fail
  visibly; this is not a claim that every RAR variant works.
- The ordinary-user installer reuses `ProcessService` and `launch.py`'s Linux
  subreaper. EXE uses the selected compatible UMU/Proton; MSI uses `msiexec /i`
  in the same isolated prefix. Native ELF/AppImage and explicitly recognised
  sh/bash installers are supported entry formats. Terminal-only, privileged or
  unsupported-architecture installers are not converted into supported GUI apps.
- Windows installation defaults to the import's own `prefix/drive_c`. Native
  installers start in the chosen game folder and receive
  `TRAINEROS_INSTALL_DIRECTORY`; users must keep their actual destination there.
  This is ordinary-user execution, **not a filesystem sandbox**. No game outside
  the selected installation root can be registered by this flow.
- Apps owns the installer's exact process-tree windows, Home/minimize/return and
  its input lease. A new child window remains part of the same installer, even
  after the first window closes. The shared input mapper provides right-stick
  pointer, A/B mouse clicks, X Backspace, Y Tab, Start Enter, Select Escape and
  shoulder scrolling. Its existing independent watchdog releases held input on
  focus loss, Home, EOF or a stall. Touch uses the actual installer windows.
- The owner-scoped import journal resumes file inspection after restart. A live
  supervisor PID/start identity protects the prefix until the installer exits.
  Cancel retains source and partial files; a separate confirmation discards only
  a marker-verified UUID destination created by this import. Recovery of a still
  running installer uses Apps; cancellation after shell restart uses its own GUI.

Home+X now toggles the shared Touch/PC keyboard, including a passive external
surface over owned game/application windows. Home alone acts on release; the
chord is consumed through neutral before navigation or game mapping resumes.
Xbox/Steam use the west X, Nintendo the north X, PlayStation the west square.
Unknown controllers use the west position. The helper revalidates active-window
ownership before each output request and closes the keyboard on focus loss.
X11/Xwayland output uses XTest and a temporarily restored unused keycode for
Unicode; native Wayland, arbitrary third-party windows and universal game text
compatibility are not claimed. `KeyboardShortcutTests.py` covers chords,
neutral gating and packet/focus boundaries; real compositor and text acceptance
remain deferred with the devices.

**Not delivered by this source increment:**
artwork/Steam-shortcut confirmation, richer shortcut/manifest-based executable
discovery, runtime version/download/dependency management (#250), and a tested
real EXE/MSI/Linux installer journey on Armada. Native installation location must
be selected in installers that ignore the working folder/environment.

The owner assigned Backup/Restore to another chat and reserved both handhelds
for other work. No handheld connection, build, deployment, reboot or input check
is part of this increment. Windows compilation, archive fixtures and mocked
window/input checks do not establish Wine/FEX, Linux subreaper or device acceptance.
Local Windows checks on 26 October: `traineros` built; PC tests 23 passed / one
Linux-libfm skip, Apps 10 passed, input profiles 11 passed and UI probes 4 passed.
The probes rendered both keyboard layouts and the compact import chooser at
960/800 widths, and dispatched a key through the external pointer path. Offscreen
window-mask/opacity warnings are backend limitations, not compositor acceptance.
The isolated Apps shell smoke produced `apps-verification.txt: PASSED`.
Python checks: shortcut/output 5, Apps lifecycle 8 and game mapping 17 passed;
PC session command checks 3 passed with 6 Linux process cases explicitly skipped.
Run `pc_games`, `pc_import_ui`, `apps`, `PcSessionTests.py`,
`test_apps_service.py`, `GameInputTests.py` and `KeyboardShortcutTests.py`; Linux-only session tests stay
explicitly skipped on Windows. Libarchive RAR fixture provenance is in
[`tests/fixtures/pc-archives`](../../tests/fixtures/pc-archives/README.md).

## Windows runtime prerequisite

The checked launcher is upstream **UMU 1.4.4**, the official zipapp release:

- https://github.com/Open-Wine-Components/umu-launcher/releases/tag/1.4.4
- Asset `umu-launcher-1.4.4-zipapp.tar`
- SHA-256 `eb590691841f7fad3fc3ad8fd5db4ccb87849fe7948e62b28ece7a4ee48cc851`
- Install the archive's `umu/umu-run` and `umu/umu_run.py` together in
  `<state>/runtimes/umu/1.4.4/`. Keep upstream licence notices with the runtime;
  upstream code/binaries are not copied into this repository.

This first binding discovers an already installed host-native standalone Proton
in Steam's `compatibilitytools.d`; it verifies the native Wine ELF and skips
tools requiring a separate Steam runtime app. The selected absolute version path
is persisted per game. Host tools are read, never modified. Missing runtime is
an explicit error, not a reason to execute a random host Wine or claim support.
SteamRT-managed Proton and runtime acquisition still belong to the version manager.

Check with `trainer_pc_tests` / CTest `pc_games` and `python3 tests/PcSessionTests.py`.
These establish import and lifecycle behavior, not a Windows game's compatibility,
physical controller acceptance or PC multiplayer support.

## Delivery checkpoint — 21 October 2026

The common mini file picker and folder import were installed on Odin and Flip
in build `9ba43660681f5ac618be89ccdf972da2b691117d9b57ac6c77afc0836f226cec`.
The retained ARM run covered PC import/session supervision, library and Files;
the subsequent Home support run passed six PC-session and 24 overlay checks.
Live Home displayed the actual native fixture on both devices. Continue followed
by Home/Exit and process completion was verified on Odin. Flip lost connectivity
before its final return/exit check. The owner subsequently rebooted Flip; no
active game remained when it reconnected. This does not verify graceful Exit.

The genre-collection follow-up passed the ARM series suite and is installed on
both devices as `ecc0d6cf8a89dee88dc55b0b1ee1b6f4499f458a1ccbc7f31c7ab8eff0e58018`.
The native fixture's card and isolated source/install/data folders were removed
from both devices after testing. Database/history verification passed separately
on each. Genre definitions are applied only after content transfer and matching.
Private screenshots, device receipts and copy manifests are kept outside Git.
