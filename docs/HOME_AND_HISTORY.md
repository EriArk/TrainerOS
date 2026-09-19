# Home selection and observed play history

**2026-09-19 target:** the existing Home input/history baseline below is retained. Updated #9 shares its selector/context across Pokémon-aware pages; #49 replaces state selection/resume with normal saves and clean exit images. No code migration is claimed here.

The user's 2026-09-11 clarification separates choosing an Adventure from launching it. Y opens the compact Continue drawer. A on a card selects the Adventure shown on Home and closes the drawer without invoking an adapter. Home displays the selected Adventure's World, title and available data. A large molded circular button, mounted in the right chassis module, starts that Adventure. Its pressed motion does not delay controller input or launch.

## Accepted two-context extension — planned

Home X toggles Pokémon/Multiverse (#31), preserving independent choices/history per Trainer. Home-launched games return to that context; Worlds-launched games retain their originating paired face and route. Multiverse shows observed general play statistics, not Pokémon progress. Its distinct inner atmosphere (#32) keeps the shared chassis and fixed A/Y controls.

**Planned #9/#49:** one shell-owned **Y · Choose Adventure** drawer selects the active Trainer's shared `CurrentPokemonAdventureContext`: Adventure ID, resolved exact build and ordinary-save identity/revision when available. Pokémon Home, Pokédex, Center, Hall, RA and Adventure-aware Trainer consume this same context. Multiverse Home remembers its own independent game selection. Before an explicit choice, use the latest actual launch in that Trainer/domain; unrelated launches do not overwrite an explicit choice.

A on a card commits the context and closes without launching; B cancels and restores the opener unchanged. L1/R1 closes the drawer without committing and switches primary page. Modal/keyboard/Start/recovery flows suppress shared Y. Worlds intentionally keeps its browser-local search/filter controls. Resolve existing local-Y conflicts explicitly when implementing the shared route. Unsupported save features never silently select another title.

Cards show the latest clean TrainerOS exit image, title, World and honest session/progress metadata. They are recent Adventure choices, not emulator-state slots. No extra persistent Current Adventure capsule/chip or independent per-feature selector: headers/content may show identity naturally.

Unobstructed Home A immediately invokes its large physical launch button regardless of prior directional input. Launch uses normal game startup and the game's ordinary save/autosave; Worlds also has an explicit launch action. Unconfigured/empty selections offer setup or Worlds exploration. The bottom-frame drawer remains compact, expands before rising inside the fixed viewport, and preserves controller focus. See [shared selection acceptance](EXPANSION_42_62.md#shared-adventure-and-paired-navigation).

**Accepted target #49, not yet implemented:** normal TrainerOS creates, manages and resumes no emulator savestates/ResumePoints, in either Pokémon or Multiverse. Ordinary game saves/autosaves are authoritative. Relaunch starts the game normally; the game loads its own save.

On user-requested exit, capture a clean gameplay screenshot **before** the overlay. Resolve exact title/integration policy `manualConfirm | autosave | unknown`; do not infer it from platform. Manual/unknown asks “Have you saved?” while the game remains alive. B returns to the same process; A confirms graceful exit. Only verified autosave skips the question. Confirmation is a user assertion, not automatic proof of saving.

Exit images feed Home/Y/history with Trainer/domain/Adventure/session provenance. Cancelled attempts and crash/kill/battery loss cannot fabricate a confirmed exit or replace valid history with a false capture. Preserve prior valid media where appropriate and mark interrupted outcomes honestly. Capture failure leaves a usable exit/cancel path, never a state-thumbnail substitute.

Checkpoint the launching page/paired face/route/focus, restore it promptly on return, and refresh ordinary-save observations asynchronously. First prove capture, overlay/input ownership and cancellation with the still-running emulator on Flip. Migration retires only verified TrainerOS-owned obsolete state artifacts safely, preserving ordinary saves, histories and independent images. [Full lifecycle/migration acceptance](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit).

## Selection and navigation

- Without an explicit selection, Home follows the latest process launched through TrainerOS.
- **Legacy implemented storage pending #49:** a card selection stores `homeAdventure` and optional `homeResume` in versioned browsing state. The explicit choice remains until another card is selected, including after restart or launching something else from Worlds.
- **Historical state behavior:** a state selection could change Home to Resume Adventure; the GBA provider later implemented it. #49 supersedes this path and cancels further state expansion. Target cards select Adventures only; normal game startup loads ordinary saves.
- Missing capabilities offer Set up Adventure; an empty Home offers Explore Worlds. Missing selected identities fall back to the latest available Adventure or the empty Home.
- A is a fixed Home action: it immediately presses the large button, even after arbitrary D-pad/stick input. There is no directional traversal of Home's static modules. Y opens/closes the selector; B/Y cancellation preserves the prior selection and returns to the fixed main action. Open lists, text entry and system panels take priority over Home shortcuts.
- The drawer scrolls horizontally under controller focus, preserving the full selected-card outline. It stays inside the fixed landscape viewport. Start traps focus above it; B restores the card. L1/R1 retain primary-page behavior.

## Recording and storage

`AdventureLaunchController` emits identified start/end events only after the owned process starts. `PlayHistoryController` creates a stable session ID, captures UTC timestamps and measures elapsed time with `QElapsedTimer`. Checkpoint errors, missing executables and rejected concurrent launches do not create history. A started process that later crashes is recorded as Failed; it is not story completion or proof of successful gameplay.

Schema 4 adds `play_sessions` with an Adventure foreign key. Start and finish writes run on the existing SQLite worker, and cache projections change only after a committed write. A finish can update only the matching running identity. No double counting, identity reassignment or overwrite of completed sessions is accepted. Normal exit drains submitted writes. Storage failures remain visible on return and do not alter game saves.

At startup, previously Running sessions become Interrupted with unknown duration/end time. TrainerOS does not count the hours between a crash and the next boot. Durations include time while the child process stays open, such as its menus; they are not in-game save playtime. Sleep-specific accounting is deferred with the user's sleep work. Only launches through TrainerOS are observed.

The UI snapshot contains the latest session for each of up to 100 Adventures, ordered by insertion rather than the adjustable wall clock. SQLite retains the full journal. Totals sum all known durations for the selected Adventure. Badge/caught/story fields remain unknown unless a separate verified provider supplies them. No personal content, saves or screenshots are bundled.

## Acceptance

The first [game-progress provider](GAME_PROGRESS.md) supplies Home's badge flags and caught count for verified English Emerald/FireRed saves, explicitly labeled as the last in-game save. Unsupported progress and story fields remain unknown; recorded duration remains independent.

Native tests use actual child processes and SQLite close/reopen: failed start/checkpoint, normal return, child crash, rejected parallel launch, monotonic duration, idempotence rejection, foreign identity rejection, interrupted recovery, schema 3 migration and preservation of external bytes.

Rendered SDL scenarios exercise five recent cards, scrolling, menu/Back/shoulders, selection without a process, immediate A launch after directional input, modal confirmation without launching, launch/return and restart persistence at 960×540 and 1920×1080. Original mock flows remain isolated. Hardware acceptance checks the same Home actions on Flip 2; passing host tests does not close that gate.
