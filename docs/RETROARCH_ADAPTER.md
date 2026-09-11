# First real Adventure adapter

Controller file attachment now prepares an installed matching core automatically for GB/GBC (Gambatte), GBA (mGBA), N64 (ParaLLEl N64) and Pokémon mini. A mismatched file or missing core leaves the Adventure unconfigured. Other adapters' configuration is preserved. Catalogue entries alone never enable launch; see [collection flow](COLLECTION_CATALOGUE.md).

Personal library records can now launch a configured RetroArch installation. The first Flip 2 verification used the ARM64 Flatpak and mGBA to open Pokémon Ruby from Worlds → Hoenn, enter the emulator menu with a controller chord, quit, and return to the same Adventure detail and focused action. This remains normal full-screen application mode in Plasma; Steam and Plasma are retained.

## Installation configuration

Create `integrations/retroarch.json` under the TrainerOS data directory (`$XDG_DATA_HOME/TrainerOS/TrainerOS`, normally `~/.local/share/TrainerOS/TrainerOS`). This is maintenance configuration, separate from the World/Adventure display model. An example for the system Flatpak:

```json
{
  "version": 1,
  "program": "/usr/bin/flatpak",
  "prefixArguments": ["run", "org.libretro.RetroArch"],
  "configFile": "/home/your-user/.var/app/org.libretro.RetroArch/config/retroarch/retroarch.cfg",
  "coresDirectory": "/home/your-user/.var/app/org.libretro.RetroArch/config/retroarch/cores"
}
```

Use real absolute paths; this file does not expand `~` or environment variables. A native installation can use its absolute RetroArch executable and an empty `prefixArguments` array. The executable must remain attached to the emulator lifetime; launchers that detach into another application are outside this adapter's contract. The installation file is read once at startup, limited to 64 KiB and 32 prefix arguments, and validated without running anything.

A registered Adventure needs adapter ID `retroarch` and an integration object such as `{"core":"mgba"}`. The current explicit combinations are:

| Core | Content extensions |
| --- | --- |
| `mgba` | `.gba` |
| `gambatte` | `.gb`, `.gbc` |
| `parallel_n64` | `.z64`, `.n64`, `.v64` |

Only available core files enable launch. Format support is not a claim that every title, ROM hack or accessory works. Missing/invalid setup keeps the library record visible with Needs setup. New entries added through Manage Adventures still start unconfigured; this increment does not add a controller setup wizard or guess core assignments.

## Launch and return

The adapter reads committed repository metadata and supplies a `ProcessCommand` to the application's existing launch coordinator. Executable and arguments remain separate throughout; content filenames never become shell commands. Capability queries do not inspect removable media or parse ROMs.

Before launch, the coordinator commits navigation to SQLite. It blocks duplicate launches and shell input while the Adventure owns presentation, hides the window once the child starts, and restores the saved page, detail, focus and window mode on completion. A failed checkpoint prevents launch. Start errors, crashes and nonzero exits restore the shell with an error notice. OS termination is preserved: SDL must not convert SIGTERM into an unconsumed SDL event and leave a systemd stop waiting for its kill timeout.

The emulator owns content loading and saving. A missing content file in the tested RetroArch installation exits with status 1. The shell does not create a game save, overwrite a state, infer progress, or promise direct resume. Exit the emulator to return; closing only its content can leave its menu running. The device configuration uses **L3+R3 for the RetroArch menu** and **Start+Select to quit RetroArch**, verified through InputPlumber-generated gamepad chords. Save in the Adventure before quitting.

## Verification and limits

- All 16 CTest entries pass on the ARM64 Fedora build environment, including actual child argv preservation, SQLite checkpointing, duplicate-launch rejection, missing setup, retained metadata with removed content, and POSIX shell termination.
- Existing rendered SDL scenarios cover success, launch failure, crash, focus restoration and neutral-input handoff with an original content-free child.
- On the ArmadaOS host, Ruby launched twice through the personal Worlds library. The second run opened the RetroArch menu and exited with gamepad chords; the same detail/action reappeared, Back worked, and stopping the shell completed promptly. Captures were inspected at 1920×1080.
- Separate startup checks rendered Ruby and Stadium in RetroArch, Diamond in melonDS, Colosseum in Dolphin and Rumble U in Cemu. The latter three are installed device applications, **not additional TrainerOS adapters**.
- InputPlumber's current `SendEvent` method panics in its async runtime; `SendButtonChord` works. These checks use the actual OS/SDL input path with injected events. Physical button labels, analog comfort, long gameplay and dedicated-session recovery still need hands-on validation. No sleep tests were performed.

Identified launch events now feed [Home history](HOME_AND_HISTORY.md), independently of save files. Resume-point enumeration, save parsing, achievements and additional adapters remain subsequent module work. No game content, BIOS, device credentials or personal library manifest belongs in the repository.
