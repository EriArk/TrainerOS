# Native development

## Current scope

These increments cover the five-page native shell, shared persistence, personal library management and a tested process lifecycle foundation. Real emulator/provider integrations and device/session acceptance remain incomplete.

Implemented:

- C++20, Qt 6 / Qt Quick and CMake; no Android dependencies.
- Full-screen landscape application, with a windowed development option.
- Shared chassis in five persistent colors, recessed light screens, colored organizer tabs, raised controls and a reduced-motion setting. Original geometric placeholders only.
- Five peer pages with saved local routes, applied filters and stable selections. Normal Home shows the local profile and an empty journey overview until real history exists.
- Worlds: reference/custom regions → bounded Adventure list → detail. Personal Adventures have stable identities, primary/additional Worlds, edition, notes and external file references. Start → Manage Adventures supports controller add/edit, shared text entry, custom Worlds and asynchronous file browsing. Unconfigured records show Needs setup; sample launch/resume behavior is limited to the explicit preview.
- Pokédex: 14 text-only reference entries, a three-row scrolling list, entry detail, combined World/type/record filters, name/number search, sorting and persistent favorites. Personal Seen/Caught is unknown in normal mode; the ephemeral fixture retains sample progress. Failed loads/writes and empty results retain recovery actions.
- Hall of Fame: four sample archive records with team/detail views, and a separate internal RetroAchievements area with Adventure sets, original sample goals and account-scoped unlock records. Mock refresh supports loading, cached/offline, error, disconnected and unsupported states. Archive access remains independent; no real account or achievement data is used.
- Shared controller keyboard: A–Z on the left, a separate 1–9/0 block on the right, Space/Delete/Clear/Apply, a bounded draft and intentional navigation through wide keys.
- Trainer create/edit with durable SQLite storage: name, three original emblems, limited featured-Pokémon choices, Save/Cancel, stable identity, validation and asynchronous write/retry behavior. `--ephemeral` uses the original empty in-memory repository.
- Home's compact lower-left Continue extension expands sideways, then upward. Y toggles it and B restores focus. Normal mode has an honest empty state; the ephemeral preview has three sample cards.
- System menu with working Settings, Manage Adventures and Controller diagnostic services, informational session stubs and an explicit Exit Development App action. Steam and Plasma integration remains unavailable.
- Domain/repository, input/navigation, Adventure adapter and platform boundaries. SQL and file listing run on workers. A test-only child exercises checkpoint → launch → return, including failures and visible focus restoration; no real emulator or network integration is enabled. See [local persistence](LOCAL_PERSISTENCE.md) and [library and launch preparation](LIBRARY_AND_LAUNCH.md).

## Dependencies

- CMake 3.24 or newer; Ninja is recommended.
- C++20 compiler.
- Qt 6.4 or newer: Core, Gui, Qml, Quick, Sql, the QSQLITE driver, QtQuick / QtQuick.Window / QtQuick.Shapes QML modules, and Test when `BUILD_TESTING=ON`.
- SDL2 2.0.14 or newer, including development headers and its CMake package.

Use a consistent compiler/Qt ABI. On the current Windows development machine the working combination is MSYS2 UCRT64 GCC and UCRT64 Qt; do not combine it with the separate MSVC Qt installation.

For consistent readable numerals, the application prefers an installed Noto Sans, DejaVu Sans or Segoe UI font, then falls back to the platform's general UI font. No font assets are bundled. Install Noto Sans or DejaVu Sans on a Linux target if neither is already present.

## Build and run on Linux

With dependencies already installed:

```sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure
./build/native/traineros --windowed
./build/native/traineros
```

The last command requests full-screen inside the existing graphical session. It does not select a new session. If Qt is installed outside the system prefix, supply `-DCMAKE_PREFIX_PATH=/path/to/qt`.

An Ubuntu development/CI example, **not an ArmadaOS installation prescription**:

```sh
sudo apt-get install ninja-build qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite libsdl2-dev \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-shapes \
  qml6-module-qtqml-workerscript qml6-module-qtqml-models
```

