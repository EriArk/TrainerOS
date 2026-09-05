# TrainerOS UX & Navigation

## Goal

TrainerOS must feel native to a handheld controller. Touch may exist as a convenience, but every normal flow must be fully usable with physical controls.

## Primary pages

Top-level order:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

These are full-screen pages. Home is not a permanent base layer.

### Global page switching

- `L1`: previous primary page
- `R1`: next primary page

These buttons are reserved for primary navigation everywhere in TrainerOS. A feature must not steal them for tab switching, local carousels, filters, or other actions.

Recommended transition: short horizontal page slide with immediate input acceptance after transition start/end. Animation should clarify spatial movement, not slow navigation.

Whether the page list wraps from Hall of Fame back to Home should be decided during device testing.

## Baseline controls

- D-pad: directional focus/navigation
- Left stick: directional focus/navigation, with appropriate dead zone
- `A`: confirm/open/activate
- `B`: back/close/cancel
- `Start`: system menu
- `L1/R1`: global page navigation

Other buttons remain available for later shortcuts, but should not become mandatory until their behavior is documented and remappable.

## Focus rules

1. A visible focus target must always exist when a screen has interactive content.
2. Focus order must be deterministic.
3. Entering a page should restore a sensible last focus when possible.
4. Leaving and returning with `L1/R1` should not unexpectedly reset scroll position.
5. Closing a modal/drawer should return focus to the control that opened it or a predictable fallback.
6. A modal/drawer traps focus until closed.
7. Scrolling lists keep a focused item visible.
8. Disabled controls must look disabled and be skipped by normal focus traversal.
9. Empty states still need an obvious next action when one exists.

## Home

Home is a living overview with several compact information areas. It is not a tile grid by default.

The selected Home module should have a strong but restrained focus treatment. Movement between modules should follow their actual spatial layout rather than an invisible linear sequence where possible.

### Continue Adventure drawer

Continue Adventure is a slide-out Home panel containing compact recent session/save-state cards.

Confirmed behavior:

- it is not permanently expanded
- it does not replace Home
- it uses compact cards
- `L1/R1` must continue to switch primary pages while the drawer is closed
- opening/closing must be possible without touch
- when the drawer is open, focus stays inside until it is closed or an Adventure is launched

The final shortcut to open the drawer is intentionally **TBD**. During prototype work, use a clearly isolated temporary mapping or a focused Home control; do not silently make a permanent mapping decision.

### Continue card content

Preferred compact card hierarchy:

1. screenshot
2. Adventure title
3. World / optional location
4. timestamp
5. tiny progress row such as badges/playtime

Avoid turning every card into a dense stats panel.

## Worlds

Worlds should prioritize horizontal/visual browsing of regions while remaining fast with the D-pad.

Opening a World moves into a detail layer for that region. In World detail:

- `B` returns to the World list
- `L1/R1` still switch the top-level primary page, not local sub-tabs
- local subsections should use D-pad/focus or other documented controls

Adventure selection should expose a simple launch/resume action first, with technical configuration in a secondary menu.

## Pokédex

The Pokédex needs fast movement through a potentially large set.

Design for:

- stable list/grid focus
- fast alphabet/number/type filters without requiring touch
- search as an optional text-entry path, not the only discovery method
- entry detail that can be browsed with D-pad
- obvious Seen/Caught/Favorite state

Do not rely on color alone for Pokémon type or progress states.

## Trainer

Trainer is mostly an information page. Keep focus targets limited to meaningful actions such as editing profile, opening a metric/history, or changing featured Pokémon.

Avoid making every statistic separately focusable if it has no action.

## Hall of Fame

Hall of Fame should behave like an archive/timeline.

- entries are controller-browsable
- opening an entry shows its team, completion details, screenshots/notes when present
- `B` returns to the archive
- editing/manual correction is a secondary action

## System menu

`Start` opens the system menu from any TrainerOS page.

The system menu overlays the current page rather than becoming another L1/R1 section.

Initial categories can include:

- Settings
- Controller
- Pokémon Center / backups
- Manage Adventures
- Android settings
- Exit/restart

`B` closes the menu and restores the previous page/focus.

## Back behavior

Back should always do the smallest sensible reversal:

1. close a context menu/modal
2. close a drawer
3. leave a detail view to its parent
4. otherwise remain on the current primary page

`B` on a primary page should not accidentally exit TrainerOS. Exiting should be an explicit system-menu action.

## Input repeat

Controller repeat should feel console-like:

- initial directional press moves once
- held direction repeats after a short delay
- repeat accelerates modestly for long lists if needed
- focus must never skip unpredictably due to frame rate

Tune on physical hardware rather than relying only on emulator previews.

## Visual focus language

Focus should be obvious at a glance but not look like Android TV defaults.

Possible ingredients:

- slight scale/elevation change
- bright outline or bracket treatment
- animated cursor/marker
- subtle background contrast shift

Use one coherent focus language across all features.

## Screen transitions

Preferred hierarchy:

- primary pages: horizontal movement
- detail open: short depth/panel transition
- drawer: directional slide from its edge
- system menu: overlay/panel

Avoid elaborate transitions that delay input or make rapid L1/R1 navigation annoying.

## Touch behavior

Touch support is optional convenience. If implemented:

- tapping should correspond to the same focus/activation model
- touch must not create controls unavailable to gamepad users
- after touch, the next gamepad input should restore visible focus cleanly

## Controller testing checklist

For every screen:

- Can it be opened with no touch?
- Is focus visible immediately?
- Can every essential action be reached?
- Does `A` do the expected action?
- Does `B` return one logical level?
- Do `L1/R1` still switch primary pages where appropriate?
- Does `Start` open system menu?
- Does focus survive scrolling and page return?
- Can the user recover from empty/error states without touch?
