# PPSSPP maintenance

## Updated-handheld recheck — 11 October 2026

The current ARM64 client is installed on both updated devices. The exact Lumines
Nearby invitation, host save/leave barrier, native discovery and paired arena
were rechecked with independent input. The first challenge returned to the lobby;
the second reached gameplay. Home/Exit cleaned both temporary configuration roots;
ordinary PSP save hashes remained unchanged. A real generic UMD parser check is
not wider-title gameplay evidence. [Full evidence and limits](../MULTIPLAYER_DEVICE_RECHECK_20261011.md).

## Generic Ad Hoc preparation — 11 October 2026

Source preparation extends the existing invitation/relay/launch route beyond the
exact Lumines disc. It uses **unchanged PPSSPP 1.20.4**, tag commit
`fa50bb1976065c4f8b1b47af227d367fe9771555`, retaining the reviewed ARM64 runtime
hash below. This does not prove that every PSP game contains or supports Ad Hoc.
The party surface asks players to choose Ad Hoc inside the game and distinguishes
original online servers. Explicit solo catalogue metadata excludes invitations;
missing metadata is not a single-player classification.

`runtime.ppsspp.adhoc.v1` admits raw ISO UMD games: bounded ISO9660 reads locate
`PSP_GAME/PARAM.SFO`, require category UG and a disc ID, reject ambiguous entries
and invalid extents, then match the whole ISO SHA-256 and runtime between friends.
IDs establish format, not multiplayer support or semantic save access. Local
titles/filenames are presentation only. CSO/CHD/PBP, UMD video and other formats
remain outside this preparation; ordinary solo launching is unchanged.

This first generic profile prepares two participants. Catalogue player counts
do not increase its capacity; larger parties need reviewed native-mode evidence.
Players use native create/join controls. Automatic game-room selection, private
game lobbies and exclusion of unrelated public-server users are **not** delivered.
Parallel private-party admission and games needing additional setup stay open.
No ROM, save or firmware is transferred.

Exact Lumines retains its existing wire identity and clock override. Generic
discs use native-default `ForceLagSync2=False` and a distinct settings fingerprint.
Both reuse private SYSTEM copies, controller bindings, saved MAC, ordinary
SAVEDATA and cleanup. PPSSPP can write its ordinary saves during play; this is not
temporary shared-console progress or TrainerOS semantic save editing.

