# Handheld controls

Start → Settings → Your handheld opens one fixed landscape panel. Up/Down selects a row; Left/Right adjusts volume or brightness in steps of five percentage points. A on Volume toggles mute. Y refreshes readings, B restores the Settings entry, and L1/R1 retain global page navigation.

Volume uses the current PipeWire default output through bounded `wpctl` calls. Brightness uses the unique readable Linux backlight and the user's existing write permission. TrainerOS does not grant new hardware permissions. Writes are bounded to 0–100% for volume and 5–100% for brightness; existing boosted audio readings are reported honestly. Missing, ambiguous or malformed controls remain unavailable. Failed writes preserve actual readings and show a retryable error.

Network status comes from NetworkManager without exposing connection credentials. Storage shows available/total capacity for TrainerOS data and the installed Adventure library. Unknown or unmounted locations remain unavailable. Device reads and writes run on a worker; the interface and controller loop remain responsive. Repeated changes while an operation is pending are ignored until the actual value returns.

Restart and Power off require A confirmation; B cancels. The shared exit path drains submitted journal, backup and device work before invoking the installed ArmadaOS helper. Smoke tests and `--ephemeral` runs use a fake backend and cannot change host audio, brightness or power. Full network provisioning and sleep configuration remain in Desktop Mode.

## Acceptance

- Native checks cover malformed/boosted/muted volume, bounded and ambiguous backlights, worker errors/retry, confirmation/cancellation and waiting for pending device work before exit.
- Rendered SDL controller checks open the panel, adjust volume/brightness, mute, reach the final action and restore Settings focus.
- On 2026-09-13 the installed Flip build passed controller volume (11% → 16%), mute and brightness (2/255 → 15/255) checks, with independent system readings. The original values were restored. Network and both storage capacities rendered on one 1920×1080 screen.
- Power-off confirmation was cancelled with B; physical shutdown was not performed. Restart was executed through A confirmation. The first reboot exposed Armada's boot reset to Steam; after adding the explicit boot-default integration, the second reboot returned automatically to TrainerOS with the saved browsing state. See `SESSION_PROTOTYPE.md`.
