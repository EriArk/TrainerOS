# Ordinary-save exit — #49 implementation

## Delivered boundary, 2026-09-19

`AdventureLaunchController` now owns an `AdventureExitController` for its running
child. This is the native protocol foundation, **not a deployed game overlay**.
A separate Qt/QML presenter is now composed but remains gated off. Production
composition does not yet connect capture, controller interception or
graceful-close providers. Availability starts disabled and resets for each game;
the existing Flip launch/exit controls remain in place until its platform gate.

The exact launch can supply `Unknown`, `ManualConfirm` or `VerifiedAutosave`.
Unknown is the default on every launch. An integration must establish autosave
from title/build evidence; a platform name is insufficient.

The sequence is:

1. A platform-approved exit request enters `Capturing` and emits an attempt token.
   No confirmation or close request is emitted yet.
2. The capture provider returns that token and a clean gameplay image, or an
   error. A 2.5-second deadline treats a missing response as capture failure.
   Images are copied into temporary memory, capped at 64 MiB. No files are written.
3. Every policy enters `Confirming`; manual/unknown asks whether the user
   saved, while verified autosave asks whether to close the game. The presenter
   routes exclusive A/B input to `confirm()`/`cancel()` here.
   Cancellation discards the pending image and requests return to the **same
   live process**, without restoring the shell's launch navigation or relaunching.
   Cancellation is also allowed during capture.
4. Explicit confirmation enters `Closing` and asks
   the platform to close its owned game gracefully. Generic terminate/kill is
   explicitly not this provider. Once a close request has been sent, cancellation
   is no longer offered: it cannot retract an asynchronous emulator close.
5. Only an actual clean child exit in `Closing` emits a completed attempt. User
   confirmation is an assertion, never detected save completion. Normal launch
   restoration still occurs exactly once with its original navigation context.

Failure and race handling:

- Missing/failed capture still permits confirmation or cancellation. Completion
  can carry no image; a future media consumer must retain previous valid media.
- Cancelled attempts, late/duplicate results and results from an earlier game
  cannot supply a completed image. Losing platform availability cancels a pending
  capture/question; it cannot retract a close already sent.
- A provider may report a rejected close while the game is still usable. This
  discards the attempt and requests return to the game; a retry captures and asks
  anew. It must not report rejection merely because an accepted close is slow.
- Crash, error, generic process stop, or an exit before confirmation publishes
  no exit image. Stop requests are no longer masked as successful history merely
  because launch state was `stopping`: they produce the existing `Failed`
  outcome. Startup recovery's existing `Interrupted` outcome remains unchanged.

`completed` is a transient protocol result, **not a durable media/history commit**.
There is no new storage schema or deletion/migration in this increment. Ordinary
saves and legacy states/screenshots are untouched. The future media consumer must
bind Trainer/domain/Adventure/session/build/revision before requesting capture,
validate that identity on completion, and publish atomically. A null image must
not overwrite an earlier valid image.

## Verification and next gate

`AdventureExitTests` uses an original child process with an observable PID and
explicit normal/error/crash controls. It checks capture-before-question/close,
same-process cancellation, both manual/unknown policies, explicit autosave,
timeouts, stale results across attempts and sessions, rejected-close retries,
capability reset, forced termination, and exactly-once navigation return. Existing
process/history tests cover the changed stop outcome. These establish the native
protocol, not actual emulator save behavior or gamepad focus isolation.

Windows UCRT64/Qt validation for this increment: native build and **31/31 CTest
targets passed**, including the exit/process/history tests and existing rendered
SDL controller scenarios. Local Markdown links/anchors also passed. No Linux or
Flip validation is claimed for this revision.

Before enabling an adapter, verify on Flip's actual Gamescope/session setup:

- Intercept the requested exit before the current emulator/bridge hotkey closes
  the game. Preserve controller use in gameplay, including DS pointer controls.
- Capture gameplay before showing any TrainerOS window; no shell/state thumbnail
  substitution. Keep the game alive and route A/B exclusively to the prompt.
- B restores the same process/window with a neutral-input gate; held buttons must
  not leak to gameplay or the returned shell. A asks the owned emulator to close
  through its normal save-flushing path, with bounded failure handling.
- Confirm ordinary save survival and originating page/focus restoration. Observe
  a manual Pokémon title and a separately evidenced autosave title when available.
- Exercise shell crash/orphan recovery and capture failure without save loss.

