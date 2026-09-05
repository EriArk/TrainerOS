# Codex Start Brief

Use this as the first implementation task after reading `README.md`, `AGENTS.md`, and all files under `docs/`.

## Objective

Create the first runnable TrainerOS Android prototype that proves the **controller-first full-screen shell** and the **Home + Continue Adventure interaction**.

Do not begin with real game/emulator integration. Use mock domain data and a `MockAdventureAdapter`.

## Required stack

Recommended baseline:

- Kotlin
- Jetpack Compose
- AndroidX ViewModel
- coroutines/Flow

Room/DataStore may be introduced immediately if it stays lightweight, but the first visible milestone matters more than persistence architecture ceremony.

## Required project behavior

### App shell

- landscape-first full-screen UI
- five primary pages in this order:
  1. Home
  2. Worlds
  3. Pokédex
  4. Trainer
  5. Hall of Fame
- `L1` moves to previous page
- `R1` moves to next page
- D-pad and left stick navigate focus inside a page
- `A` confirms/opens
- `B` closes the smallest current layer or returns from detail
- `Start` opens a system menu overlay
- `B` on a primary page must not exit the app

### Page switching

- full-screen pages, not Home-with-overlays
- short horizontal transition
- preserve useful page state/focus when switching away and back
- do not use `L1/R1` for any local component

### Home

Build a visually intentional mock Home, not a settings/dashboard grid.

Show a compact combination of mock data such as:

- active World: Hoenn
- active Adventure: Pokémon Emerald (placeholder data only)
- badge progress
- Pokédex progress
- one recent milestone/activity
- battery/time placeholders or real system values if easy

The exact final art direction is not locked. Use original geometric/gradient placeholder visuals and custom components; do not import official Pokémon artwork.

### Continue Adventure drawer

Implement a slide-out/collapsible Home drawer with 3–5 compact mock ResumePoint cards.

Each card should visibly demonstrate:

- screenshot placeholder
- Adventure title
- World
- optional location
- timestamp
- small progress line

Requirements:

- drawer is not permanently open
- drawer is not the whole Home page
- cards remain compact
- opening/closing works without touch
- focus is trapped inside while open and restored after close
- selecting a card calls `MockAdventureAdapter.resume(...)` or equivalent and shows a temporary success/debug result instead of launching a real external app
- the permanent drawer shortcut is TBD; use a clearly documented temporary control/focusable Home affordance and do not consume `L1/R1`

### Worlds

Create a first-pass region browser with mock cards for:

Kanto, Johto, Hoenn, Sinnoh, Unova, Kalos, Alola, Galar, Paldea.

At least Hoenn should show `IN_PROGRESS`; others can use mixed mock statuses.

Opening a World can show a simple detail surface with mock Adventures. `B` returns to the World browser.

### Pokédex

For the first build, only prove the shell:

- small local mock list
- focused item treatment
- basic entry detail

Do not spend time on external APIs yet.

### Trainer

Mock profile showing:

- trainer name
- favorite Pokémon placeholder
- total playtime
- Worlds visited/completed
- badge/Pokédex summary

### Hall of Fame

Mock archive with at least one sample completed Adventure and a six-slot team layout using neutral placeholders/text.

### System menu

`Start` opens an overlay with at least:

- Settings
- Controller
- Pokémon Center
- Manage Adventures
- Android Settings (can be disabled/stubbed initially)
- Exit (stub/explicit action)

`B` closes the menu and restores focus.

## Architecture minimum

Even in the prototype, create domain models or equivalents for:

- `World`
- `Adventure`
- `ResumePoint`
- `TrainerProfile`
- `HallOfFameEntry`

Create an integration abstraction and `MockAdventureAdapter` rather than putting fake resume logic directly in Compose components.

## Visual requirements

Aim for a modern in-universe handheld terminal:

- custom dark/light surface hierarchy as appropriate
- strong readable typography
- restrained colored accents
- obvious custom focus state
- region/type colors as accents, not full-screen noise
- no stock Android launcher look
- no giant uniform app-tile grid
- no mouse/desktop window chrome

Use original placeholders only.

## Suggested component set

Keep it small:

- `TrainerPageScaffold`
- `TopLevelPageIndicator` (subtle; not a permanent desktop nav bar)
- `FocusableCard`
- `ProgressChip`
- `WorldCard`
- `ResumePointCard`
- `ContinueDrawer`
- `SystemMenuOverlay`
- `TrainerFocusIndicator` / shared focus modifier

Names may change if a better architecture emerges.

## Tests for this task

At minimum cover:

- next/previous primary page logic
- `L1/R1` mapping to page actions
- Back precedence for drawer/detail/system menu
- Continue ordering by recency
- mock adapter capability/response behavior

## Deliverable

A clean build that can be installed on an Android handheld and navigated end-to-end with a controller, with mock data proving the product structure.

When done, update `README.md` status and mark the appropriate items in `docs/ROADMAP.md` rather than inventing a separate direction in code comments.
