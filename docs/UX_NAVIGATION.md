# TrainerOS UX & Navigation

Worlds includes the [collection checklist and controller attachment flow](COLLECTION_CATALOGUE.md). Grey missing cards remain focusable; A → Link a file opens the shared picker, while Save/Cancel returns to the original Worlds detail. Platform badges identify the edition without changing region-first navigation.

In a World's Adventure list, X searches title/version/platform through the controller keyboard, Y cycles All/Linked/Missing, and left/right jumps eight rows. Up/down chooses adjacent rows. Search and filter are remembered per World; empty results retain the fixed X/Y controls and a focused Back action. These shortcuts yield to open keyboards, menus and service panels; L1/R1 remains global.

The [2026-09-13 roadmap](ROADMAP.md) preserves these page-specific controls and Home's selection-without-launch behavior. Older issue #9's direct-launch rule is superseded by the owner's later clarification; global Continue access remains deferred until its Home-selection semantics and replacement local Y shortcuts are specified. Planned Caught local sections and account/artwork/service screens must fit the current mapping, yield to modal priority and never consume L1/R1. Exact new local shortcuts are selected and controller-tested in their increment, not copied uncritically from conceptual issue mockups.

Planned account entry is shared through Settings → Accounts → RetroAchievements, with Hall opening the same account surface. Pokédex All/Caught/Favorites/Journal are local collections; Caught initially shows manual species-wide assertions. Plasma Mobile is the preferred future Maintenance destination only after its recovery/input trial; see [platform consolidation](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).

## Accepted expansion routes — planned

The [expanded plan](EXPANSION_PLAN.md) preserves the five peer pages and existing A/Y behavior while adding:

- **Unobstructed Home X:** Pokémon ⇄ Multiverse, preserving separate selections, Continue/history and backgrounds for the active Trainer/context. A in Continue still selects; the large fixed Home A action launches. Modal actions take precedence over X/Y. Home-launched play returns to the same Home context; Worlds-launched play keeps its originating route.
- **Worlds local contexts:** Pokémon regions versus Multiverse systems, then game list/detail. No sixth primary page or L1/R1 local tab use. Preserve search/focus/Back per context; exact local mode-selector controls are chosen and physically tested during #28. Four visible readable game rows replace the older mock's three-row target after shared geometry/header work, not through tiny type.
- **Entry routing:** no usable Trainer → registration; multiple or PIN-protected Trainers → chooser/unlock; one unprotected default Trainer may open Home. Wizard drafts use the shared controller keyboard; PIN uses a compact numeric keypad. Before unlock, Start has only safe entry/recovery actions and primary pages cannot expose personal data.
- **Start:** mounted quick volume/brightness controls, Power → off/restart/switch/cancel, and Help. Left/right adjusts a selected quick slider; up/down traverses its list. Switching waits for safe write/Adventure boundaries. System values come from the platform. Power requires a fresh confirmation; B cancels.
- **Offline Help:** category/article hierarchy, controller-driven bounded scrolling and internal links/Back; no touch or online browser dependency. Settings adds accounts, audio/feedback, supported lighting and optional Steam through existing service navigation, never new primary pages.

**Shoulder precedence is explicit:** despite ambiguous #26/#35 wording, the established post-login rule remains: L1/R1 cancels transient menus/confirmations/drafts and switches primary pages, without applying a slider value, confirming Power or committing a draft. A held confirm cannot activate a newly opened destructive confirmation. Changing this rule would need a separate intentional UX decision. Entry/locked states cannot use shoulders to bypass profile selection. See [expanded acceptance](EXPANSION_PLAN.md#trainer-ownership-onboarding-and-power).

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
- `Y`: open/close Continue Adventure on Home

Other buttons remain available for later shortcuts but should not become mandatory until documented/remappable.

Development keyboard bindings may mirror these actions, but controller semantics remain authoritative.

## Focus rules

### Fixed handheld viewport and scrolling

Target the Flip 2 landscape display. Home and normal overview surfaces fit in one screen. Drawers and system panels open within that screen without extending the outer page. Touch swipes must never be required.

Long World/Adventure lists, Pokédex entries, and archives may scroll in bounded areas. D-pad/left-stick focus movement automatically reveals the selected item; scrolling must not move the entire shell or its primary navigation. Use a detail view for additional information instead of pushing essential actions below the screen or shrinking text to fit.

