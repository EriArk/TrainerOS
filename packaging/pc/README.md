# Local PC runtime binding

Current source, 26 October: [runtime Downloads and game Performance](../../docs/GAME_PERFORMANCE.md)
supersede the earlier installed-only / reject-all-SteamRT boundary below.
Device acceptance and initial offline SteamRT preparation remain open.

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

**Not delivered by the initial import increment (superseded in part below):**
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

The later game-settings increment below replaces the single-default discovery
with explicit installed-runner selection. The UMU installation prerequisite remains.

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

## PC game settings — 26 October 2026

Games → a PC game's context menu → **Game settings**, or Home → **Game settings**,
opens the same compact surface. The live Home route minimizes the actual game
before opening its settings; saving a launch change requires the game to be closed.
Launch, Components, Artwork & Steam and Controls lead to short contextual lists.
Controls opens the existing stable-game-ID input editor, without a second mapper.

- Import confirmation now accepts local PNG/JPEG/etc. artwork through the shared
  picker. Images are decoded with size limits and reduced to a retained owner-local
  PNG; library presentation reads that reference. Title, executable, working folder
  and argument vector edits keep the same game ID and optimistic library revision.
- Native Linux hides Wine-specific choices. Read-only discovery lists host Steam
  compatibility tools, Lutris Wine builds and the TrainerOS Proton cache, including
  distinct missing, broken and unsupported reasons. A standalone Proton needs a
  matching Wine ELF, tool manifest, UMU and no unprepared SteamRT app dependency.
  ARM discovery retains the existing explicitly named aarch64 bundle boundary;
  generic x86 Proton is not advertised on ARM. Standalone Wine is limited to x86-64.
  These are structural eligibility checks, **not gameplay certification**.
- Selecting a different installed runner persists an absolute version per game.
  On first use the helper copies the previous prefix into a new UUID environment,
  preserving Wine links without following drive links. Atomic publication prevents
  a partial copy being used. The old prefix and selection remain available through
  **Restore previous runtime**. Games installed inside drive_c are resolved inside
  the selected copy, while the original library reference/ID is retained. Prefix
  copies can be large; there is no automatic removal or runtime-cache eviction.
  Restoring the previous environment restores its earlier state, not later saves.
- Components uses maintained Winetricks verbs (`vcrun2022`, `d3dx9`, `dotnet48`)
  through UMU's supported entry point or the selected Wine + matching wineserver.
  Missing tools fail explicitly. Windows settings and the program uninstaller use
  that same environment. Native installers and patch EXE/MSI reuse ProcessService,
  the Linux child supervisor, Apps windows/input and shared Downloads cancellation.
  A prefix/session lease also prevents a second operation after a shell restart.
  Component/patch cancellation stops owned processes; already-applied installer
  changes are **not** a transactional rollback of game files or dependencies.
- The optional Steam choice is initially off and requires an explicit user target.
  `steam.py` refuses running Steam and malformed/unrecognised databases, checks an
  exact owned shortcut marker, verifies a content-addressed backup, detects changed
  source bytes and atomically replaces only the shortcut database. Add/rename is
  idempotent; removal retains unrelated entries and every game/prefix/save file.
- The shortcut stores only Trainer owner + stable game ID and contacts the active
  shell over a user-only local socket. The existing adapter reads the **current**
  registration and owns history, launch, Home/return and exit. Steam's helper waits
  for the real Adventure completion. A different active Trainer or unavailable
  shell refuses launch. Steam owns remapping for this entry; TrainerOS's game
  keyboard/mouse mapper is not started alongside Steam Input. Ordinary direct
  TrainerOS launch continues to use its saved input profile.

Deployment must include `launch.py` **and `steam.py`** in `/var/opt/traineros/pc/`.
No device deployment was performed: Odin/Flip remain reserved for the other chat.
Linux compositor, real Steam launch/control/exit, Wine/FEX/component installers,
prefix migration on SD and physical controller acceptance remain outstanding.
Version acquisition/downloads, verified FEX/runtime combinations, a broader component
catalogue, graphics/performance switches and richer installer-shortcut discovery
remain unfinished under #250/#248. This increment does not close those issues.

Local verification on 26 October (Windows GCC 16.1 / Qt 6.11.1): the full shell
build passed; `trainer_pc_tests` passed 25 with one Linux-libfm skip;
`trainer_pc_ui_tests` passed 5, and input-profile tests passed 11. UI captures at
960×540 and 800×450 were inspected in light/dark themes; the primary Save/Add
action stays pinned below the rows. The isolated Apps shell smoke returned
`apps-verification.txt: PASSED`. The offscreen backend's keyboard mask/opacity
warnings remain separate from real compositor acceptance.

`PcSteamTests.py`: 7 passed, covering owned add/repeat/rename/remove, exact backup,
changed/malformed source refusal, running Steam refusal and literal arguments.
`PcSessionTests.py`: 11 passed / 6 Linux-process skips, covering prefix separation,
copy failure, repeat launch, drive_c relocation, pin-before-first-launch and bounded
Steam context. Windows tests stub `flock`; actual Linux exclusion, process adoption,
Steam client consumption and input/compositor behavior were not tested here.

### Reused upstream mechanisms

Reviewed [Faugus](https://github.com/Faugus/faugus-launcher)'s UMU-based per-game
launcher model, [UMU's command contract](https://github.com/Open-Wine-Components/umu-launcher/blob/main/docs/umu.1.scd),
[Bottles dependency handling](https://github.com/bottlesdevs/Bottles/blob/main/bottles/backend/managers/dependency.py)
and [Lutris runners](https://github.com/lutris/lutris/blob/master/lutris/runners/wine.py)
and [Steam shortcuts](https://github.com/lutris/lutris/blob/master/lutris/util/steam/shortcut.py).
Reuse UMU/Winetricks rather than import another launcher's account/config/package
manager. TrainerOS's VDF reader/writer is bounded original code; unknown field
types are refused. No third-party runtime binaries or implementation were copied.
Keep each installed upstream package's own licence notices.

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

## Runtime downloads and performance — 26 October 2026

The earlier installed-only / reject-all-SteamRT boundary above is superseded by
[Game Performance and Runtime Downloads](../../docs/GAME_PERFORMANCE.md).
The same PC editor now obtains official architecture-matched GE-Proton versions
and pinned UMU through a shared Downloads task. Completed immutable bundles can
be selected explicitly; no update silently changes an existing pin/prefix.
Known UMU 1.4.4 SteamRT IDs are admitted. Initial SteamRT bootstrap still belongs
to UMU at first launch and needs network; complete offline preparation is open.
Install `launch.py`, `steam.py`, `runtimes.py` together under
`/var/opt/traineros/pc/` through the existing session support helper.
Power profiles and supported Proton renderer/sync overrides are owned by the
shared GamePerformance panel, with per-owner/per-game persistence and the actual
Adventure prepare/finalize boundary. Host/devices have not been exercised here.