Build on the target ARM64 system, or supply a real toolchain/sysroot for cross compilation. A Windows `.exe` is a development build and cannot run on ArmadaOS. Packaging and the exact target dependencies remain device-validation work.

## Current Windows development machine

```powershell
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Users/EriArk/msys64/ucrt64 -DCMAKE_CXX_COMPILER=C:/Users/EriArk/msys64/ucrt64/bin/g++.exe
cmake --build build/native --parallel 6
ctest --test-dir build/native --output-on-failure
./build/native/traineros.exe --windowed
```

The matching UCRT64 `bin` directory must be on PATH for the Qt/SDL runtime DLLs. Machine paths are examples for this development host, never application constants.

## Data modes and first device run

Normal runs persist the personal library, profile, favorites, color/motion preferences and browsing state under Qt's platform application-data location. The library starts empty. For isolated development, use `--data-dir /path/to/test-data`; use `--ephemeral` to run the original sample fixture without opening SQLite. Feature smoke scenarios use that fixture; persistence scenarios use temporary stores. Do not use a personal data directory for automated tests.

Storage startup/recovery, write/exit semantics and schema details are in [LOCAL_PERSISTENCE.md](LOCAL_PERSISTENCE.md). Use [FIRST_DEVICE_RUN.md](FIRST_DEVICE_RUN.md) and `tools/collect-device-baseline.sh` when the handheld arrives. The report script has been syntax-checked on the development host; its Linux output and the actual target environment remain unverified.

Start → Controller opens live button/stick/trigger observations, the foreground/neutral gate and Qt runtime/display information. Its lower rail offers Refresh display, Save report, Reset checks and Back. Export is explicit and local, including in ephemeral mode; reports go to the `diagnostics/` subdirectory of the configured data directory. See [DEVICE_DIAGNOSTICS.md](DEVICE_DIAGNOSTICS.md) for what these readings establish and the physical checks still required.

## Controller input

| Intent | SDL controller mapping | Development keyboard |
| --- | --- | --- |
| Focus | D-pad / left stick | Arrow keys |
| Confirm | A | Enter |
| Back / close | B | Escape / Backspace |
| Previous / next primary page | L1 / R1 | Q / E |
| Continue on Home | Y | Y |
| System menu | Start | F1 |

Primary pages currently stop at either end; wrapping is deferred to physical testing. L1/R1 closes transient layers and switches the primary page, even from the system menu or keyboard, discarding unsaved form/text drafts and unconfirmed picker choices. Back closes the response panel, then system menu, then keyboard, then a picker/profile form/Continue or the current feature's detail layer. It never exits a primary page. Page, Worlds/Pokédex routes, applied filters and selections survive switching away/back in memory.

SDL's mapped labels are provisional until checked against the Flip 2 controls and ArmadaOS controller-emulation mode. No mappings are hard-coded into feature QML. The first connected supported controller is used; reconnect is supported. A future device-profile/remapping UI is not implemented yet.

The stick engages at 55% and releases below 35%; directions repeat after 350 ms, then every 100 ms. Confirm, Y, Start and shoulder actions only fire on a new press. Input is disabled while the application is inactive and requires neutral controls on return, avoiding a held Adventure button triggering the shell. Keyboard repeat is supplied by Qt for arrows only.

No touch swipes or outer-page scrolling are required. The logical 960×540 design space scales into a landscape viewport, including 1920×1080, and letterboxes other aspect ratios. Text density and actual device performance remain subject to testing on the handheld.

## Try Trainer and text entry

1. R1 to Trainer, then A on Create Trainer.
2. A on Name opens the keyboard. D-pad/left stick moves, A chooses a key, and B discards the buffer.
3. Choose Apply to return the text to the profile form. This does not save the profile.
4. Up/Down navigates Name, Emblem, Favorite and Save. A cycles the emblem/favorite sample choices. Left/Right moves between Save and Cancel.
5. Save submits a local write and displays Saving until committed. Before Save, B/Cancel discards the draft. After submission, leaving the form does not cancel the write. Edit Trainer reopens stored values and preserves ID/creation time.