For the current design study, `L1/R1` closes transient panels and switches the primary page; it never scrolls a local list. Closing the system menu with `B` restores the previous drawer/detail/page focus. Page wrapping remains deferred to physical-device testing.

### Focus behavior

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

Home uses fixed physical-button actions: A invokes the large Adventure button immediately and Y opens the selector. D-pad/stick movement does not move between its static modules or change what A does. Keep a visible ready state on the main button. Across pages, prefer labeled button actions for permanent commands and reserve directional focus for choosing items in lists, grids and open panels.

### Continue Adventure drawer

Continue Adventure is a slide-out Home panel containing compact recent session/save-state cards.

Confirmed behavior:

- it is not permanently expanded
- it does not replace Home
- it uses compact cards
- `L1/R1` continue to mean primary-page navigation while the drawer is closed
- opening/closing is possible without touch/mouse
- when open, focus stays inside until the drawer closes or a primary-page switch dismisses it

`Y` is the confirmed Home shortcut for opening/closing Continue. `A` chooses a card for Home without launching; it closes the drawer and restores the large Home action button. `B` cancels, preserving Home. Outside panels, A immediately starts/resumes the chosen Adventure; no focus movement is required or available on Home. Higher-priority text-entry, selection and system panels own their confirmation and Back actions. `A` cannot launch Home's Adventure through an open panel, and `Y` does not steal input from those panels. Keep the physical binding configurable and leave `L1/R1` globally reserved.

The visible closed control is attached to the bottom-left frame, with a diagonally cut right edge. Opening expands its width before the panel rises; closing retracts it before narrowing. Its motion stays entirely within the 16:9 screen.

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

The native mock has three local levels: **regions → Adventure list → Adventure detail**. A region may contain originals, remakes and ROM hacks together.

- The region grid uses D-pad/left-stick spatial navigation; `A` opens the selected region.
- In the Adventure list, Up/Down moves focus and reveals the selected row inside a fixed four-row viewport (shared header pass, 2026-09-13). The shell, primary tabs and lower action panel remain stationary. Down beyond the final row focuses Back to Worlds; Up restores that row.
- `A` opens Adventure detail. Left/Right moves between enabled launch/resume/Back actions; unavailable actions look disabled and are skipped. Exact Continue requires both a direct-resume capability and a matching resume point. Otherwise normal launch is offered when supported.
- `B` returns detail → Adventure list → regions, preserving the selected Adventure and region. On regions, `B` stays on Worlds.
- `L1/R1` still switch the top-level primary page. Returning to Worlds restores its route, selected Adventure and focused action. Start/menu/notice layers also restore the underlying focus when closed.
- An empty region keeps Back to Worlds focused. An entirely empty World library exposes Return Home. Failed mock launches keep the detail context available for retry after dismissing the response.

Emulator/path configuration belongs in an advanced/service layer. Sample progress is labeled as such; unknown values are not silently replaced with zero or a completion state.

Acceptance: traverse a list longer than its viewport using controller events, keep the full focus outline visible, open a remake and ROM-hack example within their region, return to the exact row after detail/page/menu changes, skip a disabled launch, retry a failed launch, and recover from an empty region without touch. These checks currently exercise mock adapters; no external Adventure is launched.

## Pokédex

The Pokédex needs fast movement through a large set.

Design for:

- stable list/grid focus
- fast alphabet/number/type filters without mandatory text entry
- search as optional text entry, not the only discovery method
- entry detail browsable with D-pad
- obvious Seen/Caught/Favorite state

Provide controller-accessible World/regional-collection, type, Seen/Caught/Favorite filters, name/number search, and sorting. Filters combine; empty results retain a reset/recovery action. The filter rail is mounted to the top frame, and action strips join a frame edge. Search opens a keyboard panel mounted to the lower frame, with D-pad/left-stick key selection, `A` to enter, an explicit Apply key, and `B` to cancel. `L1/R1` never becomes local filter navigation.

The shared search/profile keyboard places letters on the left and a separate numeric block on the right: `1–3`, `4–6`, `7–9`, then a wide `0`. Space, Delete, Clear, and Apply remain visible beside the letters. D-pad/left-stick movement follows the displayed rows, crosses between both blocks, and stops at outer edges. Vertical movement preserves the intended column through wide keys. All editing and submission work with the gamepad; `B` discards the text buffer and restores focus to its opener.

