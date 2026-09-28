# Installed emulator preparation — 2026-09-28

TrainerOS now prepares the existing launch adapters automatically at shell startup.
An ordinary game remains **platform folder → wheel → A → game**. No screen or
per-title question was added. This is native development delivery, not a completed
Armada image, firmware installer or OTA updater.

## Delivered behavior

`platform/emulation/EmulatorDiscovery` inventories native PATH executables,
known `~/Applications/<binary>.AppImage` files and installed Flatpak applications.
The bounded local Flatpak queries share a two-second deadline, use no network
and never run from UI capability getters. Explicit installation descriptions
remain authoritative, including custom wrappers and Odin's ARMSX2 setup. A broken
explicit description reports its missing capability rather than silently changing
the emulator or save namespace.

| Adapter | Automatic candidates | Scope |
|---|---|---|
| PPSSPP | PPSSPPSDL/ppsspp; org.ppsspp.PPSSPP | PSP standalone preference |
| ARMSX2 | ARMSX2/armsx2; existing explicit wrapper wins | PS2; existing Odin setup retained |
| melonDS | melonDS/AppImage; net.kuribo64.melonDS | Ordinary DS, existing optional pointer bridge reused |
| Dolphin | dolphin-emu; org.DolphinEmu.dolphin-emu | Existing GC/Wii adapter; Wii controls remain separate |
| RetroArch | retroarch; org.libretro.RetroArch | Existing platform/format registry and installed cores |

Newly discovered routes are retained separately under
`integrations/discovered/<adapter>.json`. Existing `integrations/<adapter>.json`
always wins. Repeated startup does not replace these files, emulator preferences,
ROMs, saves or database identities. New games prefer supported standalones before
RetroArch; existing explicit game routes do not migrate.

For a declared Flatpak launch, a runtime pointing inside that same application's
old deployment is resolved to its current executable in the startup snapshot.
Wrappers, arguments, configuration/save locations and source JSON bytes stay
unchanged. A stable `active` link is preferred when it resolves to the returned
deployment. The adapters and save/progress providers receive the same snapshot;
there is no partial live swap while a game or save transaction is running.

Fresh configuration files are seeded only if absent:

- RetroArch: SDL autodetection, fullscreen, ordinary-save defaults, a save
  directory and the sibling Batocera `bios` directory. Existing configs and core
  paths are read as-is. Native packaged core directories are considered only
  when the default user core directory is empty.
- melonDS: actual SDL controller button/hat bindings translated to its TOML
  format, direct boot and landscape screens. Existing pointer/exit bridge is
  reused if installed. Its presence is not proof of universal Home exit support.
- Dolphin: initial GameCube SDL mapping for the connected controller. Existing
  mappings are never replaced. No generic Wii motion mapping is fabricated.
- PPSSPP keeps upstream SDL defaults and existing controls. ARMSX2 keeps its
  existing emulator setup; no PS2 firmware/memory-card/controller rewrite.

If a pad was absent, the missing initial DS/GC mapping can be seeded at a later
startup with the controller connected. Notices are diagnostic logs, not new UI.
The image still needs to ship emulators, core/autoconfig packages and the supported
input/exit helper. This discovery layer does not download or update packages.

## Verification

- Native Windows and ARM64 Linux production builds.
- Windows `standalone`, `retroarch`, `library` and `qml_smoke` targets passed.
  The Linux-filename Flatpak case is intentionally skipped on Windows and passed
  in the ARM64 Linux standalone suite: 15 cases, no failures/skips.
- Regression coverage includes fresh preparation, repeated preparation preserving
  user changes, custom ARMSX2 preservation, malformed explicit profiles refusing
  fallback, Flatpak deployment replacement retaining all other fields, one-A
  preparation/launch, folder-based GBA routing and existing launch/return tests.
- Actual Flip: isolated copied library with manual emulator descriptions removed
  discovered RetroArch, PPSSPP, melonDS and Dolphin. Existing emulator config
  hashes matched before/after preparation. One physical-position A event on the
  DS wheel launched Kirby Super Star Ultra through the discovered AppImage and
  existing pointer bridge. Title/path/identity stayed intact; only the internal
  adapter binding advanced. The actual title screen was captured.
- This DS probe did **not** establish guarded Home exit: virtual and raw Guide
  did not open that overlay. The owned process was stopped at its title screen,
  then the normal session resumed. Do not mark universal standalone exit delivered.
- Odin was unavailable over SSH during this increment. Its existing explicit
  ARMSX2 preservation is covered by the regression, not a fresh device claim.

Installed Flip production SHA-256:
`7f397c78f9a9a94e48e151309df89e317260270b10bf010d317675f987759ef5`.
Deployment preserved 3 Trainers and 830 registrations, with SQLite quick_check
passing and a binary/database rollback copy retained privately on the device.

## Remaining acceptance

Follow-up: [guarded standalone Home exit](STANDALONE_HOME_EXIT.md) closes the
representative DS/GameCube exit gap and installs session helpers reproducibly.
Its proof supersedes the earlier failed DS probe above; universal coverage,
Wii proof and image provisioning are still not claimed.

Live idle-safe refresh, package/image installation, real upstream-update rollback,
firmware inventory/preparation, first-run storage changes affecting default BIOS
locations, nonstandard/legacy emulator config locations, restrictive Flatpak
filesystem overrides, disconnected/reordered controllers, Wii mappings, and
uniform guarded standalone exit remain open. Do not call a detected executable
launch-tested, firmware-ready or save-verified. Existing protected mGBA and disc
firmware checks still apply; autodetection grants no semantic write capability.

Next work stays in the startup/library lane: prepare image-time runtime/input/
exit prerequisites and close the representative launch/return gaps, then finish
first-run Settings, system/recovery and active-Trainer RetroAchievements earning.
All deferred roadmap acceptance remains.

## Sources used

- [Flatpak command reference](https://docs.flatpak.org/en/latest/flatpak-command-reference.html):
  installed application locations and local `info --show-location` queries.
- [melonDS input implementation](https://github.com/melonDS-emu/melonDS/blob/master/src/frontend/qt_sdl/EmuInstanceInput.cpp):
  raw joystick button/hat encoding, checked against the installed Flip bindings.
- [PPSSPP SDL controller implementation](https://github.com/hrydgard/ppsspp/blob/master/SDL/SDLJoystick.cpp):
  upstream controller handling; existing controls are kept instead of overwritten.
