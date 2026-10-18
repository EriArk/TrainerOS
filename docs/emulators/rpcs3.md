# RPCS3 — ordinary homebrew launch

Owner added PS3 ordinary launch on 18 October 2026, tested on Odin with homebrew.
Multiplayer remains deferred. This is not commercial-game compatibility.

- Reuse Odin's existing Armada `rpcs3` launcher and AppImage; firmware, games,
  accounts and saves stay in their existing locations.
- Installed AppImage SHA-256:
  `f1ecd9af6073eb3643c4e10e5befff25bdcd4d2da53aec69547da5fa7f8ae1d2`.
- Observed version: `0.0.43-20291-ffba2bf0 Alpha | master`, ARM64, Qt 6.11.1;
  Vulkan Turnip Adreno 740 / Mesa 26.2.4 on Odin.
- Common `StandaloneAdapter` uses `--no-gui --fullscreen FILE` for `.self`/`.elf`.
  No package installation, directory-game or encrypted-image support is implied.
- Discovery creates a separate `input_configs/global/TrainerOS.yml` for the
  connected SDL pad and selects it with `--input-config TrainerOS`; the previous
  global/Steam mapping is retained. Existing named profiles are never overwritten.
- Controller bridge and ownership-checked Home helper share the existing live
  process, capture and explicit Exit flow. A desktop RPCS3 window without the
  managed launch flags is not admitted by this helper rule.
- Source changes: `src/platform/emulation/EmulatorDiscovery.*`,
  `src/integrations/adventure/standalone/StandaloneAdapter.cpp`, composition in
  `src/app/Main.cpp`, `packaging/integrations/overlay_support.py`.

Evidence is maintained in [LAUNCH-80](../LAUNCH80_LAUNCHER_AUDIT.md).
On Odin, Ping Pong reached a match against a bot, accepted paddle input, and
passed Home/Continue and confirmed Exit. A second cached launch and single
TrainerOS Exit confirmation also passed. Save persistence is not verified for
this sample; physical owner input/audio and commercial compatibility remain open.
Test content: [Ping Pong v1.0.0](https://github.com/utkuhalis/ps3-homebrew/releases/tag/v1.0.0),
with SHA-256 recorded there. No binary or firmware is bundled in Git.

Fresh discovery seeds Vulkan, `Async Recompiler (multi-threaded)`, no welcome
dialog and no duplicate emulator Exit prompt. Existing preferences stay intact.
For the reviewed existing Armada config, run
`python3 packaging/emulators/rpcs3/configure-existing.py` while RPCS3 is closed.
It backs up changed files, switches only the known shader-interpreter preset and
sets the two window flags. On Odin the interpreter preset stalled compiling 6650
shader variants; the ordinary async preset reached gameplay after initial PPU
compilation. Restore matching `.before-traineros-<hash>` files while closed to
roll back settings; firmware, saves and the owner's global pad profile remain intact.

Upstream contracts: [RPCS3 command-line parser](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/rpcs3.cpp),
[SDL input](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Input/sdl_pad_handler.cpp).
No upstream patch is applied. On update, recheck CLI, SDL profile naming and
normal window-close behavior; validate homebrew before updating the selected
runtime. Roll back the TrainerOS binary/helper and discovered integration together;
keep emulator data and firmware. The separate input profile can be deselected
without replacing the owner's global profile.