In the first increment the owner was away from the handheld. No device session,
emulator configuration, sleep behavior or installed binary was changed. Hardware
evidence, the QML prompt/SDL routing, canonical media consumers and safe legacy
state retirement remain open under [#49 acceptance](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit).

## Flip feasibility follow-up, 2026-09-19

The first protocol build (`ec3a8fc`) was compiled and its native exit tests passed
on the actual ARM64 Flip. An isolated RetroArch/mGBA Emerald launch used copies
of a ROM and a private test ordinary save. No personal save paths or legacy
state configuration were changed. The fixture used normal game startup with
state auto-save/load disabled.

Observed with Armada's Gamescope 3.16.19 and InputPlumber 0.79.0:

- Gamescope captured an actual 1920×1080 Emerald frame before a temporary test
  window appeared. The inspected frames were clean **title-screen** frames;
  this does not yet certify gameplay-scene provenance or all emulator renderers.
  Capture/conversion took about 1.1–2.3 seconds in these three probes.
- A temporary GTK/X11 confirmation-shaped probe took compositor focus while
  the same RetroArch process remained alive. Unmapping it returned focus to the
  same game window; this proves platform feasibility, not the future Qt prompt.
- InputPlumber `InterceptMode=2` delivered physical face-button events over
  D-Bus while the SDL virtual gamepad reported no pressed buttons throughout.
  The owner confirmed that physical **A/right is `ui_back`**, and the repeated
  A-then-B probe observed **B/bottom as `ui_accept`**. With that mapping, A stayed
  in the probe and B release cancelled it without relaunching the game.
- The probe restored the original intercept mode (`0`) both after cancellation
  and its timeout. The proposed real overlay must additionally handle process
  death with an independent restoration watchdog and a neutral-input gate.
- A separate XRes ownership check sent `WM_DELETE_WINDOW` to the test emulator.
  It exited and focus returned to the existing TrainerOS window. The source
  test save's SHA-256 was unchanged and the staged ordinary save remained
  131072 bytes. This is not evidence of an in-game save operation or universal
  save-flushing behavior; the future A confirmation was not connected to close.

The intercept mechanism follows [InputPlumber's documented overlay routing](https://github.com/ShadowBlip/InputPlumber/blob/main/docs/usage.md#intercept-mode),
but physical button meanings above come from this device's observed events.
Do not translate `ui_accept`/`ui_back` literally for this Flip profile.

The owner then requested Switch-style A/B for the whole shell. `ControllerInput`
and diagnostic labels now use right A to confirm and bottom B to go back; raw
SDL reports retain positional names. Existing UI smoke scenarios exercise the
new raw positions. Emulator mappings, X/Y and L1/R1 are unchanged.

Remaining #49 gates: production exit-chord interception, a Qt/QML prompt wired
to the native protocol, input-lease crash recovery, real graceful-close adapter,
scoped durable media and legacy-state migration. No normal exit overlay is
enabled by this feasibility probe, and no verified-autosave title was exercised.

### Installed A/B increment

Windows passed **31/31 CTest targets** with Switch-position SDL events. Native
ARM64 passed **32/32**, including the session supervisor and exit protocol. An
initial headless ARM run exposed a missing offscreen environment on the pointer
test; its normal CTest registration now uses the same offscreen/software setup
as the other QML tests. The test passed with that setup and in the full rerun.

The non-testing ARM build was installed atomically after binary/database backup;
read-back SHA-256 was
`9293b1b045b68bf16caf8f277ea28c322a978937efc47b1e75fc54f4369bfb28`.
Database integrity, schema and Adventure count were unchanged. Inspected
Gamescope captures show the installed shell's Start menu, east/A opening
Settings, and south/B returning to the menu. These final installed-UI
checks used InputPlumber gamepad-event injection; physical A/B events were
observed in the preceding probe. The ordinary #49 exit overlay remains disabled.

## Qt presentation increment, 2026-09-19

Owner clarifications supersede the earlier autosave shortcut: **every exit asks**.
Manual/unknown uses "Have you saved your game?"; verified autosave uses
"Close this game?" and reminds the player to wait for any saving indicator.
A grants permission to close, never proof that a save finished. B returns to
the same process in both policies. The native protocol no longer auto-closes
verified-autosave sessions. Home/Guide is the target physical request instead
of Start+Select; Start keeps its shell-menu role.

`AdventureExitPresentation` presents the protocol without owning input devices,
capture, processes or files. `AdventureExitWindow.qml` is a separate, initially
hidden Qt window sharing the shell's theme, molded panel and mounted action caps.
It has no transient-window dependency on the hidden shell. It appears only after
capture has resolved, has fixed A/B actions without directional selection, and
shows a waiting state after close dispatch. After eight seconds it explains
that the game has not closed; it never escalates to a kill. Window-close requests
can cancel an armed question but cannot cancel an already dispatched close.

Input remains disabled until the provider establishes exclusive ownership,
the window has focus, and a **fresh complete neutral snapshot** arrives. That
snapshot must come from the intercepted physical stream, not a muted SDL virtual
pad. Buttons, both sticks and triggers participate. Focus/lease loss, disconnect
and phase changes invalidate the generation of queued input; each new attempt
requires a new proven lease. Simultaneous A+B selects Back. The provider must
separately keep its lease until controls are released when returning to gameplay;
this presenter gate is not a replacement for that platform release/watchdog.

The production composition includes the hidden presenter and suppresses shell
navigation during exit. It does **not** enable exit availability or install a
Home handler. Flip's current InputPlumber advertises `Gamepad:Button:Guide` on
the Retroid Pocket Flip 2 device, with intercept mode still zero. This inventory
does not prove the physical Home mapping or freedom from competing handlers.
The owner pressed physical Home during this increment and reported that no menu
opened; the actual event and request routing still require observation.
Existing emulator exit bindings and installed binary remain unchanged in this
increment. No ordinary saves, savestates or personal history are migrated.

Tests use an original live child and SDL gamepad events. They cover capture
failure, held-opening A, focus/lease revocation, stale events, disconnect,
same-PID cancellation, retry, A-before-actual-exit, waiting, both question texts
and restored shell page navigation. A synthetic autosave policy is a fixture,
not validation of an actual autosave title. Real Home routing, capture and
crash-safe input release are the next integration gate; durable media and safe
legacy-state retirement follow it.

Verification for this increment: Windows **33/33** and native ARM64 **34/34**
CTest targets passed. Rendered controller scenarios were inspected at 960x540
and 1920x1080. The first rendered run exposed the hidden transient-parent
problem; removing that dependency fixed it, followed by both full-suite runs.
The same final test binary also passed its SDL scenario on Flip's actual
Gamescope X11 display using Qt Quick's software renderer; its rendered 1080p
window was inspected and the original installed shell remained running. This
was an isolated original process fixture, not a real game or physical-input
lease test. The installed production executable was not replaced.
The ARM64 `BUILD_TESTING=OFF` production target also built successfully.

## Home transport increment, 2026-09-19

`AdventureOverlayService` now connects the native presenter to an opt-in
Flip/Gamescope helper. A private configuration enables it only for real
dedicated-session launches. It owns no game termination path: XRes must identify
the active window as a descendant of the launched process, whose start identity
must still match, and that window must advertise WM_DELETE_WINDOW.
This increment enables the question for RetroArch windows only. Standalone
emulators retain their existing exit route until their own close prompts are
verified; a generic window-close protocol alone does not prove that behavior.

The helper leases InputPlumber mode 1 only from mode 0. Physical Guide changes
it to mode 2 before the request. Read-only evdev snapshots include all buttons,
sticks and triggers; they are not inferred from the muted SDL target. Observed
physical codes are Home 316, right A 305 and bottom B 304. The Qt question waits
for its own focus and a fresh neutral generation. B keeps the same process;
input returns only after full neutral remains stable for 80 ms.

A separate watchdog holds the lease lock and a separate physical input/D-Bus
connection. GUI heartbeats, not merely helper activity, keep it alive. EOF or
a two-second heartbeat stall hides only the owned exit window and restores
input after release. A surviving helper also restores if its watchdog dies.
Service-owner replacement never mutates the replacement service. Lost close
transport clears the question without killing the game or publishing media.

Captures use the current user's single Gamescope compositor and a private
temporary directory. Stale capture results cannot confirm a different attempt;
capture failure still allows cancellation or confirmed exit. These frames are
not yet persisted as Adventure media. No saves, states or history are migrated.

Physical Home outside gameplay now returns to the Home page; Start and L1/R1
retain their existing roles. Home respects the shell's storage/service gate.

Device probes verified physical Home routing, a clean Emerald capture, same-PID
cancellation, normal owned-window close, and input restoration after helper
SIGKILL, watchdog SIGKILL and GUI-heartbeat stall. The recovery tests explicitly
set intercept mode; they do not pretend that injected Guide is a physical press.
The root hook was also checked by restoring read access from the hidden pad's
mode 000. Its service-start drop-in still needs a real reboot check.

The first installed run exposed a device-selection regression: granting raw
read access made SDL choose the grabbed Retroid source instead of InputPlumber's
Xbox target. The session now excludes physical USB `2020:3001` using SDL's
ignore list, inherited by child emulators. Rechecking the SDL inventory showed
only the virtual controller; installed Start, R1 and Home navigation then worked.
The initial lack of response was not a stranded mode-2 lease (mode was zero).

Installation/rollback and exact prerequisites are in the
[transport packaging notes](../packaging/integrations/README.md). Broader
adapter/title coverage, durable scoped media and legacy-state migration remain
open #49 gates; installed legacy exit bindings remain available during this
integration check.

Windows passed **33/33** tests, ARM64 **36/36**, including the real-process
stdio transport test. The non-testing ARM executable was installed with binary
and database backup; read-back SHA-256 is
`86fae040ab914483108293871db54651792048a11fafc50e72e0bbbad0e1bc1d`.
Database integrity, schema 6 and 686 Adventures were unchanged. Installed SDL
events exercised Start, R1, Home from Worlds and Home from the menu. The owner
confirmed that the physical in-game exit question worked, then requested a
compact panel over gameplay instead of the current full-screen presentation.
That visual follow-up is separate from the delivered transport.
