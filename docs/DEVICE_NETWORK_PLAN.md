# Odin 2 and network controls

Owner addition, 2026-09-27. The requirements below retain their acceptance;
the dated delivery section distinguishes installed behavior from remaining work.
[ROADMAP](ROADMAP.md#unified-execution-order--existing-work-and-new-issues)
remains the execution queue.

## Delivered radio slice — 2026-09-28

Installed on Flip and [Odin 2](ODIN2_BRINGUP.md): a compact three-button Start
row above volume/brightness, and Connections in the existing two-pane Settings.
Both use one asynchronous device service and actual NetworkManager/BlueZ state.
Writes use a root-owned fixed-verb helper and a narrowly scoped sudo policy;
QML never invokes platform commands. Repeated toggles queue during refresh.
External changes are polled while the controls are visible. Failures retain
actual state and expose an actionable error rather than optimistic success.

`packaging/integrations/install-radio-control.py --user USER` installs the helper
without changing radio state. Airplane mode turns Bluetooth off before Wi-Fi;
leaving it restores the pre-flight radio choices held in a root-only `/run` file.
Unknown/unavailable controls stay unavailable. This is software radio control,
not an override of hardware blocks.

Bluetooth off/on and readback were exercised on Flip through controller events;
both handhelds report live state. Wi-Fi-off/airplane end-to-end recovery was
deliberately deferred because Wi-Fi is the only unattended SSH path.
Remaining: network scan/connect/password/disconnect/forget, Bluetooth discovery,
pairing/code confirmation/connect/remove, and local hardware-block/recovery proof.
These remain required; three toggles do not complete the network acceptance.

## Second device

When the owner is home and provides Odin 2 access, inspect the exact device,
installed OS/build, supported ArmadaOS installation route, storage and recovery.
Do not infer Linux/Armada support from a similar chipset or the Flip deployment.
Keep the native Linux product direction and preserve a verified recovery path.
Begin with normal application deployment where the actual OS supports it;
dedicated-session/default-start behavior follows device-specific validation.

Record a separate #78 device profile and verify display/scaling, printed button
mapping, sticks/triggers, Home, volume, network and launch/return. Preserve the
owner's existing data; installation preparation does not authorize an unreviewed
wipe. Report unsupported hardware paths honestly rather than claiming portability.

Odin 2 and Flip become the intended pair for #45 Link testing once both work.
That availability enables testing; it does not itself prove Bluetooth transport,
exact-game compatibility, trusted lineage or two-device transaction recovery.
Keep disconnect/retry/restart/rollback and duplicate-commit prevention in the
Link acceptance. Independent development continues while device access is pending.

## Settings and Start

Use the existing two-pane, controller-first Settings approach and shared platform
services. Keep runtime commands, network implementation details and credentials
out of feature QML and ordinary UI copy.

| Surface | Required behavior |
| --- | --- |
| Settings → Wi-Fi | Enable/disable, scan/list networks, show connection state, connect with controller text entry, disconnect and forget a saved network. Preserve working settings when a connection fails. |
| Settings → Bluetooth | Enable/disable, discover devices, pair with the appropriate confirmation/code flow, connect/disconnect and remove a pairing. Show actual availability and connection state. |
| Top block of Start | Compact Wi-Fi, Bluetooth and airplane-mode quick controls beside the existing volume/brightness area. Detailed configuration belongs in Settings; do not add a second menu column. |
| Airplane mode | Perform actual supported radio blocking, with aggregate state reflecting actual radios. Account for hardware blocks and partial failures; no cosmetic Boolean that leaves radios enabled. |

Quick controls and Settings share the same service/state. Refresh external system
changes; coalesce repeated actions and show pending/failure state accurately.
Preserve deterministic focus, modal priority, L1/R1/L2/R2 rules and footer-only
contextual hints. Password/PIN entry must work entirely from the controller.
Store secrets through the supported platform mechanism, never logs or Git.

Verify the actual platform APIs on each device before selecting an implementation.
Keep remote recovery in mind during radio-off checks: do not cut the only SSH
connection while relying on it to finish or reverse the operation; perform those
physical checks with the owner/device access available. No radio changes are
part of this planning increment. Sleep stays disabled under its separate gate.

At implementation time audit other essential system settings already available
through platform services (for example output-device selection and date/time),
then justify any missing useful controls. This is not authorization for a large
speculative settings catalogue or unsupported toggles.

## Schedule and delivery gates

- Network Settings/Start controls belong to R11 device/system controls, before
  R14 onboarding/Help. A bounded prerequisite can move earlier when real device
  setup or Link requires it, without displacing the current Emerald/Journey work.
- Odin bring-up is an owner-availability lane under #78/R11: inspect when access
  arrives; move only the needed device/runtime prerequisites forward.
- Test actual controller navigation, password entry, pairing, external state
  changes, offline/unsupported states, failed operations and radio recovery on
  each accessible device. Do not claim unsupported device coverage.
- Deliver code, checks, device screenshots, commit/push and updated capability
  evidence in the implementation increment. These documentation changes require
  no device deployment or radio operations.
