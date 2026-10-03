# PPSSPP maintenance

Updated 2026-10-03. Actual multiplayer acceptance is tracked in
[PSP multiplayer](../PSP_MULTIPLAYER.md); isolated LAN gameplay is observed,
while integrated, sustained and online acceptance remain open.

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
| Online relay mode `1`, `socom.cc`, built-in server off | Experimental native relay route; TCP reachability does not establish a playable online match. |
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
