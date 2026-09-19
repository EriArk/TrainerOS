# Ordinary-save exit — #49 implementation

## Delivered boundary, 2026-09-19

`AdventureLaunchController` now owns an `AdventureExitController` for its running
child. This is the native protocol foundation, **not a deployed game overlay**.
Production composition does not yet connect capture, controller interception or
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
3. Manual/unknown policy enters `Confirming`; the presenter can now ask whether
   the user saved. The future A/B routing calls `confirm()`/`cancel()` here.
   Cancellation discards the pending image and requests return to the **same
   live process**, without restoring the shell's launch navigation or relaunching.
   Cancellation is also allowed during capture.
4. Confirmation, or verified autosave after capture, enters `Closing` and asks
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
