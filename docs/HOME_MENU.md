# Physical Home menu — #112 / #49

The local menu reuses the existing shell, exit controller and owned-process
transport. It is separate from Start and from the Home primary page.

- In the shell, Home opens a compact Home / Friends / Chats / Notifications selector over the
  retained page, face and focus. Home again or B dismisses it without navigating.
  Selecting a destination is explicit. Friends/Chats use the existing Social
  faces. Notifications opens the unread/request list in the same overlay;
  A goes directly to the relevant conversation or selects the pending friend
  request without accepting it. B returns to quick access. See [Social](SOCIAL.md).
- During a supported Adventure, Home opens Continue / Exit game over a temporary
  game preview. Continue, B and Home again return to the same process. Opening
  the menu does not close play history, save, relaunch or request pause.
- Only Exit game calls the existing safe-exit controller. It hides the menu,
  retains exclusive input and captures a fresh game frame before the save question.
  Manual/unknown titles ask; verified autosave skips the manual question after a
  successful capture. Failed capture preserves the cancellable confirmation and
  previous valid media. No platform-wide autosave assumption is introduced.
- Start retains its system controls. It is not intercepted inside games or
  copied into Home. Native save writes still require a closed game and the
  existing owner/revision/transaction gates.

## Input, identity and recovery

The game presenter remains bound to the launch controller's actual session;
its title is resolved at game start, never from a mutable Home/library selection.
Ending a session or losing helper availability invalidates pending menu previews.
Menu captures use a separate generation and never enter exit media/history.

Helper protocol 2 supplies physical Home and vertical direction state in addition
to A/B, connection and full neutral state. Every layer/focus/lease transition
requires release. Down+A in one sample selects an action without activating it;
B/Home takes priority over A. Shell menu transitions also reset the central
controller neutral gate, including both sticks. Stable action positions cannot
be moved by presence updates; future capabilities must retain action identities.

Before either capture the helper checks that compositor focus belongs to the
owned game window, waits for its frame, and retains interception throughout.
Explicit exit never reuses the menu preview. Late worker results retain their
original capture kind/token; cancellation cannot relabel them. The independent
watchdog restores input on helper/UI failure without killing the game. Existing
graceful-close ownership and single-request safeguards remain.

## Remaining #112 work

2026-10-02 increment: supported RetroArch games now expose Screen & graphics
inside this same compact overlay. Per-game ratio (original/4:3/16:9/fill), pixel
filtering and shader enable/default/reset are persisted in TrainerOS-owned
configuration layered over the emulator configuration. They apply on next launch.
Shader toggle/next preview uses stdin commands on the owned running process.
Command delivery is not an emulator acknowledgement. Named shader/frame selection
was added in the outage exception below. Non-RetroArch controls remain open.

The shared [communication service](SOCIAL_MEDIA_VOICE.md) supplies a live unread
summary, incoming-call answer/decline and microphone/output/leave controls. It
does not dismiss or terminate the game. The summary never acknowledges messages;
outside a game the existing direct Social destinations remain. Emulator-native
multiplayer invitations still require #107's exact runtime route.

Supported emulator-native widescreen controls and non-RetroArch appearance
remain open. Use the emulator's own setting/patch support, not a new TrainerOS
patch collection; ordinary 16:9 scaling is a different operation. Unsupported
controls are omitted. In-game activity invitations await their actual runtime
route; no duplicate chat, voice or save implementation is introduced here. Full issue acceptance remains
in [the register](EXPANSION_98_112.md) and [roadmap](ROADMAP.md).

## Verification

### Installed shader/frame selection — 2026-10-02 outage exception

The owner authorized independent planned work during the public Fluxer outage.
Communication block 3 remains open. Screen & graphics now selects installed
presets by name within the existing Home overlay; A saves and returns to the
display controls, B returns without changing the choice, Home resumes the same
game. Settings apply at the next ordinary launch, never an automatic restart.
Default/Off are independent for shader and frame; Reset removes this game's
appearance preferences and inherits the emulator configuration again.

