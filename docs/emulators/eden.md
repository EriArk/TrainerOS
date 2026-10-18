# Eden — ordinary homebrew launch

Owner added Switch ordinary launch on 18 October 2026, tested on Odin with
homebrew. Multiplayer and commercial-game compatibility remain outside this check.

- Official [Eden 0.2.1 release](https://git.eden-emu.dev/eden-emu/eden/releases/tag/v0.2.1),
  source `58c1e20ee5`, Linux ARM64 Clang PGO AppImage, 63,437,527 bytes.
  SHA-256 `b64f926cbf74fd870a39b144971084323d895d51919b91e906328e7f81bea087`.
- Discover `eden` on PATH or `~/Applications/eden.AppImage`. Common standalone
  launch uses literal `-f -g FILE`; `.nro`, `.nsp`, `.xci` are recognized formats,
  not a claim that encrypted content or its prerequisites are supplied.
- Fresh configuration seeds the connected SDL pad's actual button/hat/axis
  bindings, including Nintendo face layout, sticks and triggers. Eden's GUID
  convention clears SDL's name CRC. Existing configuration is never overwritten.
  Controller replacement/remapping remains separate acceptance.
- Controller bridge and the ownership-checked Home helper reuse normal process,
  capture and explicit Exit handling. No persistent savestate timeline is added.
- Implementation is in the shared discovery/standalone adapter, composition
  root and `overlay_support.py`; no upstream emulator patch is applied.

Evidence: [LAUNCH-80](../LAUNCH80_LAUNCHER_AUDIT.md#odin-homebrew--18-октября). Test content is
[2048 v1.1.0](https://github.com/BernardoGiordano/2048/releases/tag/1.1.0).
On Odin, actual board input, Home/Continue, `+` save, relaunch with the same board
and score, and confirmed Home Exit passed. No keys/firmware were added.

## Reviewed Odin configuration and remaining gaps

Run `python3 packaging/emulators/eden/configure-odin.py` as the handheld user,
with the sibling `odin.env`, only while Eden is closed. It checks the exact package
hash and Turnip ICD, backs up changed files and atomically configures Dynarmic,
Ask_Never for Eden's duplicate stop confirmation, and the AppImage environment.
`VK_LOADER_DRIVERS_SELECT=*freedreno*` selected Turnip Adreno 740 instead of
llvmpipe; `DISABLE_AUTO_UPDATES=1` prevents package-managed update dialogs.
This explicit maintenance step is not universal first-run provisioning.

The Qt frontend still asks about missing decryption keys even for this keyless
homebrew; dismissing the prompt allows play. Quitting inside 2048 returns to the
Eden library rather than automatically returning to TrainerOS. Home Exit closes
the emulator with one TrainerOS confirmation. These first two UX gaps remain open.
The included `eden-cli` frontend was tested and rejected: immediate SIGABRT,
`basic_string: construction from null is not valid`. The working Qt route was
restored. The AppImage wrapper can return zero after a child crash, so a recorded
`returned` alone is not success evidence; accurate crash propagation remains open.

Rollback: while closed, restore the matching `.before-traineros-<hash>` config/env
backups and previous AppImage/integration. Never replace NAND/SDMC saves to roll
back code. No upstream patch or encrypted-content compatibility is claimed.

Contracts at the selected tag:
[CLI and window close](https://git.eden-emu.dev/eden-emu/eden/src/tag/v0.2.1/src/yuzu/main_window.cpp),
[SDL inputs](https://git.eden-emu.dev/eden-emu/eden/src/tag/v0.2.1/src/input_common/drivers/sdl_driver.cpp).
On update, revalidate these contracts, the runtime hash, GPU startup, input and
ordinary save/exit cycle. Retain the old binary and config backup; roll back the
selected AppImage plus TrainerOS integration without replacing user NAND/SDMC data.
