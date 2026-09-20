# Controller and display checks

Start → Settings → Controller opens **Check your field gear**, an attached service panel. It is preparation for the ArmadaOS baseline, not a calibration wizard or evidence that the device is supported.

## Reading the panel

- Button caps light while SDL reports them held; a small dot means a signal has been observed since Reset checks. Compare the displayed names with the actual printed controls yourself. A signal does not prove the mapping is correct.
- Both stick wells show SDL-mapped positions before TrainerOS dead zones. L2/R2 show trigger levels. The exported report also contains observed axis minima/maxima; those ranges do not automatically declare drift, calibration or full travel correct.
- Last action shows what the central TrainerOS input service actually delivered from a controller. Keyboard events never satisfy these observations. The list in the report is bounded to the latest 24 actions, including directional repeats.
- The input gate distinguishes ready, inactive and waiting for neutral. After foreground return, held controls may light while the shell correctly waits for release and a centered left stick.
- Device detection without an SDL controller mapping is a separate state from no device. The current source still uses one mapped controller; remapping and choosing among multiple controllers remain future work.

The panel keeps the normal controls: Left/Right selects its lower action rail, A activates, B returns to the system menu, Start overlays it and L1/R1 switch primary pages. Up/Down can be tested without moving the rail selection. After a global action, reopen Start → Settings → Controller to inspect the observed marks. A on the initial Refresh display button is harmless. Observations continue in memory after first opening the panel, including while it is hidden. A different controller mapping resets button/range history; disconnect retains the last device descriptor and observations with an explicit disconnected state. Nothing is recorded to disk automatically.

## Display readings

Refresh display captures Qt's current window/screen coordinates, device pixel ratio and reported refresh rate, plus OS/CPU, Qt/SDL versions and the Qt platform backend. The report names screen data `qtReportedScreen`: its width/height are Qt coordinates and are not independent measurements of panel resolution or scanout mode. Use the device baseline report and physical observation to establish the active mode. QPA backend names do not establish which compositor/session arrangement ArmadaOS uses.

Window size, visibility and screen-change notifications also refresh these readings automatically, including when moving to another screen or changing its reported geometry/DPI/refresh rate.

The panel fits the existing 960×540 logical layout, scales at 1920×1080 and letterboxes other aspect ratios. This says nothing about readability or performance on the actual 5.5-inch display until checked there.

## Local report

Save report writes a timestamped, uniquely named `device-check-*.json` in the `diagnostics/` subfolder of the application data directory. When running with `--data-dir /path/to/test-data`, the reports are under `/path/to/test-data/diagnostics/`. The explicit save action also works in the ephemeral preview; it does not open a personal database. Existing reports are retained.

Writing runs on a worker with QSaveFile's atomic commit and no direct-write fallback. Duplicate requests while saving are ignored; failures offer the same Save report action for retry. A completion message remains available if the panel has been left. No upload occurs. The report includes only selected runtime/controller observations: no Trainer profile, library, ROM/save content, file paths from the library, host/user identity, serial numbers or environment-variable dump.

Normal application teardown drains an already requested report before stopping its worker. It does not promise completion after forced termination or storage loss.

Use Reset checks before a fresh controller trial. There is no automatic hardware pass/fail verdict, no remapping, calibration write, session switch or OS setting change. The JSON is an observation artifact, separate from TrainerOS personal persistence.

## First-device sequence

1. Collect `tools/collect-device-baseline.sh` as described in `FIRST_DEVICE_RUN.md`, then run TrainerOS in the existing desktop with an isolated data directory.
2. Open Controller and reset checks. Compare every button name, move both sticks fully and release them, and exercise both triggers. Verify normal L1/R1, Start and Back behavior; reopen the panel as needed.
3. Leave and return to the application through the existing desktop. Check the ready/neutral gate and confirm a held Adventure button does not trigger a shell action on return.
4. Refresh display, save the report, and record physical observations using the table below. Repeat after window/fullscreen and controller-mode changes if relevant.
5. Keep the JSON, baseline text and observations together. Do not mark the ArmadaOS baseline complete until the native application and its physical controls have actually passed.

| Check | Observation to record on the handheld |
| --- | --- |
| Physical labels | A/B/X/Y, shoulders, Start/Select match the intended actions |
| Sticks/triggers | Direction, full travel, center stability and neutral return |
| Focus | Visible and stable through lists, services, keyboard and page changes |
| Display | Actual active mode, scaling, readable text and motion on the physical screen |
| Foreground/wake | App return, suspend/wake and hinge behavior using existing OS controls |
| Environment | Actual ArmadaOS build, desktop/session path and installed emulator launch conventions |

## Verification and current limit

`diagnostics` exercises SDL virtual-button/axis input, separation from keyboard events, foreground/neutral gating, unmapped devices, disconnect retention and atomic report success/failure/retry. `diagnostics_qml_smoke` exercises the real QML panel through SDL input, checks actual focus, global controls, live sticks/buttons, report export/reset and three viewport sizes. It writes captures under `build/native/screenshots/diagnostics/`.

These desktop checks prepare the next roadmap gate. Real emulator launch conventions, physical input mapping, graphics performance, wake/hinge and safe session switching require the handheld. Do not implement guessed ArmadaOS paths or advance automatic progress providers to bypass that dependency.
