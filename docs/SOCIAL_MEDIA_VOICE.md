# Communication media and voice — 2026-10-02

## Web incoming and paired decline checks — 2026-10-02

This continuation supersedes the unobserved web ring and expired decline
attempts recorded below. On the unchanged delivered build, an ordinary public
web-client call reached Flip Home with Answer/Decline. Closing and reopening
Home selected the fresh Answer action; A answered while retaining the underlying
Trainer/RA page. Home voice controls then showed Connected and Microphone off
(`calls-oct2-fast-ring.png`, `calls-oct2-web-answered.png`,
`calls-oct2-web-connected.png`). The web participant list showed both users.
The web caller and native receiver explicitly left afterwards.

The web client's [call command](https://github.com/fluxerapp/fluxer/blob/main/fluxer_app/src/features/voice/commands/CallCommands.ts)
rings after media connection; the earlier attempt did not establish a fresh
ringing observation. A displayed call message alone is insufficient evidence.
The successful check proves incoming signaling, direct Home answering and session
joining, not web/native decoded audio or real-microphone quality.

Both native directions now have explicit decline evidence taken while the ring
was present: `calls-oct2-declined.png` (Odin) and
`calls-oct2-flip-declined.png` (Flip). A on Decline removed the incoming actions
without joining. Separate fresh unanswered calls ended by the caller produced
Missed call notifications on each handheld. Opening those notifications displayed
the correct conversation without calling back (`calls-oct2-missed-inbox.png`,
`calls-oct2-missed-open.png`, `calls-oct2-flip-missed-inbox.png`,
`calls-oct2-flip-missed-open.png`). These are actual handheld captures under
private `work/research/`, driven through the existing controller-input helper.

Reverse web attachment acceptance remains open: the internal browser could
display the upload menu, but its exposed actions could not select the file.
The native UI bridge was unavailable and Companion observation returned
WINDOW_OCCLUDED for both browser windows. No upload or reverse-media success
is claimed. Real input/headset quality, decoded web/native audio and
separate-network acceptance also remain open; block 3 is not complete.

No production source or binaries changed during this continuation. Final live
verification still matches the binary/helper/save hashes in the preceding
delivery evidence, with Flip PID 1410186 and Odin PID 104735. Database counts
remain 3/830 and 1/25; boot configuration and integration helpers are preserved,
and neither device has a pending Link transaction. No reboot, game launch or
save operation was needed; microphones remained muted throughout.

## Resumed call checks and stable Home selection — 2026-10-02

Both native sessions and the ordinary web client regained connectivity. An
Odin-originated call reached Flip; Guide then A answered directly and retained
the Trainer page. Home voice controls subsequently showed Connected and
Microphone off (`calls-answer-connected.png`). An earlier ring expired while
Home was open and its numeric selection moved to Notifications. Shell Home now
preserves the action ID, falls back to Home when that action disappears, and
does not resurrect the old selection on a later ring. This matches the existing
in-game menu policy. Notification copy now says Press Home to answer.

Web-originated calls produced call messages, but a fresh incoming ring was not
observed on Flip in these attempts. Joining the existing web call from native
Social reached the web participant list; the native capture was taken while
Connecting audio. This is not proof of decoded web/native audio or reliable
web-originated ringing. Browser and native calls were explicitly left; real
microphones stayed muted. Reverse web attachment/audio acceptance, real input,
headset quality, separate-network proof and the remaining call matrix stay open.
No game or save operation was performed during this continuation.

After delivery, a Flip-originated call displayed Answer/Decline in Odin Home
(`calls-final-odin-ring.png`). The attempted decline occurred after the short
ring had disappeared: selection had returned to Home, and Down/A opened Friends.
Do not record that attempt as a passed decline check. The caller then left.

ARM64 checks passed: Social 79 cases, interactions 36, exit presentation 9;
production was rebuilt with BUILD_TESTING disabled. Both live binaries match
`658d2a1188a5c70f0bea06134bd779ae16ab688a44b01672aac55519b21cca48`
(Flip PID 1410186, Odin PID 104735 at verification). Database counts remain
3 Trainers/830 Adventures and 1/25 respectively. Boot preferences, integration
helpers and both Emerald save hashes below are unchanged; no Link is pending.
No device reboot was needed. Social attribution was visually checked on both
actual handheld captures `calls-resume-delivery-flip.png` and
`calls-resume-delivery-odin.png`, private under `work/research/`.

Block 3 is still incomplete. Continue its outstanding runtime acceptance rather
than treating this correction as a substitute completed block.

