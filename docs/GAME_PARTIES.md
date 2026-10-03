# Runtime game parties

## Shared group call checkpoint - 2026-10-04

The owner clarified that one ordinary group voice call serves friends playing
different games or forming independent game parties. Separate per-party voice
rooms are removed from scope. No extra voice selector or group migration is needed.

Using the existing public-Fluxer test group on Flip/Odin, both joined the same
call before launching ordinary games. Flip ran Streets of Rage 2 and announced
its own 1/2 party; Odin ran NES Pong and announced a separate 1/2 party in that
group. Both voice workers retained their original PIDs through game launch and
party creation. An isolated original 440 Hz signal sent from Flip was received
on Odin (peak 1336, measured tone amplitude 1200.85). No physical microphone was
recorded; physical outputs stayed silent.

Odin explicitly ended its party, then exited NES Pong through the ordinary Home
save question. Flip's game/party remained active. A fresh signal after that exit
still reached Odin (peak 1284, amplitude 1203.71) through the same voice worker.
Flip then exited normally; its Home menu still offered the existing call.
These are two independent organizers and two ordinary games, not four-client
gameplay, simultaneous paired netplay sessions or physical speech-quality proof.
Both devices were on one router. Remaining distinct-network, larger-gameplay,
router-free and deferred human audio checks remain open.

