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
  **At that checkpoint independent controller gameplay was unproven.** Remote
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

Next: finish online room authentication/recovery and the router-free nearby
route. Independent remote controller gameplay is now proven below; physical
button checks remain owner-deferred. Do not repeat the already-proven invitation
as a substitute. Both ordinary installed shells keep the experiment disabled meanwhile.
Native saved-Pokemon Link, background calls and all earlier acceptance remain.

## Independent controller gameplay - 2026-10-03

A paired LAN run reached Contra's two-player level using remote evdev controller
input. Flip controlled blue player 1: Right and its bomb action moved that player
and reduced only P1's bomb count from one to zero. Odin controlled orange player
2: Left and its bomb action moved P2 and reduced only P2's bomb count. These are
actual game actions on the two handhelds, not menu navigation or keyboard proof.
Owner-operated physical buttons remain a separate deferred check.

Flip inherited SDL2 for joypad input; its RetroArch menus responded, but that
configuration did not establish gameplay control in the paired check. Selecting
udev established the observed two-player control. The exact Linux netplay profile
now selects udev in its temporary session config on both devices, with settings
identity `snes9x-default-no-sram-v2`. Ordinary emulator config/bindings stay intact.
This is a bounded verified route, not a universal diagnosis of SDL2.

The newly built candidate was installed on both devices and launched another
paired session. Both actual session configs selected udev, disabled public lobby
announcement and reached player 1/player 2. Flip's ordinary configuration was
byte-identical to its pre-diagnostic copy. Temporary SDL mappings, background
input settings and logging changes were removed. An intermediate static screen
was Contra's own pause after repeated Start input; it was not connection proof.

ARM compilation and the focused RetroArch test suite passed. Repeated diagnostic
shell restarts triggered Flip's existing three-failures/120-second Plasma fallback;
logs showed our SIGTERM exits. The installed session control restored TrainerOS
without a reboot, retaining the recovery safeguard. Both games then closed through
the owned Home exit flow and the development wrappers were removed.

This advances the controller gate only. Internet automatic authentication,
separate-network operation/recovery and router-free nearby gameplay remain open;
block 1 and #107 are not complete.

## Previous ordinary delivery

Both Flip and Odin run executable SHA-256
`dc0d71d4ec5f4dd6f6faf5e6f0a7326ff7ccd656ef82119edd4e7adbff7cfe3f`
with the experiment disabled. Verified one live shell per device, no abandoned
RetroArch process or runtime multiplayer listener, healthy profile/library
databases, unchanged boot preferences, Emerald save hashes and nearby helpers.
Flip output remains muted; Odin remains at zero.
No device reboot was needed. The two experimental invitation captures are in
[the screenshot gallery](../screenshots/README.md#experimental-multiplayer-disabled-by-default).

Sources: [upstream netplay guide](https://docs.libretro.com/guides/netplay-getting-started/),
[versioned runtime implementation](https://github.com/libretro/RetroArch/blob/v1.22.2/network/netplay/netplay_frontend.c),
[versioned command interface](https://github.com/libretro/RetroArch/blob/v1.22.2/command.h).

## Public relay authentication - 2026-10-03

Block 1 remains **in progress**. This checkpoint fixes online invitation delivery
and automatic joining, not all multiplayer acceptance.

The directory failure above was our HTTPS assumption: the versioned RetroArch
implementation uses the public HTTP lobby. The HTTP list and Madrid tunnel lookup
answered, and both handhelds reached the returned European relay. The directory
contains public room metadata; the one-time password travels in the accepted
friend session, never in a directory query or application log.

Stock RetroArch 1.22.2 opens its password keyboard for a challenged guest;
its `netplay_password` setting configures a host, not an automatic client answer.
The session now owns a bounded loopback bridge: it sends the relay RATS room
prefix, preserves the protocol header/NICK exchange, answers the host's salted
SHA-256 PASSWORD challenge, then forwards the game stream. Only the local
client's copy of the salt is zeroed to suppress the keyboard. The real host still
checks the invitation secret. No RetroArch fork or global configuration change
is needed. This is the upstream password protocol, not encrypted game transport.
The bridge has bounded buffers/connection time and closes with the owned game.

The runtime friend probe also used the group-channel `recipients` request by
mistake, then silently discarded the reply. It now reuses an existing one-to-one
DM, or creates one with `recipient_id`. Failure reaches the game menu directly;
a cancelled request cannot start a late probe. Generation and friend checks remain.
The consent header now says GAME INVITATION for either runtime transport.

Actual Flip -> Online friend -> Odin consent launched matching Contra III /
Snes9x / RetroArch processes automatically. Both reported player 1/player 2 and
166 ms in the first relay run; Odin's TrainerOS connection reached the external
relay on TCP 55435 while its emulator connected only to the loopback bridge.
No password keyboard appeared. Both returned through Home -> Exit without reboot.

These devices still shared one home network. This is a public-relay handshake
and launch proof, **not** the required different-internet gameplay proof. The
captured game screens in this run included the game's attract/demo sequence;
they are not evidence of independent online player control. Preserve the earlier
LAN control evidence, but do not promote it to online gameplay acceptance.
An attempted exact-socket `ss -K` fault returned a kernel Invalid argument;
no network-loss recovery is claimed from it. Do not repeat generic input scans
or the already-passed password handshake as a substitute for these open gates.

ARM compilation and four focused suites passed: RetroArch, the new fragmented
relay/challenge/stream/disconnect tests, OnlineLink and Social (including DM
contract and cancelled-response regressions). No microphone check was repeated.

Remaining: deliberate online P1/P2 actions, distinct-internet operation, bounded
interruption/reinvitation and router-free nearby runtime play. The experiment
remains disabled in ordinary installed launches until those gates pass.

Protocol source: [RetroArch v1.22.2 netplay implementation](https://github.com/libretro/RetroArch/blob/v1.22.2/network/netplay/netplay_frontend.c),
including `netplay_handshake_init`, `handshake_password` and `netplay_mitm_query`;
[command definitions](https://github.com/libretro/RetroArch/blob/v1.22.2/network/netplay/netplay_private.h).

Final delivery: both ordinary shells run SHA-256
`aa9cdc270d407b39275fba3e1cfaa32ca03600eab5b3054ed11216542b2a772e`.
Experimental wrappers were removed. One shell per device, no remaining game or
multiplayer listener, healthy databases, unchanged Emerald saves, boot preferences
and nearby/voice helpers were verified. Flip is muted; Odin volume is zero.
The final caption was inspected on Odin and saved as screenshot 22. A later
invitation was allowed to expire during documentation; the host returned an error
and retained its original game. That is expiry handling, not relay-loss recovery.
