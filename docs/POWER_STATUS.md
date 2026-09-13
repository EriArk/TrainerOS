# Battery status

The fixed lower chassis panel shows the device battery percentage and reported charging state. The gauge stays outside page navigation and never takes controller focus. Low charge changes the gauge color (amber at 20%, pink at 10%); charging adds a lightning mark. An unavailable reading shows an em dash instead of a fabricated percentage. No remaining-time estimate is inferred.

`platform/power/PowerStatus` reads the Linux [power-supply ABI](https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-power) on a worker, at startup, every 30 seconds and when the application becomes active again. The platform reader accepts one present system Battery, ignores USB supplies and Device-scoped peripheral batteries, and leaves ambiguous multiple packs unavailable. Reads and enumeration are bounded; invalid or missing capacity clears the old value. QML receives status properties only. Other development hosts report unavailable unless a test reader is supplied.

This is read-only status reporting. Sleep, charging policy, shutdown and session selection remain ArmadaOS responsibilities; this increment changes none of those settings.

Acceptance: content-free fixtures cover missing/invalid/zero/full readings, charging, peripheral exclusion, absent/multiple batteries and a slow worker with responsive event processing. SDL/rendered scenarios retain Home/section/menu focus while checking normal, zero, low, charging and unavailable gauge states. The installed Flip reading must agree with the device's battery supply.

On 2026-09-13 the production ARM build displayed **52% / ON BATTERY** on Flip 2, matching the independently read **52 / Discharging** supply values. Controller page changes and Start/Back remained responsive. Charging and low/unknown states were exercised with fixtures; this check did not physically connect a charger or test sleep. All 25 checks passed on Windows, Ubuntu 24.04 and ARM64; the final Windows SDL check also passed after the last test-only adjustment.
