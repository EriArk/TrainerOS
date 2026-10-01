# Five-section navigation and legacy migration — #111

Implementation date: 2026-10-01. [Acceptance](EXPANSION_98_112.md).

Home / Worlds / Companions / Trainer / Social remain five full-screen peers.
L1/R1 stops at the existing primary edges. L2/R2 wraps section faces:

- Home and Worlds: Pokémon / Multiverse.
- Companions: Guide / Party / Boxes / Center / Playroom / Shops.
- Trainer: Profile / Journey / Hall / RA.
- Social: Messages / Groups / Communities / Search (owner refinement; [native Social](SOCIAL.md)).

Messages opens the last selected or first available conversation automatically.
Search uses a full-width discovery layout, not the conversation sidebar; existing
friends/requests are inside Messages and the Home Friends shortcut. Per-face
conversation choices remain account-scoped. Back retains the current conversation.

The Trainer profile, existing Hall controller's three independent face views and
their providers are reused. No dashboard, additional submenu or save writer was
introduced. First Trainer entry is Profile; ordinary visits retain its selected
face. `goToTrainerFace(profile|journey|hall|ra)` is the explicit semantic deep-link
boundary. Nested Champion/archive/achievement routes keep their own selection
and focus. Start's routes/quick controls and modal/transaction priority remain.
The original unlinked Social placeholder is superseded by [native Social](SOCIAL.md).
It reuses the existing Home shortcuts and keeps game selection out of Social.

## Navigation version 2

`ShellController::navigationState()` writes semantic primary IDs
`home/worlds/companions/trainer/social`, `trainerFace`, `socialFace` and the
existing nested controller states. It is an owner-scoped navigation JSON version,
not a SQLite schema or save-format migration.

Version 1 `pokedex` maps to Companions; `trainer` opens Profile; `hall` maps to
Trainer's Journey/Hall/RA according to its nested route. Legacy numeric slot 4
also maps to history, never Social. Hall's existing selected archive/set/goal,
Champion and per-face views remain intact. Valid version 2 checkpoints retain
the same face; unknown routes/checkpoint versions fall back to Home rather than
guessing a numeric destination. Invalid Trainer faces default to Profile. The
store accepts navigation v2 on reopen; a persisted version newer than the binary
retains its existing recovery/error gate, preserving that file instead of silently
rewriting future state. Image rollback must keep a compatible database/state pair.

The same restore boundary handles stored navigation and Adventure launch/return
checkpoints. Existing Settings is an overlay over the retained source route.
The selected Adventure's provider context is established before restoring nested
history views, so initial RA reconciliation cannot erase an inactive detail.
Owner changes reconstruct owner-bound stores/views through SessionState; external
account bindings and game saves are unchanged. Empty/future state resets new shell
face choices safely. Worlds primary re-entry still resets to its grid; restoring
an actual checkpoint restores its local wheel route as before.

## Separate pending work

The later [#112 local Home menu](HOME_MENU.md) supersedes this increment's
physical Home jump/game exit question with quick access and explicit Exit.
Start is not repurposed. #112 appearance and #99–110 social/online capabilities
and every earlier roadmap acceptance remain open independently.

## Verification

Native Windows and ARM64 builds cover the changed routing/QML. Automated coverage
includes named/numeric legacy pages, four/two-face wrap, explicit history links,
invalid/future checkpoints, modal priority, owner isolation, durable v2 reopening,
restart and child-process launch/return. SDL scenarios exercise the relocated
Journey/Hall/RA views, Social, primary edges, Settings and editor/keyboard focus.

The broader audit also found two ordinary-use focus/return bugs: initial RA
context reconciliation reset a restored inactive detail, and a newly materialized
Choose Adventure card could miss focus after fast navigation or Start dismissal.
The drawer now reconciles focus on materialization/visibility changes, only while
expanded, without polling. Return from an unpaired Link error now closes its
workspace instead of opening an empty peer list; pending settlement and paired
recovery remain guarded. Software-rendered tests wait for deferred focus/settled
wheel geometry, retaining bounded failure and clipping checks; long headings keep
their existing two-line elision policy across installed fonts.

Actual device delivery evidence is recorded below. Historical five-section
Journey captures remain dated old-layout evidence in NAVIGATION_20260927 and
their original module docs.

### Installed device evidence — 2026-10-01

Flip and Odin run the same production ARM64 binary, SHA-256:
`791fc67efe6bd20f993975dfe7b54d3810bb6f8f92e9a68498218d418ac830a4`.
Each deployment kept a paired binary/SQLite backup. SQLite quick-check passed;
Flip retained three Trainers/830 registrations and Odin one/25. Boot preferences,
nearby helpers, existing ROMs, artwork and ordinary saves were preserved;
InputPlumber remained active and no bilateral operation was pending.

Actual Gamescope captures show Home, Trainer/Profile/Journey/RA and Social's two
faces. Injected controller events exercised primary/secondary navigation and
Start dismissal; leaving Social restored the retained RA face on both devices.
Flip reopened on Social/Chats after a supervised process restart. Home A launched
Emerald immediately; input on the physical Retroid source opened the existing
guarded exit question and confirmed return to Home. This does not claim the
separate #112 menu delivered or constitute long-duration hardware acceptance.

All 56 ARM Linux checks are covered by passing results: the broad run passed
55/56, then the corrected persistence/QML scenario and wheel were rerun (2/2).
Affected Windows core/UI checks passed, with a bounded wheel rerun after a timing
failure; the final drawer change passed all five repeated persistence/QML checks.
Adapter-knowledge validation passed for 31 files across three exact profiles.

Odin froze before this binary was installed. The old QSGRenderThread was named
in an Adreno GPU fault, followed by a preemption timeout and blocked GPU worker.
SIGTERM could not finish, and a normal reboot request did not recover SSH. The
owner physically restarted it; Steam graphics returned, then the updated native
TrainerOS session rendered and accepted controller navigation. Its original
Steam boot preference was restored. See the dated incident in NEARBY_PLAY.
Recovery by restart is not a driver fix or proof that earlier freezes are solved.