The initial keyboard has uppercase Latin letters plus digits. Additional languages/case layouts and direct desktop-keyboard typing are not implemented. Name length is at most 24 grapheme clusters after trimming; blank names and embedded control/newline characters are rejected. The shared keyboard preserves an existing Unicode value and Delete removes a full grapheme rather than splitting a surrogate/combining sequence. Pokédex search reuses this component with a separate consumer and a 32-character limit.

Start can overlay the keyboard; closing the menu restores its exact key and buffer. Y never types or opens Continue while editing. Editing identity does not change Adventure progress. Personal journey statistics remain unknown; the ephemeral preview supplies separate sample progress.

## Trying the personal library and settings

1. Start → Manage Adventures → Add Adventure. Choose a local file, title and primary World; optionally choose additional Worlds, a ROM-hack edition, notes or Create a World. All text uses the controller keyboard.
2. Save commits the record. B cancels an unsubmitted draft. A saved Adventure appears in every selected World; editing or relocating its file preserves its ID. No external file is changed.
3. Start → Settings changes the chassis color and reduced motion. Both survive restart after a successful save.
4. Browse Worlds with the controller. Additional custom regions scroll inside the bounded grid. Real launch remains unavailable until an adapter is configured and validated on the target device.

See [LIBRARY_AND_LAUNCH.md](LIBRARY_AND_LAUNCH.md) for file-picker controls, validation, failure handling and lifecycle acceptance.

## Trying the sample Worlds preview

Run with `--ephemeral` for these sample scenarios:

1. R1 from Home opens Worlds. D-pad/left stick selects a region; A opens it.
2. Hoenn contains five sample Adventures. Up/Down selects rows and automatically reveals them in the three-row viewport. Down beyond the last row focuses Back to Worlds; Up returns to that row. A opens the selected Adventure.
3. Left/Right chooses an enabled detail action; A activates it. Emerald can simulate continuing the latest resume point or a normal launch. Ruby demonstrates launch without direct resume. Sapphire demonstrates setup being unavailable: the disabled action is skipped, and Back remains accessible.
4. B returns detail → Adventure list → regions, restoring the previous selection. Start/B and L1/R1 away/back preserve the Worlds route and focus.
5. Sinnoh demonstrates an empty region. FireRed demonstrates a known badge count of zero; unavailable values elsewhere use a dash. All status/progress values are sample data, not parsed game saves.

`Emerald: New Trails` is an explicitly fictional ROM-hack record for testing grouping and scrolling. No game content is bundled. Preview launch/resume only displays a mock result; simulated failure is exercised by automated tests. The in-memory preview repository is synchronous. Normal personal data loads asynchronously at startup and exposes committed snapshots to feature controllers.

## Trying Pokédex

1. Open Pokédex with L1/R1. Up/Down browses the list; moving above its first entry enters the upper filter rail. Down from the rail returns to the remembered entry, or the recovery action when no entries match.
2. Left/Right on the rail chooses Search, World, Type, Records, Order or Reset. A opens the keyboard or a choice panel. Move within a choice panel using the D-pad and press A to apply; B or Cancel leaves the previous filter intact. Choices show their currently applied value.
3. Filters combine. Hoenn + Water + Caught finds Mudkip in this sample. The World filter describes a partial reference collection, not the Trainer's encounter history. Types with no sample entries remain valid filters and produce an empty result.
4. Search matches part of a name without case sensitivity, or an exact national number (25 and 025 both find Pikachu). Apply changes the query; B cancels the draft. Start preserves the keyboard key/buffer; a primary-page change cancels the unsubmitted text while keeping the last applied query. Search never edits the Trainer name.
5. A on a list entry opens detail. Left/Right chooses Favorite or Back; A submits the favorite write. Seen/Caught remains read-only and unknown in persistent mode. If removing a favorite removes it from the current result set, focus returns to another matching row or empty-state recovery.
6. Reset clears filters/query and restores number order. Not caught and Not seen include only explicit negative records; unknown progress has its own Not recorded filter. An empty/failed reference load offers Retry field guide. A failed refresh retains the previous reference snapshot.

The reference fixture is intentionally partial, uses original geometric artwork and contains no fetched descriptions or game saves. Favorite marks are separate from the Trainer profile's featured identity choice. Reference loading is synchronous and immediate because it performs no I/O; persistent favorite reads/writes and startup loading use the asynchronous shared store.

