# Local PC runtime binding

`launch.py` belongs at `/var/opt/traineros/pc/launch.py`, readable by the ordinary
session user. It is launched through the existing `ProcessService` and
`AdventureLaunchController`, never through a shell command string or Steam.
The Linux subreaper waits for adopted children after launcher exit; cancellation
signals only this session's descendants. There is no global `wineserver -k`.

PC registrations use `adapterId=pc-local`, `platformId=pc`, `domain=multiverse`
and stable `pc:<UUID>` IDs. This is an execution binding, not an Experience adapter.
`contentPath` is the selected executable. `integrationConfig` holds `runner`,
`installRoot`, `workingDirectory`, the owner-specific `prefix`, pinned `proton`
and `umu` paths, native `dataRoot`, and `ownership=copied`. Native games receive
owner/game-specific XDG config, data and cache paths; programs ignoring XDG may
still write beside their executable or under HOME. Files, prefixes and personal progress
are not deleted when importing another game. No ROM extension registry is used.

Current import accepts self-contained game **folders**, copied using libfm-qt
through the shared Files task service. Symlinks and special files are rejected;
there is no implicit link-only import or automatic permission change. ELF
architecture and Windows PE signatures are inspected before confirmation.
Archives, installer execution, executable repair, runtime selection/download UI,
Steam shortcuts and input editing remain separate pending work in #247–250.

## Windows runtime prerequisite

The checked launcher is upstream **UMU 1.4.4**, the official zipapp release:

- https://github.com/Open-Wine-Components/umu-launcher/releases/tag/1.4.4
- Asset `umu-launcher-1.4.4-zipapp.tar`
- SHA-256 `eb590691841f7fad3fc3ad8fd5db4ccb87849fe7948e62b28ece7a4ee48cc851`
- Install the archive's `umu/umu-run` and `umu/umu_run.py` together in
  `<state>/runtimes/umu/1.4.4/`. Keep upstream licence notices with the runtime;
  upstream code/binaries are not copied into this repository.

This first binding discovers an already installed host-native standalone Proton
in Steam's `compatibilitytools.d`; it verifies the native Wine ELF and skips
tools requiring a separate Steam runtime app. The selected absolute version path
is persisted per game. Host tools are read, never modified. Missing runtime is
an explicit error, not a reason to execute a random host Wine or claim support.
SteamRT-managed Proton and runtime acquisition still belong to the version manager.

Check with `trainer_pc_tests` / CTest `pc_games` and `python3 tests/PcSessionTests.py`.
These establish import and lifecycle behavior, not a Windows game's compatibility,
physical controller acceptance or PC multiplayer support.

## Delivery checkpoint — 21 October 2026

The common mini file picker and folder import were installed on Odin and Flip
in build `9ba43660681f5ac618be89ccdf972da2b691117d9b57ac6c77afc0836f226cec`.
The retained ARM run covered PC import/session supervision, library and Files;
the subsequent Home support run passed six PC-session and 24 overlay checks.
Live Home displayed the actual native fixture on both devices. Continue followed
by Home/Exit and process completion was verified on Odin. Flip lost connectivity
before its final return/exit check. The owner subsequently rebooted Flip; no
active game remained when it reconnected. This does not verify graceful Exit.

The genre-collection follow-up passed the ARM series suite and is installed on
both devices as `ecc0d6cf8a89dee88dc55b0b1ee1b6f4499f458a1ccbc7f31c7ab8eff0e58018`.
The native fixture's card and isolated source/install/data folders were removed
from both devices after testing. Database/history verification passed separately
on each. Genre definitions are applied only after content transfer and matching.
Private screenshots, device receipts and copy manifests are kept outside Git.
