# TrainerOS UX & Navigation

## Goal

TrainerOS must feel native to a handheld controller. Touch may exist as a convenience and mouse/keyboard may exist in development or maintenance mode, but **every normal TrainerOS flow must be fully usable with physical controls**.

The normal shell must never feel like a KDE desktop application stretched full-screen.

## Primary pages

Top-level order:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

These are full-screen peers. Home is not a permanent base layer.

### Global page switching

- `L1`: previous primary page
- `R1`: next primary page

These buttons are reserved for primary navigation everywhere in TrainerOS. A feature must not steal them for tab switching, local carousels, filters, or other actions.

Recommended transition: short horizontal page slide that explains spatial movement without delaying input.

Whether the page list wraps from Hall of Fame back to Home should be decided during physical-device testing.

## Baseline controls

- D-pad: directional focus/navigation
- Left stick: directional focus/navigation with appropriate dead zone
- `A`: confirm/open/activate
- `B`: back/close/cancel
- `Start`: TrainerOS system menu
- `L1/R1`: global primary-page navigation

Other buttons remain available for later shortcuts but should not become mandatory until documented/remappable.

Development keyboard bindings may mirror these actions, but controller semantics remain authoritative.

## Focus rules

1. A visible focus target must always exist when interactive content exists.
2. Focus order is deterministic.
3. Entering a page restores a sensible last focus when possible.
4. Leaving/returning with `L1/R1` should preserve useful scroll/detail state.
5. Closing a modal/drawer returns focus to its opener or a predictable fallback.
6. A modal/drawer traps focus until closed.
7. Scrolling lists keep the focused item visible.
8. Disabled controls look disabled and are skipped by normal traversal.
9. Empty/error states still expose an obvious recovery action when one exists.
10. Switching away to an external Adventure and returning should restore the TrainerOS context quickly.

## Home

Home is a living overview with several compact information areas. It is not a tile grid by default.

The selected Home module should have a strong but restrained focus treatment. Movement should follow the actual spatial layout rather than an invisible arbitrary sequence where practical.

### Continue Adventure drawer

Continue Adventure is a slide-out Home panel containing compact recent session/save-state cards.

Confirmed behavior:

- it is not permanently expanded
- it does not replace Home
- it uses compact cards
- `L1/R1` continue to mean primary-page navigation while the drawer is closed
- opening/closing is possible without touch/mouse
- when open, focus stays inside until the drawer closes or an Adventure launches

The final shortcut to open the drawer is intentionally **TBD**. During mock work, use a clearly isolated temporary mapping or a focused Home control; do not silently make a permanent mapping decision.

### Continue card hierarchy

Preferred compact card hierarchy:

1. screenshot
2. Adventure title
3. World / optional location
4. timestamp
5. tiny progress row such as badges/playtime

Avoid turning every card into a dense statistics panel.

## Worlds

Worlds should feel like choosing destinations rather than browsing files/emulators.

Opening a World moves into a detail layer for that region.

In World detail:

- `B` returns to the World list
- `L1/R1` still switch the top-level primary page, not local tabs
- local subsections use D-pad/focus or another documented secondary control

Adventure selection exposes a simple launch/resume action first. Emulator/path configuration belongs in an advanced/service layer.

## Pokédex

The Pokédex needs fast movement through a large set.

Design for:

- stable list/grid focus
- fast alphabet/number/type filters without mandatory text entry
- search as optional text entry, not the only discovery method
- entry detail browsable with D-pad
- obvious Seen/Caught/Favorite state

Do not rely on color alone for types/progress.

## Trainer

Trainer is primarily informational. Keep focus targets limited to meaningful actions such as editing the profile, opening a metric/history, or changing the featured Pokémon.

Avoid making every static statistic focusable.

## Hall of Fame

Hall of Fame behaves like an archive/timeline.

- entries are controller-browsable
- opening an entry shows team/completion details/screenshots/notes when present
- `B` returns to the archive
- editing/manual correction is secondary

## System menu

`Start` opens the TrainerOS system menu from any page.

The menu overlays the current page rather than becoming another L1/R1 section.

Initial categories may include:

- Settings
- Controller
- Pokémon Center / backups
- Manage Adventures
- Audio / brightness / network shortcuts
- Power
- **Desktop / Maintenance Mode**
- Restart TrainerOS

`Desktop / Maintenance Mode` deliberately leaves the TrainerOS everyday shell and enters KDE Plasma. It should be clearly framed as an advanced/maintenance transition, not another app in a launcher list.

`B` closes the menu and restores previous page/focus.

## Back behavior

Back always performs the smallest sensible reversal:

1. close context menu/modal
2. close drawer
3. leave detail view to parent
4. otherwise remain on the current primary page

`B` on a primary page must not accidentally exit TrainerOS, terminate the graphical session, or enter Plasma. Session/desktop transitions are explicit system-menu actions.

## Adventure launch / return

Launching is a deliberate temporary exit from the shell presentation.

Expected interaction:

- user activates an Adventure/resume point
- TrainerOS records the current context
- external application takes over presentation
- when it exits, TrainerOS returns promptly
- previous primary page/detail/focus is restored where sensible
- external metadata refresh happens after the UI is already usable

The user should not see a desktop/taskbar between TrainerOS and a supported Adventure in production session mode.

## Input repeat

Controller repeat should feel console-like:

- initial directional press moves once
- held direction repeats after a short delay
- repeat may accelerate modestly for long lists
- focus must never skip unpredictably because of frame rate

Tune on physical hardware.

## Visual focus language

Focus must be obvious at a glance without resembling desktop or TV-framework defaults.

Possible ingredients:

- slight scale/lift
- bright outline/bracket treatment
- animated marker/cursor
- subtle surface contrast shift

Use one coherent focus language across all features.

## Screen transitions

Preferred hierarchy:

- primary pages: horizontal movement
- detail open: short depth/panel transition
- Continue: directional drawer slide
- system menu: overlay/panel
- Adventure launch: quick intentional handoff, not desktop minimize animation

Avoid elaborate transitions that delay rapid controller input.

## Touch/mouse behavior

Touch support is optional convenience. Mouse/keyboard support is useful for development but is not part of the normal product UX.

If touch is implemented:

- tapping corresponds to the same focus/activation model
- touch does not create essential controls unavailable to controller users
- next gamepad input restores visible focus cleanly

Do not design hover-only interactions.

## Desktop / Maintenance Mode UX

Plasma is intentionally separate from TrainerOS.

Before switching, TrainerOS may show a short confirmation such as “Open Desktop Mode?” if accidental activation would be disruptive.

The system should preserve a straightforward path back into TrainerOS. Detailed Plasma UX is outside TrainerOS scope.

## Controller testing checklist

For every screen:

- Can it be opened without touch/mouse?
- Is focus visible immediately?
- Can every essential action be reached?
- Does `A` perform the expected action?
- Does `B` return one logical level?
- Do `L1/R1` retain their global meaning?
- Does `Start` open TrainerOS system menu?
- Does focus survive scrolling/page return?
- Can the user recover from empty/error states without touch?
- Does returning from an Adventure restore a sensible context?
