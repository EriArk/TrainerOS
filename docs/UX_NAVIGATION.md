# TrainerOS UX & Navigation

**2026-09-24 simplification:** Pokédex has no manual journal, note editor or
Select shortcut. A still toggles a favorite; Y chooses Adventure. Center →
Activities → Party Playroom uses the verified Party: Left/Right selects a stable
member, A calls, X greets, Select plays catch, B returns. Play recruits the next
awake member or stays solo; resting members stay resting. Emotion bubbles use
the same exact-form portrait set. All legends stay in the bottom chassis.

**Library context actions — 2026-09-24:** hold physical A in a populated game
wheel for Rename / Move / Delete / Properties; short A retains its prior action.
Both wheels wrap vertically at either end. Settings → Library unlocks the World
wrench/Select editor. Delete now permanently removes the ROM after confirmation;
only older recoverable trash remains in Settings while nonempty. Legends stay in the bottom
footer, including context dialogs. B returns from a subdialog to its game menu
and then to the same wheel selection; L1/R1 close transient menus and change page.

**Physical A/B convention — owner clarification, 2026-09-19:** use Switch-style
positions in the shell: **A on the right confirms; B at the bottom goes back**.
On Flip's current InputPlumber Xbox target these are SDL B/east and A/south.
The controller layer and diagnostic labels share this mapping; feature actions
and keyboard Enter/Escape remain unchanged. This changes shell controls only,
not emulator gameplay mappings. L1/R1 and X/Y keep their existing behavior.

