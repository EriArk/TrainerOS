# Communication media and voice — 2026-10-02

## Shared call across game parties - 2026-10-04

Owner scope: one group call, independent of game-party membership. Separate party
voice rooms are no longer required. Existing Flip/Odin behavior retained the call
while launching different games, creating separate group parties and ending one
party/game; synthetic audio was received before and after the exit through the
same worker. [Exact evidence and graphics recovery](GAME_PARTIES.md#shared-group-call-checkpoint---2026-10-04).
Physical speech/headset checks and distinct-network acceptance remain open.

## Remaining checklist — 2026-10-03

The owner explicitly deferred checks requiring physical input, listening or
speaking. Do not request them during remote work or mark them passed by synthetic
signal tests. Block 3 remains open; this is not a switch to another block.

| Check | How to finish | Status |
| --- | --- | --- |
| Live microphone route and capture-process recovery | Flip controller settings/Home with isolated virtual inputs; retain the voice worker after input loss and retry | Local failure/remute/retry verified below; physical headset removal is separate |
| Measured microphone switching and call-volume/mute effects at the receiver | Two working clients and isolated synthetic signal/output | Verified Flip to Odin below after recovery; prior output rerouting proof remains valid |
| Web-to-native pictures | Send an image from the ordinary web client and open it on the handheld | Verified via the web client's saved-media sender below |
| Web-to-native microphone and voice-message interoperability | Ordinary web client with an isolated test microphone | Pending; no controllable isolated input in the current internal browser |
| Ordinary web local-file chooser | Upload a local test file through the ordinary web UI | Tool-limited: browser Upload has no exposed action and native window capture returns WINDOW_OCCLUDED; independent of the verified saved-media delivery |
| Different internet connections and reconnection | Two independently routed clients, preserving recovery access | Pending; same-LAN tests do not satisfy this |
| Speech clarity, echo and delay in both directions | Owner with microphone headsets on both consoles | Deferred by owner |
| Unplug/reconnect an actual headset during a call | Owner; confirm selected/follow-system route and usable audio afterward | Deferred by owner |
| Physical Home/A/B controls and hearing a call while playing | Owner; receive/answer, mute, navigate, launch/exit a game and leave the call | Deferred by owner; injected-controller/background-worker evidence already exists |

## Ordinary web picture received on Flip — 2026-10-03

The internal CodexWeb browser remained signed into the designated Odin test
account. Its normal Add to saved media and Media sender re-uploaded the existing
TrainerOS test screenshot as a new `picture.jpg` attachment to the Flip test DM.
This used the ordinary public web client, not a fabricated Gateway event, database
insertion or direct API substitute. Flip received the attachment live. Right then
A selected and opened it in the installed image viewer, showing the same screenshot
and the correct sender. The read badge also returned from 2 to 1.

The original device capture is committed as
[19-social-received-picture.png](../screenshots/19-social-received-picture.png).
No production change or rebuild was required. This verifies web-to-native picture
delivery and decoding, including the supported saved-media upload path. It does
not verify the Windows local-file chooser or reverse voice messages/microphone.
The Windows bridge again returned WINDOW_OCCLUDED after one refreshed-window
retry; do not keep repeating that blocked chooser check without a tool change.

Both installed devices passed final executable/helper, database, save-hash and
boot-preservation checks on the unchanged `32dd52f` runtime. Flip stayed muted,
Odin stayed at 0%; no call, recording or actual microphone was started, and no
device restart was needed. Block 3 remains open for the checklist above.

## Remote input recovery and Odin interruption — 2026-10-03

On the unchanged `32dd52f` production build, Flip joined the public voice room
with its microphone initially muted. The explicit input was a temporary virtual
source; no real microphone was enabled. Home enabled capture on input A. Stopping
only that `parec` process remuted the microphone, removed capture and showed
Microphone unavailable while the same voice worker (PID 1520454) stayed connected.
One Home confirmation restarted capture on A without rejoining the room.
Communication settings then switched the live capture process to virtual input B;
the same worker remained. Turning Microphone on to Off stopped capture while
retaining the call. This proves local input lifecycle/route control, not reception
or speech quality: Odin was unavailable and the call had one participant.

Private installed-device captures are `oct3-input-lost.png`,
`oct3-input-recovered.png`, `oct3-live-input-b-verified.png` and
`oct3-input-muted-settings.png` under `work/research`.

The call was explicitly ended. Follow system input was restored only after
muting; virtual sources/sinks were removed and original system defaults retained.
Flip's system output stayed muted. The unchanged executable/helper, SQLite
integrity and 3 Trainers / 830 Adventures, Emerald save hash, boot preference and
nearby helpers passed final verification. No code change or new build was needed.

Before the paired test, Odin was reachable by SSH but could not produce a fresh
compositor capture. The kernel recorded an Adreno translation/GPU fault at
00:04:15 on 3 October. TERM, KILL and a session restart left a TrainerOS thread
in uninterruptible `dma_fence_default_wait`. No game was running. Remote reboot
was requested; SSH initially remained unavailable. The owner was told that manual
recovery was needed when they requested immediate reboot notices. Odin then
returned during this session. Its guarded session switch restored TrainerOS and
a fresh compositor capture verified rendering; the guard restored the original
Steam boot preference. This is recovery, not a fix for the GPU fault.

### Paired quiet signal measurements after recovery

Both installed clients joined one public Fluxer call. Flip used only isolated
virtual input A/B; Odin received into a virtual sink. System audio stayed at
Flip muted / Odin 0%, and no environmental microphone was enabled. The sender
played an original 440 Hz synthetic signal at amplitude 1800. Five-second PCM
captures at Odin's selected sink measured the following peak tone amplitudes:

| Controller action | Received 440 Hz amplitude |
| --- | ---: |
| Input A, call volume 100% | 1798.99 |
| Switch Flip to silent input B | 0 |
| Switch back to input A | 1802.66 |
| Odin call volume 50% | 900.87 |
| Odin call volume 0% | 0 |
| Restore 100%, mute Flip microphone | 0 |
| Unmute Flip synthetic microphone | 1801.14 |
| Odin Hear conversation off | 0 |
| Odin Hear conversation on | 1801.09 |

The receiver worker stayed PID 15300 across all measurements, without leaving or
rejoining. This proves actual remote signal routing, call gain and mute effects;
it does not establish human speech clarity, echo, latency or headset recovery.
Private evidence is `oct3-pair-input-b.png`, `oct3-volume-half.png`,
`oct3-volume-zero.png`, `oct3-pair-muted.png` and the devices' temporary
`quiet-measurements.jsonl` logs. No runtime source change was necessary.

Both calls were explicitly ended, microphone capture stopped, Follow system
routes restored and all virtual audio modules removed without changing defaults.
Final verification on both retained the production executable, voice/nearby
helpers, boot preferences, database integrity/counts (Flip 3/830, Odin 1/25) and
both ordinary Emerald save hashes. No live voice worker/capture remained; each
session had one already-defunct `pacat` child, which cannot produce audio.

## Readable call history and handheld gallery — 2026-10-02

Call events now name the caller (or say You started a call), with the local date,
time and server-reported call duration in the conversation itself. Existing known
missed events keep their missed status and a distinct tint; participant lists do
not fabricate missed-call status. Missing or reversed timestamps never fabricate
a duration. This follows the upstream [message call object](https://docs.fluxer.app/http-api/messages/#message-call-object).

ARM64 Social **85**, Interaction **37** and ExitPresentation **9** checks passed.
Both handhelds received and ran the same production executable, SHA-256
`5e1cde8d9900dc282097aa795899a738d5695c856d8f5d60acf6abe4a61aca28`.
Flip's actual conversation capture verifies the new incoming/outgoing rows and
durations. Odin's conversation, system menu and settings also rendered normally.
SQLite integrity/counts, ordinary save hashes, boot settings and nearby helpers
were preserved; the voice helper was unchanged. Odin output remains 0% and Flip
muted. No microphone or new call was activated for this gallery pass.

The owner-requested [18-screen gallery](../screenshots/README.md) contains original
captures from both installed handhelds with device/build provenance. This is UI
evidence, not new audio acceptance. Browser Upload still exposes no usable file
chooser through the available automation, so reverse media remains unverified.
Block 3 stays open for real headset/speech quality, remaining web/native audio
and reverse media interoperability, and separate-network acceptance.

## Retained call invitation and quiet paired audio — 2026-10-02

Odin recovered after the owner's manual restart. The guarded live-session switch
started TrainerOS and restored its original boot preference after acknowledgement.
No further device reboot was needed. This does not resolve the earlier Adreno
fault or establish its cause.

A native paired attempt reproduced a separate Home selection problem: ringing
expired between the captured Answer selection and the next A press, which then
selected Home. The shell now retains the selected invitation. When the same call
is still ongoing, Answer becomes Join call with the same action identity; one A
joins without changing the underlying page. If that call has ended or become
unavailable, a disabled row remains until navigation/dismissal. An asynchronous
update can no longer turn that pending A into Home. Unselected ongoing calls are
not added to Home, and reopening the menu starts normally.

The delivered production build was exercised on Flip with an Odin call: capture
`ring-fix-incoming.png` shows Answer, `ring-fix-delayed.png` shows retained Join
after ring expiry, and `ring-fix-answered.png` shows the joined two-person native
call. These are actual handheld captures in private `work/research`.

Before final delivery, a paired native call retained the same workers while
changing audio routes through controller-operated Communication settings. An
original synthetic 440 Hz signal entered Flip through an explicitly selected
virtual source; no environmental microphone was enabled. Odin received decoded
PCM in a non-default virtual output. Switching its output A to B moved the tone:
measured amplitude was about 89.5 before switching, zero on the old output, and
89.5 on the new output, with the same worker PID 5908. This establishes native
transport, decoding and live output rerouting without playing through speakers.
The call also survived leaving Settings/Social for Trainer and opening Home.

Input switching changed the selected capture route without replacing the call
worker, but the silent-input measurement coincided with zero receiver call
volume, so it is not an isolated quantitative input-switch proof. An earlier
volume check jumped between endpoints; a temporary diagnostic rebuild showed
normal five-point steps. No cause or permanent volume fix is claimed. Diagnostic
logging was removed and all tracked build inputs were reconciled before the
final production rebuild. The final Odin build then showed 100% -> 95% on a
single Left press and restored 100% on Right (`clean-volume-left.png` and
`clean-volume-restored.png`). Do not confuse call gain with system speaker volume.

Both calls were explicitly ended; virtual audio modules were removed and
original default routes preserved. Input/output preferences returned to Follow
system and call gain to 100%. Odin's system output remains 0%; Flip remains
muted. This is synthetic-signal evidence, not headset hot-plug or speech quality.

ARM64 checks passed: Social **84**, Interaction **37**, ExitPresentation **9**.
Both running production executables match SHA-256
`76e7065f5bf780a27c8da482ad6d7d11462652b0c7b9f6ae487d5b014d92fcda`.
SQLite integrity, Trainer/Adventure counts (Flip 3/830, Odin 1/25), ordinary
Emerald save hashes, boot preferences and nearby helpers were preserved. The
voice helper is unchanged. Block 3 remains **open** for real microphone/headset
quality, remaining web/native audio and reverse media interoperability, and
separate-network acceptance. No other numbered block was started.

## Live settings retention and Odin GPU interruption — 2026-10-02

On the unchanged `05c251c` delivery, Flip joined a call with the ordinary public
web client signed into the designated Odin test account. Both clients showed two
participants with microphones muted. Through the physical-controller input path,
Communication settings changed input/output from Follow system to the enumerated
headset input and speaker output, and call volume from 100% to 95%, then restored
the original selections. The voice worker retained PID `1469626`; leaving Settings
and Social for Trainer/RA retained the connection and Home call controls.
This proves session retention and settings acceptance, **not** live PCM rerouting,
headset hot-plug, speech quality or audible output. Flip's system output remained
muted; Odin's volume had been verified at 0% before its failure. Both test clients
explicitly left the call afterward; no microphone was enabled.

Private actual-device captures: `audio-live-device-selection.png`,
`audio-live-restored.png` and `audio-background-call.png` under `work/research`.
Flip's installed binary/helper, SQLite integrity, 3 Trainers / 830 Adventures,
boot settings, nearby helpers and Emerald save hash match the previous delivery.
No application binary or helper was changed in this check.

The initial native-to-native attempt did not connect Odin: its Home capture showed
Answer, but the next observed frame showed Home without a joined call. That result
does not establish the cause. Shortly afterward compositor capture stopped; the
kernel recorded GMU timeouts, an Adreno translation fault and an offending
`QSGRenderThread` belonging to TrainerOS at 22:18:04 local time. SIGTERM left PID
`154522` defunct with thread `154580` blocked in `dma_fence_default_wait`; SIGKILL
could not recover it. No game was running. A remote reboot was requested, but SSH
did not return during the subsequent checks; the owner then reported a manual
restart. This is another occurrence of the unresolved Odin GPU recovery gate,
not a diagnosed call-menu defect or a completed recovery.

Block 3 remains open. Next: verify Odin's recovery and repeat the paired settings
check, then the real microphone/headset, reverse web media/audio and
separate-network acceptance. Preserve quiet-device settings.

## Communication settings delivery — 2026-10-02

Start -> Settings -> Communication now uses the existing two-pane popup on
both handhelds. The right pane contains profile, audio, notifications, current
call and account controls; there is no provider settings application or extra
profile confirmation page. Name/about use the shared controller keyboard and
one Save. Picture selection stays in that pane, with bounded Pictures thumbnails
and Remove picture. Notification controls moved here from conversation Options.

Profile changes use the supported
[current-user API](https://docs.fluxer.app/http-api/users/current-user/), limited
to display name, bio and avatar. The server response is authoritative; failed
saves report failure. No private account response is exposed to QML. Pictures
are re-encoded as JPEG before upload. The actual service returned an eight-digit
asset hash, exposing an old overstrict avatar filter; short safe hashes now work.
Avatar requests use PNG because the installed handheld Qt image plugins rejected
WebP. Actual Flip capture confirms the uploaded image is visible after restart.

Audio preferences follow the existing Trainer/account namespace. Available
microphones and outputs are refreshed asynchronously only while this category
is open; monitor sources are excluded. Follow system remains the default.
Call volume and explicit routes are passed to the existing voice worker at join
and through a live command without joining again or unmuting. The microphone
test displays a local level for at most ten seconds, saves/uploads nothing and
stops on leaving settings or changing account. Existing recording/playback uses
these preferences too. Do Not Disturb, sounds and private previews reuse the
existing notification state. No active call keeps the two call-control rows in
place, so an ended call cannot shift their selection onto Sign out.

Observed controller checks: Flip name changed and restored through the server;
synthetic avatar uploaded, displayed and removed; both devices opened the new
category; volume and notification controls changed; preferences survived the
delivery restart. Flip discovered its input/output routes and ran the local
level test. Odin correctly reported no usable microphone. A zero-level capture
does not prove usable microphone audio. A new call reached Connected on Flip
and produced Odin incoming actions, but this attempt did not establish a paired
audio-settings retention result. A delayed test-input sequence reached Home and
launched Emerald on Odin; it stayed at the title sequence and was stopped before
deployment. The ordinary save hash was checked afterward.

The owner then requested silence on Odin: its system output was set to **0%**
and must remain there. Remote ringing checks stopped. Actual headset/voice
quality, live input/output switching during a paired call, decoded web/native
audio, reverse web media and separate-network acceptance remain open. Block 3
is **not complete**.

Validation: ARM64 Social **83**, Interaction **36**, ExitPresentation **9**
checks passed; production build and Python syntax checks passed. Actual captures
are in private `work/research/communication-*.png`, including
`communication-flip-delivered.png`, `communication-picker-fixed.png`,
`communication-flip-audio-final.png` and `communication-odin-notifications.png`.
Both delivered binaries have SHA-256
`2504fe7fe6b043942707ad205da1bc11a63172321872d0c0610125a3a7aab917`;
the voice helper is
`e8eba37559689420070c3612542f2102656a498c02597cb6cecd4fa6b2468452`.
Backup directories are `~/traineros-social/backup-1790967578` (Flip) and
`backup-1790967581` (Odin). Delivery restarts only TrainerOS, never either device.
Final checks found one installed process per device (1454862 / 154522), clean
SQLite integrity, unchanged Trainer/library counts (3/830 and 1/25), preserved
boot configuration and nearby helpers, and no pending Link transaction. Emerald
save SHA-256 remains `cf39ceece96e0b8804864a23fedb81bfdf6781ed3b560fff039ec21bf61666cf`
on Flip and `3fca83edc8bb3d69f5627a6ecec820069ca7fac8699676ecade2fd748ea47c7a`
on Odin. The test profile image/name were restored; test communication audio
preferences returned to Follow system/100%, independently of Odin's **0% system
volume**. Notification defaults were restored after the toggle checks.

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
