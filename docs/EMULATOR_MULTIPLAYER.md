# Emulator multiplayer (#107)

Status: **in development, not accepted**. This is distinct from Emerald's
save-based Link activities. The owner moved this block ahead of the remaining
block 3 physical/interoperability checks on 2026-10-03.

The integration is **off by default**, including the final Flip/Odin delivery.
Only a development session with `TRAINEROS_EXPERIMENTAL_NETPLAY=1` advertises it.
Do not enable it in ordinary sessions or close #107 on the evidence below.

## First exact route

Contra III: The Alien Wars (USA), SNES, RetroArch 1.22.2 (69a4f0ea1e), Snes9x.
Both handhelds currently have the same core binary (SHA-256
`e8d8f5cf93898c7be3ca2590d4d75dc8e9a89b891589c2da1a7917fa27411d21`).
The unheadered ROM identity is
`a93ea87fc835c530b5135c5294433d15eef6dbf656144b387e89ac19cf864996`.
One existing owner archive is also recognized; filenames alone never establish
compatibility. No game content is distributed in this repository.

The first profile has no persistent progress. A multiplayer invitation starts a
**new two-player game**, with temporary SRAM on both peers, no loaded savestate,
no achievements, no automatic port forwarding and no changes to emulator defaults.
This does not establish host/guest save policy for other titles.

## Implementation boundary

- Physical Home uses one compact Invite friend panel: Nearby / Online friend.
- Nearby's first runtime route is the existing local network, on separate runtime
  discovery/session ports 47855/47856. This does not change native Link's Bluetooth
  default and does not claim router-free Bluetooth gameplay.
- Draft online routing uses authenticated friend consent for signalling, followed
  by RetroArch's upstream relay. It is not runtime-verified. No gameplay frames
  travel through chat messages.
- Compare exact content, runtime, core and the options profile before joining;
  recheck local files during launch preparation.
- The guest explicitly accepts. An unrelated running game is never replaced.
- Use the ordinary process/checkpoint/return path and per-session temporary config.

## Evidence / outstanding gates

2026-10-03, installed development candidates on both handhelds:

- Flip Home -> Invite friend -> Nearby -> EriArk produced the named Accept/Decline
  invitation on Odin. Acceptance gracefully closed the original Contra process,
  then launched separate host/client RetroArch instances. Ordinary launches remain
  unchanged. The host sends connection readiness only after RetroArch reports
  player 1, not merely after the Flatpak launcher starts.
- The actual host reported the guest joining as player 2 (16 ms in the captured
  run); Odin reported itself as player 2. TCP 55435 was established between their
  emulator processes. Keyboard input advanced the synchronized game into a level.
  **This is not proof of independently controlled two-player gameplay.** Remote
  evdev input did not establish controller operation; a temporary explicit SDL
  mapping did not resolve that check and was removed. Actual physical checks are
  owner-deferred. Separate SDL enumeration saw the expected virtual Xbox pad on
  each device. A later InputPlumber SendEvent probe timed out; do not repeat it
  blindly or treat it as controller evidence.
- Home -> Exit -> A returned both sessions without rebooting. Keep Home's input
  lease during capture/close: dismissing it first makes the helper reject capture.
  A regression test covers that ordering. Earlier failed-candidate Flatpak wrapper
  termination orphaned its emulator; the exact test PIDs were cleaned up. Failure
  now retains the owned session for normal Home exit instead of killing a wrapper.
- Stock 1.22.2 prompts clients for a password regardless of the host-password
  config. The LAN experiment therefore uses a trusted local network, without a
  gameplay password or public lobby announcement. Shell consent is **not** network
  authentication. Internet rooms still require a verified automatic authentication
  solution; exposing RetroArch's password keyboard is not accepted UX.
- Public lobby HTTPS timed out from both Windows and Flip. Its JSON relay endpoint
  interpretation, authenticated joining, separate-network play and interruption
  recovery remain unverified. No working internet route is claimed.
- The owner's existing Contra archive supplied the raw 1 MiB Odin test copy;
  Odin's library changed from 25 to 26 records. No ROM is committed.
- ARM compilation and six focused network/social/adventure suites passed. Windows
  exit-presentation and OnlineLink suites passed, including retained-lease restart
  and exact descriptor/consent cases. Automated passing does not close runtime gates.

Next: establish input delivery through the actual runtime controller path and
prove independent players, then finish online room authentication/recovery and
the router-free nearby route. Do not repeat the already-proven invitation as a
substitute. Both ordinary installed shells keep the experiment disabled meanwhile.
Native saved-Pokemon Link, background calls and all earlier acceptance remain.

## Final ordinary delivery

Both Flip and Odin run executable SHA-256
`cfc06343ce650f7582f081d3f34b8606313ffa7223123fb2efb05acac063d1d8`
with the experiment disabled. Verified one live shell per device, no abandoned
RetroArch process or runtime multiplayer listener, healthy profile/library
databases, unchanged boot preferences and nearby helpers. Controller navigation
returned both shells to Home. Flip output remains muted; Odin remains at zero.
No device reboot was needed. The two experimental invitation captures are in
[the screenshot gallery](../screenshots/README.md#experimental-multiplayer-disabled-by-default).

Sources: [upstream netplay guide](https://docs.libretro.com/guides/netplay-getting-started/),
[versioned runtime implementation](https://github.com/libretro/RetroArch/blob/v1.22.2/network/netplay/netplay_frontend.c),
[versioned command interface](https://github.com/libretro/RetroArch/blob/v1.22.2/command.h).
