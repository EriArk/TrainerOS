# First ArmadaOS device run

This procedure uses a normal application in the existing desktop session. It does not install a TrainerOS session, change the default login, or remove Steam Gaming Mode or KDE Plasma. A Windows executable cannot run on the ARM64 handheld.

## Before building

1. Enter the existing Desktop / Maintenance Mode and identify how to return to the current gaming mode.
2. From the repository root, run `sh tools/collect-device-baseline.sh > device-baseline.txt`. The report is read-only: system base, session metadata, session-entry names, display modes, input names and dependency/command availability. Keep it outside Git; it is not device-validation evidence until actually collected on the handheld.
3. Compare the report with `ARMADA_PLATFORM.md`. Record the actual ArmadaOS version/base, session/compositor and controller mode. Missing `pkg-config` metadata or commands do not prove an emulator/runtime is absent; it may be packaged differently.
4. Choose the native Qt/SDL build or packaging method for that actual base. The Ubuntu CI package list in `DEVELOPMENT.md` is not a prescription for ArmadaOS. Do not install packages until this is established.

## Run in isolation

After building against the device's toolchain/runtime using `DEVELOPMENT.md`:

```sh
ctest --test-dir build/native --output-on-failure
./build/native/traineros --windowed --data-dir "$HOME/.local/share/TrainerOS-device-check"
./build/native/traineros --data-dir "$HOME/.local/share/TrainerOS-device-check"
```

Use the same explicit data directory for both runs. It keeps the first device check separate from normal application data. The directory contains only TrainerOS-owned metadata. Adventure management stores references to user-selected files and checks their filesystem metadata; it does not read game progress or modify their contents. `--ephemeral` instead runs the original in-memory sample fixture without touching a store.

## Acceptance record

Record observed results, not assumptions:

- **Display:** measured active resolution, scaling, orientation and refresh rate. Advertised connector modes are only possibilities. Verify readable text, bounded lists and the keyboard's separate number block on the physical screen.
- **Controller:** every D-pad direction, left-stick direction, A/B/Y, Start, L1/R1; verify physical labels against semantic actions. Hold/repeat, disconnect/reconnect and neutral controls after returning to the app.
- **Diagnostics:** Start → Controller → Reset checks. Observe both sticks, triggers and button caps; verify foreground/neutral behavior and Refresh display, then Save report. JSON files are in the test data directory's `diagnostics/` folder. Use [DEVICE_DIAGNOSTICS.md](DEVICE_DIAGNOSTICS.md) to distinguish software observations from physical validation.
- **Focus:** traverse every primary page, return from detail/pickers/keyboard/system menu, and verify the visible focused control. No touch or mouse should be needed.
- **Library:** Start → Manage Adventures → Add Adventure. Use an original test file for the baseline; choose it with the controller file picker, enter a title, select primary/additional Worlds and save. Edit the record, cancel a draft, and add a custom World. Verify bounded scrolling and recovery from empty folders/missing files.
- **Persistence:** create a Trainer entirely with the controller, mark a Pokédex favorite, apply search/filters, choose the saved Adventure and change theme/motion in Settings. Exit through Start → Exit Development App and relaunch with the same data directory. Verify library, profile, favorite, preferences, page and selection; no unfinished form/keyboard/menu should reopen.
- **Recovery:** while the app is open, start another copy using the same test data directory. It must show recovery controls. Exit the second copy with B; the first must keep working. After closing the first, reopening must succeed.
- **Foreground / wake:** leave/re-enter the app through the existing desktop, then test suspend/wake and the hinge using existing OS controls. Record input/focus/rendering behavior. Do not infer dedicated-session readiness from this test.
- **Performance:** record visible motion stutter, input delays, startup and resume behavior. Desktop/offscreen tests do not establish handheld performance.
- **Exit:** both the explicit system action and a desktop-window close must return to the existing environment after pending writes finish.

Normal Worlds uses the personal library; Home/Continue and game progress remain empty or unknown until real history/providers exist. Archive records and achievement sets still use labeled samples. Real launch/resume remains unavailable; the original child-process probe is automated test infrastructure only. This visit establishes the platform baseline for the first real Adventure adapter, not emulator or RetroAchievements support.

If something fails, retain the report, exact command, console output and reproduction steps. Do not change the default session to work around an application problem. Update `ARMADA_PLATFORM.md` only with findings actually observed on the device.