Acceptance: using gamepad events alone, enter mixed letters and digits, reach every numeric key, cross both ways between blocks, use Space/Delete/Clear/Apply, and cancel without changing the previous value. Focus remains visible and inside the keyboard throughout entry.

The native keyboard now implements this shared interaction for Trainer names and Pokédex search. The initial layout offers uppercase Latin letters and digits; additional languages/case layouts are deferred. Start places the system menu above the keyboard and Back restores the same key and buffer. Y does nothing inside text entry. L1/R1 cancels transient input and any containing profile draft, then changes the top-level section. The previously applied search query remains intact. Neither changing sections nor Apply silently saves a profile.

Current Pokédex mock navigation:

- Up/Down follows the entry list inside a fixed three-row area. Up from its first row enters the frame-mounted filter rail. Down from that rail restores the remembered row, or the recovery control when results are empty.
- Left/Right on the rail selects Search, World, Type, Records, Order and Reset. A opens the corresponding text/choice panel. The five-column choice grid uses spatial D-pad navigation; A applies, while B or Cancel preserves the old value. L1/R1 dismisses an unconfirmed picker and remains global.
- Filters intersect. Status choices include Seen, Caught, Not caught, Favorites, Not seen and Not recorded; unknown records are not treated as explicit negatives. Search matches a name substring or an exact national number. Sorting preserves the selected entry by ID when it still matches.
- Entry detail opens with A; Left/Right selects Favorite or Back. B returns to the same row. Removing a favorite while filtering Favorites returns to a remaining row, or focuses Reset filters if none remain. A failed write preserves the old mark and allows retry from the same detail action.
- Start/B restores picker, keyboard or entry focus. Section changes preserve applied filters, query, sort, detail route and useful focus. Empty reference data/load errors offer Retry; unmatched filters offer Reset. Failed reference refreshes keep the last successful snapshot available.

Acceptance: browse beyond the visible rows, combine three filters with a query, select the last type choice with the controller, cancel search without changing results, preserve detail through page switching, and remove the last filtered favorite without losing focus. Test name entry and numeric entry through the shared keyboard without updating the Trainer profile.

Do not rely on color alone for types/progress.

## Trainer

Trainer is primarily informational. Keep focus targets limited to meaningful actions such as editing the profile, opening a metric/history, or changing the featured Pokémon.

Avoid making every static statistic focusable.

Create Trainer is available when no local profile exists. Edit Trainer opens a draft of the saved name, avatar/emblem, and favorite Pokémon. All fields, Save, and Cancel are controller-accessible; the name field can open the same on-screen keyboard used by search. Back closes the keyboard before cancelling the profile draft. Saving updates the profile without changing its identity or progress, and cancelling preserves the existing profile.

Current prototype: Up/Down moves among Name, Emblem, Favorite and Save; Left/Right moves between Save and Cancel. A on Emblem/Favorite cycles the visible sample choice. A on Name opens the shared keyboard. Apply changes the form draft; Save writes to the fake repository. A blank/invalid name returns focus to Name; a failed write leaves Save focused with the draft available for retry. The UI states that this prototype keeps profiles only until the application closes.

Acceptance: create a Trainer from the empty state, cancel a keyboard edit, cancel the whole form, save a later edit without changing ID/creation time, recover from a failed save, and switch sections from an open keyboard without committing drafts. Editing identity does not change sample Adventure progress.

## Hall of Fame

Hall of Fame contains the completed-Adventure archive and RetroAchievements achievements within the same primary page.

- entries are controller-browsable
- opening an entry shows team/completion details/screenshots/notes when present
- `B` returns to the archive
- editing/manual correction is secondary
- achievements have controller-browsable lists/details with their game and source identified
- internal archive/achievement selection uses focused controls and A/B; L1/R1 still switch primary pages
- missing connection, unsupported content or refresh errors leave local archive navigation available
- show cached external data as cached and unknown availability honestly, without presenting it as zero unlocks

The native mock has two frame-mounted local selectors: Archive and RetroAchievements. Up above the first list row enters their rail; Left/Right selects and A opens an area. L1/R1 continues to switch primary pages. Lists reveal the selected item in a three-row viewport. Down after the last row enters the lower action panel, and Up restores the selected row. In an empty list, the action panel remains usable and Up returns to the rail.

Archive A opens a team/memory detail; B restores its list row. RetroAchievements uses Adventure sets → goals → achievement detail, with B unwinding one level and remembered per-set selection. Detail actions use Left/Right; Up reaches the local rail. A refresh disables its repeated activation while keeping Back focused and available. Completion can update the data while another primary page is visible without changing that page.

