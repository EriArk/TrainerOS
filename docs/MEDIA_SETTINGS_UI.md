# Two-pane Settings and Start controls - P1

The owner's 2026-09-20 correction supersedes the first media-description layout.
Settings uses a persistent category rail on the left and settings on the right,
inspired by tablet Android. Appearance, Sound, Media, Feedback, Trainer, System
Controller and Credits remain inside Settings; no primary section is added.

Up/Down selects a category and updates its right panel. A or Right enters its
controls; B returns to the same category, then to Start. Inside controls, Up/Down
selects a row and Left/Right adjusts a slider/theme/switch. A cycles themes,
toggles Reduced Motion, mutes volume, or invokes the selected setting action.
Unavailable rows remain readable and cannot open fake previews. Credits remain
available with controller Back. Global L1/R1 and Start retain priority.

Start has one full-width quick-control block above a single action list: volume
and brightness. Theme belongs only in Appearance. X toggles between controls and the remembered service.
Directional order follows the visual arrangement. Both surfaces reuse one custom
slider/setting row, real device readback and the existing coalesced platform
writer. Theme and Reduced Motion use the same durable device-wide preferences.
Brightness retains the verified safe minimum; absent hardware is not shown as 0.
The graphical sliders also support pointer adjustment, without requiring it.

This replaces the former oversized quick-control second column and media detail
pages. Media and feedback now use compact unavailable/status rows. No audio
player, video decoder, pack installer, vibration sender, RGB writer or sensor
polling is introduced. P5/P7 licensing, provider, preview/recovery and installation
gates remain unchanged. Pictures and optional Pokédex illustrations retain
separate identities and their existing source credits.

Acceptance: category/control focus, service return, global navigation, real
slider readback/mute, durable theme/motion preferences and write failure, pointer
bounds, unavailable rows, and inspected 960×540/native Flip layouts. Existing
account, Credits, device and Power routes must remain usable.

## Verification — 2026-09-20

The 35 Windows and 39 ARM checks passed after updating their controller routes
for the new visual order. Center's unchanged backup/restore assertions were
rerun with its corrected Start route; source bytes and protection copies passed.
Persistence checks cover theme/motion restart and service/Credits return.
Native Gamescope SDL checks passed focus, sliders, theme forward/reverse, category
entry/Back and unavailable rows, with no QML warnings. Start and Settings were
visually inspected at 960×540 and native 1920×1080. Device bounds retain a 5%
brightness floor and slider graphics clamp boosted volume to the track length.

The non-testing build was installed with a binary/database rollback copy. On the
real profile, injected controller input moved volume 36 → 41 → 36%, preserving
mute; independent platform readback confirmed the restored value. Settings
category/control entry and Back were inspected on the installed build. Database
integrity, the Trainer profile, 686 Adventure registrations and binary hash were
verified. Injected events do not replace the owner's physical-button check.

## Inline settings refinement - 2026-09-20

Trainer and System are category contents, not buttons leading to intermediary
service pages. Trainer exposes the profile, shared RA account and honest
unavailable separate-Trainer/PIN entry. Profile editing uses vertical setting
rows in the same right pane; keyboard and favorite picker retain modal priority.
Save/Cancel return to the exact Trainer row. System shows live network/storage
readings and refresh/restart/power actions; destructive power still confirms and
waits for pending service work. Volume and brightness remain in Sound/Appearance.
Controller is a Settings category; its live diagnostic action returns there.
Start has only volume and brightness above its single service list, without
Controller or theme. No new device capability or identity migration is enabled.

All face buttons use Switch positions in the shell: east A, south B, north X,
west Y. In particular physical west Y opens Choose Adventure; north X invokes
the labelled secondary action. Raw SDL diagnostics use the same labels. Emulator
gameplay mapping and physical keyboard X/Y remain unchanged.

Refinement verification: all 35 Windows and 39 ARM checks passed, with the
changed SDL/navigation scenarios rerun after correcting their routes. The
extended native Gamescope scenario passed without QML warnings; inspected
Trainer editing, account rows, System readings and Controller at 1920x1080.
The production build was installed after binary/database backup. Installed
profile editing/cancel, System readings/power confirmation and Controller return
were inspected through InputPlumber events; the actual profile, SQLite integrity,
686 Adventure records and installed binary hash were verified.

Input verification boundary: Flip 2 uses InputPlumber's ret2 capability map,
which converts source BTN_NORTH to West and BTN_WEST to North. Direct D-Bus
chords bypass that source mapping: on this installed Xbox target, injected West
arrives as SDL Y and injected North as SDL X. Read the source mapping when
interpreting remote events; these are not claims of physically pressing the
handheld. SDL tests cover raw west X -> shell Y and raw north Y -> shell X;
installed events were checked with the actual ret2 translation taken into account.
