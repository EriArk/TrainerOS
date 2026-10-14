# Isolated Remote Play interoperability probe

Developer tooling for [L07](../../docs/REMOTE_PLAY_TRANSPORT.md), not an installed
TrainerOS feature. Linux ARM64, GStreamer 1.28.8 and the handheld V4L2 codecs were
used for the recorded checks. No full Moonlight/Sunshine/Wolf application, HTTP
server, pairing UI, application runner or desktop control is
included. There is deliberately no root build dependency or `install()` target.
The game probe can create one explicitly permitted Inputtino virtual controller.

The separate [automatic socket transport gate](transport/README.md) now checks
ICE/DTLS, encrypted fresh session material and Moonlight interoperability without
manual IP/key delivery. It remains a diagnostic integration candidate; live
lobby signaling, TURN provisioning and the full product launch are still open.

On the reviewed Armada baseline, safe capture teardown also requires the
[Gamescope buffer lifetime repair](../gamescope-capture/README.md). The shared
capture helper returns PipeWire buffers before writing to a bounded nonblocking
output queue, and handles closed readers/SIGTERM. The companion repair and helper
are installed on both development handhelds; they are not a full Remote Play
installation or automatic image delivery. Real Linux pad exclusion uses SDL
2.24+ device paths to recover names normalized by SDL.

## Build

```sh
cmake -S tools/remote-play-stream -B build-stream -DCMAKE_BUILD_TYPE=Release
cmake --build build-stream -j2
ctest --test-dir build-stream --output-on-failure
```

Requires C/C++17, CMake 3.24+, Python 3, OpenSSL, fmt, Boost **headers**, and the
GStreamer core/base/app/video, SDL2 and libevdev development packages. Runtime
plugins include V4L2 H.264, h264parse, Opus, videoconvert, audiomixer,
rawaudioparse, PulseAudio and, for display, glimagesink. The host also needs
`parec`, `pactl`, Python GI and existing permission to create a uinput device.
FetchContent pins the dependencies; the Moonlight revision pins its own ENet and
nanors submodules. Offline builds can set `FETCHCONTENT_SOURCE_DIR_ML`, `_WOLF`
`_WOLF_RS` and `_INPUTTINO` to already verified checkouts at those exact revisions.

The client and host are separate processes. Only the client links
moonlight-common-c; the host uses its RTSP parser/ENet and Wolf's packetizers,
crypto and separate nanors revision. Their FEC implementations must not be linked
into one executable by assuming that equal symbol names mean equal ABI.

## Run in a controlled LAN

Create a **fresh** random 16-byte key, 16-character ping token and nonzero uint32
connect token for each run. Deliver matching configuration files through the
existing private developer channel. Do not put them in Git, command lines, logs
or screenshots. Files must be regular, owned by the running user and mode 0600;
symlinks, unknown/duplicate fields and invalid bounds are rejected.

Fields (`key` is 32 lowercase hexadecimal characters):

```text
address=<host IPv4>
peer=<guest IPv4>
key=<fresh key>
ping=<fresh 16-character token>
connect=<fresh nonzero uint32>
seconds=25
display=0
sound=0
mode=synthetic
drop_every=0
```

Run `stream_host /private/session.conf`, then `stream_guest /private/session.conf`.
Host duration should exceed guest duration by at least five seconds. Allowed
lifetime is 5–120 seconds. Ports 49010–49013 are bound only for the experiment;
only the configured peer is accepted. The guest's `display=1` needs its graphical
session environment. Audio is decoded to measured PCM; `sound=1` additionally
plays it through the guest's ordinary PulseAudio output. Default `sound=0`
preserves quiet diagnostics; this option never changes system volume.

In synthetic mode the guest periodically sends controller A through Moonlight.
The host changes a test image's colour; it does not inject operating-system input.
The reported interval ends at the guest's decoded RGB sample, not at screen scanout.
`drop_every=30` drops the first data packet of each thirtieth video frame to exercise
the existing FEC; it does not simulate an internet connection or burst loss.
`drop_frame=180` removes **every** packet of frame 180, including parity, so FEC
cannot conceal the missing frame. It can be combined with `drop_every`.
The guest logs received IDR frame numbers and the last accepted frame; require a
new IDR and continued video/input after the loss, not just a successful event.

For game video, set `mode=game` on both sides and use, in the host's graphical session:

```sh
python3 tools/remote-play-stream/run-game.py CURRENT_GAME_PID build-stream/stream_host /private/session.conf
```

This reuses the installed TrainerOS `GameCapture` ownership/identity checks and
application-specific Gamescope helper. No root capture fallback exists. Closing
the owned game or changing capture identity stops the producer. The existing
helper remains limited to about 30 fps. Audio is selected from the same game's
exact process/Flatpak instance and captured with Pulse's per-stream monitor,
then mixed by GStreamer. No desktop mix/microphone fallback exists. An idle or
silent game sends silence. Child capture processes stop when the host exits;
the already running game is never launched or terminated by this wrapper.