## Trying Hall of Fame

1. Open Hall of Fame with L1/R1. Up/Down browses a three-row archive viewport; A opens a memory with team, date, playtime, notes and source. Missing values stay unknown. B returns to the selected row.
2. Up above the first row enters the two-button rail. Left/Right and A choose Archive or RetroAchievements within the same primary page. Down returns to the remembered row; Down beyond the last row reaches the lower action panel.
3. RetroAchievements opens sample Adventure sets. A opens a set, then an achievement; B returns achievement → goals → Adventure sets. Definition IDs may repeat in different sets, and local selection is remembered separately for each set.
4. Achievement detail distinguishes Unlocked, Locked and Not recorded; unlocked samples show Standard/Hardcore and a date only when recorded. The sample archive contains a past Emerald run without changing the Home/current Adventure's progress.
5. In the goals/detail view, choose Refresh records with Left/Right on the lower action panel. While refreshing, Back remains available and disabled Refresh is skipped. Normal mock completion runs on the next Qt event-loop turn; automated tests hold requests to inspect loading and inject failures without adding a fake production delay.
6. A disconnected account or unsupported set still offers Back and the Archive rail. Offline/failed refreshes retain same-context saved records and label them accordingly. Account changes clear old snapshots and ignore responses from the previous context. These states are injected in tests; normal product UI does not expose a debug-state chooser.
7. Refresh archive reloads the fake local repository. Empty data keeps a recovery action focused, and a failed refresh preserves the last successful archive snapshot. Start/menu and L1/R1 restore the underlying detail/focus.

All achievement definitions, game/set identifiers and unlocks are fictional fixtures, not real RetroAchievements sets or a claim of game support. The mock account is not authenticated, and refresh sends no network request. Archive samples are read-only: manual creation/editing, persistent storage and actual provider integration remain later work. Original geometric placeholders are used for teams and achievement artwork.

## Verification

`core` uses Qt Test and SDL virtual joystick button/axis events to exercise mapping, repeat, dead zone, foreground gating and disconnection. It also checks page/focus restoration, Back precedence, mock resume validation and recency order.

`interactions` checks every letter/number, grid boundaries, preferred-column preservation through wide keys, Apply/Cancel, length limits, Unicode deletion, profile validation, create/edit/cancel, failed-write retry, identity/creation-time stability and separation from Adventure progress.

`worlds` checks region grouping, optional progress, layered navigation, per-region selection, newest resume-point selection, per-Adventure capabilities, disabled actions, launch-failure retry and global-layer restoration. Repository refresh tests preserve selection by ID through reordering and reject a removed record before invoking the adapter.

`pokedex` checks combined filters, case-insensitive names and exact/zero-padded numbers, unknown versus explicit negative progress, selection through sorting, picker cancellation, favorite-write retry, removal of the sole filtered record, empty/failed reference loading and separation of the keyboard's search/profile consumers.

`hall` checks archive sorting/identity restoration, partial teams and unknown fields, empty/error recovery, distinct unlock states/modes/dates, per-set selection, loading/retry/cached behavior, invalidation across account changes and rejection of snapshots with a mismatched context or set. An asynchronous refresh and shell-state test verifies that achievements do not change Home/current-save data or local archive records.

`qml_smoke` launches the real compiled QML application. An isolated SDL virtual controller navigates the shell, activates a resume card and system menu, creates/edits a Trainer, enters `ERI 2`, reaches all ten numeric keys and exercises Space/Delete/Clear/Apply. It checks actual `QQuickWindow::activeFocusItem()` restoration through keyboard/menu/notice/form layers and global page changes. QML warnings and empty rendered frames fail the test. It also renders at 960×540, 1920×1080 and a letterboxed 1024×768 window; its `verification.txt` records the result and diagnostics alongside screenshots.

`worlds_qml_smoke` uses the same rendered application and SDL virtual input to browse/scroll Hoenn, open details, simulate launch failure/retry and resume, visit an empty region and return through menu/page changes. It checks the actual active focus item and its outline against clipping ancestors, including the scrolled list. Screenshots at the same three viewport sizes and a separate `verification.txt` are saved under `screenshots/worlds/`. This scenario is compiled when `BUILD_TESTING=ON` and can be invoked with `--worlds-smoke-test` in place of `--smoke-test` below.