Sources checked:
[Ad Hoc/relay](https://www.ppsspp.org/docs/multiplayer/how-to-play/),
[quickstart](https://www.ppsspp.org/docs/multiplayer/quickstart/),
[ISO structure](https://github.com/hrydgard/ppsspp/blob/fa50bb1976065c4f8b1b47af227d367fe9771555/Core/FileSystems/ISOFileSystem.cpp),
[SFO structure](https://github.com/hrydgard/ppsspp/blob/fa50bb1976065c4f8b1b47af227d367fe9771555/Core/ELF/ParamSFO.cpp).
The bounded reader is TrainerOS code, not a copied emulator filesystem.

Local validation: Windows client and scoped targets build successfully;
`standalone`, `game_party`, `runtime_multiplayer` pass **3/3**, 7.01 seconds.
The three new raw-disc/clock/lifecycle regressions also have explicit QtTest file
logs (none skipped), because this Windows CTest run captured no console output.
Linux memory-stick symlink cases remain platform-skipped on Windows; no new Linux
save-isolation result is claimed. Adapter exports/registry and local doc links
validate. Final Windows debug client SHA-256:
`3955ba7105f732df27e80a1e875ab13bd44ebaacbec3115ac50292a0ff3b9b94`.
Ninja recovered a truncated dependency cache and performed a full rebuild; only
the final build above includes the complete source changes.

Handheld installation,
wider-game discovery/play/controls, four clients, loss/reinvite and distinct
networks remain deferred. Earlier Lumines evidence is not promoted to new titles.
No public relay or handheld was contacted. Rollback removes the generic profile
while retaining Lumines; no emulator migration or persistent settings replacement.

## Earlier installed evidence

Updated 2026-10-03. Actual multiplayer acceptance is tracked in
[PSP multiplayer](../PSP_MULTIPLAYER.md); integrated invitations, short LAN and
external-relay rounds, and normal Home exit/cleanup passed on both devices.
The LAN challenge required one retry. A 12-second relay loss during rematch startup
returned both games to the lobby; a fresh challenge restored play without restart.
Distinct-network, mid-round loss and sustained reliability remain unproven.

## Upstream and observed installation

- Upstream: [hrydgard/ppsspp](https://github.com/hrydgard/ppsspp), tag `v1.20.4`.
- Observed package: Flatpak `org.ppsspp.PPSSPP`, ARM64 `PPSSPPSDL`, Flip 2 and
  Odin 2. Runtime SHA-256:
  `c99cec693067f8c94ae0d8b5e7299392ed5c0cfae3f8231176ed25ea6c155dfc`.
- This is the observed netplay allowlist, not a reproducible image package lock.
  No TrainerOS patch to the PPSSPP binary/source is currently installed.

## Maintained integration

Source: [PpssppNetplay.cpp](../../src/integrations/adventure/standalone/PpssppNetplay.cpp),
[StandaloneAdapter](../../src/integrations/adventure/standalone/StandaloneAdapter.cpp),
[discovery](../../src/platform/emulation/EmulatorDiscovery.cpp).
The exported runtime copy lives in [adapter implementations](../adapters/implementations/runtime/README.md).

| Change | Reason and ownership |
|---|---|
| Flatpak `--command=env` with `XDG_CONFIG_HOME` set inside the sandbox | Flatpak resets the reserved variable when passed as an ordinary environment override. Native launch uses `/usr/bin/env`. |
| Temporary configuration root inside ordinary `PSP/SYSTEM` | Copy global/per-game/controller INIs because PPSSPP saves appended options. Retain ordinary SAVEDATA and optional GAME/TEXTURES through links; remove only the private root on process settlement. |
| Network `EnableWlan=True`, `EnableUPnP=False`, `PortOffset=10000` | Session settings, not global preference replacement. |
| LAN relay mode `2`, host's actual LAN address, host-only `EnableAdhocServer` | Loopback allowed coordination but prevented real opponent discovery. |
| Online relay mode `1`, `socom.cc`, built-in server off | Exact Lumines pair reached gameplay through external TCP 27312/27313 and recovered via a new challenge after rematch-start loss. Both clients shared home internet; separate-network acceptance remains open. |
| Network savestate/speed control off; General auto-state-load, cheats/plugins off; Achievements disabled | Keep the experimental paired runtime consistent without rewriting the ordinary profile. |
| SystemParam nickname from Trainer; **no MAC override** | Preserve global and per-game console identities. Earlier per-invitation randomization was removed on 2026-10-03. PPSSPP warns that saves can be tied to a MAC. Never put a shared fixed MAC in the image. |
| General `ForceLagSync2=True` in the exact Lumines session; settings fingerprint `ppsspp-adhoc-isolated-config-clock-v2` | Default-clock diagnostic failed after matching; the controlled clock-sync run reached a LAN arena and independent inputs on Flip/Odin. Keep ordinary/per-game preferences intact. This is not a universal PSP compatibility workaround. |
| Flip ordinary graphics backend changed from OpenGL to Vulkan | Observed OpenGL startup crash; private backup retained. Odin already used Vulkan. This is device evidence, not a universal backend policy. |

No socket-timeout or first-connect workaround is enabled. Clock synchronization
uses upstream `ForceLagSync2` in `[General]`, not `[CPU]`. Rollback removes the
session override and restores the previous settings fingerprint together; it
does not rewrite the ordinary INI or saves. Recheck the matching transition on
both devices when updating PPSSPP before removing or retaining this workaround.
Do not silently migrate existing saves to another device/MAC or overwrite a
per-game override to solve an unproven networking hypothesis.

## Update and rollback procedure

1. Record candidate package/commit and runtime hash; compare upstream
   `Core/Config.cpp`, `UI/NativeApp.cpp` and ad hoc implementation against the
   keys and launch assumptions above. If a source patch becomes necessary,
   record its file/build recipe here with the upstream base and rebase result.
2. Test the candidate with copied configuration and copied saves first. Check
   sandbox visibility/write access and `/proc` environment, including that
   the normal memory stick and controller mapping are used by the real route.
3. Run `trainer_standalone_tests`, export/check adapter knowledge, then check
   launch, input, physical Home return, ordinary save readback and cleanup on
   both supported devices. Compare original config/save hashes for diagnostics.
4. For networking, prove an actual match, independent controls, exit and
   interruption/recovery. Online needs separate route evidence. Do not widen
   the exact runtime/content allowlist solely because the build or lobby works.
5. Record the proven candidate and only then change the image/package pin and
   applicable runtime allowlist. Preserve per-game settings, unique device MAC,
   controller preferences and saves through the update.
6. Keep the previous compatible package and pre-migration configuration backup.
   Stop the owned process before rollback; restore only migrated integration
   settings, not old personal saves. A source-patched runtime needs its own
   fingerprint and paired proof before it replaces the installed version.

Relevant primary sources: [network settings and MAC warning](https://www.ppsspp.org/docs/settings/network/),
[v1.20.4 configuration](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/Config.cpp),
[matching implementation](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/HLE/sceNetAdhocMatching.cpp).
