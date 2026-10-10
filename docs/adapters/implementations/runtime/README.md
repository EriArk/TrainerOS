# Runtime profile source copy

Original TrainerOS source: **GPL-3.0-or-later**. Keep [LICENSE](LICENSE)
and [scope/third-party notice](LICENSING.md) with this reusable copy.


Generated source/dependencies and per-game profiles accompany the research index.
These are the actual TrainerOS PPSSPP/RetroArch identity and launch-configuration
modules, not ROMs, emulator binaries or save-format adapters. Refresh/check with
`tools/export-game-adapters.py` / `tools/check-adapter-knowledge.py` at repo root.
State and remaining gates in each profile are binding; this copy grants no new
compatibility claim. See [PSP evidence](../../../PSP_MULTIPLAYER.md) and
[RetroArch evidence](../../../EMULATOR_MULTIPLAYER.md) for upstream sources.

The [shared RetroArch profiles](../../../RETROARCH_MULTIPLAYER_PROFILES.md) also
cover reviewed platform/core pairs without a per-ROM allowlist. Call
`netplayIdentity` with the actual installation and content, then `prepareNetplay`
with the accepted peer identity. Runtime/core/content and known firmware must
match; local display names may differ. Disc validation is included in this copy.
These profiles use private temporary-progress sessions, not semantic save
editing or handheld link. Call `netplayCapacity(platform, core, metadata, contentPath)` and
pass its result as `netplayIdentity`'s final argument to enable reviewed
Snes9x/PCE-family three/four-pad layouts. The default remains two. European FBNeo
`batcir` also supports three/four through its private generated cabinet settings;
clones/other cabinets stay two. The included `FBNeoRomSets.h` supplies pinned
own/parent/BIOS dependencies; regenerate in the parent repo with
`tools/import-fbneo-dependencies.py`, then export/check again. Unknown driver
names do not produce multiplayer identities. `prepareNetplay` revalidates the accepted
capacity/layout. Exact legacy profiles retain their specific controls.

Linux, C++20 and Qt 6 Core/Gui/Network (Gui is a model-header dependency):

```sh
cmake -S . -B build
cmake --build build
./build/inspect_psp /path/to/owned.iso /path/to/PPSSPPSDL
```

The example only fingerprints the two supplied files. A match is an exact profile
identity, not proof of paired gameplay. Missing/mismatched files return 1.

The receiving host must supply explicit invitation consent, current local library
resolution, owned-process launch/exit, cancellation, and ordinary save protection.
Before configuring PPSSPP, recheck `netplayIdentity`, prepare the normal command,
and pass the accepted identity to `configureNetplay`. Preserve `ProcessCommand`
and call its `settled` callback after failed preparation or owned-process exit.
That lifetime owns the private temporary configuration. SAVEDATA stays in its
ordinary directory and may be written by the game; never delete or duplicate it
as an incidental networking operation. Never use a loopback host address for LAN.
Online uses the upstream public relay, not a private TrainerOS matchmaking room.

Configuration modules can be compiled independently; the header snapshots also
describe host-facing model/process interfaces. They do not include the complete
TrainerOS invitation controller, database or process runner. Integration into
another project must implement those contracts, rather than bypassing them.

Independent handheld link also includes the maintained core recipe/RTC patch in
`packaging/emulators/handheld`. Call `recoverHandheldReturn` before **ordinary**
GB/GBC launch, and retain `finalize` errors/private recovery folders on network
exit. The Gen II SRAM/RTC intent must be recovered before exposing its files to
an emulator. See [core maintenance](../../../emulators/doublecherrygb.md) and
[handheld evidence](../../../HANDHELD_MULTIPLAYER.md); a copied adapter does not
remove the receiving host's process/save ownership responsibilities.

Generic battery GB/GBC pairs additionally include `LinkedSavePreparation` and its
versioned Python helper recipe. The receiving host must bind preparation to an
accepted two-member party, validate exact ROM/core/runtime/helper identity, stop
the helper on cancellation, then wait for both SRAM preparation and the final
RetroArch endpoint before launching. `ownSram`, `ownSramExisted` and host-only
`peerSram` are private launch data, not logs or persisted UI state. Never bypass
`prepareLinkedSave`'s preimage check or the assigned-slot finalizer. The helper
uses Qt Core/QProcess; Online preparation requires the pinned external Python
dependencies. See [SameBoy maintenance](../../../emulators/sameboy.md) for exact
subsystem IDs, patch/build requirements, privacy and compatibility limits.