The former diagnostic `--allow-p2` switch is removed. The trusted host's
`LobbyRuntime::input()` supplies a one-use inherited descriptor through
`TRAINEROS_LOBBY_INPUT_FD`, after authenticating the transport connection and
preparing its emulator port. `LobbyInput` checks the playing remote-mode room,
ticket, generation, member/role/seat and live-admission identity. It permanently
closes that grant on removal, context change, account loss or room closure.
Native netplay and spectators never qualify. An old connection's release cannot
revoke a newer grant; only one guest is supported by the current transport.
The local supervisor combines that permission with foreground ownership of the
owned game is foreground. Inputtino creates `TrainerOS Remote P2`; Moonlight
forwards the guest's first SDL-mapped physical controller. Guide exits locally.
Stale local grants or network input release controls after 350 ms plus the host
poll interval; a neutral state is required before input resumes. Keyboard,
mouse and Guide input are never injected. TrainerOS excludes these virtual
devices from shell navigation, even as a preferred controller. SDL normalizes the
Xbox name; the shared `RemoteGamePad.h` also checks the original kernel name
through SDL's device path. The input bridge and guest use this boundary too.

The virtual device name does **not** assign an emulator port. The adapter's
`launchRemotePlay` prepares strict native RetroArch P1/P2 reservations in a
temporary append configuration, preserving save paths and ordinary settings.
It accepts only the reviewed exact udev binary and a host-local controller
identity. The full device-assignment table stays enabled because RetroArch 1.22.2
checks spare slots before matching reservations; unused core ports are disabled.
Hotkeys remain on P1. Other emulator recipes, production session orchestration
and authenticated automatic internet admission remain required.
No permission comes from spectator status or a
network packet. `stream_guest_fixture` is a BUILD_TESTING-only client with a
process-local SDL controller; only the authenticated network can carry its input
to the host. It is not physical-controller acceptance.

Inputtino is pinned at `fd136cfe492b4375b4507718bcca1f044588fc6f`, built as a
static library without a Wolf runner. Preserve its
[MIT notice](https://github.com/games-on-whales/inputtino/blob/fd136cfe492b4375b4507718bcca1f044588fc6f/LICENSE)
with any distributed binary, along with all other dependency notices.

## Adaptations to pinned upstream code

`prepare.py` copies reviewed source directories into the build tree, preserving
upstream notices and Wolf's LICENSE. Each replacement requires an exact unique
match and fails on an upstream change. Original downloaded checkouts are unchanged.

- Preserve the returned owner of `gst_buffer_append`.
- Construct/destruct C++ string/shared_ptr members in the audio GObject; make
  buffer disposal idempotent.
- Start video numbering at 1, matching Moonlight's first accepted frame. Otherwise
  the initial SPS/PPS/IDR at frame 0 is discarded.
- Make crypto failures throw instead of logging and returning unauthenticated
  plaintext. The negative key/tag/nonce/ciphertext tests are mandatory on rebase.
- Replace only the logging interface with a small fmt wrapper, avoiding Boost.Log
  shared libraries. Packet/FEC/media protocol implementations remain upstream.

Control uses Moonlight's modern 12-byte GCM nonce, validates envelope/payload
lengths and rejects sequence replay. The host starts media only after successful
authenticated control. This is a **lab boundary**, not production admission:
RTSP and video are unencrypted and automatic authenticated internet admission
is not implemented. The local room/grant service is not network credential
distribution or production pairing. Do not expose the probe as an internet service.

## Evidence and remaining work

See [the dated device results](../../docs/REMOTE_PLAY_TRANSPORT.md#парный-стенд--14-октября-2026).
Paired emulator P2 movement, genuine game PCM, foreground permission withdrawal
and guest exit preserving the host game are now checked after device recovery.
SIGTERM of the video producer stops and waits for its capture child. Neither
these checks nor earlier FEC/negative-key evidence establish physical-controller,
audible/headset, WAN or product acceptance. See the later recovered-device entry
in that document and the later lobby/port entry. The root project's shared input
identity header is the only source dependency outside this tool directory;
for an exported source tree, set `TRAINEROS_INPUT_INCLUDE` to its reviewed copy.

The original Flip kernel driver emits a bottom band and lacks requested-IDR
support. The [maintained Iris fixes](../../packaging/kernel/iris/README.md) passed
geometry and real requested-IDR checks on Flip/Odin and paired whole-frame loss
recovery. They were loaded temporarily; they are not in the installed image.
Odin module unload then hung and required recovery, recorded in
[the device evidence](../../docs/ARMADA_IRIS_ENCODER.md). Do not repeat hot module
replacement there. The host counts actual SPS/IDR NALs, not just keyframe flags.

TrainerOS files: GPL-3.0-or-later. The fetched projects retain their licenses and
per-file notices (Moonlight GPL-3.0, Wolf root MIT, separate plugin/dependency
notices). Redistribution review, corresponding-source packaging and the complete
binary/image dependency inventory remain [#115](../../docs/PUBLIC_READINESS.md).
The size of one probe binary is not the total distribution footprint.
