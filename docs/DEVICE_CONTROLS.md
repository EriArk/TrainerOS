# Handheld controls

Start → Settings → Your handheld opens one fixed landscape panel. Up/Down selects a row; Left/Right adjusts volume or brightness in steps of five percentage points. A on Volume toggles mute. Y refreshes readings, B restores the Settings entry, and L1/R1 retain global page navigation.

Volume uses the current PipeWire default output through bounded `wpctl` calls. Brightness uses the unique readable Linux backlight and the user's existing write permission. TrainerOS does not grant new hardware permissions. Writes are bounded to 0–100% for volume and 5–100% for brightness; existing boosted audio readings are reported honestly. Missing, ambiguous or malformed controls remain unavailable. Failed writes preserve actual readings and show a retryable error.

Network status comes from NetworkManager without exposing connection credentials. Storage shows available/total capacity for TrainerOS data and the installed Adventure library. Unknown or unmounted locations remain unavailable. Device reads and writes run on a worker; the interface and controller loop remain responsive. Pending adjustments are coalesced to a bounded latest target per control, then drained without an idle gap. Displayed values remain actual backend readings. A rejected write clears pending requests and preserves a visible error until a successful adjustment; background refresh cannot hide it.

Restart and Power off require A confirmation; B cancels. The shared exit path drains submitted journal, backup and device work before invoking the installed ArmadaOS helper. Smoke tests and `--ephemeral` runs use a fake backend and cannot change host audio, brightness or power. Full network provisioning and sleep configuration remain in Desktop Mode.

## Acceptance

- Native checks cover malformed/boosted/muted volume, bounded and ambiguous backlights, worker errors/retry, confirmation/cancellation and waiting for pending device work before exit.
- Rendered SDL controller checks open the panel, adjust volume/brightness, mute, reach the final action and restore Settings focus.
- On 2026-09-13 the installed Flip build passed controller volume (11% → 16%), mute and brightness (2/255 → 15/255) checks, with independent system readings. The original values were restored. Network and both storage capacities rendered on one 1920×1080 screen.
- Power-off confirmation was cancelled with B; physical shutdown was not performed. Restart was executed through A confirmation. The first reboot exposed Armada's boot reset to Steam; after adding the explicit boot-default integration, the second reboot returned automatically to TrainerOS with the saved browsing state. See `SESSION_PROTOTYPE.md`.

## Start quick controls and Power - 2026-09-20

Start now contains mounted Volume and Screen brightness controls beside the
existing services. X jumps directly to Volume and returns to the prior service
entry; Up/Down traverses the menu; Left/Right changes the focused
quick control by five percentage points. A on Volume toggles mute. Unknown
controls stay grey and display Unavailable. Start/Back restores the underlying
screen or editor; L1/R1 closes the menu and switches primary pages. While Start
or Your handheld is visible, actual readings refresh every 1.5 seconds when idle,
including external hardware/desktop changes. Closing both surfaces stops polling.
There is no duplicate persisted volume or brightness preference.

Power replaces the old Leave TrainerOS action. It opens on Cancel, with Power off,
Restart and Switch Player. With real profile storage, Switch Player opens the
[Trainer chooser](TRAINER_OWNERSHIP.md); B restores that Power row. Development
samples retain the explicitly unavailable switching entry.
Power off/Restart require a fresh A confirmation; B restores Power, another B
restores its Start entry. L1/R1 and Home cancel pending confirmations. Existing
input edge handling rejects held/repeating A. The shared journal/device/backup
exit gate still drains work before a platform transition. Development exit or
entry into the dedicated session remains explicit in non-dedicated app mode.
Desktop and Steam remain explicit Start actions pending the later migration.

### Physical volume-key route

On the current Flip session, injecting KEY_VOLUMEUP/KEY_VOLUMEDOWN into the
physical evdev nodes initially left PipeWire volume unchanged: Gamescope consumed
the keys but no session volume handler changed the sink. This is synthetic event
injection into real device nodes, not evidence of pressing the physical switches.

The native platform VolumeKeys reader runs only in the installed dedicated
TrainerOS session, never in a normal Plasma app, ephemeral preview or smoke test.
It discovers readable physical devices advertising volume keys, skips virtual
input devices and never grabs input. It handles only volume up/down, with bounded
repeat and release/device-loss handling; power, lid and gameplay keys are ignored.
It stays active while an Adventure owns the foreground. Hardware adjustments read
the current default sink before writing, so a stale hidden-shell reading cannot
replace an external volume change. Existing user permissions suffice; no new
privileged daemon, session package or device permission is installed.

Unit and SDL acceptance covers coalescing, bounds, failures, external volume,
key repeat/release, quick controls, safe Power defaults and modal restoration.
Physical switch feel/hold comfort still requires the owner's hands-on check.

### Installed verification - 2026-09-20

Windows passed 35 checks and native ARM passed 39; affected device/SDL checks
were repeated after final adjustments. The actual Gamescope render and SDL
navigation passed on Flip. The production binary was backed up, replaced and
verified by SHA-256; schema 7 and all 686 library entries were preserved.
Real evdev event injection verified both volume keys, hold/release and volume
changes with RetroArch in the foreground. Start volume/mute/brightness and
external-volume refresh were independently checked against PipeWire/backlight;
original volume (41%, unmuted) and brightness were restored. Power confirmation
was cancelled: this increment did not perform shutdown/restart or sleep tests.

The additional game check found the existing physical-pad read ACL absent even
though the InputPlumber startup drop-in remained installed. Re-running the
existing identity-checked grant hook restored access; a subsequent launch
started the exit helper and its watchdog with interception mode 1. Both test
games were closed using ownership-checked WM_DELETE_WINDOW. This is not a new
physical Home/A/B acceptance: why the ACL disappeared and its persistence across
boot/device re-enumeration remain a platform follow-up. No new broad permissions
were granted for volume handling.
