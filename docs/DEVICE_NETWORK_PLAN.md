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
The following delivery replaces the earlier toggle-only boundary. Local
hardware-block/radio-off recovery proof remains open.

## Connections delivery — 2026-09-28

Connections now owns one inline Wi-Fi/Bluetooth list in the existing right-hand
Settings pane. Left/right selects the radio, Y searches, A connects or offers
one disconnect confirmation, Select forgets a saved entry with confirmation,
and B cancels or returns to categories. X toggles the selected radio. Contextual
legends stay in the footer; no intermediate device/details/action-menu stack.
The shared controller keyboard masks Wi-Fi passwords and handles Bluetooth
PIN/passkey entry. BlueZ confirmation/display codes appear inline. Leaving the
page cancels the pending operation; late callbacks cannot reopen its keyboard.

`NetworkService` runs the bounded JSON-lines `network-control.py` asynchronously.
Listing runs unprivileged; mutations have an exact no-arguments sudo rule.
Passwords travel over stdin, never command arguments, diagnostic output or
TrainerOS persistence. NetworkManager keeps successful credentials. Wi-Fi
activation creates a 90-second NetworkManager checkpoint first; a new profile
is initially memory-only, saved after successful activation, and removed on
failure/cancellation while the old connection is restored. Process loss retains
the timed rollback. Successful IP activation does not imply Internet access or
completion of a captive portal.

Supported new networks are open, WPA/WPA2 personal and WPA3 SAE. Already configured
enterprise profiles may reconnect through NetworkManager; new enterprise/WEP
configuration and manually entering a hidden SSID remain Desktop Mode work.
Bluetooth uses a temporary, target-bound KeyboardDisplay agent, never replaces
the desktop default agent, and handles confirmation, PIN/passkey and display
requests. Discovery, connect/disconnect and removal operate on actual BlueZ
devices; only the explicitly paired device is trusted.

Evidence: both Flip and Odin returned actual saved/nearby lists and completed
Wi-Fi and Bluetooth scans without losing their active SSH connection. Installed
controller checks covered lists, face switching, scrolling, masked keyboard and
cancel/back. Four Python tests cover request/security validation, success-only
profile persistence, rollback/cancellation and stale targets; native tests cover
pairing prompts, target/focus retention, cancellation and delayed callbacks.
Actual new-network password authentication and accessory pairing/code exchange
still need local hardware acceptance; these are implemented flows, not claimed
end-to-end device proof. Radio-off/airplane recovery remains deferred until a
local recovery path is available. No existing saved connection was forgotten.

API references: [NetworkManager manager/checkpoints](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.html),
[wireless devices](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.Device.Wireless.html),
[BlueZ agent contract](https://github.com/bluez/bluez/blob/master/doc/org.bluez.Agent.rst).

Installed ARM64 SHA-256 on both devices:
`b0b1f608b70cb85e399bd984478ec44fd10958dce9af7f3fcc558f3214773d19`.

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
