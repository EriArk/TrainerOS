# DuckStation — ordinary standalone launch

## LAUNCH-80 integration, 18 October 2026

TrainerOS reuses the installed DuckStation package without emulator patches.
The common standalone adapter launches CUE/CHD/PBP/M3U/EXE with literal argv
`-batch -fullscreen -- CONTENT`, following the
[upstream CLI](https://github.com/stenzek/duckstation/wiki/Command-Line-Arguments).
Discovery covers native launchers, the usual DuckStation AppImage name and
`org.duckstation.DuckStation`. Explicit profiles retain priority. Existing PS1
RetroArch bindings are not switched; memory cards and user settings stay in
their original locations. No new emulator revision is shipped by this change.

The shared controller bridge and process-owned Home/exit helper are reused.
The helper admits only game-launch flags, not a plain desktop window; normal
window close remains the emulator's graceful shutdown path. Whether the selected
device package asks for confirmation must be checked in the real launch/exit
cycle. Input, game-options capabilities and save acceptance are not inferred
from CLI support. This integration does not provide DuckStation netplay.

After a package update, verify the CLI and runtime executable identity, run the
standalone and overlay regressions, then check launch/Home/return/close using
existing card storage. Rollback restores the previous TrainerOS binary/helper;
do not delete profiles or move cards. No source patch needs rebasing.

Source implementation is not yet an installed delivery. Full evidence and
remaining checks: [LAUNCH-80 audit](../LAUNCH80_LAUNCHER_AUDIT.md).