`pokedex_qml_smoke` starts with a simulated failed reference load, retries, scrolls to Mudkip, combines World/type/Caught filters, types `25` and `MUD` using actual SDL virtual-controller events, cancels drafts through menu/page transitions, retries a failed favorite write, empties the Favorites result and sorts by name. It checks active focus and clipping, including restoration after cached-reference failure. Screenshots and `verification.txt` are saved under `screenshots/pokedex/`, including 960×540, 1920×1080 and letterboxed layouts. Use `--pokedex-smoke-test` in a testing build to run it independently.

`hall_qml_smoke` uses SDL virtual-controller events to scroll archive/goals, browse team and achievement details, retain focus through system/global navigation, refresh into offline/error/success states, recover from missing cache, switch mock accounts and revisit the local archive. It checks actual active focus and its outline within clipping ancestors, including list rebuilds. Screenshots and a report are saved under `screenshots/hall/`, with 960×540, 1920×1080 and letterboxed captures. Run `--hall-smoke-test` in a testing build to invoke it separately.

`storage` checks actual SQLite reopening, identity preservation, failed-write retry, event-loop responsiveness while SQL waits, safe exit, independent favorites, missing/stale navigation, QObject callback lifetimes, instance locking, unavailable paths and preserving corrupt/foreign/newer files.

`library` checks version-1 migration, reopened metadata/relationships/configuration, stable identities, stale revisions, missing/moved files, asynchronous directory batches and persisted preferences. `process` launches an original content-free child to check checkpoints, normal/nonzero/crash exits, start failure, cancellation, stopping, single restoration and literal arguments.

`persistence_process` starts the rendered app in separate processes with temporary stores. SDL virtual input creates a profile/favorite/search/selection and verifies restart/recovery. Additional phases create and edit a personal Adventure, relocate its file, add a custom World, change all colors/motion, reopen, and check actual focus and clipping. A test-only adapter hides/restores the QML window around child success, start failure and crash, including a held-button handoff. Screenshots and reports are under `screenshots/persistence/` and `screenshots/library/`.

`diagnostics` checks live SDL signals versus keyboard/semantic actions, foreground/neutral gating, unmapped devices, disconnection, report contents, duplicate suppression and write failure/retry/teardown. `diagnostics_qml_smoke` operates the real diagnostic service through SDL input and checks active focus, global navigation, observed marks, stick/trigger display, report export, reset and three viewport sizes. Captures and reports are under `screenshots/diagnostics/`. The normal app contains the diagnostic service; only this automated scenario is gated by `BUILD_TESTING`.

The suite contains fifteen CTest entries; the Windows build passes all fifteen. A `-o report.txt,txt` option on a Qt Test executable saves detailed diagnostics if its Windows console output is unavailable.

CTest sets `QT_QPA_PLATFORM=offscreen` and `QT_QUICK_BACKEND=software`; it does not need a desktop. It saves rendered images in the build directory's `screenshots/` folder. To choose a different output directory manually:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/native/traineros --smoke-test --screenshot-dir build/screenshots
```

PowerShell equivalent:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QUICK_BACKEND = 'software'
./build/native/traineros.exe --smoke-test --screenshot-dir build/screenshots
Remove-Item Env:QT_QPA_PLATFORM, Env:QT_QUICK_BACKEND
```

The Linux CI workflow runs the same build and tests when invoked on GitHub. Local Windows results do not establish ARM64 support, real GPU performance, physical controller mapping, or safe ArmadaOS session behavior. Those acceptance gates remain open until tested in their actual environments.

## Next increments

1. Validate the native build on Linux and the actual Flip 2; finish shared controller/focus behavior against real input.
2. Use the persistent library and process foundation to complete the first real Adventure adapter according to `ROADMAP.md`, once the target environment is known.
3. Introduce real Adventure/providers and session switching only after their prerequisites pass. RetroAchievements belongs within Hall of Fame.