The launch worker discovers local device presets plus a small stock selection
(CRT, LCD grid, scanlines and sharp bilinear when installed), matching the
configured graphics driver. Mega Bezel and local presets come before stock effects. It does not present the
entire desktop shader corpus as handheld-compatible. The first Flip check caught
an incorrect driver mapping: `glcore` requires Slang, whereas legacy `gl` uses
GLSL. That mapping is corrected and covered by a regression test. Selecting a
custom local preset does not certify its GPU cost or all its dependencies.
Flatpak `/app` references remain runtime paths across deployment revisions.
Missing selections fall back to the base appearance and report that in Home.

Mega Bezel's installed GDV-MINI Light and Reflections variants are offered with
readable names. A small shell-owned reference preset retains the upstream source
and sets non-integer scale to 96%, automatic game aspect and no curvature. Its
fullscreen shader canvas disables manual viewport-owning borders to prevent double
frames. The later automatic-artwork increment below restores the Frame row for
margin-only decoration alongside Mega Bezel.
Light is the conservative choice; Reflections is optional, not universally
performance-certified. Existing miniature handheld device presets are retained
on disk but omitted from this picker.

Frame selection uses installed static, single-image, non-controller borders.
Only large clear openings (at least 85% of image height and half its width) enter
the picker; incompatible/obstructed decorations are skipped.
At launch the selected image's transparent rectangular opening is checked and
the game viewport is fitted into it at the display size. A frame therefore owns
the viewport while enabled; choose Off/default to use the separate Screen ratio.
Unsupported openings fall back rather than obscure gameplay. Both GL and Vulkan
use RetroArch's centered custom-viewport offsets; treating them as absolute
coordinates was caught and corrected on both handhelds. Base configuration,
save paths and input mappings are not rewritten. Shader presets with their own
decorative border remain distinct from a separately selected overlay frame.