This increment extends the existing native Fluxer user client. It does not
replace provider permissions, consent, privacy settings or local Link.

## Pictures and recorded voice

Conversation Options offers Send picture and Record voice message. The picture
picker reads at most 80 files from Pictures; explicit selection creates a bounded
JPEG copy without EXIF/location metadata. The original stays untouched. A preview
names the destination before Send. A on a single received attachment opens it
directly; Select retains message management.

Recordings use the selected microphone, mono Opus, a 120-second limit, waveform
and local playback before Send. Cancelling discards the temporary recording.
No automatic recording or autoplay is introduced. Missing input/encoder and
disconnected/no-audio sources report actionable errors. Speaker monitor sources
are not treated as a microphone. Pictures and audio are bounded to 16 MiB.

Uploads use supported `files[0]` multipart messages. Voice uses Fluxer's voice
message flag, duration and waveform. A successful message acknowledgement ends
the preview; uncertain delivery is not automatically retried. Downloads use the
public instance media hosts (`fluxerusercontent.com`, `media.fluxer.app`), HTTPS,
bounded bytes and no credential forwarding or arbitrary redirect following.

## Calls

DM/group Options starts or joins an ordinary Fluxer call. Incoming rings appear
in Notifications and can be answered/declined in the game Home menu. Calls have
microphone mute, output mute, ring and leave controls, and persist across pages
and ordinary game launch. Microphones start muted. A disconnected input remutes
and reports its failure without crashing TrainerOS.

The native shell obtains the supported Gateway voice grant. A TrainerOS-owned
Python worker uses the official LiveKit SDK and PipeWire/Pulse audio. Secrets
travel over its stdin, not argv or logs. Heartbeat deadlines and parent-death
signals terminate orphan workers and their audio children. No root audio daemon
or extra network service is installed.

`packaging/integrations/install-voice.sh` installs the bounded worker and private
venv for the handheld user. It requires python3, pactl, parec, pacat and ffmpeg;
the image integration must include these at release. It preserves existing
emulators, audio configuration and user saves. The SDK is currently pinned to
1.1.19 for the verified route; SDK updates need compatibility checks.

## Notifications and in-game Home

The existing provider unread/request inbox gains an account-scoped notification
sound setting. Mute/DND/private-preview rules remain in force. While a supported
RetroArch game is running, message notices use the owned runtime command channel.
Home shows the unread/request summary without acknowledging messages or replacing
the game. Its list updates in place, scrolls and adds no duplicate detail screen.
Outside a game the existing direct conversation/request destinations remain.
The live call panel uses the same call service as Social.

## Evidence and remaining gates

Two native handheld clients joined the same public Fluxer/LiveKit call. A bounded
synthetic 440 Hz input on Flip was received through Odin's audio output; captured
amplitude was about 1202. Both showed two participants. This proves actual audio
transport, not microphone/headset quality or interoperability with the web client.
Temporary virtual audio sources were removed and original source routing restored.

A picture was sent from Flip and received on Odin through the public service.
Initial readback exposed the public media hostname difference; the implementation
now accepts the instance's actual `fluxerusercontent.com` route as well as its
documented proxy host. Odin opened the downloaded picture directly with A. A
51-second Opus voice message with a waveform was recorded from a temporary test
input, sent from Flip, downloaded on Odin and played in its native message viewer.
This verifies recorded-message plumbing; it is not a real microphone quality test.

The final native ARM64 binary was installed on both handhelds. Emerald launched
with the existing save/RA layers and the new appearance layer. Actual physical-input
injection opened Home over each live game; Flip changed/restored its display
preference, sent shader toggle commands and continued the same runtime. A new
message from Odin appeared in Flip's in-game unread list and RetroArch notice.
The read-only list did not expose a false Open action or acknowledge the chat.
Explicit Exit retained the ordinary save question and clean-capture route.
Both resulting history sessions are `returned`; their stored exit JPEG hashes
verify and the images contain no Home/confirmation overlay. The two ordinary
Emerald saves retain their pre-check hashes. Both SQLite integrity checks pass;
Flip retains 3 Trainers / 830 Adventures and Odin 1 / 25. Boot preference,
nearby helpers and controller service were preserved. Both run ARM64 SHA-256
`e516a2395cab6a1cb9949415c0678331c1af31b49f3af77e5ae89cd81631c6d9`.

