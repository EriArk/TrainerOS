# Odin 2 and network controls — planned acceptance

Owner addition, 2026-09-27. This is a planning record, not device-support proof
or a claim that these controls are installed. [ROADMAP](ROADMAP.md#unified-execution-order--existing-work-and-new-issues)
remains the execution queue.

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
