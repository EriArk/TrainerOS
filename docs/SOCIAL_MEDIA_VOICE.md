# Communication media and voice — 2026-10-02

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