Private Gamescope captures include `game-home-final.png`,
`odin-game-home-final.png`, `game-display-original.png`,
`game-notifications-final.png`, `picture-received.png` and `voice-playback.png`.
These are actual device images, not mock renders or distribution assets.

Windows native build passed. The 50-entry CTest run passed 49 entries; the Hall
render scenario exceeded its 30-second deadline under parallel load. Its isolated
rerun passed in 26.94 seconds without code or timeout changes. Thus all 50 entries
passed across the run and focused rerun. The portable adapter knowledge/source
check and staged whitespace check also passed. ARM64 production build passed;
the current increment does not claim a new full ARM64 automated-suite run.

Both devices used one home internet connection. Separate-internet, real headsets,
group-call/full reconnect matrix and web-client encrypted voice interoperability
remain distinct gates. These are not erased by one
successful call. Active-game invitation to the emulator's own multiplayer (#107)
is a separate consumer, not the existing save exchange or a text invitation.
It is not implemented by this increment. During route investigation,
`lobby.libretro.com/list/` and the documented Madrid relay lookup timed out from
both the Windows workstation and Flip. This is a local connectivity observation,
not a claim that the public service is down everywhere. No placeholder invitation
button is added in place of an actual emulator session.

Primary references: [message uploads](https://docs.fluxer.app/topics/uploads/),
[messages](https://docs.fluxer.app/http-api/messages/),
[voice](https://docs.fluxer.app/voice/),
[calls](https://docs.fluxer.app/http-api/calls/),
[Gateway commands](https://docs.fluxer.app/gateway/commands/).

## Background-call recovery — 2026-10-02

The owner's background-audio requirement is implemented at the session boundary:
ordinary navigation, Home overlays and Adventure launch/return do not leave the
voice room. Chat Gateway loss also leaves healthy media connected. A fresh READY
checks channel access and reattaches the same placement without ringing. Replayed
grants do not restart audio; genuinely changed placements replace the worker and
retain microphone/output choices. LiveKit reconnect has a bounded recovery period.
Network loss can still interrupt media; no uninterrupted-network guarantee is made.

Audio output retries after a Pulse output-process failure. Microphone capture
follows the default input when a headset changes; a missing input remutes and
reports failure. Speaker monitors are never accepted as normal microphones.
Recording controls are unavailable during a live call, and answering releases an
existing recording/playback. Logout, explicit Leave and lost group access stop
the call. Participant counts represent people, including when the same account
also joins from the ordinary web client.

### Current device evidence

- Flip and Odin joined a native DM call. A temporary synthetic 440 Hz microphone
  on Flip reached Odin's output (measured RMS about 1230). Flip then left Social,
  launched Emerald, opened physical Home call controls and returned normally,
  keeping its voice worker alive throughout.
- A bounded 45-second firewall rule interrupted only Flip's chat Gateway TCP
  route. The Gateway reconnected with a new socket while the same voice worker
  continued. Odin received the tone during that interval (RMS about 1201).
  The temporary rule was removed; no persistent network configuration changed.
- The ordinary public Fluxer web client joined the same call using its supported
  additional-connection action. Native Flip audio reached web playback (RMS about
  7340, 100/120 non-silent windows). This establishes native-to-web encrypted
  media interoperability, not the untested reverse microphone direction.
- Both handhelds joined the existing test group call; audio arrived on Odin
  (RMS about 1204). Removing Odin as a group recipient stopped its voice worker
  and removed the group. The native Add member flow completed the supported
  challenge and restored the original two-member test group afterward.
- Flip sent a picture and a 79-second Opus voice message through the native UI.
  Both rendered in ordinary Fluxer web; its voice player produced audible output
  measured by a bounded in-memory loopback check (RMS about 2720). This extends
  earlier native-to-native picture/recording proof, without claiming web upload
  or physical microphone quality.
- Final Home controls on both consoles exposed mute/output/leave in the existing
  material panel. Flip output mute/unmute worked; Back and Home retained the call.
  Test calls were explicitly ended before final deployment. Temporary synthetic
  audio sources were removed and original input routing restored.

All signal checks used synthetic audio on designated test accounts. No recording,
credential, save or proprietary image is committed. Actual handheld captures are
private `calls-game-controls.png`, `calls-return-social.png`,
`calls-home-voice-controls.png` and `calls-odin-voice-controls.png`.

### Remaining acceptance

The current paired native artifact is SHA-256
`ba4421aae0f5a7f1edf1b422d7c10e15785a798169f4e956056c001e583a11e6`;
the voice helper is
`29ac0073e8d239017bbefa6a2974dff3bdc0934f68e6540f2ff8d33c25f0d3e1`.
Both running executables were verified separately. SQLite integrity, each boot
preference, InputPlumber and nearby helpers pass their preservation checks.
Flip retains 3 Trainers / 830 Adventures, Odin 1 / 25. The Emerald save hashes
remain Flip `cf39ceece96e0b8804864a23fedb81bfdf6781ed3b560fff039ec21bf61666cf`
and Odin `3fca83edc8bb3d69f5627a6ecec820069ca7fac8699676ecade2fd748ea47c7a`.
Final Flip X dismissal removed the inbox entry while the unread badge remained,
confirming that dismissal did not mark the conversation read. Its installed
captures are `calls-notifications-final.png` and `calls-notifications-dismissed.png`.

Windows production/full native build and ARM64 production build passed. The
50-entry Windows suite initially passed 48 entries: practice exceeded its
30-second limit under load and Social initially used intermediate objects.
After rebuilding, practice passed alone in 16.94 seconds; Social exposed an
outdated DM-mention expectation, corrected to check both ordinary DMs and actual
community mentions. Its final CTest run passed. All 50 entries therefore passed
across the initial run and focused reruns. Final Social coverage is 71 cases on
each architecture; ARM64 passed all 71 after the final expectation correction.
The Python worker syntax check, portable adapter knowledge check and staged
whitespace check passed. Windows Ninja reported a truncated dependency cache
and rebuilt dependencies; no timeout or product behavior was relaxed for tests.

Block 3 remains open. Odin currently exposes output monitors but no real
microphone input; the owner has been asked to connect a microphone headset.
Actual speech/echo quality, headset reconnect during a conversation, web-to-native
microphone/attachment checks and separate-internet recovery remain unverified.
The observed same-network synthetic audio and deterministic lifecycle checks do
not replace those gates. No work on block 1 is substituted for them.

Recovery follows the official [Gateway event contract](https://docs.fluxer.app/gateway/events/)
and [call contract](https://docs.fluxer.app/http-api/calls/): READY is followed by
active CALL_CREATE events; an unavailable CALL_DELETE retains an unavailable call
until recovery instead of fabricating a fresh ring.

### Prepared continuation; owner checks deferred — 2026-10-02

The owner explicitly deferred hands-on checks until returning home. This is a
continuation of block 3, not its completion or a switch to emulator multiplayer.
No new live call, game launch, microphone capture or network-fault exercise was
started during this continuation.

- Incoming calls observed ringing this account become missed-call notifications
  only on a real call end. Answering elsewhere or explicitly declining removes
  the pending notice. Gateway failure and unavailable CALL_DELETE do not mark
  a call missed. The latter rule follows the official contract linked above.
- Opening the entry displays its conversation without calling back. Dismissal
  does not ACK messages. Viewing an already-read call message after the caller
  hangs up clears the new local missed notice without another provider ACK.
- Missed-call records are limited to 32 conversations and the account-bound
  30-day history cache. Logout clears them. Calls that were never observed while
  online are not reconstructed. Cached ended-call messages retain their ended
  label without storing call rosters or voice grants.
- Social, shell Home and in-game Home identify the current call conversation,
  regardless of which chat/page is underneath. These are existing controls;
  no new call page or automatic navigation was introduced.

ARM64 Social tests pass all 75 cases, including the four new missed-call/cache/
background-name cases. ARM64 production builds with tests disabled. Both running
handheld executables now match SHA-256
`b2e445bb7df64bbb8a7e26af569696a9a366b073c7b3ae6a8b460989f2aa0f9b`.
Flip PID at verification was 1030636; Odin was 61640. Each device's existing
database counts, boot choice, helpers and Emerald save hashes above are retained;
InputPlumber is active and no Link settlement is pending. Voice helper unchanged.
Actual installed idle captures are private `calls-prepared-flip.png` and
`calls-prepared-odin.png`; they establish the restored shell only, not call UI or
speech acceptance. The pending live checks above remain pending.

The Windows production build and all four affected CTest entries passed:
Social (75 QtTest cases), controller interactions, exit presentation and its
rendered QML scenario. Ninja again recovered a truncated local dependency cache
and rebuilt dependencies; no product test limit was relaxed. The ARM64 suite
was rerun after the final already-read-call correction (75 passed). No unrelated
full-suite or live-audio repetition was used for this follow-up.

### Direct incoming-call controls — 2026-10-02

This remains block 3 preparation with owner hands-on checks deferred. Home now
offers Answer/Decline directly; opening Home during an answerable ring selects
Answer. An incoming event itself never takes focus or opens Home. Accepting from
shell Home or an incoming inbox entry closes Home and keeps the previous page.
The inbox footer distinguishes Answer/Decline from ordinary Open/Dismiss.
Missed calls still open their conversation without calling back.

Shell Home and in-game Home share the same call-action eligibility and reject
an expired ring, disconnected answer or implicit replacement of an existing
call. In-game dynamic actions preserve the selected action by ID; if it vanishes,
selection returns to Continue instead of a different action at the old index.
No transport, microphone default, draft or save-operation policy changed.

ARM64 tests pass: Social 78 cases, interactions 36, exit presentation 8. The
new checks cover direct Home/inbox answering, origin retention, decline, stale
answers, existing calls, and focus during changing in-game call actions.
Production was rebuilt with tests disabled, including the changed QML. No new
Windows build or full-suite run is claimed for this ARM64 controller change.
Both live devices now match SHA-256
`1413a8cf24fee471f534980198b8aea89cc6f974b3c9e777d6d5433a574bba0f`:
Flip PID 1043286, Odin PID 73229 at verification. Database integrity/counts, boot
choices, InputPlumber, nearby/voice helpers and both Emerald hashes above are
unchanged; no Link settlement is pending. There were no active games/calls at
installation. No live call was started for this delivery; the actual incoming
call visual/controller walkthrough and microphone checks remain deferred.

### Remote continuation blocked by public service outage — 2026-10-02

The owner remains away and explicitly authorized available remote checks. Both
handhelds respond over SSH and to controller navigation; neither shell is frozen.
The installed/running binary still matches `1413a8cf24fee471f534980198b8aea89cc6f974b3c9e777d6d5433a574bba0f`
(Flip PID 1043286, Odin PID 73229). Database integrity/counts, boot choices,
InputPlumber, integration helpers and both Emerald save hashes above were
rechecked and preserved. No new binary or helper was installed in this check.

The ordinary Fluxer web client and both native clients fail to reconnect.
Bounded unauthenticated probes from the PC and handhelds time out against the
public API; Odin also times out against Gateway and the main site, while GitHub
and Cloudflare HTTPS respond normally. Local DNS, Cloudflare DNS-over-HTTPS and
Google DNS-over-HTTPS agree on the API/Gateway address. Flip has no nftables
tables left from the earlier connection-failure exercise.

During diagnosis, the [official status page](https://fluxerstatus.com/) published
**Issues with platform connectivity**, **Investigating / Major outage**, started
at **2026-10-02 12:30:03 UTC** (incident `cmuqxxrlm004e1mpckq7sm4uf`). The web
client also displays that incident. Its earlier status-page response was stale;
do not record this as a TrainerOS-only failure or change DNS to work around it.
Private evidence is `work/research/fluxer-status-20261002.html` and the actual
`calls-live-flip-social.png` / `calls-live-odin-social.png` handheld captures.

The attempted web-originated call never produced an observed incoming ring on
Flip. Direct Answer/Decline, missed-call runtime behavior and reverse web media
acceptance therefore remain unverified, not passed. Browser reload discarded
the unsuccessful join UI. Neither real microphone was unmuted and no game/save
operation was started. Both handhelds were returned to their Trainer pages.
Odin still exposes output monitors without a real microphone input.

Resume this same block after service recovery: verify fresh incoming Home/inbox
answer/decline and missed calls on both consoles, then the remaining web-to-native
media/audio checks. Human speech/headset checks await the owner; separate-internet
proof remains distinct. This outage does not complete block 3 or authorize moving
to block 1.

Follow-up at 12:48 UTC: the status page marked the incident resolved at 12:36:06,
but local access remained intermittent. Flip briefly showed Connected without
a shell restart (same PID), then API and Gateway HTTPS probes again timed out.
The web client and Odin returned to Reconnecting. One native Odin call attempt
transitioned from Connecting to "Couldn't start the call. Try again."; no voice
worker remained and controller navigation still worked. This establishes the
failed-start cleanup, not successful ringing or audio. Actual captures are
`calls-recovery-flip.png`, `calls-recovery-start.png` and
`calls-recovery-timeout.png`; the current status-page snapshot is
`fluxer-status-recovery.html`, all private under `work/research/`.
Both shells retained their running build and were returned to Trainer; no code,
network settings, saves or microphone routing were changed. Remaining live gates
above are unchanged.
