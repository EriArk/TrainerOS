# Armada sleep assessment — 2026-09-30

The owner reopened sleep assessment after updating Flip 2 and Odin 2. This is
a bounded early investigation of R13/#79, not completion of that roadmap row
or authorization to treat an untested wake path as reliable. Both devices boot
`20260929.5915c28`; see [paired maintenance](ARMADA_UPDATE_20260930.md).

## What actually changed

[Armada's September 26 release](https://github.com/armada-os/armada/releases/tag/20260926)
reports reduced sleep drain on SM8550/SM8650/SM8750, faster Wi-Fi reconnection,
WPA3 reconnection fixes, optional sleep logs, and removal of an unstable CPU
realtime option. Its release date is September 27; the release label is not the
installed testing-image version.

The [kernel/power change](https://github.com/armada-os/armada/commit/431bf573af943b6f8dba5039a8f4d505d9d4ffa9)
includes regulator/PCIe/codec/MCU/fan changes. The reported Odin-family power
measurement is **Odin 2 Mini**, not our regular Odin 2. SM8250 Flip 2 is not in
the announced power-reduction group. Do not extrapolate a 50% reduction or a
fixed wake bug to either device without measurement.

Comparing each device's actual previous commit with `5915c28` matters:

- Flip's `1de6489` predates the sleep changes. Its newer image also includes
  audio suspend/resume work, but that is not proof of Flip wake reliability.
- Odin's `eba5232` already included the principal sleep power changes. The
  subsequent update adds graphics queue-priority/Gamescope fixes and other
  compatibility changes; it is not a newly installed sleep fix for Odin.

The [official sleep documentation](https://armadaos.dev/using-armada/sleep-shutdown-and-battery/)
still calls native s2idle work in progress, with remaining bugs and excessive
power draw. Fake sleep remains an upstream alternative. Neither is enabled
as a newly verified TrainerOS capability by this assessment.

## Live device state

| Check | Flip 2 | Odin 2 |
| --- | --- | --- |
| Upstream device profile | `retroid-pocket-flip2`, SM8250 | `ayn-odin-2`, SM8550 |
| Configured upstream suspend | `s2idle` | `s2idle` |
| Kernel memory-sleep choices | `[s2idle]` | `[s2idle] deep` |
| RTC wake source | Enabled, no alarm armed | Enabled |
| Sleep/suspend target policy | Existing masks retained | Targets unmasked/static |
| Upstream user power-button handler | Masked | Active in Steam |
| Current graphical session | TrainerOS | Original Steam session |

Flip's existing logind drop-in ignores power/suspend/hibernate keys, lid events
and idle actions. Existing sleep settings were not changed. Odin does not have
that drop-in; do not describe sleep as disabled device-wide on Odin.

The installed `systemd-suspend.service` override uses upstream
`/usr/libexec/armada/suspend-dispatch`, which selects s2idle and runs systemd's
sleep hooks. Preserve this route instead of writing `mem` directly and bypassing
hooks. RTC wake capability is only preparation, not a successful wake test.

Upstream `armada-powerbuttond.service` is part of the Steam session and invokes
`/usr/libexec/armada/powerbuttond`; its button/lid action asks Steam to sleep.
It is not a general TrainerOS power-button implementation. Simply unmasking
sleep targets or starting this Steam handler in TrainerOS is insufficient.
Any shell-owned route must retain storage/Link guards and upstream resume hooks.

No suspend command, power-button/lid test or permanent sleep-policy change was
performed in this pass. The physical-availability question remains unanswered.

## Odin incident: separate radio evidence

The previous boot's user journal continued recording Steam/Decky events until
18:32, well after SSH stopped completing its handshake around the TrainerOS
entry at 17:01. This contradicts an assumption that the entire machine immediately
stopped running. It does not prove the graphical session stayed responsive.

A standalone, process-owned test of the installed nearby helper ran without
TrainerOS, invitations, connections or save writes. It restored its temporary
radio identity/listening state on exit. Two approximately 43-second probes
produced:

| Probe | Matching ath12k error log lines | Subsequent SSH |
| --- | --- | --- |
| Existing Find + ExtendedListen behavior | 8 | Available |
| Same Find, positive ExtendedListen calls omitted | 4 | Available |

These are log-line counts, not independent failure counts. Both include
`No VIF found`, missing vdev 1 and firmware-stat `-71`; the latter probe also
recorded a firmware-stat timeout outside that narrow count. A first probe that
exited immediately on stdin EOF was discarded, not treated as passing evidence.

The [wpa_supplicant API](https://w1.fi/wpa_supplicant/devel/dbus.html)
describes Find as search/listen discovery and ExtendedListen as periodic listen
configuration. Removing the latter did not eliminate the driver warnings.
Neither probe reproduced loss of SSH, so the warnings are a reproducible lead,
not the proven cause of that incident. No nearby implementation was changed or
disabled on the strength of these two probes. Private logs remain outside Git.

## Next bounded acceptance

When physical recovery is available, test one device at a time:

1. Confirm no Adventure/save operation or pending Link settlement; save the
   device's masks/logind policy and arrange bounded rollback before changing it.
2. Perform a short native-sleep trial through upstream systemd dispatch, with
   RTC wake backup. Check display, physical controller, SSH/Wi-Fi and session
   recovery; then test the actual power key and Flip lid separately.
3. Prove the TrainerOS button/lid route and one supported running Adventure's
   wake/normal exit. Keep automatic idle sleep off during these trials.
4. Enable only the demonstrated device/runtime route. Retain the remaining #79
   battery/thermal/duration and protected-operation acceptance in ROADMAP.

A changelog, a successful reboot, or an RTC capability read cannot replace
these wake checks. If native sleep fails, restore the prior policy and evaluate
upstream fake sleep separately rather than presenting a black screen as success.
