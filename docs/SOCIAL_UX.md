# Social / Together interface - UX-01

**Compact Social revision installed, 8 October 2026:** the first UX-01
composition was rejected for oversized controls and insufficient chat space.
The replacement compact interface and native Fluxer profiles are now installed
on Flip/Odin; see [current interface and evidence](SOCIAL_COMPACT.md). Resume
**MP-02**, preserving its uncommitted timer work. Earlier functional evidence
below remains valid; the old layout is superseded. Owner physical/visual
acceptance and the remaining #135 gates stay open.

8 October 2026. Implements the interface outcome in
[the #127-135 register](EXPANSION_118_135.md#ux-01-one-social--together--party-journey).
This bounded interface delivery does not close #135 or the runtime blocks.

## Shared touch and controller operations

Messages includes DMs and small groups. Communities retains channels; Discover
retains provider-supported people search, community discovery and invitation
links. The former Groups route migrates its remembered conversation after
account/channel loading, rather than selecting a different DM during startup.
Groups retain their channel identities, membership, history and drafts.

A selects; X composes; Y always opens Together inside a conversation, including
while reading history. Select opens context. Explicit keyboard Send and the
persistent conversation Send button use the same captured draft/channel.
Picture, voice-message and call buttons are visible beside the composer.
Pictures use the existing PNG/JPEG/WebP provider; arbitrary files are not newly
supported. Recording stays unavailable during a call. Menu close, group creation,
Earlier and Latest are also directly accessible on screen.

Together summarizes the current game/available group parties and independently
retains the call. Its deliberate action list exposes Join/Ask, party management,
call controls and existing native activities according to capability. Group
Details contains membership, game admission and notifications; account-global
history clearing and sign-out are in Settings > Communication.

## Parties, consent and identity

Home Play together opens people/groups directly. Nearby and online identities
remain separate unless their binding is authenticated; equal names are never
proof. The existing runtime decides the connection and effective capacity.
Home and Social render one party state with the same HomeMenuCard component.
Readiness gates organizer Start. Group policy and the temporary party override
(including Use group setting) remain distinct from membership and voice consent.

Incoming runtime requests join the existing missed-notification destination.
Review/accept/decline actions carry their request ID; expired entries cannot
accept a replacement. Menus retain action identity when rows change, showing an
unavailable selection rather than silently redirecting Confirm. DND, private
background previews, protected overlays and ordinary-game save/exit gates remain.

The walkthrough found and corrected a pre-existing legacy runtime identity bug:
reading an absent `transport` through mutable QJsonObject indexing during the
ordinary-game exit inserted null into the host's accepted descriptor. The read
now uses `value()`; exact guest identity verification remains in force. Pending
invite cancellation also preserves the complete qualified peer ID.

## Checks and remaining acceptance

Final ARM binary SHA-256:
`ddc7773f77896c80ae52cde49f5304ce26706b853675eef283eee922fbfb1e3c`.
Installed and running-image hashes matched on Flip (PID 1264853) and Odin
(PID 359105). The binary includes the preserved pre-existing MP-02 timer work;
those three source hunks remain uncommitted for the MP-02 continuation. No GB
cable/save-wait acceptance is inferred from this interface walkthrough.

ARM compilation and Social, interaction, party and exit-presentation tests pass.
The additional Windows build was interrupted after compilation while linking
unrelated test targets. Restarting the bounded targets hit Ninja dependency-cache
recovery and required another full rebuild; Windows validation is not claimed.
The ARM build, tests and installed-device checks provide this delivery's proof.
The affected wider regression run passed online Link, process lifecycle,
adventure exit, overlay transport and exit QML smoke. Core, aggregate QML smoke
and persistence-process failures were reproduced against unchanged `826879d`:
two old Worlds-navigation expectations, the same World smoke stages 6/7/9 and
three previously recorded persistence subprocess failures. They are not marked
fixed by UX-01; retain the contributor/test-readiness gate.

Actual Flip/Odin checks used the installed native application and its real
development accounts. Remote pointer and controller events exercised the same
QML/controller actions, without calling provider operations from a harness:

- Opened the picture picker, composed on the on-screen keyboard, retained the
  draft with Back, and sent one test message through the persistent Send button;
  the peer received it. Group and DM entries remain distinct in one inbox.
- Started/joined the existing muted group call through Call. Home Play together
  selected the group directly. The authorized member joined from Together;
  Home/Social showed the same two-seat roster/readiness and organizer Start.
- Both devices reached actual NES Pong gameplay. Guest Home > Exit returned to
  the conversation; Together > Join returned to the running host. Both then
  exited through Home normally. This tests the changed UI, not new network scope.
- The group call survived game launch, guest leave/rejoin and conversation
  switching. Workers 1227803 (Flip) and 339717 (Odin) remained active after the
  rejoin; both system outputs stayed at 0%, with no microphone recording streams.
- DM Together > Activities probed the peer, offered the supported native
  activities and sent an Exchange invitation. Home Notifications opened its
  explicit Accept/Decline. Acceptance reached the existing Link Trade workspace
  on both consoles. The walkthrough stopped before selecting/confirming an
  exchange: it proves entry and routing, not new save-write or trust acceptance.
  An earlier unanswered invitation expired into Missed invitation as intended.

Curated captures in `screenshots/social-2026-10-08/` show the installed interface.
Only the two obsolete Social images referenced by README are replaced; other
captures and the separate #116 artwork review remain intact.

Both devices remain quiet; physical touch/button feel,
human speech/headsets, distinct networks, four real users, later runtime families
and Hotseat keep their earlier separate acceptance gates. No universal runtime,
artwork-rights, installer or final-review acceptance is implied.