Sources: [Mega Bezel setup and parameter guidance](https://github.com/HyperspaceMadness/Mega_Bezel),
[shader presets](https://docs.libretro.com/guides/shaders/),
[OpenGL Core shader format](https://www.libretro.com/index.php/op-koretroarch-1-7-7-new-opengl-core-driver-supports-slang-universal-shader-spec/),
[custom viewport behavior](https://github.com/libretro/RetroArch/blob/v1.22.2/gfx/video_driver.c),
[configuration keys](https://github.com/libretro/RetroArch/blob/v1.22.2/configuration.c).
Odin's existing ARMSX2 `EnableWideScreenPatches = true` was inspected and retained;
this increment does not yet expose its toggle in TrainerOS.
[Dolphin also supplies a native widescreen hack](https://dolphin-emu.org/blog/2024/02/10/dolphin-progress-report-november-and-december-2023-january-2024/).

ARM64 affected tests pass: 20 RetroArch cases and 9 Home/exit presentation cases,
including per-game isolation, independent Off/reset, removed files, Flatpak
update paths, frame fitting/rejection, and picker Back/Home lifecycle. The
production build has tests disabled. Private captures/logs use
`work/research/appearance-*`; no emulator artwork is added to the repository.

Paired delivery uses production SHA-256
`64fdb867e5776a95175eeb6b4060648cb2a0444d2b5932fdd158ec790c9767dd`.
Actual Gamescope captures on Flip (GLCore) and Odin (Vulkan) show Emerald at
approximately 96% height, original 3:2 geometry, no handheld mockup and no double
frame. Both retain Mega Bezel Light for this test game. Picker selection,
next-launch application and defaults/reset were exercised through injected device
controls. Database integrity, Trainer/library counts, boot configuration and
nearby/voice helpers were preserved; both Emerald save hashes remain unchanged.
Reflections is listed from the installed pack but has no paired performance proof.

On the final repeated Odin launch, the injected physical Home event no longer
opened the overlay (RetroArch showed its pause indicator); the process and SSH
remained responsive. Earlier picker/exit sequences succeeded. Bounded process
and journal diagnostics were collected, then the title-screen test emulator was
terminated and TrainerOS restarted for delivery. After restart, physical Home
opened the overlay again and explicit Exit returned normally. This is an unresolved Home/input
recovery observation, not a claim of an overnight stable game/voice session.

### Automatic game artwork — 2026-10-02

Frame now defaults to Automatic: exact ROM filename, then normalized No-Intro
name, then the canonical catalogue title of an official edition. Matching stays
inside the platform's `overlays/GameBezels/<system>` directory. Known dump tags
(region/revision/languages/SGB enhancements) are stripped; unknown subtitles and
hack tags remain significant. A renamed official Emerald still matches; a hack
never borrows the official edition's catalogue identity. Existing manual choices
and Off remain explicit. Emulator default and Reset opt out of automatic art.
No binding, file picker or additional launch confirmation is required.

A missing/unreadable matching image falls back to the platform PNG, then normal
emulator appearance. Installed local/configured overlay roots are supported,
including Flatpak host mapping. The bounded loader runs only during launch,
not navigation. Derived PNG/config files are atomically cached in the existing
RetroArch config directory; source size/mtime invalidate the cache. Nothing is
written to ROMs, saves, upstream presets or the base emulator configuration.

The generated artwork clears the entire maximum game rectangle. Thus even an
opaque system illustration cannot cover gameplay. Ordinary rendering uses the
full-size aspect-correct viewport; Mega Bezel keeps its own 96%-height game and
thin rim, with matching artwork in unused margins. Full-screen shader canvas is
RetroArch index 24 (21 is square pixels). The generated configuration contains
only one value for each overridden key; a live check caught that duplicate
`input_overlay_enable` entries otherwise kept the earlier false value.
Screen Fill and unsupported/dynamic-aspect platforms do not apply automatic art.

Private provision on both handhelds: 53 source PNGs, 10,234,586 bytes, selected
for the existing GB/GBC/GBA library plus all three platform fallbacks. This is a
subset of Bezel Project, not its full collection or an automatic downloader.
Future matching artwork can be added in the same upstream directory structure.
The current platform map also recognizes common RetroArch console packs; those
routes and wider artwork coverage are not live-certified by this GBA check.
Non-RetroArch emulators and final generic artwork pack management remain open.

Sources: [Bezel Project GBA](https://github.com/thebezelproject/bezelproject-GBA),
[GB](https://github.com/thebezelproject/bezelproject-GB),
[GBC](https://github.com/thebezelproject/bezelproject-GBC),
[RetroArch aspect enum](https://github.com/libretro/RetroArch/blob/v1.22.2/gfx/video_defines.h).
Exact source revisions, URLs and SHA-256 values travel with the private images in
`overlays/traineros-bezel-sources.json`; images are not redistributed in Git.

ARM64 checks: 22 RetroArch and 9 Home/exit presentation tests pass, covering exact
and canonical matching, platform/hack isolation, opaque fallback masking, corrupt
image fallback, Off/reset, and automatic art coexisting with Mega Bezel. The
portable game-adapter knowledge check passes unchanged (32 files, 3 exact games).

Both handhelds now run production SHA-256
`f91ad4f8f2b4851ececb5cd084753efaaefded0d42c40baec2755765daf8792c`.
Actual Gamescope captures `work/research/bezel-flip-verified.png` and
`bezel-odin-verified.png` show Emerald with matching side artwork and large 3:2
gameplay (Flip GLCore, Odin Vulkan). Automatic was selected through the existing
Home picker on Flip; Odin used the new default without per-game setup. The
retained Mega Bezel Light preset coexists with both. Input was injected remotely,
not an owner physical-button acceptance test.

Odin recovery during delivery: its previous TrainerOS process was a zombie with
one uninterruptible GPU wait thread (`dma_fence_default_wait`). Restart did not
recover rendering. A reboot was requested with no active emulator. After the
owner reported startup, SSH returned and the final build was installed. Odin's
original Steam boot policy was restored after entering the dedicated TrainerOS
session. This does not diagnose or fix the underlying GPU wait.

The earlier repeated-launch physical Home observation remains open. During this
pass menu visibility/input was intermittently delayed, and one intermediate
Flip title-screen test required bounded emulator termination and shell restart.
Final Home overlays appeared on both consoles. No game progress was written.
Both final explicit Exit confirmations returned to the shell with no remaining
RetroArch process. Final verification matched the installed and live binaries,
retained 3 Trainers / 830 Adventures on Flip and 1 / 25 on Odin, and confirmed
unchanged Emerald save hashes, boot policy, nearby helpers and voice helper.
This display increment does not claim to complete #112 or communication block 3.

### Background voice controls — 2026-10-02

An active call adds Voice call to the existing shell Home menu on every primary
page. The same panel holds microphone mute, call-output mute, Leave and Back;
it does not navigate into Social first. Physical Home during a supported game
uses the existing overlay controls and the same session-owned call. Back/Home
retain both the call and the originating page or game. Only Leave ends it.
Notifications remain passive and dismissing them does not acknowledge messages.
The Voice call action and its panel identify the current conversation, even if
the underlying page displays another chat. Missed calls open their conversation
from Notifications without automatically joining or ringing anyone.
Incoming calls expose Answer/Decline in the same Home menu. Outside a game,
opening Home with an answerable ring selects Answer immediately; accepting from
Home or its inbox retains the underlying page and closes Home. A current call
must be explicitly left before accepting a different one. In-game Home uses the
same validated call actions. Dynamic actions retain selection by ID, so a new
ring cannot move Answer underneath an already selected Notifications action;
if the selected action disappears, focus returns to Continue.
See [current paired audio evidence and remaining gates](SOCIAL_MEDIA_VOICE.md#background-call-recovery--2026-10-02).

2026-10-01 delivery:

- Windows native build and five affected CTest entries passed: core input,
  interactions, exit lifecycle, presentation and rendered exit scenario.
- ARM64 exercised all 56 CTest entries in a network-isolated container. The
  initial run passed 54; the old Home interaction expectation was updated and
  the video test rerun with the headless Qt environment. The final eight-entry
  rerun passed, including helper recovery, transport and rendered Home/Exit.
  Production was rebuilt with `BUILD_TESTING=OFF`.
- Both live binaries match SHA-256
  `8f801f20ddbdc3486491000f14c18f812a46e7ff2c68bafda0e88a458e7b3429`.
  Both protocol-2 helper files were installed with backups; each device's own
  database, boot preference, library and nearby helper files were retained.
  Flip retains 3 Trainers / 830 registrations; Odin retains 1 / 25.
- Actual Gamescope captures show the shell menu and Emerald overlay on both
  handhelds, preserving their different shell themes and emulator appearance.
  Events were injected through the handheld input sources over SSH, not pressed
  by the owner. Continue/repeated Home and cancellation retained the same live
  process (Flip 439458, Odin 99199). Explicit Exit then returned both to TrainerOS;
  both history outcomes are `returned`, with no remaining emulator process.
- Reopened SQLite exit images have verified checksums and contain clean game
  frames, without the Home menu or save question. On Odin the retained emulator
  bezel/shader is part of that game image. No in-game progress was written for
  this check. The owner still has the normal physical-button feel check.

Private logs/captures are under `work/research/home112-*` and each device's
`~/traineros-home112`; they are not distribution assets. Synthetic autosave
evidence does not certify an actual game's autosave policy or an untested
runtime's focus/pause behavior. The provider does not request pause, but an
emulator's own inactive-window setting can still pause it; online runtime proof
remains separate.
