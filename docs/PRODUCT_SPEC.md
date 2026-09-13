# TrainerOS Product Specification

The 2026-09-11 collection extension is specified in [Worlds collection catalogue](COLLECTION_CATALOGUE.md): region-first missing/owned editions, visible platform badges and controller file attachment. This intentionally extends the former installed-only library and exception to hiding all platform information; emulator commands and core details remain outside the normal UI.

## Purpose

TrainerOS is a software-only, controller-first **Linux handheld shell/session** for a Pokémon-focused Retroid Flip-class device running ArmadaOS.

It should feel like a dedicated personal trainer terminal rather than a generic Linux desktop or emulator frontend.

Normal flow:

**open/wake device → TrainerOS → browse progress or choose/continue an Adventure → configured emulator/application → return to TrainerOS**

KDE Plasma exists as explicit Desktop / Maintenance Mode and should not appear during ordinary use.

TrainerOS is the intended main/default mode. The accepted 2026-09-13 issue [#11](https://github.com/EriArk/TrainerOS/issues/11) replaces permanent Steam retention with reversible removal. The owner's subsequent clarification prefers Plasma Mobile as the only maintenance shell if viable on Flip. Existing Steam/Plasma Desktop remain until replacement and recovery are proven; shared KDE dependencies and personal data remain protected. See [session consolidation](ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13).

## Accepted next modules — planned, not implemented

The [roadmap](ROADMAP.md) integrates issues #1–41, all earlier unfinished work and deferred/future ideas. Detailed new-batch acceptance is in [EXPANSION_PLAN.md](EXPANSION_PLAN.md). These extensions have dependencies and separate device gates:

- A private English-first collection audit precedes verified relinking/quarantine. #30 intentionally narrows the former broad historical catalogue to substantial Pokémon titles with credible Flip/controller routes; the full eligible missing/owned checklist, meaningful variants and ROM hacks remain. Removing a reference card never deletes personal files/history.
- Shared Adventure media supplies recognizable Worlds wheel/marquee/cover artwork and Home's subtle return-screenshot background. Existing patterns, contrast, controller behavior and screenshot provenance remain intact.
- RetroAchievements account ownership moves to Settings → Accounts through one global service serving the active Trainer's identity/cache; achievement browsing stays in Hall of Fame. Emulator earning, earned-state display and deduplicated unlock notification have separate acceptance; return-time notification precedes any proven live overlay.
- External Pokédex art packs use species/form identities and remain useful offline. Caught becomes a dedicated personal collection, first from manual species-wide journal marks; forms, individual Pokémon and catch dates are not invented.
- Pokémon Center may offer paid party healing only through a verified save-writing capability, with an exact fee, balance, explicit confirmation and protection backup.
- Plasma Mobile replacement and reversible Steam removal are platform migrations, not prerequisites for ordinary Adventures or UI work. Sleep stays disabled pending separate validation.
- The owner's additional system-cleanup request covers unnecessary apps, autostarts and background services after a dependency audit. Preserve user data, required emulator/platform components and rollback; report measured disk-space and idle-load changes separately from ROM cleanup.

### Expanded device experience — accepted 2026-09-13

- First-run registration, multiple switchable Trainers and optional local PIN protection build on owner-scoped persistence. Shared library/installations/media remain device-wide; journal/history/Hall/Home/external accounts belong to a Trainer. Per-Trainer external saves require explicit adapter capability; a new Trainer cannot silently claim an existing shared save's progress.
- Worlds keeps Pokémon region navigation and gains a separate non-Pokémon **Multiverse** system browser. It reuses existing playable identities/adapters and stays inside the same primary page. Known system names and requested platform badges are intentional normal-UI information, while emulator commands remain hidden.
- Home gains X to switch Pokémon/Multiverse, separate remembered selections and scoped Continue. A remains the immediate mounted launch action; Y/card A selects without launching. Multiverse Home shows honest general play statistics and a distinct inner atmosphere, retaining the common chassis. Pokémon progress remains in its own context.
- Shared panel/tab geometry, headings, patterns and mounted controls come before the complete feature-content visual pass. Healthy status is quiet; actionable errors and provenance remain visible. Start provides quick volume/brightness, Power and offline Help; these are services, not extra primary pages.
- Local selected-game videos, selectable original/licensed music/UI sound packs, optional debounced charger haptics and capability-driven Armada RGB follow their media/lifecycle/device gates. No unavailable hardware or codec is represented by a pretend working setting.
- Optional Steam install/restore in Settings reuses #11's reviewed mechanism. A version-aware offline guide covers setup, personal games/BIOS/media, saves/accounts and recovery. Branded startup uses safe supported stock-Armada layers, ends at readiness and preserves maintenance recovery; no custom image is required.

## Vocabulary

- **World**: a Pokémon region such as Hoenn or Sinnoh.
- **Adventure**: one configured playable title/profile; Pokémon entries have World relationships. Planned Multiverse entries use a separate system context without invented Pokémon geography.
- **Choose Adventure**: recent resumable session/save-state cards.
- **Trainer**: persistent personal profile and long-term progress; planned multiple Trainers own separate personal records while sharing device library/installations.
- **Multiverse**: planned non-Pokémon library context within Worlds, browsed by system, with its own Home presentation.
- **Pokédex**: reference data plus personal Seen/Caught/history data.
- **Hall of Fame**: completed-Adventure archive and RetroAchievements achievements.
- **Pokémon Center**: optional user-facing name for backup and maintenance services.
- **Desktop / Maintenance Mode**: deliberate transition from TrainerOS into KDE Plasma for advanced system tasks and recovery.

Normal UI should avoid technical terms such as emulator binary, core, platform, process, file path, package name, and similar implementation details unless the user opens advanced/maintenance tooling.

## Primary sections

Primary sections are full-screen peers:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

`L1/R1` switch these pages horizontally. Home is one page; it is not a permanent shell behind the others.

## Home

Home is a living trainer overview, not a launcher grid and not a giant Continue screen.

Useful Home modules may include:

- current Adventure and active World
- badge and World progress
- Pokédex Seen/Caught counts
- latest milestone or Hall of Fame activity
- recently discovered/caught Pokémon when available
- selected favorite/featured Pokémon
- time, battery, network, storage, backup/sync status

Keep the layout calm; not every module must be visible at once.

### Choose Adventure drawer

Home contains a compact slide-out **Choose Adventure** panel.

Each mini card can include:

- screenshot preview
- Adventure title
- World
- location when known
- state/session timestamp
- concise progress such as playtime or badges

The 2026-09-11 clarification makes a card a selection for Home: it changes the displayed Adventure, World and available per-Adventure data and closes the drawer. Selecting a card never starts a process. Outside panels, A immediately presses Home's large physical-style button, with no preliminary focus movement. Y independently opens the selector; directional navigation is confined to its list. The action button resumes a selected exact state when supported, otherwise it starts the Adventure normally. It offers setup for an unconfigured Adventure and Worlds exploration when no Adventure is selected.

`Y` opens/closes Choose Adventure on Home. In the drawer, `A` chooses the Adventure for Home; on Home's main button it launches. `B` cancels the drawer without changing Home. `L1/R1` remain primary-page navigation. Home defaults to the latest actual launch until the Trainer explicitly chooses a different Adventure; that choice is persisted separately from launch history. Recorded process duration belongs to this Adventure, not aggregate Trainer progress. See [Home and play history](HOME_AND_HISTORY.md).

The closed affordance is a compact left-hand extension of the bottom frame, protruding into the main screen with a diagonal right edge. Opening it first expands its working width, then raises the recent cards inside the fixed viewport. Closing reverses that motion. It never makes Home taller than the screen.

## Worlds

Pokémon Worlds is the Adventure library organized by region first, not by hardware platform or emulator. The accepted Multiverse extension is a distinct local system-browser mode for non-Pokémon games; it does not turn those systems into Pokémon Worlds.

Normal application mode starts with reference regions and an empty personal library. Start → Manage Adventures provides controller add/edit, a local file picker, primary/additional World relationships and custom Worlds for ROM hacks. Saving stores TrainerOS metadata while the selected file remains external and unchanged. Records retain their identity when edited or relocated. See `LIBRARY_AND_LAUNCH.md` for the implemented flow and acceptance criteria.

Registration does not imply launch or progress support. Until a real adapter is configured and validated, detail offers a clear setup-needed state; Home/Continue does not invent play history or resume points. The ephemeral preview keeps sample Adventures separate from personal data.

Initial set:

- Kanto
- Johto
- Hoenn
- Sinnoh
- Unova
- Kalos
- Alola
- Galar
- Paldea

A World can show status, Adventure count, total playtime, badges, Pokédex completion, and last visit. Opening it reveals configured Adventures in that region.

An Adventure may contain user-facing title/progress data plus hidden integration metadata such as adapter ID, content reference, launch settings, save/state references, and parser/provider configuration.

The model should support one primary World plus additional World relationships for titles spanning multiple regions.

## Pokédex

The Pokédex combines reference information with the Trainer's own history.

Required browsing controls include combined World/regional-collection, Pokémon type, and personal-status filters (Seen, Caught, Not caught, Favorites), name/national-number search, and number/name sorting. Every control is reachable with the controller. Search offers a controller-operated on-screen keyboard and is optional for browsing. Empty results provide a controller-accessible way to clear filters.

Regional Pokédex membership and personal encounter Worlds are different concepts: reference providers supply regional membership; Trainer history supplies encounter locations. The mock may use a small explicitly partial local dataset.

Possible reference data:

- national number and localized name
- species/category
- types
- evolution relationships

Possible personal data:

- Seen/Caught
- favorite
- first encounter Adventure/date
- Worlds encountered
- party history
- shiny history
- notes/tags

The Pokédex should remain useful offline after data has been cached or imported. Network/data providers are adapters, not UI dependencies.

TrainerOS must not require proprietary artwork stored in the repository; the UI must work with original placeholders, user-supplied assets, appropriately licensed data, or no artwork.

## Trainer

Trainer is the persistent personal profile. It may show:

- trainer name and avatar
- favorite/featured Pokémon
- current Adventure/World
- total playtime
- Worlds visited/completed
- championships and badges
- Pokédex Seen/Caught totals
- milestones

It should feel like an in-universe trainer card/profile rather than account settings.

### Create and edit Trainer — required functionality

The product must support creating the local Trainer profile when none exists and editing it later from Trainer. This is a functional requirement, not a decorative mock profile.

- Create a name, choose an original/user-provided avatar or emblem, and optionally choose a favorite Pokémon.
- Allow name entry entirely with the controller through an on-screen keyboard.
- Validate a non-empty display name; preserve input on validation failure.
- Edit existing profile fields with explicit Save and Cancel behavior. Back cancels the draft without altering the saved profile.
- Persist the profile through the repository layer and restore it after restart. Renaming must preserve the profile ID and its progress/history relationships.
- Refresh Home and Trainer after a successful save; a failed write must preserve the previously saved profile and leave a retry/cancel route.

Acceptance: create using physical controls, restart and recover the same profile, edit and save, cancel a second edit without changing saved data, and retain all existing progress. Multiple independent Trainers are now an accepted next stage in #19–20, after the lossless ownership migration; see [Trainer acceptance](EXPANSION_PLAN.md#trainer-ownership-onboarding-and-power). The existing single-profile implementation remains the baseline; chooser/PIN isolation is not yet delivered.

## Hall of Fame

Hall of Fame contains the long-term archive of completed Adventures and the Trainer's RetroAchievements achievements. A completed-Adventure entry may contain:

- World and Adventure
- completion date
- playtime at completion
- final party of up to six Pokémon
- optional screenshot and notes
- source: automatically derived or manually entered/confirmed

Manual correction remains valid even if automatic extraction exists later.

RetroAchievements belongs within this existing primary section, with controller-browsable achievement lists/details associated with supported games/Adventures. Keep provider-supplied unlock state and dates separate from local completion records and the current playthrough's badges, party or Pokédex. Manual archive edits must not create external RA unlocks.

The archive remains useful without an RA connection. Disconnected, unsupported, loading, cached/offline and failed-refresh states must be represented honestly; an unavailable provider is not zero earned achievements. Integrate only supported content and verified provider capabilities, when the Hall of Fame module is reached in the roadmap.

Acceptance: browse archive and achievements without touch, open/close details with A/B, retain global L1/R1 navigation, keep local history available when the provider is absent or fails, and never infer current-save completion solely from an external account unlock.

## Pokémon Center / maintenance services

Pokémon Center is optional branding for service features and is not required to be a primary L1/R1 page.

Potential functions:

- backup/restore
- state/session management
- sync status
- storage and integration status
- relinking missing Adventure content
- integration diagnostics in an advanced layer

Destructive actions must be explicit and safe.

## System menu

`Start` opens a TrainerOS system menu from any primary section. It may expose:

- TrainerOS settings
- controller mapping
- visual/accessibility preferences
- Pokémon Center services
- Adventure management
- audio/brightness/network shortcuts
- sleep/restart/shutdown
- **Desktop / Maintenance Mode**
- restart TrainerOS / recovery actions

The system menu is part of TrainerOS, not a desktop taskbar or application launcher.

## Desktop / Maintenance Mode

KDE Plasma remains installed as a deliberate advanced environment.

Use it for:

- files
- terminal/development
- package management
- manual emulator configuration
- network troubleshooting
- recovery
- advanced system settings

Normal users should not need Plasma for ordinary Adventures, Continue, Trainer, Pokédex, Hall of Fame, or common handheld controls.

TrainerOS must provide a clear way back from maintenance mode.

## Input and accessibility

- normal use must not require touch, mouse, or keyboard
- every interactive element needs a visible focus state
- D-pad and left stick work consistently
- `A` confirms, `B` goes back/closes
- `L1/R1` remain global primary-page navigation
- `Start` opens system menu
- no state should be communicated by color alone
- avoid tiny text/targets
- controller mapping should become configurable

## Persistence

Persist at minimum:

- current primary section
- useful per-page UI state/focus
- configured Worlds and Adventures
- Trainer profile
- recent Continue entries/cache
- Hall of Fame history
- local Pokédex progress
- settings and mappings

External emulator save/state data remains external source data. TrainerOS references/manages it safely rather than assuming its own database is authoritative.

## Adventure lifecycle

Launching an Adventure should behave as one controlled transition:

1. persist current TrainerOS context
2. validate the configured adapter
3. start the external emulator/application
4. present the Adventure without desktop chrome
5. detect/handle exit where reliable
6. return to TrainerOS
7. restore prior page/focus immediately
8. refresh metadata/resume points asynchronously

The UI must not expose process management details.

## Platform and session requirements

- ArmadaOS is the system base.
- TrainerOS is a native Linux application/session, not an APK.
- KDE Plasma remains a secondary maintenance/recovery session.
- The first development build runs as a normal full-screen Qt application.
- Only after stable crash/recovery and process lifecycle testing should TrainerOS become the default graphical session.
- Session/compositor details remain behind a platform layer because ArmadaOS can evolve.

## Development model

The first milestone is a complete controller-navigable mock of the product structure.

The confirmed development sequence is bottom-up in dependency order:

**native project skeleton → shared interface/controller skeleton → shared backend and persistence → individual modules → real integrations → optional supported-game enrichment**

This replaces the former top-down plan. Establish the shared foundation before deep feature integration, then complete modules one at a time. Automatic save parsing and RA integration follow their roadmap prerequisites. Only verified capabilities may be presented as working product features; unknown data must remain unknown.

Visual design is explicitly not frozen by the mock. Whole compositions may be replaced as real data and physical-device testing reveal better solutions.

## Early non-goals

Do not block the first UI milestone on:

- making TrainerOS the default OS session immediately
- a custom ArmadaOS image
- universal save parsing
- universal exact state resume
- cloud accounts
- social/trading/battle features
- perfect Pokédex coverage
- removing KDE Plasma
- hardware modification

## Product success test

A Pokémon fan should be able to pick up the device and understand it as a dedicated personal trainer terminal without needing to know that ArmadaOS, Linux, Plasma, emulator binaries, or filesystem paths exist underneath the experience.
