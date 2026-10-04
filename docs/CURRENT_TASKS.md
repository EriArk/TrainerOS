# Current tasks

Updated 2026-10-04. Execution order lives in [ROADMAP](ROADMAP.md#active-execution-queue--2026-10-04).
This register decomposes accepted work; it is not a second roadmap or a promise
that each subtask receives a separate turn. Issues and existing evidence retain
their full acceptance. A checkbox means the stated outcome is delivered, not
merely that code exists. Do not close a parent block from partial child evidence.

## Current task: MP-01 — classic RetroArch family and shared online-party acceptance

**State:** existing NES/Mega Drive/SNES exact-game routes are implemented and
have paired gameplay evidence. Shared multi-user and distinct-network acceptance
is incomplete; ordinary builds remain experimental-off. No missing NES transport
implementation was identified in the 4 October continuation. Do not describe
MP-01 as implementing NES again. The shared core/platform expansion is now
implemented; its broader coverage is not universal game-level runtime proof.
See [profiles and evidence](RETROARCH_MULTIPLAYER_PROFILES.md).
**Parent:** communication block 1, [#107](https://github.com/EriArk/TrainerOS/issues/107),
expanded by the owner's company, capacity, reverse-request and oldest-first rules.
**User outcome:** invite an online friend or request to join their game; play
with the supported number of people; leave/rejoin without damaging ordinary
play or interrupting the group's call. A company can run independent parties.
No player IP entry, router setup or VPN administration.

### Implemented baseline — retain, do not restart

- [x] Existing exact NES, Mega Drive and SNES profiles with paired invitation,
  independent gameplay and explicit loss/exit/rejoin evidence.
- [x] Shared coordinator for invitations/reverse requests, capacities/seats,
  company access and independent parties. Automated and bounded two-user UI
  evidence exists; it is not full four-user acceptance.
- [x] NES Four Score port assignment and independent P3/P4 emulator input.
- [x] Concurrent runtime room lifetimes and ordinary core-option isolation.

- [x] Replace the test-title-only boundary with reviewed classic core/platform
  profiles using the existing invitation/relay flow; retain exact profiles,
  private settings/saves and controller exceptions. Generic baseline is two;
  metadata-selected three/four seats now exist for Snes9x and PCE-family layouts.
  Established exact NES Four Score remains four. See the profile record for
  source-backed coverage versus actual device evidence.
- [x] Three/four-seat controller preparation and three-seat admission/rejoin
  regression coverage. The four-client FBNeo relay probe confirms assigned seats,
  not independent four-player gameplay. FBNeo automatic admission stays two:
  Battle Circuit's default cabinet was two-player. No four-user acceptance implied.

### Remaining acceptance and release work

The following items are acceptance gaps, not evidence that those mechanisms are
absent. Fix concrete failures found while completing them; do not invent a new
implementation task when an external condition is all that is missing.

- [ ] Close the actual multi-user TrainerOS acceptance: invitations, accepting and
  declining, reverse join requests, capacity/readiness, seat assignment and
  authorized group entry. Reuse the existing coordinator/UI; fix demonstrated
  gaps instead of building another lobby or generic session framework.
- [ ] Close parallel-party acceptance through actual TrainerOS accounts and
  normal UI: members select the intended party; capacity, permissions, starts,
  exits and rejoining are independent. Company size is not game capacity. One
  shared group call survives game/party changes without new microphone consent.
- [ ] Complete the first exact route across separate internet connections using
  Invite/Accept/Join, real independent gameplay and clean Home return. Verify
  bounded peer departure/loss/rejoin on that route; fix failures before enabling
  it. A public relay reached from one home network is not this acceptance.
- [ ] Reconcile concrete route differences across the reviewed classic profiles:
  effective capacity, compatibility, saves/session settings, installed profile
  availability and cleanup. Reuse passed paired gameplay and isolation evidence;
  only exercise differences or changed behavior. Avoid testing every title.
  Concrete findings: ordinary Flip/Odin FBNeo BIOS manifests differ; broad known
  BIOS hashing can reject an unused dependency. Arcade title maximum also does
  not establish cabinet mode. Resolve these without enabling unverified capacity
  or replacing the owner's ordinary BIOS/configuration wholesale.
- [ ] Enable only supported profiles whose required gates passed;
  keep others honestly unavailable. Update the matrix, maintenance record and
  acceptance evidence, deliver both available handhelds and commit/push.

**Owner deferral, 2026-10-04:** separate-network testing waits until the owner is
home. Do not request another network switch or repeat same-router tests while
waiting. Multi-user acceptance and release enablement remain open separately.
The owner subsequently clarified that other classic RetroArch systems come
before handheld link, grouped by their common mechanism. That generic expansion
is implemented in this delivery. Before MP-02, resolve concrete classic-family
gaps and the shared release decision; do not schedule a separate transport
project for every console or repeat passed scenarios. The FBNeo probe established
connection/launch, not independent simultaneous controls; retain that distinction.

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
| Generic classic profiles, content/BIOS/track compatibility and renamed-title invitations | [Profiles](RETROARCH_MULTIPLAYER_PROFILES.md); automated coverage plus bounded FBNeo connection/launch, not all-title gameplay |
| Shared group-call synthetic continuity through gameplay/loss/exit/rejoin | [Game parties](GAME_PARTIES.md); human speech/listening still separate |

### External acceptance conditions

| Missing condition | What it closes | What it does not justify |
| --- | --- | --- |
| Actual compatible TrainerOS clients/accounts for the claimed larger and concurrent sessions | End-to-end admission, UI and lifecycle beyond two players | Calling fixture processes extra users; redoing already passed emulator seats |
| Two independent internet connections, with target devices reachable for recovery; owner deferred until home on 4 October | Actual internet route, admission and recovery | Treating two devices behind one router as separate-network proof, or repeatedly requesting a switch during the deferral |
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
