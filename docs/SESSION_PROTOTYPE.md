# Dedicated session: observed integration boundary

This is the read-only preparation for milestone 10, collected on the owner's Flip 2 on 2026-09-13. No TrainerOS graphical session, session switch or autologin change was installed during this review. The running product remains a normal full-screen application launched in Steam Gaming Mode.

## Current ArmadaOS installation

| Boundary | Observed behavior |
| --- | --- |
| Display manager | SDDM; the installed Armada configuration enables relogin and selects `gamescope-session-steam.desktop`. |
| Available peers | Armada Plasma, Plasma Mobile and Steam Big Picture remain installed. |
| Steam session entry | Runs `gamescope-session-plus steam`. |
| User service | `gamescope-session-plus@steam.service`, with the generic `%i` launcher and graphical-session target bindings. |
| Client configuration | The generic launcher sources `sessions.d/<client>` from `/usr/share`, `/etc`, then the user's configuration directory. |
| Device configuration | The generic launcher consumes Armada's `device-env`, including the Flip 2 profile. Reuse this boundary instead of copying display/rotation/GPU assumptions into TrainerOS. |
| Qt/display environment | The generic launcher selects `QT_QPA_PLATFORM=xcb`, starts Gamescope, and obtains the actual display names from its startup socket before running the client. |
| Session discovery | This installed SDDM's `--example-config` accepts comma-separated Wayland `SessionDir` entries; defaults include `/usr/local/share/wayland-sessions` and `/usr/share/wayland-sessions`. |
| Official transitions | `/usr/libexec/armada/session-control` supports desktop/gaming switches and default-only choices. A switch writes an autologin override and restarts SDDM or logs out Plasma. It does not accept a TrainerOS mode. |

The installed generic launcher is extensible, but its Steam client configuration is not a reusable TrainerOS template: that file also modifies Steam bootstrap/configuration and runs Steam-specific helpers. The generic launcher's short-session tracker is shared with Steam and its default recovery callback does nothing. Neither a client name nor a successful application launch proves safe failure recovery.

## Proposed bounded prototype

1. Keep the current Steam autologin and working Plasma entries. Add a separately named TrainerOS client/session through the existing generic launcher after validating the actual writable session-install location. Preserve all existing SDDM search paths if an additive configuration is needed.
2. Use an independently installed launcher as the client command. Keep command arguments in that launcher: the upstream `CLIENTCMD` execution uses shell word splitting. Do not source the Steam client file or replace it.
3. Put controller input, logs, exit/restart coordination and platform mode transitions behind the existing application/platform boundaries. A normal Adventure exit returns to the same TrainerOS process; an app crash must not kill an emulator that is still writing saves.
4. Before the first session switch, prepare a tested recovery route that does not depend on TrainerOS drawing a frame. Bound restart attempts, preserve crash evidence, and restore the known-good desktop/default without disturbing Steam's shared failure tracker. Do not rely on the generic empty recovery hook.
5. Validate the session on the device: landscape output, gamepad ownership, Home/Continue, a verified Adventure launch/return, Start → Desktop, ordinary exit, startup failure, application crash and rollback. Check that the existing Steam and Plasma choices still work.
6. Only after those checks may milestone 11 change the normal/default session. Wake/hinge behavior has its own physical-device gate; the current no-sleep setup stays in place until that work is resumed.

Open integration questions include Gamescope's Steam-mode focus/app identity for a standalone Qt client, per-game environment without the Steam shortcut wrapper, controller/Guide routing, and coexistence with Armada's mode selector. These need execution evidence rather than assumptions from the installed scripts.

Acceptance for this preparation is the inspected installed files and the explicit prototype/recovery boundaries above. It does not close milestone 10. See [platform model](ARMADA_PLATFORM.md), [device baseline](ARMADA_DEVICE_BASELINE.md) and [roadmap](ROADMAP.md).
