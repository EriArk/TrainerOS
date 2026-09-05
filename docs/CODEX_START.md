# Codex Start Brief

Use this as the first implementation task after reading `README.md`, `AGENTS.md`, and all files under `docs/`.

## Objective

Create the first runnable **native Linux TrainerOS prototype** that proves the controller-first full-screen shell and the Home + Continue Adventure interaction.

This is no longer an Android project.

Do not begin with:

- Android/Kotlin/Jetpack Compose
- APK packaging
- replacing the ArmadaOS session
- real emulator integration
- save-file reverse engineering

Use mock domain data and a `MockAdventureAdapter`.

## Required baseline

- C++20
- Qt 6
- QML / Qt Quick
- CMake

Keep persistence lightweight for the first visible milestone. SQLite/repositories may be introduced immediately if they do not slow the controller/UI proof.

## Required project behavior

### App shell

- landscape-first full-screen Qt Quick UI
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
- `Start` opens a TrainerOS system-menu overlay
- `B` on a primary page must not exit TrainerOS

The first build should be runnable from a normal Linux/Plasma development session. Do not configure a dedicated/default graphical session yet.

### Input layer

Create a centralized semantic input layer rather than handling raw button codes separately in every QML screen.

At minimum normalize:

- Up
- Down
- Left
- Right
- Confirm
- Back
- SystemMenu
- PreviousPage
- NextPage

Allow a development keyboard mapping in addition to real gamepad input, but controller behavior is the product requirement.

Do not let feature QML consume `L1/R1` for local behavior.

### Page switching

- full-screen peer pages, not Home-with-overlays
- short horizontal transition
- preserve useful page state/focus when switching away and back
- rapid L1/R1 input must remain comfortable

### Home

Build a visually intentional mock Home, not a settings/dashboard grid.

Show a compact combination of mock data such as:

- active World: Hoenn
- active Adventure: Pokémon Emerald (textual placeholder data only)
- badge progress
- Pokédex progress
- one recent milestone/activity
- mock or easy real system status values

The visual design is intentionally **not final**. Use original geometric/gradient placeholder visuals and custom QML components; do not import official Pokémon artwork.

### Continue Adventure drawer

Implement a slide-out/collapsible Home drawer with 3–5 compact mock ResumePoint cards.

Each card should demonstrate:

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
- opening/closing works without touch/mouse
- focus is trapped inside while open and restored after close
- selecting a card calls `MockAdventureAdapter::resume(...)` or equivalent and shows a temporary mock/debug result instead of launching a real emulator
- permanent drawer shortcut remains TBD; use a clearly documented temporary Home affordance and do not consume `L1/R1`

### Worlds

Create a first-pass region browser with mock entries for:

Kanto, Johto, Hoenn, Sinnoh, Unova, Kalos, Alola, Galar, Paldea.

At least Hoenn should show `IN_PROGRESS`; use mixed statuses elsewhere.

Opening a World may show a simple detail surface with mock Adventures. `B` returns to the World browser.

### Pokédex

For the first build only prove the shell:

- small local mock list/model
- focused-item treatment
- basic entry detail

Do not integrate external APIs yet.

### Trainer

Mock profile showing:

- Trainer name
- favorite Pokémon placeholder
- total playtime
- Worlds visited/completed
- badge/Pokédex summary

### Hall of Fame

Mock archive with at least one completed Adventure and a six-slot team layout using neutral placeholders/text.

### System menu

`Start` opens an overlay with at least:

- Settings
- Controller
- Pokémon Center
- Manage Adventures
- Desktop / Maintenance Mode (stub only in this milestone)
- Restart TrainerOS / Exit Development App

`B` closes the menu and restores focus.

Do not implement real Plasma session switching yet.

## Architecture minimum

Even in the prototype, create domain types or equivalents for:

- `World`
- `Adventure`
- `ResumePoint`
- `TrainerProfile`
- `HallOfFameEntry`

Create an integration abstraction plus `MockAdventureAdapter` rather than putting fake launch/resume logic directly in QML components.

Create at least minimal boundaries for:

- navigation/shell state
- input service
- Adventure adapter registry/service
- persistence/repository placeholder
- platform/system service placeholder

Do not put shell commands, emulator paths, filesystem scans, or SQL directly in QML.

## Visual requirements

Aim for a modern in-universe handheld terminal:

- custom surface hierarchy
- strong readable typography
- restrained colored accents
- obvious custom controller focus state
- region/type colors as accents, not full-screen noise
- no KDE/desktop look
- no giant uniform app-tile grid
- no mouse/desktop window chrome
- no stock Qt Widgets aesthetic

Use original placeholders only.

## Suggested component set

Keep it small and replaceable:

- `TrainerPageScaffold.qml`
- `TopLevelPageIndicator.qml`
- `FocusableCard.qml`
- `ProgressChip.qml`
- `WorldCard.qml`
- `ResumePointCard.qml`
- `ContinueDrawer.qml`
- `SystemMenuOverlay.qml`
- shared focus visual component/state

Names may change when a better architecture emerges.

Do not treat this component list as a commitment to the final visual design.

## Suggested early directory shape

```text
CMakeLists.txt
src/
  app/
  core/
    model/
    input/
    navigation/
    repository/
  integrations/
    adventure/mock/
  platform/
    linux/
  qml/
    shell/
    pages/
    components/
    overlays/
    themes/
tests/
```

Keep it pragmatic; the first goal is a clean controller-navigable shell, not maximum modularity ceremony.

## Tests for this task

At minimum cover:

- next/previous primary page logic
- `L1/R1` mapping to page actions
- Back precedence for drawer/detail/system menu
- Continue ordering by recency
- mock adapter capability/response behavior
- shell state restoration basics

## Deliverable

A clean native Linux build that can run full-screen and be navigated end-to-end with a controller using mock data.

The first deliverable is **not an APK** and is **not yet a replacement ArmadaOS session**.

When done:

- update `README.md` status
- mark the appropriate roadmap items
- document any input/device assumptions
- do not silently start real emulator/session integration unless the next task explicitly moves to that milestone
