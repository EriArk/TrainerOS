# xemu — original Xbox ordinary launch

## 18 October 2026 delivery

Original Xbox is catalogue ID `xbox`, ISO content, standalone adapter `xemu`.
TrainerOS discovers an existing native/AppImage or `app.xemu.xemu` installation
and uses the [upstream CLI](https://xemu.app/docs/cli/):
`-full-screen -dvd_path CONTENT`. The common controller bridge and owned-process
Home/exit integration are registered at composition, not in a per-game branch.
The ordinary UI does not ask the player to edit a binding.

Changed files: `data/rom-platforms.json`, `EmulatorDiscovery.cpp`,
`StandaloneAdapter.cpp`, `Main.cpp`, and `overlay_support.py`; standalone and
owned-process tests cover the Xbox command. Existing firmware/HDD/EEPROM and
controller settings are retained. No emulator source patch or firmware redistribution.

Observed Odin `xemu.AppImage` SHA-256:
`3f87b43fc6c080a7106faf44a1d9afd1871fb8ab7d39c7cfb7b6e8f831b6bf32`.
This identifies the tested existing package; it is not a newly selected upstream
version or a reproducible distribution pin. Package version/rebuild acceptance
remains for the emulator standard.

**Device check:** Games → Mojo! → A opened the game; right changed Single Player
to Cooperative. Home/Exit closed it before the next sample. The existing input
configuration worked and was not replaced. No multiplayer, full game, audio,
save persistence or physical-controller acceptance is claimed. Xbox was tested
on Odin; the common TrainerOS binary was also delivered to Flip.

Update: retain per-user firmware/EEPROM/HDD identity and use the same ordinary
launch/input check after changing xemu. Rollback the shell/helper together using
the pre-LAUNCH-80 backup; retain the independent emulator and personal data.
Never infer a working Xbox installation merely from the new catalogue entry.

[Whole sweep and installed shell identity](../LAUNCH80_LAUNCHER_AUDIT.md#общий-проход-до-xbox-original--wii-u).