**Home button — owner clarification, 2026-09-19:** when no Adventure is running,
physical Home/Guide returns from any peer page to Home and closes transient
menus/drafts using normal page-switch semantics. It does not launch anything.
Active storage/service gates retain priority. Start still opens the system menu.
During a supported Adventure, Home requests the guarded exit question instead;
right A confirms, bottom B returns to the same game. See the device-specific
[integration boundary](ADVENTURE_EXIT.md#home-transport-increment-2026-09-19).

**Implementation versus target:** shared Y, the Pokedex/Center and Hall/RetroAchievements L2/R2 pairs, and opt-in confirmed RetroArch exit are delivered. Worlds/Multiverse and Home X now have a [P1 UI foundation](MULTIVERSE_UI.md); real library binding, full Center/Playroom and additional exit adapters remain planned. Existing reference/manual-journal, keyboard, menu and pointer checks remain evidence for their delivered routes. [Full new acceptance](EXPANSION_42_62.md).

Worlds includes the [collection checklist and controller attachment flow](COLLECTION_CATALOGUE.md). Grey missing cards remain focusable; A → Link a file opens the shared picker, while Save/Cancel returns to the original Worlds detail. Platform badges identify the edition without changing region-first navigation.

**Multiverse browser, 2026-09-23:** after the system grid, Up/Down moves a logo
wheel on the left; the same screen shows selected-game imagery and metadata on
the right. A chooses the linked game for Home without launching or opening a
separate detail step. B returns to systems. X searches and local Y filters.
Missing files remain readable but cannot replace Home's choice. L1/R1 and L2/R2
preserve their page/face roles and the wheel's useful position.

Pokemon region contents use that same wheel/details layout, retaining edition
platform badges, grey missing editions, Left/Right jump-eight and the established
A detail/actions/file-linking route. B returns to regions; Down after the last
edition wraps to the first; Up from the first wraps to the last. B owns return.
Region organization is unchanged.

In a World's Adventure list, X searches title/version/platform through the controller keyboard, Y cycles All/Linked/Missing, and left/right jumps eight rows. Up/down chooses adjacent rows. Search and filter are remembered per World; empty results retain the fixed X/Y controls and a focused Back action. These shortcuts yield to open keyboards, menus and service panels; L1/R1 remains global.

**Target reconciliation — 2026-09-19 (#62).** The accepted [#42–62 specification](EXPANSION_42_62.md) supersedes older product direction. Planned behavior below is not a claim that the deployed build has changed; see the [working baseline](ROADMAP.md#working-baseline) and dated module evidence.

## Accepted expansion routes — planned

The five full-screen peer pages remain **Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**, selected with **L1/R1**. Home is not a permanent background shell.

**Planned #43:** L2/R2 switches paired faces within Worlds ⇄ Multiverse, Pokédex ⇄ Pokémon Center, and Hall/Journey ⇄ RetroAchievements. Preserve each face's route/focus/filter and the launch return route. B unwinds local detail; it does not flip the pair. Use a compact existing-header/chassis indicator, not a sixth page or large second tab row. Home retains its separate **X** Pokémon/Multiverse toggle (#31).

Updated #9 shares one Trainer-scoped Adventure selection across supported Pokémon-aware pages, with independent Multiverse Home choice. The [Choose Adventure drawer](#choose-adventure-drawer) below defines A/B/Y, Worlds/modal exceptions and persistence; selection never launches.

First-run/chooser/PIN and Power/quick controls retain [earlier acceptance](EXPANSION_PLAN.md#trainer-ownership-onboarding-and-power). Before unlock, pages cannot expose personal data. After login L1/R1 cancels transient unsubmitted drafts/confirmations and switches primary pages; it never confirms Power or a save write. Submitted safety-critical work retains its service gate. Held A cannot cross a fresh destructive confirmation. L2/R2 never switches faces through a modal or intercepts gameplay input.

Start/Settings owns account, device, audio, lighting, Steam and Help services. Existing feature-local Y commands need explicit rebinding when #9 lands; keyboard Y symbols and Worlds Y filtering retain priority. No silent shortcut collision.

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
- `Y`: shared Choose Adventure on Home/Pokédex/Center/Trainer/Hall/RA; Worlds keeps its filter and modal editors keep local Y actions
- `Select`: labelled local journal/new-memory/achievement-refresh/save-backup shortcut

`L2/R2` switches the delivered Pokédex/Center pair. Hall/RetroAchievements is also delivered, with independent list/detail/action positions and restart restoration. Worlds/Multiverse L2/R2 and Home X switching now have a transient P1 presentation, with isolated sample selection and honest empty production lists. Real domain binding and persistence remain P2/P3/P4; see [Multiverse UI](MULTIVERSE_UI.md). [Shared routing and remaining boundaries](SHARED_ADVENTURE.md).

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

### Choose Adventure drawer

**P1 delivered:** the shared selector and Pokedex/Center and Hall/RA pairs now work on the single-Trainer baseline. [Implementation and remaining boundaries](SHARED_ADVENTURE.md). The full ownership/provider contract below remains the target.

**Planned #9/#49:** one shell-owned **Y · Choose Adventure** drawer selects the active Trainer's shared `CurrentPokemonAdventureContext`: Adventure ID, resolved exact build and ordinary-save identity/revision when available. Pokémon Home, Pokédex, Center, Hall, RA and Adventure-aware Trainer consume this same context. Multiverse Home remembers its own independent game selection. Before an explicit choice, use the latest actual launch in that Trainer/domain; unrelated launches do not overwrite an explicit choice.

A on a card commits the context and closes without launching; B cancels and restores the opener unchanged. L1/R1 closes the drawer without committing and switches primary page. Modal/keyboard/Start/recovery flows suppress shared Y. Worlds intentionally keeps its browser-local search/filter controls. Resolve existing local-Y conflicts explicitly when implementing the shared route. Unsupported save features never silently select another title.

Cards show the latest clean TrainerOS exit image, title, World and honest session/progress metadata. They are recent Adventure choices, not emulator-state slots. No extra persistent Current Adventure capsule/chip or independent per-feature selector: headers/content may show identity naturally.

Unobstructed Home A immediately invokes its large physical launch button regardless of prior directional input. Launch uses normal game startup and the game's ordinary save/autosave; Worlds also has an explicit launch action. Unconfigured/empty selections offer setup or Worlds exploration. The bottom-frame drawer remains compact, expands before rising inside the fixed viewport, and preserves controller focus. See [shared selection acceptance](EXPANSION_42_62.md#shared-adventure-and-paired-navigation).

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
- In the Adventure wheel, Up/Down moves selection cyclically. The shell, primary tabs and footer remain stationary; B returns to Worlds.
- `A` opens Adventure detail. Left/Right moves between enabled launch/Back actions; unavailable actions look disabled and are skipped. Target #49 uses normal launch/save loading only; the existing state-resume detail action is legacy and scheduled for removal.
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

The native keyboard now implements this shared interaction for Trainer names and Pokédex search. The current layout supports printable ASCII, case and symbol pages as described under Shared text entry; additional languages remain future work. Start places the system menu above the keyboard and Back restores the same key and buffer. Keyboard Y cycles symbol pages; shared Choose Adventure must not intercept it. L1/R1 cancels transient input and any containing profile draft, then changes the top-level section. The previously applied search query remains intact. Neither changing sections nor Apply silently saves a profile.

Implemented reference/manual-journal navigation (the current-save projection below is planned):

- Up/Down follows the entry list inside a fixed three-row area. Up from its first row enters the frame-mounted filter rail. Down from that rail restores the remembered row, or the recovery control when results are empty.
- Left/Right on the rail selects Search, World, Type, Records, Order and Reset. A opens the corresponding text/choice panel. The five-column choice grid uses spatial D-pad navigation; A applies, while B or Cancel preserves the old value. L1/R1 dismisses an unconfirmed picker and remains global.
- Filters intersect. Status choices include Seen, Caught, Not caught, Favorites, Not seen and Not recorded; unknown records are not treated as explicit negatives. Search matches a name substring or an exact national number. Sorting preserves the selected entry by ID when it still matches.
- Entry detail opens with A; Left/Right selects Favorite or Back. B returns to the same row. Removing a favorite while filtering Favorites returns to a remaining row, or focuses Reset filters if none remain. A failed write preserves the old mark and allows retry from the same detail action.
- Start/B restores picker, keyboard or entry focus. Section changes preserve applied filters, query, sort, detail route and useful focus. Empty reference data/load errors offer Retry; unmatched filters offer Reset. Failed reference refreshes keep the last successful snapshot available.

Acceptance: browse beyond the visible rows, combine three filters with a query, select the last type choice with the controller, cancel search without changing results, preserve detail through page switching, and remove the last filtered favorite without losing focus. Test name entry and numeric entry through the shared keyboard without updating the Trainer profile.

Do not rely on color alone for types/progress.

## Planned Pokédex and Center projection routes

Pokédex remains useful offline and controller-operated: bounded lists/detail, combined regional-collection/type/status filters, name/number search, sorting and clear empty/reset states. Search uses the shared keyboard with digits in a separate right block. Reference regional membership is not proof of a personal encounter.

**Planned #46** separates five layers:

1. Offline species/form/reference facts.
2. The selected Adventure's verified ordinary-save Seen/Caught as primary current progression, with only proven regional/National/form semantics.
3. Preserved Trainer-owned manual journal, Caught collection (#14), favorites and history, explicitly sourced and secondary to the current-save view.
4. Optional installed classic illustration artwork (#13) for the primary long list.
5. Optional PMDCollab animated sprites/portraits (#51) for detail and living-party scenes, not the main list artwork.

Unknown is distinct from false/not-caught. Aggregate counts, species flags and individual Pokémon are separate evidence levels; none fabricates catch dates/forms. Failed reads retain a labeled complete last-good snapshot for the same source. Save rollback does not erase manual/history records. Shared Y refreshes the same Adventure across features; L2/R2 preserves each Dex/Center route.

Art providers are separate from reference facts and game media. No official artwork is bundled; missing/partial packs use honest fallback. The runtime artwork prerequisites are **#58 raw corpus -> #60 real Flip profiles -> #57 required contract/validation**. Per the owner's 2026-09-20 clarification, **#59 Pack Studio comes last**, after UI and asset structure stabilize; Settings does not wait for Studio. No resident downloader. [Detailed acceptance](EXPANSION_42_62.md#artwork-sequence).

**Planned #44/#53:** Pokémon Center is a first-class L2/R2 companion to Pokédex, using the shared Adventure context. It is not a new primary page or merely system-maintenance branding.

- Practical Party cards/list and details expose verified slots, level, HP/status, moves/PP and held items.
- Storage exposes the actual title's boxes/slots. Read browsing precedes separately proven reorder, Party/Storage moves and release; destructive actions require fresh explicit confirmation and the shared safe transaction.
- Heal/Backup/Restore services retain ordinary-save protection. Paid healing requires proven party/money fields, exact fee/balance and in-game verification; backup-only fallback remains useful.
- Link Counter trade/transfer/sale is later exact-pair, recoverable two-device work; sale means in-game currency only.
- Party remains a stable management UI. A separate Party Playroom provides optional interactions; Practice Battle copies real Party data into a read-only sandbox and never mutates saves or grants rewards.

Device/account/integration settings remain in Start/Settings. No second Center save picker. [Center acceptance](EXPANSION_42_62.md#pokémon-center-and-practical-party), [Link Counter](EXPANSION_42_62.md#link-counter), [Playroom/battle](EXPANSION_42_62.md#sprites-living-party-playroom-and-practice-battle).

## Trainer

Trainer is primarily informational. Keep focus targets limited to meaningful actions such as editing the profile, opening a metric/history, or changing the featured Pokémon.

Avoid making every static statistic focusable.

Create Trainer is available when no local profile exists. Edit Trainer opens a draft of the saved name, avatar/emblem, and favorite Pokémon. All fields, Save, and Cancel are controller-accessible; the name field can open the same on-screen keyboard used by search. Back closes the keyboard before cancelling the profile draft. Saving updates the profile without changing its identity or progress, and cancelling preserves the existing profile.

Current prototype: Up/Down moves among Name, Emblem, Favorite and Save; Left/Right moves between Save and Cancel. A on Emblem/Favorite cycles the visible sample choice. A on Name opens the shared keyboard. Apply changes the form draft; Save writes through the profile repository (SQLite normally; fake in ephemeral tests). A blank/invalid name returns focus to Name; a failed write leaves Save focused with the draft available for retry. Normal profiles persist across restart; memory-only storage is an isolated historical mock behavior.

Acceptance: create a Trainer from the empty state, cancel a keyboard edit, cancel the whole form, save a later edit without changing ID/creation time, recover from a failed save, and switch sections from an open keyboard without committing drafts. Editing identity does not change sample Adventure progress.

## Hall of Fame

**P1 entry delivered, 2026-09-20:** new Hall navigation opens Journey, reusing existing Home badge/Caught observations. A opens manual memories; X opens Champion availability/sample records; Select creates a manual memory. B from the archive list returns to Journey. Existing persisted archive/RA routes are preserved. Large badge presentation does not establish new save support or completion. [Controller and data boundary](JOURNEY_UI.md).

**Planned #47/#48/#64:** Hall's first face is a live save-backed **Journey Record**, useful before completion, plus preserved Champion/completed-run history. Show only proven title-specific badges, milestones, playtime and Dex totals, with larger game-accurate badge assets or deliberate neutral mounts; no universal eight-badge/percentage/date assumptions. See [exact badge identity and artwork acceptance](EXPANSION_63_64.md#accurate-league-badges-and-neutral-fallback).

Champion snapshots preserve exact build/playthrough/source revision and verified historical team/progress. Current Party is not the historical winning team; older saves do not erase the archive. Observation time is not victory time. Manual memories/editing remain valid and explicitly sourced.

RetroAchievements is Hall's **L2/R2 companion**, following the shared Adventure through verified content/set matching. It remains an external account source independent from current-save or manual completion truth. Settings owns the active Trainer's account; unsupported mapping never substitutes another game. Same-account complete offline caches retain earned/unknown distinctions. Earning, earned-state UI and verified notification have separate gates (#12/#24/#25/U7).

Acceptance includes controller list/detail/Back, paired-face restoration, Y changes, unsupported/corrupt/rollback states, preserved manual/Champion history, two-Trainer isolation and offline/wrong-account rejection. [Projection acceptance](EXPANSION_42_62.md#pokédex-journey-and-achievements).

## System menu

`Start` opens the TrainerOS system menu from any page.

**Delivered 2026-09-20:** Start X jumps to Volume and returns to the remembered
service entry. Up/Down selects; Left/Right adjusts volume or brightness; A on
Volume toggles mute. Values refresh from the device while controls are visible.
Power opens on Cancel; Power off/Restart require a separate confirmation, B
unwinds one layer, and L1/R1 cancels then switches pages. Switch Player remains
explicitly unavailable until P2. [Device controls](DEVICE_CONTROLS.md).


The menu overlays the current page rather than becoming another L1/R1 section.

Initial categories may include:

- Settings
- Controller
- Pokémon Center / backups
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

**Accepted target #49, not yet implemented:** normal TrainerOS creates, manages and resumes no emulator savestates/ResumePoints, in either Pokémon or Multiverse. Ordinary game saves/autosaves are authoritative. Relaunch starts the game normally; the game loads its own save.

On user-requested exit, capture a clean gameplay screenshot **before** the overlay. Resolve exact title/integration policy `manualConfirm | autosave | unknown`; do not infer it from platform. Manual/unknown asks “Have you saved?” while the game remains alive. B returns to the same process; A confirms graceful exit. Per the owner's 2026-09-19 clarification, verified autosave also asks "Close this game?"; it never exits without A. The target trigger is the physical Home/Guide button instead of Start+Select, subject to the Flip routing gate. Start retains the shell system menu. Confirmation is a user assertion, not automatic proof of saving.

Exit images feed Home/Y/history with Trainer/domain/Adventure/session provenance. Cancelled attempts and crash/kill/battery loss cannot fabricate a confirmed exit or replace valid history with a false capture. Preserve prior valid media where appropriate and mark interrupted outcomes honestly. Capture failure leaves a usable exit/cancel path, never a state-thumbnail substitute.

Checkpoint the launching page/paired face/route/focus, restore it promptly on return, and refresh ordinary-save observations asynchronously. First prove capture, overlay/input ownership and cancellation with the still-running emulator on Flip. Migration retires only verified TrainerOS-owned obsolete state artifacts safely, preserving ordinary saves, histories and independent images. [Full lifecycle/migration acceptance](EXPANSION_42_62.md#ordinary-saves-and-screenshot-first-exit).

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

Folders populate the library automatically. Long A in either Worlds game wheel opens Rename / Move / Удалить / Properties. Move offers the destination platform and its folders (including a new folder), then confirms with Cancel selected first. This corrects a ROM placed under the wrong platform. Pokémon also retains World reassignment. The former global Start → Manage Adventures entry is retired. Missing catalogue editions still open Link a file; its editor uses the existing shared keyboard and bounded file picker.

The file picker keeps directory scrolling inside a bounded list, with Locations, Parent folder, Previous/Next batch, Refresh/Retry and Cancel on an attached action rail. B retraces entered folders or cancels at the starting point; B during loading cancels and ignores late results. Empty/error states keep a recovery or Cancel action focused. Choosing a file returns to the editor without saving it.

Start overlays service drafts without changing them. Closing the service returns to the system menu's remembered selection. Opening another service or using L1/R1 cancels unsubmitted edits. Submitted writes continue; a failure is shown even if the form has been left. Save errors retain the draft for correction/retry.

Start → Settings changes shell color and reduced motion. Changes apply after successful persistence, without altering layout or focus order. Settings is a service, not an L1/R1 page.

Settings → Trainer & accounts opens the existing profile editor or the shared
RetroAchievements account panel; B returns from the account to this service.
Multiple Trainers/PIN remain unavailable in personal-library mode. The sample
`--ephemeral` mode offers a labelled registration/chooser/keypad rehearsal with
no persistence or unlocking. [Flow and P2 boundary](TRAINER_SETUP_UI.md).

Settings → Your handheld contains sound, brightness, network/storage readings and power actions on one screen. Up/Down selects rows, Left/Right adjusts sound/brightness, A on sound toggles mute, and Y refreshes. Restart/Power off require confirmation and save the journal first. B returns to the remembered Settings row. See `DEVICE_CONTROLS.md`.

Start → Controller opens a fixed diagnostic panel with Refresh display, Save report, Reset checks and Back on its lower rail. Left/Right selects an action; A activates and B returns to the remembered menu entry. Start and L1/R1 retain their normal priority. Button/range observations survive leaving and reopening the panel within the same run, allowing those global controls to be tested. Keyboard input is not counted as a controller observation. See `DEVICE_DIAGNOSTICS.md`.

Worlds includes each Adventure under its primary and additional regions. Custom Worlds extend the region grid; the focused row scrolls into view inside that grid while the rest of the page stays fixed. See `LIBRARY_AND_LAUNCH.md` for detailed acceptance.

## Controller testing checklist

### Persistent application restart

Normal startup restores the last primary page, stable content selections, local routes and applied Pokédex filters after local storage opens. Keyboard/form drafts, unconfirmed picker choices, notices, the system menu and open Continue drawer do not reopen; the committed Adventure choice is retained (legacy stored state selection is migrated by #49). Save displays a pending state and remains submitted if the user leaves with B or L1/R1. Before Save, Back discards the draft as usual.

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

## Pointer visibility and selector naming — 2026-09-14

The Home selector is labeled **Choose Adventure**. Y opens/closes it, A on a card chooses for Home, and unobstructed Home A launches through the large button. Internal Continue identifiers retain compatibility with existing navigation data. L1/R1 keep their established global section actions.

TrainerOS starts with a hidden cursor. Touch input hides it without consuming the touch or its synthesized mouse click. The dedicated handheld session keeps the shell window's cursor blank, including Gamescope's touch-to-mouse events and Qt Quick's later cursor resets. This policy belongs to TrainerOS's window and does not alter Adventure or maintenance-desktop cursors. In a normal desktop window, genuine mouse input restores the cursor; controller actions hide it again.

Acceptance: Qt Quick touch and mouse events still activate controls; the handheld cursor stays hidden through press/release, while desktop mouse use restores it. Another window retains its own cursor. Verify the live Gamescope cursor as well as rendered controller navigation. Automated input does not substitute for physical finger calibration.

## Center Party/Storage presentation - 2026-09-20

Center now opens Party initially; X switches Party/Storage, A opens a read-only slot detail, A/B returns to its grid. Select opens ordinary-save backups; inside that shelf existing Select backup and X refresh remain, while idle B returns to the prior grid. L1/R1 and L2/R2 keep global/paired ownership. Shared Y changes the same Pokemon context and clears obsolete detail on a different choice. Production shows reader-unavailable states, not sample individuals. [Exact P1 boundary and remaining integration](CENTER_PARTY_UI.md).

## Center activities - 2026-09-20

Down from the final Party/Storage slot row selects Activities on the attached lower panel. Its separate Playroom, Practice and Link Counter routes preserve management focus and existing shared Y/global shoulders. Real library mode shows unavailable capability states; explicit development mode rehearses actor reactions, practice layout and interrupted-link UI without game or transport operations. [Exact controls](CENTER_ACTIVITIES_UI.md).

## Media and feedback settings - 2026-09-20

Settings has a separate Media & feedback list. A opens availability details or toggles the shared Reduced Motion preference; B restores the prior row/Settings entry. Global shoulders and Start retain priority. No unsupported audio/haptic/lighting control is presented as active. [Boundary](MEDIA_SETTINGS_UI.md).

## Settings layout correction - 2026-09-20

The owner supersedes the prior media detail pages and Start second column: Start uses a top quick-control block with volume/brightness sliders; theme stays in Appearance, above one services list. Settings keeps categories left and controls right; A/Right enters, B returns to categories, Left/Right adjusts values. [Current contract](MEDIA_SETTINGS_UI.md).

Trainer and System now expose their setting rows directly in the right pane.
Trainer editing stays in that pane with vertical Save/Cancel. Controller lives
in Settings, not Start; its test view returns to the selected Controller row.
The shell maps north X to secondary actions and west Y to Choose Adventure,
completing the existing east-A/south-B Switch layout. Emulator input is unchanged.

## Pokédex sprite detail - 2026-09-23

Illustrations remain in the list and selected-entry area. The optional header
actor has no focus stop or separate inspector. The owner's subsequent combined
browser replaces the separate detail route: A toggles favorite, B moves between
entries and the rail; Form and Artwork are rail controls. Start restoration,
global L1/R1 and modal priority remain unchanged. Author-entry tooling and
consolidated Credits belong to the final pack/Pack Studio stage; underlying
attribution records stay intact.
[Source, movement and pause contract](POKEDEX_SPRITES.md).

## Shared bottom legends and combined browsing — 2026-09-23

Controller legends belong only on the bottom edge throughout TrainerOS, including
Settings, Start, journals, keyboards and selectors. The shell footer changes with
modal priority; independent startup/access, storage recovery and game-exit surfaces
own their own bottom legends. Actual action buttons retain meaningful labels;
the physical Home A button and chassis-mounted Choose Adventure Y control remain.
Do not repeat button instructions inside cards, page headings or descriptions.

Pokédex uses a narrower species list beside illustration, form facts and six
colored numeric stat cells. Selection, filtering, forms, journal and favorites
remain controller-accessible on the combined view; unavailable facts remain dashes.
Worlds and Multiverse use a gold faceted arrow at the wheel's left edge rather
than a selection plate. It slides out on selection, honoring Reduced Motion.
Down clamps at the final game; B returns to regions/systems.

Acceptance: SDL scenarios cover modal legends, footer bounds outside the battery,
list/form/journal navigation, old detail-route restoration, favorite persistence,
empty filters and section/Start restoration. Inspect installed Flip screenshots
for combined Pokédex, both wheels and contextual bottom legends.

## Party/Storage split view — 2026-09-24

Center keeps six Party slots or a 30-slot Storage box beside the selected
Pokémon summary. X changes the face; Up from the first box row reaches the box
selector, Left/Right changes boxes, Down/B returns to remembered slots.
A opens a compact actions panel; Select retains ordinary backups. L1/R1 and
L2/R2 retain their global roles. See [presentation and provider boundary](PARTY_STORAGE_UI.md).
