# Multiplayer experience: companies, game parties and joining

**Planned UX-02 entries, 9 October 2026:** [precise map](UX_OPTIONS_MAP_RU.md).
Home/library game, DM/group and live Game Options all invoke one GameParty flow
with known game/recipient prefilled. Live-game context is never borrowed from Home
selection. Minimize/Return retains process, network timing, party/save ownership
and independent group call; unsupported runtime handoff stays unavailable.
The [new DS requirements](EXPANSION_161_173.md) belong to MP-06; they do not establish
installed DS netplay or change existing consent, admission and route evidence.

**Target refinement, 2026-10-08:** [#126–135](EXPANSION_118_135.md) add distinct
live Hotseat and reconcile Social/Home around Together and one party surface.
Choose people/groups before automatic transport; Nearby/Online normally become
status rather than a required first choice. Existing capability, admission,
save, voice and independent-party rules remain. Owner order: UX-01 first using
working routes, then MP-02 and the preserved oldest-first runtime queue. Hotseat
and future-route proof are separate later gates; no transport rewrite is implied.

Owner agreement, 2026-10-03. **Accepted target, not delivered behavior.**

The first [runtime-party implementation checkpoint](GAME_PARTIES.md) now covers
the bounded coordinator, direct invitation/request surfaces and group-backed
companies with selected-member entry. It does not complete the acceptance list
below, particularly real three-to-four-player runtime proof and the remaining
background-audio checks. Owner clarification, 2026-10-04: use one shared group
call across independent game parties; separate party voice rooms are not required.
[ROADMAP](ROADMAP.md) owns scheduling; [runtime evidence](EMULATOR_MULTIPLAYER.md)
owns implementation claims. [Transport audit](MULTIPLAYER_TRANSPORT_AUDIT.md)
contains hypotheses, unresolved choices and the proposed comparison.

## Connection priority

Internet play is the primary everyday scenario (owner clarification, 2026-10-03).
Local LAN and direct nearby play remain first-class supported targets. Preserve
one familiar invitation flow; resolve transport automatically after the user
chooses Nearby or Online friend. Do not substitute a same-router match for
internet acceptance or require player-entered IPs, router settings or VPN
administration. Runtime choice and implementation order follow this priority.

## Three independent activities

- **Play together:** the actual game's multiplayer through its emulator.
- **TrainerOS Link:** supported save-based trades, sales, gifts and battles.
  These retain their exact-game transaction/confirmation gates; an open game
  party never authorizes money, stakes or save changes.
- **Call:** communication that survives normal navigation and game launch/return.

Reuse Social identities, supported provider permissions and existing Home
controls. Ordinary contacts keep messaging/calls without being TrainerOS clients.
Game participation requires live compatible-client capability, never a username
suffix or a presence label alone. Advertise the active Trainer name nearby.

## A company can contain several game parties

| Concept | Lifetime and ownership |
| --- | --- |
| Company | Persistent named group, membership and shared conversation across games. The implemented first mapping reuses Fluxer private groups; larger community facilities retain their separate acceptance. |
| Game party | One running or preparing game session with its own organizer, participants, game/mode, capacity and access policy. Several parties may coexist within a company, including separate sessions of the same game. |
| Voice conversation | One shared company/group call. Friends can play different games and join separate parties while talking together. Game transitions never switch or terminate the call or change microphone consent. |

Example: eight company members form a four-player party and a separate two-player
party, while two people only chat. Company size is independent of game capacity.
Do not confuse these game parties with the save-derived Pokemon Party feature.
Any eligible company member can start another party; there is no single company
game host. Provider membership/permission limits still apply.

## Direct entry points

| Entry | Result |
| --- | --- |
| Physical Home during play -> Invite friend | Nearby / Online friend; current game is already selected. No second game picker or connection wizard. |
| Conversation/person card -> Invite | Invite that person; choose a game only when the context has not supplied one. |
| Friend's current-game activity -> Request to join | Send a request naming the requester and exact target party/game. Organizer gets one compact Accept/Decline popover. |
| Activity/party with free access -> Join | User chooses to join; admission, preparation and connection run automatically without another organizer prompt. |
| Company conversation | Show each party's game, participants, free places and direct Join/Request action. Selecting one does not lead through a separate lobby dashboard. |

Full parties show no free places. Unsupported multiplayer has no misleading
Join action. Missing content, incompatible runtimes and ended sessions report the
actual condition; do not offer per-game binding or silently launch another title.
Invitations contain the intended activity, not a preliminary generic pairing
step followed by another redundant approval. Incoming notifications preserve the
current game/page and existing DND/privacy behavior.

## Access and consent

Three policies: **By request**, **Selected company members**, **Closed**.
Company free-entry policy is explicitly enabled, can be inherited by a game
party and overridden there through compact Home controls. Existing chat-group
membership does not silently enable open game access. Revoking access prevents
new admission; membership/access changes must be rechecked when joining.

"Automatic" means the guest deliberately presses Join and preparation is
automatic. A friend becoming online or starting a game never launches games on
other members' devices. A guest accepts an invitation explicitly; free-entry
membership replaces the organizer's repeated approval, not the guest's intent.

Acceptance must not silently close an unrelated running game or discard progress.
Use the existing save/exit flow when switching or restarting is necessary, and
explain the restart before it happens. Decline, cancellation and expiry preserve
the originating game/page/call and prevent a late launch.

## More than two participants

- Organizer can invite several people, or add people to an existing party.
- Show real participants/readiness and occupied/available places, such as 3/4.
  Capacity comes from the game, mode and verified runtime configuration, not
  an emulator-wide theoretical maximum or company size.
- After acceptance, prepare automatically. When coordinated startup is required,
  the organizer starts once the intended participants are ready; do not launch
  prematurely after the first reply or require every company member to attend.
- Mid-game admission and waiting until the next round follow actual runtime/game
  capability. Concurrent requests for the last place cannot overfill the party.
- Guest departure releases its place. Organizer departure follows emulator
  capabilities; do not promise transparent host migration or kill another
  person's game unnecessarily.
- Mixed nearby/remote parties are a desired scenario with a separate feasibility
  gate, not an artificial UI prohibition or currently verified support.

## Automatic work and deliberate choices

TrainerOS resolves local content, compatibility, technical host (normally the
organizer), controller slots, supported route and temporary emulator settings.
IPs, ports, room secrets and relay selection stay out of ordinary interaction.
Do not invent universal identical-ROM rules for games with supported paired
editions; each runtime profile defines its real compatibility requirements.

People choose participants, game/mode, invitations, access, microphone/call and
any save-affecting exit. Keep a game's native Create/Join or character selection
when no reliable integration exists; avoid fragile scripted menu presses.
Bounded reconnect may be automatic only when the current session permits it.
A new launch, disruptive network switch or lost-progress risk remains explicit.

Nearby and Online friend describe the situation, not a fixed transport. Reuse
LAN when suitable; router-free play needs its own proof. Preserve the Bluetooth
default for native Link and the Wi-Fi Direct deferral. Never silently disconnect
internet/voice to make a local route. Fluxer carries social coordination; the
emulator's route carries gameplay. No virtual-LAN product is selected yet.

### Connection setup requirement

Owner clarification, 2026-10-03: connecting must be fast and automatic from the
player's perspective. At most initial account registration/sign-in belongs in
the first-run wizard. Normal play is Invite -> Accept -> automatic preparation,
or deliberate Join under the agreed access policy. Existing game/save consent
remains; network administration is not an additional player task.

No separate VPN installation/configuration, network IDs, manual addresses, port
forwarding, provider dashboard visits or manual device approval by players.
TrainerOS must handle supported provisioning, session access, route setup and
cleanup, and ordinary reconnection after reboot/network changes. If direct
connection is unavailable, use a supported automatic relay when the selected
route provides one; otherwise report the actual failure, not a router tutorial.

An already configured developer network does not prove this requirement.
Evaluate automation APIs, account model and operational requirements before
committing to a transport. Verify two freshly configured devices/accounts through
the normal wizard, invitations and return after reboot; record connection time
and every required user action. No specific latency target or provider is assumed.
The necessary provisioning/relay service may be operated for the product, but
must not become a server that every family has to configure.

## Implementation checkpoint — 2026-10-03

Existing group DMs now supply persistent company membership and parallel party
cards. An organizer's per-company preference is inherited by new parties and may
be overridden per party; it never authorizes other organizers' games. Selected
members enter through deliberate Join without a second host prompt. Flip/Odin
company admission and startup are verified; independent two-party coordination
has automated four-client coverage. Full multi-party gameplay remains open.
The shared-call device check is recorded in GAME_PARTIES.md; physical speech
quality remains separately deferred. See [exact evidence](GAME_PARTIES.md).

## Acceptance additions (whole-block acceptance remains open)

- [ ] Fresh-device online setup requires at most the wizard account step;
  invitations/Join, routing and ordinary reconnection need no VPN administration.
- [ ] Named invites and reverse requests from actual game activity, including
  decline/cancel/expiry, unavailable games and stale session destinations.
- [ ] Deliberate one-action Join with company free access; policy inheritance,
  per-party override and changed membership/access checked at admission.
- [ ] Persistent company with two simultaneous independent parties; ending,
  declining or filling one does not affect the other or the common chat/call.
- [ ] Real 3-4 participant game, correct independent controls, capacity race,
  organizer startup, late-join restrictions, departure and reinvitation.
- [ ] One shared group call survives independent game-party and game transitions
  without involuntary call/microphone changes. Synthetic two-device continuity is
  verified; physical speech/headset checks remain deferred.
- [ ] Controller and touch journey remains direct in existing Social/Home;
  device screenshots demonstrate the landscape composition.
- [ ] Existing block 1 transport/gameplay/save/recovery and block 3 audio gates
  remain separate; two-device delivery does not prove four-client play.

Protocol/storage details are intentionally not frozen by this document. Changes
must extend existing narrow services and preserve provider permissions, owner
isolation and every earlier acceptance; this is not a second social platform.