Actual device captures show [Flip's game and call](../screenshots/57-group-call-game-flip.png),
[Odin's independent party](../screenshots/58-group-call-party-odin.png), and
[the retained call after Flip's return](../screenshots/59-group-call-return-flip.png).
No application code changed in this checkpoint; both use executable SHA-256
`7a1f591d808909fa6700978fd2889a382c7e0e686f02afc31b0b0bbc142806e9`.

During final Social navigation on Odin, compositor captures stopped updating.
After shell termination, one thread remained uninterruptible in
`dma_fence_default_wait` (`traine:traceq0`). Audio had continued before cleanup.
This is an unresolved graphics recovery failure, not a passed navigation check;
the symptom alone does not identify its cause. Temporary audio modules/defaults
were restored and ordinary experimental-off binaries installed before recovery.
Flip restarted normally; Odin required remote reboot after its game had exited.
The reboot command was accepted, but SSH had not returned at final verification;
Odin's post-reboot live process is therefore not verified. Flip's final live SHA,
experimental-off environment and database integrity passed. Both devices' database
integrity, boot/helper preservation and audio-route cleanup passed before reboot.
Odin remained at volume zero; Flip remained muted (20% battery).

**Recovery follow-up, 2026-10-04:** after the owner reported another reboot,
Odin returned over SSH in Steam Gaming Mode. The installed session control
started TrainerOS, with its original Steam boot preference restored afterward.
The live executable SHA above, experimental-off environment, database integrity
and absence of RetroArch passed. Fresh compositor captures showed Home and
controller navigation to Worlds. Volume remained zero and battery was 100%.
Recovery is verified; the cause of the earlier GPU wait is still unresolved.

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

One device joins one runtime party. Group-backed companies, parallel party discovery
and selected-member free entry are implemented below. One shared group call
serves all parties (owner clarification, 2026-10-04); separate party voice rooms
are not required. A party currently uses one route (nearby or online). Ordinary chat
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

The preceding ordinary Flip/Odin delivery had executable SHA-256
`9f5b528ed1ee37f26f2b038018ed19ddb87cc512599e792aa199918b41acde15`.
Both running processes match that build, without the experimental flag, emulator
processes or runtime invitation/game listeners left behind. Database counts,
boot preferences, nearby/voice helpers, input service and recorded Emerald save
hashes passed the final preservation checks. Odin remains at zero volume; Flip
remains muted. No device reboot was needed.

## Next unresolved work

The corrected Lumines profile now has integrated LAN and external-relay gameplay,
plus recovery through a new native challenge after brief rematch-start packet loss;
see [current PSP evidence](PSP_MULTIPLAYER.md). Earlier startup-only checkpoints
below remain historical. Continue the agreed block's real runtime acceptance and
shared group-call continuity: larger/concurrent gameplay, remaining route-specific
recovery, distinct-network and router-free
gates, and the deferred human audio/controller checks. Do not repeat already
passed invitation-only checks as a substitute for those results.

## Group-backed companies — 2026-10-03

The persistent company is an existing Fluxer group DM: its name, people and chat
remain provider-owned. Home -> Invite friend -> Online friend also lists groups;
selecting one announces the current supported game. Other members see its party
above the existing group conversation. Right selects that strip, Left/Right moves
between parties, A joins, Down returns to messages and X composes. Touching a
party joins it directly. The joined party becomes one status strip rather than a
second duplicate card. Empty groups keep their ordinary conversation layout.

Each organizer saves **Who can join my games** per Trainer/account/group:
Ask me first, Selected members can join, or Invitations only. Selected mode includes
an explicit member checklist. New parties inherit this preference. Home's
**Who can join** overrides it for this party or restores the group preference;
it does not rewrite the persistent preference. These are organizer-owned defaults,
not permission for a group administrator to open somebody else's game. Ending or
leaving a party does not leave the group or change the call. Per the owner
clarification on 2026-10-04, this shared call is the intended group model;
independent voice rooms are not required.

A company can contain more people than one game supports. Each organizer has a
separate UUID, roster, admission queue and capacity; multiple organizers advertise
independent parties in the same group. A device itself still owns only one game.
Joining is always the guest's explicit action. Allowed members bypass only the
organizer's repeated prompt, not game compatibility/capacity or save/exit consent.

Announcements/query packets go to the group and expire within 90 seconds. Their
strict field allowlist excludes endpoints/admission messages. Admitted players
receive startup details in private DMs scoped to the company. Existing membership
is checked on receipt and again when queued packets are sent; members need not
also be mutual friends, but provider DM privacy/block/rate limits still apply.
Recipient removal invalidates access immediately, before the asynchronous group
refresh. Running signalling loss does not kill an emulator. A failed group
announcement reports the failure instead of presenting a successful room.
Public Fluxer clients can see these ordinary text envelopes, as with earlier
runtime coordination; TrainerOS filters them. This is not hidden rich presence.

Provider basis: the pinned upstream [private-channel contract](https://github.com/fluxerapp/fluxer/blob/cdcaba34ce538ee75c73feaa5aedc0331085add9/fluxer_docs/src/content/docs/http-api/users/private-channels.mdx)
was reread for group ownership, recipient semantics and DM creation. No new VPN,
service account, duplicate per-party chat or user networking setup is introduced.

### Evidence and remaining boundaries

Automated coverage now includes two independent hosts and guests in one group,
private endpoint delivery, selected-member admission with another request pending,
closed access, expiry, cross-group rejection, immediate membership removal,
queued-send revalidation, blocked/non-friend members, controller party selection,
joined-card deduplication and owner-scoped persistent preferences. All eight
isolated ARM suites listed above passed after the final correction.

On the actual Flip/Odin test accounts, the existing persistent group was reused.
Flip selected Odin as a free-entry member using the controller. Its running
Lumines was announced from Home, appeared above Odin's group chat, and Odin's
Right/A entered the 2/2 roster without a Flip consent popup. Flip's explicit Start
retained PSP's save question and then launched both standalone PPSSPP instances.
Both process environments used private netplay configuration roots; both selected
`socom.cc`, WLAN enabled and UPnP disabled. Both were exited through normal Home
controls. This proves the company admission/startup chain, not a successful
Lumines VS match, four physical clients or independent internet paths.
Existing runtime failures/gates remain open.

The final presentation check removes the duplicate joined card and corrects the
Right-button hint. After restarting both applications, the retained selected-member
preference again admitted Odin without an organizer prompt. Actual handheld captures:
[company game](../screenshots/28-experimental-company-party.png),
[joined party](../screenshots/29-experimental-company-joined.png),
[member preference](../screenshots/30-experimental-company-access.png).
The last correction preserves Up-to-earlier-history while parties are visible
and distinguishes closed/waiting parties from running games. Eight ARM suites
passed again, including the history-navigation regression.
The final Windows application build and both affected suites (`game_party` and
`social`) also passed.

Final ordinary Flip/Odin processes both run executable SHA-256
`e7e281760dd4e9196f124be9e836f2bf9fb872de9988cbc67eddd56c898598fe`.
The experimental flag is off; no emulator or runtime invitation/game listeners
remain. Database integrity/counts, boot preferences, nearby/voice helpers and
input services passed preservation checks. The recorded Emerald save hashes
remain unchanged. Odin volume remains zero and Flip remains muted. Only the shell
processes were restarted; neither device required a reboot.
