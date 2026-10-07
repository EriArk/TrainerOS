# Issues #118–135: accepted scope and dependencies

Reviewed against live GitHub issues and all issue comments on 8 October 2026.
The newest issue was #135; #118–135 are 18 additions beyond the previous #117
register. #70 now explicitly replaces its image-only boundary; comments on
#101 and #112 defer conflicting presentation requirements to #127–135.
The original reconciliation planned these outcomes. UX-01 is now installed on
Flip/Odin; [delivery evidence and remaining #135 gates](SOCIAL_UX.md). The
distribution and Hotseat outcomes below remain planned.

[ROADMAP](ROADMAP.md#active-execution-queue--2026-10-08) owns execution order.
[Current tasks](CURRENT_TASKS.md) owns remaining work. Original issue acceptance
remains authoritative; the rows below group whole outcomes, not one turn per issue.
Existing R1–R18, R7a, R18a/R18b, U/P and communication obligations are retained.

## Distribution: one release, two delivery paths

Parent: [#118](https://github.com/EriArk/TrainerOS/issues/118).
The prepared ArmadaOS image remains required. A convergence installer also brings
supported SteamOS and Bazzite handhelds to the same TrainerOS-owned release state.
This is a dedicated selectable session with managed runtimes, not a frontend that
inherits arbitrary host emulator versions. The host's Gaming Mode/Desktop and
unrelated emulator installations remain available.

| Issue | Complete outcome | Dependency and acceptance |
| --- | --- | --- |
| [#119](https://github.com/EriArk/TrainerOS/issues/119) · REL-01 | One versioned, machine-readable release/ownership definition | Reuse image, update and emulator manifests. Include application, host/device profile, session/helpers/services, runtimes/cores/patches, configuration migrations, data compatibility and integrity. Distinguish owned, host-provided, user-owned and optional state. Read-only diff; no secrets, ROMs, firmware, saves or private artwork. |
| [#120](https://github.com/EriArk/TrainerOS/issues/120) · INSTALL-01 | Selectable TrainerOS session on SteamOS and Bazzite | REL-01 and actual host/device profiles. Gamescope/display/input/Home integration, helper lifetime, game launch/return, system hooks and clean transition back. Privileged integration is narrow, recorded, idempotent and reversible; failure leaves host recovery usable. |
| [#121](https://github.com/EriArk/TrainerOS/issues/121) · INSTALL-01 | Release-owned emulator/core stack | Pinned reproducible runtime and patch records, private session configuration, input/save/update identity. Coexist with user installations; reuse only proven exact matches. A package name is not equivalence. |
| [#122](https://github.com/EriArk/TrainerOS/issues/122) · INSTALL-01 | Fresh install, repeat install and interrupted-install recovery | REL-01 + session/runtime components. Preflight host, device, storage, graphics/input and existing owned state. Stage/verify/apply/validate in recoverable order; matching state is a no-op. No false success marker or manual per-emulator setup. |
| [#124](https://github.com/EriArk/TrainerOS/issues/124) · INSTALL-01 | Safe, rerunnable uninstall | Ownership recorded by the same installer. Remove only owned components; retain ROMs/firmware/saves/Steam/unrelated apps. Keep Trainer data by default; deleting local personal data requires a separate explicit choice. Reinstall recovers retained data; interrupted removal preserves the host. |
| [#123](https://github.com/EriArk/TrainerOS/issues/123) · MAINT-01 | Audit and repair after host updates | REL-01 + INSTALL-01. Distinguish repairable owned drift, changed/unsupported host capability and missing user data. Explicit Settings/System health/repair restores only owned state; no host downgrade. Exercise removed components, host-update drift, repeat repair and normal launch/Home return. |
| [#125](https://github.com/EriArk/TrainerOS/issues/125) · R17 | Image/SteamOS/Bazzite equivalence and upgrade proof | R15 + INSTALL-01 + R16 + MAINT-01. Compare first entry, navigation/library/input, launch/Home/return, claimed save ownership, Social/calls/invitations/gameplay, system controls, recovery and release identity. Upgrade N→N+1 and preserve Trainer/library/save identity. Explicit capability differences; never infer installer acceptance from Armada evidence. |

REL-01 follows stable product/asset inputs and precedes both distribution builders.
R15 is the prepared-image outcome; INSTALL-01 delivers install/use/uninstall on
both supported host families as one coherent outcome. R16 updates both paths
through shared ownership/migration rules; MAINT-01 handles host-induced drift.
R17 includes the three-path comparison. Pack Studio and optional ROM-native
integration remain before the image; installer work does not bypass those gates.
Runtime recipes and host constraints should be maintained in their existing
records as relevant work proceeds, without implementing the installer early.

Physical SteamOS and Bazzite targets are required for their acceptance. Flip/Odin
Armada evidence and synthetic host fixtures cannot substitute. Lack of a target
keeps the specific gate open; it does not justify repeatedly rebuilding the image.

## HS-01: live Hotseat Relay

Source: [#126](https://github.com/EriArk/TrainerOS/issues/126).
Add one live game, one logical gameplay controller and an ordered explicit control
token shared by several connected participants. This is a distinct capability
alongside shared-console netplay, independent handheld link, wireless/native game
networking and TrainerOS save-based activities. Player count alone cannot enable it.

- Reuse Social/GameParty invites, reverse requests, readiness and organizer Start.
- Use an existing appropriate synchronized runtime; no speculative new transport.
- Only the token owner supplies game input; local Home/system input stays local.
- Manual ordered handoff is the baseline. Stale/duplicate messages cannot take
  control; active-player loss requires an explicit pause/pass decision. Rejoining
  restores identity but does not seize the token. Leaving updates order safely.
- Expose participant count, current owner and handoff in the existing party/Home
  experience. Calls and conversations remain independent.
- Define persistence ownership first; prefer a first title/mode with safely
  isolated progress. No persistent save copying between participants or turns.
- Prove real normal-UI launch, non-owner input exclusion, several handoffs,
  disconnect/rejoin, Home exit and call continuity on actual devices.
- No asynchronous turns, offline save mailing or later-turn notification queue.

Scheduled after MP-02…MP-07 within block 1, preserving the owner's oldest-first
runtime queue. The first supported title needs reviewed pass-controller evidence;
this plan does not select a game from an unverified name or metadata count.

## UX-01: one Social / Together / party journey

Parent: [#127](https://github.com/EriArk/TrainerOS/issues/127).
The product concepts are conversation, activity, game party and independent call.
Compose working providers, transports, parties and save transactions; do not
replace them to obtain a simpler presentation. This is one integrated delivery,
not eight disconnected screens declared complete in separate checkpoints.

| Issue | Target behavior | Preserved acceptance |
| --- | --- | --- |
| [#128](https://github.com/EriArk/TrainerOS/issues/128) | Messages combines DMs and small groups; Communities keeps channels; Discover covers people/communities/invite links | Obvious friend/request entry; exact provider-supported identity search; compatibility as badge/filter, not separate universe. Retain drafts, unread/history, membership permissions, last conversation and restart/face focus. Migrate old Groups/Search routes without losing state. |
| [#129](https://github.com/EriArk/TrainerOS/issues/129) | One compact conversation Together area for pending/join/preparing/running/rejoin and supported native activities | Multiple parties use a compact summary and deliberate list; messages retain space. Call status/lifetime remains independent. Together dispatches distinct capabilities, not a universal permission. |
| [#130](https://github.com/EriArk/TrainerOS/issues/130) | A select/confirm; B/Left local back; X compose; Y Together; Select context; L2/R2 faces | Explicit keyboard Send, visible focus regions, accessible old history/Latest and message edit/delete/media. No hidden reading state that unexpectedly changes core button meanings. Validate both handhelds. |
| [#131](https://github.com/EriArk/TrainerOS/issues/131) | Home Play together / Invite players selects people/groups first | Nearby/online are useful status, normally automatic routing. Deduplicate proven identities; do not merge people by display name. Preserve Join/Ask to join, permission, compatibility, consent and no manual network administration. Ask route only for a real unresolved user tradeoff. |
| [#132](https://github.com/EriArk/TrainerOS/issues/132) | One consistent party surface from Home and Social | Game/mode, seats/invites/readiness, justified Start, access, leave/end/rejoin and Hotseat control. One backing state; party does not own group membership/call/save transaction. Effective capability determines capacity. |
| [#133](https://github.com/EriArk/TrainerOS/issues/133) | Group details owns members, persistent game access and notifications | Ask first / selected members / invitation-only retain organizer scope. Party override includes Use group setting. Provider permissions remain authoritative. Short contextual Options; relocate account-global actions without deleting functionality. |
| [#134](https://github.com/EriArk/TrainerOS/issues/134) | Consistent incoming call/game/join/Link request presentation and missed Notifications destination | Who/what/action, expiry and correct deep links; no automatic callback, launch, read acknowledgement or save consent. Preserve DND/privacy, protected modal priority, page/game and action-identity focus safety. Backends retain independent safety. |
| [#135](https://github.com/EriArk/TrainerOS/issues/135) · REVIEW-05 | Whole-user-journey acceptance and curated real-device captures | Friend Join/Ask → party → Start → gameplay → Home → leave/rejoin; invite from game; independent simultaneous group parties; unchanged group call through game switches; Social/native entry reaches the same Link workspace. Check two/four seats, Hotseat when available, stale/empty states and both layouts. QML smoke is insufficient. |

**Owner priority, 8 October: implement UX-01 before resuming MP-02.** Use the
already working runtime and native-activity consumers; do not wait for all later
emulator families, Hotseat or LINK-04 completion. Within the outcome, establish
conversation routing/controller grammar, bind Together and party/group details,
then invitations/notifications; review the full composition before delivery.
Existing provider messaging expansion (replies/quotes, reactions, mentions,
compact person card, persistent composer/media/emoji and dismissible reply preview)
remains in REVIEW-05, using supported contracts; history search remains excluded.
Owner refinement on 8 October: touch and controller are parallel input methods.
Keep visible Send, attachment, voice-message, call and contextual buttons in
the conversation; use the same operations, drafts and consent from either input.
Current attachment support is pictures; generic file support is not implied.
Exercise #135 journeys supported by the current implementation during UX-01,
including actual game/party/Home/return and available native Link entry. Keep
future Hotseat/routes and deferred real-client/network/voice checks explicitly
open for their implementing tasks and final REVIEW-05. Do not close #135 early,
defer existing feature checks or repeat already settled networking.

The four Social faces and transport-first picker are historical evidence,
superseded by the installed UX-01 interface. Provider/capability/save guarantees
remain. Its curated README update replaces only the two old Social references;
it does not close #116's artwork redistribution review.

## Preserved prerequisites and handoff

- UX-01 is first. MP-02 resumes afterwards: GB invitation → real Gen I/II/TCG cable transaction →
  ordinary play/readback, then remaining GBA battle/cross-edition/Advance Wars and
  actual four-client activity. Reuse delivered Emerald/Red and Gen II clock proof.
- Pre-existing `RuntimeMultiplayer.cpp` changes stop connection timing/relay
  polling while waiting for an ordinary-game save/exit and guard cancelled queued
  launch. They are uncommitted implementation work, not validated by this review.
  Inspect/recover the interrupted device session before resuming its verification.
- MP-01/COM-03 distinct-network, real larger-group and human voice gates remain
  owner-deferred. Ordinary installations keep implemented multiplayer accessible.
- #93/#94 trust-sensitive activity prerequisites, #75/#76 save ownership and
  ordinary-game exit/rollback protections remain. No UX consent collapses them.
- #115 binary/image notices, #116 asset rights and #117 clean contributor build
  remain explicit release gates. The three recorded `persistence_process`
  subprocess-start failures are not fixed by this docs-only reconciliation.
- ScreenScraper still waits for owner access; suspend stays deferred. No new
  issues, comments, closures or GitHub Actions changes are made by this review.
