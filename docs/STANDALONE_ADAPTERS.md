# Standalone Adventure launch adapters

`AdapterRouter` dispatches capabilities, launch, resume and file attachment to the Adventure's adapter. It preserves unknown custom adapters and the existing RetroArch saved-moment path. Home, Worlds, play history and process return use the same interfaces for each integration.

The standalone module currently provides melonDS (DS) and Dolphin (GameCube/Wii) launch profiles. It does not imply universal game compatibility or direct resume. The installed melonDS 1.1 CLI exposes fullscreen launch but no initial save-state argument; the installed Dolphin 2606a CLI exposes batch launch and an initial save-state option. Only launch is enabled in this increment. State discovery, provenance and safe exact resume require a separate provider.

## Explicit installation profiles

Normal mode reads `integrations/melonds.json` and `integrations/dolphin.json` under TrainerOS's local data directory. Missing or invalid profiles leave the relevant records unavailable. Profiles must name existing absolute executables, bounded literal prefix arguments and explicitly validated platforms. Library attachment selects a matching installed profile; changing a file never guesses whether an ISO belongs to GameCube or Wii.

Example structure for a locally validated melonDS installation (replace the placeholder paths):

```json
{
  "version": 1,
  "adapter": "melonds",
  "program": "/usr/bin/python3",
  "runtimeFile": "/path/to/melonDS.AppImage",
  "prefixArguments": ["/path/to/controller-bridge.py", "--pointer", "--", "/path/to/melonDS.AppImage"],
  "validatedPlatforms": ["nds"]
}
```

The adapter adds `-f` and the exact referenced file as separate arguments. Dolphin adds batch/fullscreen/execute arguments to its configured executable or Flatpak prefix. Supported file extensions are DS `.nds`, GameCube `.iso/.gcm/.rvz`, and Wii `.iso/.rvz/.wbfs/.wad`. No platform is enabled merely because a configuration filename exists; its explicit profile must validate. Missing media or changed executables fail on a worker before launching and return to the preserved TrainerOS context.

The controller bridge is installed under `libexec/traineros` by CMake. It supplies Start+Select as a normal X11 close request, plus optional right-stick pointer / R2 left-button control for DS. It checks the active window's host process ancestry through XRes 1.2 before interacting: Flatpak's `_NET_WM_PID` is namespace-local and cannot prove ownership. It initializes its dependencies before starting the emulator and never kills a running game. It requires Python 3, SDL2, X11, Xtst and XRes (including server extension version 1.2) on the host. Gamescope does not advertise `_NET_CLOSE_WINDOW`, so the bridge sends `WM_DELETE_WINDOW` directly when the owned client supports it; EWMH is a fallback only when advertised. Gamescope/Armada display setup remains in the session layer. Touch-heavy gameplay still needs physical comfort testing.

## Acceptance and current boundaries

- Native tests exercise explicit profile validation, routing and preservation of other adapters, literal filenames containing shell characters, checkpoint/launch/return through an actual content-free child, and missing-file recovery without altering the source.
- A Linux subprocess test verifies that missing controller-bridge display/dependencies prevent an uncontrolled game launch.
- The 2026-09-13 Flip session check enabled explicit profiles for 36 existing DS/GameCube/Wii records. Diamond and Colosseum launched from Worlds, rendered their introduction/title screens and responded to controller input. PokéPark Wii reached its save-slot menu using the installed sideways controller profile. Normal controller exit returned to TrainerOS. These checks prove launch/input/return, not complete gameplay compatibility. PokéPark 2 produced a black screen in the initial check; a successful process start alone is not a playable-game claim.
- The DS save resolver recognizes both native `melonDS` and the truncated `melonDS.AppImage` process name, preventing backup/restore while either is live. Defunct AppImage helpers do not block a closed Adventure. Save support still requires the separate explicit profile described in the backup documentation.
- 3DS and Wii U adapters, additional exact-resume providers and expanded backup coverage remain separate follow-up work.
