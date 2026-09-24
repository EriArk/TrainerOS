# Local game video previews

## Game information refinement — 2026-09-24

Both wheels now share GameInformation: a larger preview beside labelled pastel
fields for year, players, genre, developer and publisher. Empty fields disappear;
developer and publisher remain separate. The platform badge stays beside the
title. Game synopsis comes only from actual library metadata, never generated
import/setup instructions. Existing registration descriptions remain unchanged.

The description occupies the remaining lower area. Overflow waits 6.5 seconds,
then scrolls at 12 logical pixels/second, pauses at the end and returns to the
start. Selection changes, inactive pages and menus reset it. Reduced Motion
disables this automatic movement. No extra focus target, inline button legend or
separate game-detail page is added. All game-selection shortcuts stay intact.

ScreenScraper is paused until the owner reports access; local Batocera data is
the current source. Missing source metadata is not filled with invented facts.

Verification: Windows video, Batocera, Worlds and both shell/Worlds QML checks
passed. The ARM64 Release build was installed with binary/database backups;
829 Adventure records, three owners and schema 13 were retained. Native Flip
captures covered N64 Zelda/Banjo with all five facts and Emerald video with
only its known year. Controller navigation and L1/R1 return passed; captures
confirmed the long synopsis reaches its end and resets after page switching.
Sixteen exact generated import/checksum notes were removed from device XML
description fields (GB/GBC/GBA), with XML backups and preservation comparison.
Other descriptions, ROMs, media paths, saves and source-server files were kept.

Worlds and Multiverse share a single preview component in the existing game
details area. Batocera `gamelist.xml` supplies a local `<video>` path; the normal
scanner resolves it alongside screenshot, marquee and other artwork. No network
request or automatic video download occurs while browsing.

After selection stays still for 700 ms, the visible, focused wheel creates one
Qt Multimedia player. Playback is muted and loops while eligible. Changing the
selection, hiding the page, losing application focus, opening menus/dialogs or
launching an Adventure destroys the player. Returning starts the delay again.
The screenshot stays underneath; missing/unsupported media falls back to it.
Aspect ratio is preserved, including dual-screen and portrait footage.

Settings → Media → Video previews toggles playback device-wide. A/Left/Right
work through the existing controller settings flow. Schema 13 adds the persisted
`preferences.video_previews` flag, default on; no Adventure or Trainer identity
changes. Disabling this preference never deletes media.

Qt Multimedia and its QML plugin are runtime requirements. Codec availability
depends on the installed Qt backend; an unsupported file must not block browsing.
The existing still-image path remains independent of decoder success.

## Verification

The native `video_preview` test generates an original MP4 using ffmpeg, observes
decoded video frames, verifies settling delay, local-only input, error fallback,
page hiding and player destruction/recreation. Library tests cover controller
toggle and persistence; migration/ownership tests retain existing records.
The Windows suite passed 42/42; final scraper/video/library/ownership checks were
rerun after backend changes. ARM64 scraper/library/ownership tests passed 3/3.

The non-testing Release build was installed on Flip with binary and SQLite
rollback copies. Schema 12 → 13 retained 829 Adventure records and three owners;
SQLite integrity passed. An eight-second 540×360 H.264 title-screen recording
from the owner's Emerald ROM was attached using `<video>`, with an XML backup.
The installed wheel decoded it and was captured at native 1920×1080. Injected
SDL controller events exercised Settings → Media, Left/Right toggle and Back.
Turning playback off left no open video file and kept the wheel responsive.
Opening Start also released the video and decoder threads. These injected
events are device evidence, not a claim of a new physical-button user test.
L1/R1 page changes released/recreated the player, and a normal Emerald launch
kept preview media closed. The guarded exit returned to the shell; playback was
restored on returning to the wheel. The preference was left enabled.

The current Armada Qt backend falls back from unavailable Vulkan video decode
to software for this clip. A short sample measured about 50% of one CPU core
during playback versus 2% with playback off on the same wheel. This is bounded
clip evidence, not a long-duration/battery or all-codec performance guarantee.

ScreenScraper video acquisition and batch UI remain part of #65. Complete game
metadata/description layout remains in ROADMAP 6b; playback does not close either
of those scopes.
