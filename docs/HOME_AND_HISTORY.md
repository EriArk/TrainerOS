# Home selection and observed play history

The user's 2026-09-11 clarification separates choosing an Adventure from launching it. Y opens the compact Continue drawer. A on a card selects the Adventure shown on Home and closes the drawer without invoking an adapter. Home displays the selected Adventure's World, title and available data. A large molded circular button, mounted in the right chassis module, starts that Adventure. Its pressed motion does not delay controller input or launch.

## Selection and navigation

- Without an explicit selection, Home follows the latest process launched through TrainerOS.
- A card selection stores `homeAdventure` and optional `homeResume` in versioned browsing state. The explicit choice remains until another card is selected, including after restart or launching something else from Worlds.
- Selecting a real state may make the Home action Resume Adventure. A recent-process card offers normal launch; it never claims to be an emulator state. Actual state enumeration/direct resume remains future adapter work.
- Missing capabilities offer Set up Adventure; an empty Home offers Explore Worlds. Missing selected identities fall back to the latest available Adventure or the empty Home.
- Left/Down on Home reaches the Continue affordance, Right/Up reaches the main button. Y works from either. B/Y cancellation preserves the prior selection and opener focus. A confirmed selection focuses the main button.
- The drawer scrolls horizontally under controller focus, preserving the full selected-card outline. It stays inside the fixed landscape viewport. Start traps focus above it; B restores the card. L1/R1 retain primary-page behavior.

## Recording and storage

`AdventureLaunchController` emits identified start/end events only after the owned process starts. `PlayHistoryController` creates a stable session ID, captures UTC timestamps and measures elapsed time with `QElapsedTimer`. Checkpoint errors, missing executables and rejected concurrent launches do not create history. A started process that later crashes is recorded as Failed; it is not story completion or proof of successful gameplay.

Schema 4 adds `play_sessions` with an Adventure foreign key. Start and finish writes run on the existing SQLite worker, and cache projections change only after a committed write. A finish can update only the matching running identity. No double counting, identity reassignment or overwrite of completed sessions is accepted. Normal exit drains submitted writes. Storage failures remain visible on return and do not alter game saves.

At startup, previously Running sessions become Interrupted with unknown duration/end time. TrainerOS does not count the hours between a crash and the next boot. Durations include time while the child process stays open, such as its menus; they are not in-game save playtime. Sleep-specific accounting is deferred with the user's sleep work. Only launches through TrainerOS are observed.

The UI snapshot contains the latest session for each of up to 100 Adventures, ordered by insertion rather than the adjustable wall clock. SQLite retains the full journal. Totals sum all known durations for the selected Adventure. Badge/caught/story fields remain unknown unless a separate verified provider supplies them. No personal content, saves or screenshots are bundled.

## Acceptance

Native tests use actual child processes and SQLite close/reopen: failed start/checkpoint, normal return, child crash, rejected parallel launch, monotonic duration, idempotence rejection, foreign identity rejection, interrupted recovery, schema 3 migration and preservation of external bytes.

Rendered SDL scenarios exercise five recent cards, scrolling, menu/Back/shoulders, selection without a process, main-button launch/return, retained Home focus and restart persistence at 960×540 and 1920×1080. Original mock flows remain isolated. New hardware acceptance and deployment require the Flip 2 connection; passing host tests does not close that gate.