Start/B restores the underlying action, while L1/R1 preserves local routes and useful focus. Disconnected/unsupported data is not an empty earned-achievement total. Cached results are labeled offline or failed-refresh, and account changes remove the previous account's records. Local archive browsing remains available in every external-provider state.

Acceptance: browse a team with six members and a partially recorded team, scroll beyond visible archive/goals, visit a locked and unknown goal, preserve unlock mode/unknown date, retry a failed refresh, return to archive during loading, reject prior-account records and retain focus after an empty archive is reloaded. All current cases use original fixtures, not real provider data.

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

- attached relief or a pressed/inset response that preserves contact with the supporting panel
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

## Shared text entry

Letters and punctuation occupy the left keyboard module; the separate numeric block stays on the right. X switches letter case and returns to letters. Y cycles two punctuation pages and letters. All printable ASCII characters are reachable with the controller, including spaces through Space. Empty punctuation slots are hidden and skipped by directional focus. Apply submits; B discards the buffer. Password entry starts lowercase and exposes only masked display text to the QML field. Applying or cancelling clears the controller's draft. Start preserves the draft beneath its menu, and L1/R1 still close text entry and change the primary page.

Acceptance: native checks cover punctuation, hidden-key navigation, limits, masking and draft clearing. The SDL smoke path enters mixed case, punctuation and a digit, checks rendered bullets, captures both symbol pages and verifies Back clears the draft.

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

## Library and settings services

Start → Manage Adventures opens a controller-operated service panel. A edits the selected record; its action rail offers Add Adventure and Back. The editor keeps six fields in a two-column grid above Save/Cancel. Text fields use the shared keyboard, World pickers keep a focused row in a bounded list, and additional World selections require Apply. Creating a World remains part of the unsaved Adventure draft until Save commits both.

The file picker keeps directory scrolling inside a bounded list, with Locations, Parent folder, Previous/Next batch, Refresh/Retry and Cancel on an attached action rail. B retraces entered folders or cancels at the starting point; B during loading cancels and ignores late results. Empty/error states keep a recovery or Cancel action focused. Choosing a file returns to the editor without saving it.

Start overlays service drafts without changing them. Closing the service returns to the system menu's remembered selection. Opening another service or using L1/R1 cancels unsubmitted edits. Submitted writes continue; a failure is shown even if the form has been left. Save errors retain the draft for correction/retry.

Start → Settings changes shell color and reduced motion. Changes apply after successful persistence, without altering layout or focus order. Settings is a service, not an L1/R1 page.

Settings → Your handheld contains sound, brightness, network/storage readings and power actions on one screen. Up/Down selects rows, Left/Right adjusts sound/brightness, A on sound toggles mute, and Y refreshes. Restart/Power off require confirmation and save the journal first. B returns to the remembered Settings row. See `DEVICE_CONTROLS.md`.

Start → Controller opens a fixed diagnostic panel with Refresh display, Save report, Reset checks and Back on its lower rail. Left/Right selects an action; A activates and B returns to the remembered menu entry. Start and L1/R1 retain their normal priority. Button/range observations survive leaving and reopening the panel within the same run, allowing those global controls to be tested. Keyboard input is not counted as a controller observation. See `DEVICE_DIAGNOSTICS.md`.

Worlds includes each Adventure under its primary and additional regions. Custom Worlds extend the region grid; the focused row scrolls into view inside that grid while the rest of the page stays fixed. See `LIBRARY_AND_LAUNCH.md` for detailed acceptance.

## Controller testing checklist

### Persistent application restart

Normal startup restores the last primary page, stable content selections, local routes and applied Pokédex filters after local storage opens. Keyboard/form drafts, unconfirmed picker choices, notices, the system menu and open Continue drawer do not reopen; the drawer's selected card is retained. Save displays a pending state and remains submitted if the user leaves with B or L1/R1. Before Save, Back discards the draft as usual.

Startup storage failure traps focus on Retry / Exit (B exits); feature pages are unavailable until loaded. During normal use, a browsing-state write failure offers Retry / Keep browsing, while L1/R1 may still change pages. Exit waits for submitted writes, and offers an explicit option to skip only optional browsing state if its final write fails. See `LOCAL_PERSISTENCE.md` for failure and recovery behavior.

### Per-screen checks

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
