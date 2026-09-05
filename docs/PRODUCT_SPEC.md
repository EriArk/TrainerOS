# TrainerOS Product Specification

## Purpose

TrainerOS is a software-only, controller-first **Linux handheld shell/session** for a Pokémon-focused Retroid Flip-class device running ArmadaOS.

It should feel like a dedicated personal trainer terminal rather than a generic Linux desktop or emulator frontend.

Normal flow:

**open/wake device → TrainerOS → browse progress or choose/continue an Adventure → configured emulator/application → return to TrainerOS**

KDE Plasma exists as explicit Desktop / Maintenance Mode and should not appear during ordinary use.

## Vocabulary

- **World**: a Pokémon region such as Hoenn or Sinnoh.
- **Adventure**: one configured playable title/profile associated with a World.
- **Continue Adventure**: recent resumable session/save-state cards.
- **Trainer**: persistent personal profile and long-term progress.
- **Pokédex**: reference data plus personal Seen/Caught/history data.
- **Hall of Fame**: archive of completed Adventures.
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

### Continue Adventure drawer

Home contains a compact slide-out **Continue Adventure** panel.

Each mini card can include:

- screenshot preview
- Adventure title
- World
- location when known
- state/session timestamp
- concise progress such as playtime or badges

Selecting a card should resume that exact state when the configured adapter supports it. Otherwise it may launch the Adventure normally.

The exact dedicated drawer shortcut is still open; do not reuse `L1/R1`.

## Worlds

Worlds is the Adventure library organized by region first, not by hardware platform or emulator.

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

## Hall of Fame

Hall of Fame is a long-term archive of completed Adventures. An entry may contain:

- World and Adventure
- completion date
- playtime at completion
- final party of up to six Pokémon
- optional screenshot and notes
- source: automatically derived or manually entered/confirmed

Manual correction remains valid even if automatic extraction exists later.

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

After that, implementation proceeds modularly and top-down in vertical slices:

**UI/interaction → domain/use case → repository/service → adapter/integration → persistence/device behavior → real-device validation**

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
