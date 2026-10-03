# Runtime game parties

2026-10-03 implementation checkpoint within communication block 1, which remains
open. This replaces the runtime invitation's single-peer coordinator; saved-game
Link and calls retain their separate providers. The accepted broader experience
is in [MULTIPLAYER_EXPERIENCE](MULTIPLAYER_EXPERIENCE.md).

## Implemented behavior

- Home during a supported game invites a nearby Trainer or online friend.
  Accepted players appear in the same compact menu. The organizer explicitly
  starts the game; the first acceptance no longer starts it prematurely.
- Invited places are reserved. Cancelling/declining/expiry releases a reservation;
  accepted participants can leave. Running signalling loss does not free their
  controller place or terminate their emulator/call.
- A compatible friend's current game appears above the existing conversation.
  Touch **Ask to join** or controller **Y** sends a reverse request. The organizer
  receives Accept/Decline; during play Home contains that request. No extra
  conversation-opening or accepted-invitation acknowledgement is required.
- Start reuses existing native RetroArch/PPSSPP preparation and normal owned
  game exit. PSP restart still asks about saved progress. An unrelated guest
  game is never silently replaced.
- Party messages use their own bounded, expiring namespace over friend DMs or
  multiple local TCP connections. Account identity comes from the DM sender;
  nearby identity is a local device claim, not competitive authentication.
  Compatibility compares the exact existing runtime profile. No VPN setup is
  added. Protocol messages do not appear as chat or notifications, evict visible
  live messages, or overwrite ordinary unread state. Loading a protocol-only
  history page automatically looks back through a bounded number of pages.

## Capacity and boundaries

`GameParty` coordinates two to four seats from the exact descriptor, defaulting
to two. Current installed runtime profiles are **still two-player profiles**.
The four-member state-machine tests do not establish four-controller emulator
support. Adding a larger real profile requires its controller/network setup and
real multi-client proof; it is not enabled by this coordinator alone.

One device joins one runtime party. Persistent companies, parallel parties within
one company, membership-based free entry and separate party voice audiences are
still pending. A party currently uses one route (nearby or online). Ordinary chat
and the existing call stay independent. Public Fluxer clients can see the textual
coordination envelopes; only TrainerOS filters them. This is not a hidden upstream
presence feature or a general-purpose company service.

The production feature gate stays **off** until the existing runtime/recovery
acceptance passes. No block 1 or #107 closure is implied.

## Verification

Automated coverage checks four-member explicit startup, unique seats/shared
endpoint, capacity, cancellation, reverse admission, wrong editions, late/replayed
responses, expiry, quiet background failures and retained running seats. A real
local TCP test routes three guest sockets independently. Existing Social, saved
Link, runtime and Home-exit checks cover the shared boundaries. Home navigation
skips non-actionable roster rows; background discovery failure shows no modal.

Eight isolated ARM suites passed: game_party, link_peer, social, online_link,
retroarch, interactions, adventure_exit and adventure_exit_presentation.
The final Windows application build and game_party, social and
adventure_exit_presentation suites also passed.
The last presentation-only change removes an unknown 0/2 count while a reverse
request is still awaiting acceptance; the ARM application was rebuilt afterwards.

On Flip/Odin, the new LAN invitation reached the second device, acceptance kept
both players waiting, the organizer saw 2/2 and explicitly started. The existing
PSP save confirmation was retained; both standalone PPSSPP processes launched.
This is launch/coordination evidence, not a Lumines VS match.

The public Fluxer friend-DM route was then exercised in reverse: Odin's actual
conversation showed Flip's running Lumines, Y sent the request, and Flip's Home
showed EriArk's Accept/Decline. Acceptance produced the 2/2 roster with focus on
Start game. Starting and confirming the PSP restart automatically launched Odin's
own copy. Actual process environments used private configuration roots; both INIs
selected `socom.cc`, WLAN enabled and UPnP disabled. No manual address/provider
step was presented. This proves online signalling and relay-configured startup,
**not** relay gameplay, a VS match, separate internet networks or fresh onboarding.
An unanswered earlier request expired without closing the original game.

Actual Gamescope captures:
[friend activity](../screenshots/25-experimental-ask-to-join.png),
[incoming request](../screenshots/26-experimental-join-request.png),
[accepted roster](../screenshots/27-experimental-game-party.png).
They used development build
`9c1afa4748de963798ecc1e4590c6ed8ac2e6a72a40367d0abaf54c6c7f6e484`.
Both PSP instances were exited through normal Home controls after the check.

Final ordinary Flip/Odin delivery has executable SHA-256
`9f5b528ed1ee37f26f2b038018ed19ddb87cc512599e792aa199918b41acde15`.
Both running processes match that build, without the experimental flag, emulator
processes or runtime invitation/game listeners left behind. Database counts,
boot preferences, nearby/voice helpers, input service and recorded Emerald save
hashes passed the final preservation checks. Odin remains at zero volume; Flip
remains muted. No device reboot was needed.

## Next unresolved work

Continue the agreed block's company membership/parallel-party/access model,
then its real runtime acceptance. Preserve the existing Lumines VS group-transition
failure, full RetroArch paired-control/recovery, distinct-network and router-free
gates, and the deferred human audio/controller checks. Do not repeat already
passed invitation-only checks as a substitute for those results.
