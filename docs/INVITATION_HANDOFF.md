# Invitations while playing

Owner request, 10 October 2026: an invitation can stay in a corner while its
recipient continues playing and saves. It survives the recipient closing that
game independently. Accepting after saving closes the current game normally,
then continues the existing multiplayer preparation. This is shared behavior
across runtime families, not a cable or franchise-specific feature.

## Player flow

- The warm yellow people badge is passive: incoming invitations do not take
  controller focus, stop gameplay, launch a game or accept a party membership.
- Tap the badge to open the invitation. Controller users retain Home → Incoming
  activity during gameplay and Home → Notifications in the shell. Both use the
  same request ID, consent and expiry.
- **Keep for later** (or Back from the invitation) collapses it and returns
  controls. The guest can save in the original game. Closing that game normally
  keeps the unanswered invitation and badge available in TrainerOS.
- **Saved — join** explicitly authorizes closing the current game. A fresh clean
  capture and the normal owned-process graceful exit still run; acceptance is
  sent only after process exit and save settlement succeed. There is no second
  save question for this explicit action. It is consent to exit, not proof that
  the player actually saved. With no current game, the action is **Accept**.
- New hosts allow five minutes to answer. Legacy hosts retain their one-minute
  window. This does not extend packet replay validity. Host cancellation,
  disconnection, expiry, account reset or refusal removes the corresponding
  request; a replacement invitation cannot inherit an earlier consent.
- Preparation and networking still use the established GameParty/emulator
  routes. The organizer retains the existing Start game action and group voice
  is unaffected.

## Runtime boundary

Matching a title, platform, native-online label or remote `joinInPlace` field
never authorizes reusing a running process. `RuntimeMultiplayer::RunningJoin`
requires both a trusted local capability check and a join operation, plus the
exact compatible installed game and current lifecycle identity. Such a runtime
joins the already running game after consent and leaves that process alive.
Failure preserves it; it is not silently converted into a restart.

The currently installed emulator profiles still require prepared launches.
No native-game hot-join provider is registered by this change. Its contract test
checks the shared exception with an independent provider; it does not establish
support for an unintegrated native game, PSP ad hoc or DS wireless mode.

## Platform and protection

The Gamescope surface uses the upstream `GAMESCOPE_EXTERNAL_OVERLAY` role before
mapping, without requesting input focus. Its transparent full-output buffer
contains only the small corner badge. The supported X11 pointer path grabs a
press briefly, consumes only a deliberate badge tap, and replays the original
outside event with `XAllowEvents(ReplayPointer)`. No synthetic click is sent to
the game. Keyboard/controller input remains with the game until the existing
owned-window Game Options handoff succeeds. Source contracts:
[Gamescope compositor](https://github.com/ValveSoftware/gamescope/blob/master/src/steamcompmgr.cpp),
[Xlib event replay](https://www.x.org/releases/X11R7.6/doc/libX11/specs/libX11/libX11.html).

The helper checks shell/game process ancestry and start identity, and limits
input interception to those focused windows. An independent heartbeat watchdog
can stop only that helper, releasing its X connection/grab. A lost helper gets
bounded consecutive retries, renewed after a healthy interval; the invitation
remains in the ordinary inbox. Gamescope retains unmapped overlay frames, so both
the window and helper explicitly set zero opacity while hidden or unavailable.
Preview and Exit
capture hide the badge, preserving clean gameplay history. Other notification
types retain their existing presentation.

## Validation and delivery

The shared runtime tests cover an unrelated running game, collapse without a
process change, independent exit with the same unanswered request, acceptance
only after successful save settlement, cancellation during capture, stale consent
against a replacement request, and trusted native-provider reuse without exit.
Party tests separate the five-minute answer window from packet replay validity.
The Linux transport test opens Game Options from a deliberate programmatic
request and retains the owned process; pointer tests cover original-event replay,
inside release, drag-away, focus loss and watchdog PID reuse.

On Flip and Odin, the ordinary Mario Golf guest closed before the host received
acceptance for Into the Blue. Both then joined the existing public RetroArch
relay (150/282 ms reported in this run) and entered the original two-machine VS
setup. This is a same-router relay check, not separate-network acceptance.
Flip also received the reverse invitation while its ordinary Into the Blue
process remained alive: tapping the badge opened the invitation, Keep for later
returned to that same process, and an independent normal exit retained the badge
in Home. On the final installed build, tapping that retained badge opened the
ordinary Social invitation panel; refusing then removed the badge.

Device testing found and corrected two presentation/recovery issues: Gamescope
can keep drawing an unmapped external-overlay frame, and the pointer helper's
watchdog can fire across a slow window transition. Hidden-window opacity is now
explicit; consecutive retries are limited, with their budget renewed after ten
healthy seconds instead of being exhausted permanently across unrelated events.
The watchdog still releases input on a stalled GUI/helper. Whole-screen evidence
uses `GAMESCOPECTRL_DEBUG_REQUEST_SCREENSHOT=4`, waiting for completed output;
the installed `gamescopectl screenshot ... 4` path omitted the external layer in
these checks and must not be used as evidence that the badge was absent.

On each final installed device, three deliberate terminations of only the owned
badge helper, separated by healthy intervals, each produced a new ready helper
without restarting TrainerOS. This crosses the previous lifetime retry limit.
Both shells were left idle with no emulator running and output volume at zero.
The three previously recorded Mario Golf save files (two on Flip, one on Odin)
retained their hashes against the settled baseline from before this increment.

Odin's touch driver reported phantom contacts during charging; the owner
identified charging interference and disconnected the charger. Controller and
remote pointer/touch-path checks are not owner physical-touch acceptance. Quiet
output, original saves, separate-network checks, native-game providers and all
prior MP-02 acceptance remain distinct. No issue is closed by this delivery.

The final ARM build and eight affected Linux test targets passed: party,
runtime multiplayer, process, exit controller, exit presentation, overlay
transport, Python overlay helpers and support-file installation (12.52 seconds).
The Windows build and six affected checks also passed before the final badge
recovery correction. A further Windows rebuild encountered the existing Ninja
dependency-log recovery/full-rebuild issue and was stopped; the final correction
is verified by the ARM build, Linux checks and both installed handhelds.
The adapter knowledge check retained 32 semantic / 56 runtime entries and three
exact builds. README screenshots and queued controller work were preserved.

Both devices run the same application and support files, checked against the
live process as well as the installed file:

| Artifact | SHA-256 |
| --- | --- |
| TrainerOS ARM application | `65d838038bc5ce6be4d5847230ec6a6469a5e1b69a5da056a1582f284f3127ff` |
| Adventure overlay helper | `0237b882ee705ef199b0d54f765d3cd0b9cec3784975c5480205a50e5986b59e` |
| Invitation overlay helper | `37fe3658f277779308526e25f29be135882f68b67551d09ea228cd129a78288f` |
