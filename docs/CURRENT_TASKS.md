# Current tasks

Updated 2026-10-04. Execution order lives in [ROADMAP](ROADMAP.md#active-execution-queue--2026-10-04).
This register decomposes accepted work; it is not a second roadmap or a promise
that each subtask receives a separate turn. Issues and existing evidence retain
their full acceptance. A checkbox means the stated outcome is delivered, not
merely that code exists. Do not close a parent block from partial child evidence.

## Current task: MP-01 — usable online parties on the older RetroArch routes

**State:** in progress; substantial implementation exists, acceptance incomplete.
**Parent:** communication block 1, [#107](https://github.com/EriArk/TrainerOS/issues/107),
expanded by the owner's company, capacity, reverse-request and oldest-first rules.
**User outcome:** invite an online friend or request to join their game; play
with the supported number of people; leave/rejoin without damaging ordinary
play or interrupting the group's call. A company can run independent parties.
No player IP entry, router setup or VPN administration.

### Remaining work, completed together with necessary fixes

- [ ] Finish the actual multi-user TrainerOS journey: invitations, accepting and
  declining, reverse join requests, capacity/readiness, seat assignment and
  authorized group entry. Reuse the existing coordinator/UI; fix demonstrated
  gaps instead of building another lobby or generic session framework.
- [ ] Complete parallel-party behavior through actual TrainerOS accounts and
  normal UI: members select the intended party; capacity, permissions, starts,
  exits and rejoining are independent. Company size is not game capacity. One
  shared group call survives game/party changes without new microphone consent.
- [ ] Complete the first exact route across separate internet connections using
  Invite/Accept/Join, real independent gameplay and clean Home return. Verify
  bounded peer departure/loss/rejoin on that route; fix failures before enabling
  it. A public relay reached from one home network is not this acceptance.
- [ ] Reconcile the remaining route differences for NES, Mega Drive and SNES:
  effective capacity, compatibility, saves/session settings, installed profile
  availability and cleanup. Reuse passed paired gameplay and isolation evidence;
  only exercise differences or changed behavior. Avoid testing every title.
- [ ] Enable only the exact supported profiles whose required gates passed;
  keep others honestly unavailable. Update the matrix, maintenance record and
  acceptance evidence, deliver both available handhelds and commit/push.

**Completion:** all applicable items above are fulfilled and remaining scope is
explicitly resolved. An unavailable external condition keeps MP-01 open; it is
not a new feature called "test multiplayer." Do not claim all of block 1 complete
while later accepted emulator families remain unfinished.

### Evidence already available — do not repeat as new stages

| Available evidence | Source / limit |
| --- | --- |
| NES two-player online invitations, reverse request/group entry, independent controls and loss/exit/rejoin | [NES](NES_MULTIPLAYER.md); same-router devices, not distinct internet connections |
| SNES Contra and Mega Drive Streets of Rage 2 independent P2 and loss/exit/rejoin | [Runtime evidence](EMULATOR_MULTIPLAYER.md); reuse for unchanged paths |
| Four Score seats, P3/P4 independent input and reversed join order | [NES](NES_MULTIPLAYER.md), `a2b5d12`; four emulator clients, not four real TrainerOS users |
| Concurrent public-relay rooms, independent lifetimes, private core options and ordinary settings preservation | [RetroArch record](emulators/retroarch.md), `b83d4bc`; temporary fixture clients, not complete company UI acceptance |
| Shared group-call synthetic continuity through gameplay/loss/exit/rejoin | [Game parties](GAME_PARTIES.md); human speech/listening still separate |

### External acceptance conditions

| Missing condition | What it closes | What it does not justify |
| --- | --- | --- |
| Actual compatible TrainerOS clients/accounts for the claimed larger and concurrent sessions | End-to-end admission, UI and lifecycle beyond two players | Calling fixture processes extra users; redoing already passed emulator seats |
| Two independent internet connections, with target devices reachable for recovery | Actual internet route, admission and recovery | Treating two devices behind one router as separate-network proof |
| Owner available to listen/speak, with usable microphone input | Remaining block 3 human audio quality/interoperability acceptance | Repeating synthetic audio checks or blocking unrelated permitted implementation |

Verify availability when needed; these are requirements, not claims that today's
devices are unreachable. Do not create accounts, send messages or alter unrelated
network infrastructure without applicable authorization. If no independent work
remains, state the prerequisite and wait instead of looping.

## Subsequent block 1 tasks — oldest first

Every row includes automatic invitation/compatibility/preparation, actual game
input, graceful exit/recovery, save/config protection, clear unsupported states,
both-device delivery and a maintained emulator record. Internet is primary;
LAN and the accepted router-free nearby route remain acceptance, not substitutes.
Use [standard runtimes](EMULATOR_STANDARD.md), the [route matrix](EMULATOR_MULTIPLAYER_MATRIX.md)
and [accepted experience](MULTIPLAYER_EXPERIENCE.md). No new general transport
survey unless a concrete route requires it; virtual LAN remains conditional.

| ID | Whole outcome | Dependency / retained work |
| --- | --- | --- |
| MP-02 | GB/GBC/GBA supported internet link between separate emulated handhelds | MP-01. Resolve actual link support per runtime/game; shared-controller netplay does not meet this requirement. |
| MP-03 | Selected PS1/N64 multiplayer routes integrated into the same invitation experience | MP-02. Mechanism and capacity follow the runtime; unsupported link/game modes remain explicit. |
| MP-04 | Dreamcast supported online/LAN mechanism with automatic connection | MP-03. Reuse Flycast package/maintenance work; installation is not multiplayer. |
| MP-05 | PS2/GameCube viable multiplayer routes | MP-04. Preserve existing ARMSX2; do not imply every PS2 game supports online play. Reuse Dolphin paired proof; finish its remaining internet/capacity/recovery gates. |
| MP-06 | DS/PSP viable online/link routes | MP-05. Reuse PPSSPP Lumines LAN/relay proof; retain mid-round loss, admission/isolation and distinct-network gaps. DS needs its own mechanism. |
| MP-07 | 3DS and eligible newer families, then reconcile block 1 coverage | MP-06. Source-backed support decisions and representative complete routes, not every ROM or obscure platform. Close block 1 only against the entire accepted scope. |

These are task families inside the accepted block, not permission to split a
promised complete delivery into a succession of handshake-only sessions. At each
start state the actual outcome being completed; do not promise every platform
until the remaining support and prerequisites are understood.

## Retained communication tasks

| ID | State / complete outcome | Acceptance source |
| --- | --- | --- |
| COM-02 | Delivered everyday messenger baseline; retain residual accepted expansion and fix real regressions without restarting the baseline | [Social](SOCIAL.md), #100/#101 and queued expansion |
| COM-03 | Open: remaining notifications/calls acceptance, including real speech and background conversation; human checks deferred by owner | [Media/voice](SOCIAL_MEDIA_VOICE.md), #102/#103 |
| LINK-04 | Queued after block 1: native online battle/trade/sale/gift with consent and protected two-endpoint settlement/recovery | [Emerald Link](EMERALD_LINK.md), #104/#105/#109/#110, #93/#94 trust prerequisites |
| REVIEW-05 | Queued: complete #113 reviews inside existing Properties and final integrated communication UX audit; include residual accepted messenger functionality | [Reviews](ADVENTURE_REVIEWS.md), [Social](SOCIAL.md) |

## Preserved project backlog

The [R1–R18 queue and preservation map](ROADMAP.md#unified-execution-order--existing-work-and-new-issues)
remain intact. After communication, perform remaining work in that order, never
repeat delivered foundations: shop/adapter/navigation residuals; companion/save
features and RA; varied-game adapters; runtime readiness and Steam; device/input,
atmosphere and portable Trainer; maintenance; first boot/help; image and OTA;
final acceptance; artwork/sprite packs and Pack Studio; conditional ROM extraction
and final integration. R7a/R18a/R18b and older U/P acceptance remain included.
ScreenScraper waits for owner access. Suspend stays deferred. Preserve Playroom
and current artwork. This register removes no issue or acceptance requirement.

## Updating this register

At each delivery update the affected task's state and remaining checkboxes, with
a link to evidence and source revision. Record actual implementation, actual
device behavior and external acceptance separately. Keep detailed logs in their
existing feature documents; do not prepend another competing queue. Before the
next pass ask: what new user outcome remains, and why would any passed check need
repeating? If the answer is only unavailable acceptance, report that honestly.
