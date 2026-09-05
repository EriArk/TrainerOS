# TrainerOS Product Specification

## Purpose

TrainerOS is a software-only, controller-first Android frontend for a Pokémon-focused Retroid Flip / Flip-class handheld. It should feel like a dedicated trainer terminal rather than a generic Android launcher.

Normal flow: **open device → TrainerOS → browse progress or choose/continue an Adventure → configured game application → return to TrainerOS**.

## Vocabulary

- **World**: a Pokémon region such as Hoenn or Sinnoh.
- **Adventure**: one configured playable title/profile associated with a World.
- **Continue Adventure**: recent resumable session/save-state cards.
- **Trainer**: persistent personal profile and long-term progress.
- **Pokédex**: reference data plus personal seen/caught/history data.
- **Hall of Fame**: archive of completed Adventures.
- **Pokémon Center**: optional user-facing name for backup and maintenance services.

Normal UI should avoid technical terms such as emulator package, platform ID, file path, and similar implementation details unless the user opens advanced settings.

## Primary sections

Primary sections are full-screen peers:

**Home ⇄ Worlds ⇄ Pokédex ⇄ Trainer ⇄ Hall of Fame**

`L1/R1` switch these pages horizontally. Home is one page; it is not a permanent shell behind the others.

## Home

Home is a living trainer overview, not a launcher grid and not a giant Continue screen.

Useful Home modules may include:

- current Adventure and active World
- badge and World progress
- Pokédex seen/caught counts
- latest milestone or Hall of Fame activity
- recently discovered/caught Pokémon when data is available
- selected favorite/featured Pokémon
- time, battery, network and backup status

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

Selecting a card should resume that state when the configured integration supports it. Otherwise it may launch the Adventure normally. The exact dedicated drawer shortcut is still open; do not reuse `L1/R1`.

## Worlds

Worlds is the adventure library organized by region first, not by hardware platform.

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

A World can show status, Adventure count, total playtime, badges, Pokédex completion, and last visit. Opening it reveals the configured Adventures in that region.

An Adventure may contain user-facing title/progress data and hidden integration data such as a content reference, adapter ID, launch settings, and save/state references.

The model should support a primary World plus additional World relationships for titles that span more than one region.

## Pokédex

The Pokédex combines reference information with the trainer's own history.

Possible reference data:

- national number and localized name
- species/category
- types
- evolution relationships

Possible personal data:

- seen/caught
- favorite
- first encounter Adventure/date
- Worlds encountered
- party history
- shiny history
- notes/tags

The main Pokédex should remain useful offline after data has been cached or imported. Network providers are adapters, not hard UI dependencies.

TrainerOS should not require proprietary artwork to be stored in this repository; the UI must work with original placeholders, user-supplied assets, appropriately licensed data, or no artwork.

## Trainer

Trainer is the persistent personal profile. It may show:

- trainer name and avatar
- favorite Pokémon
- current Adventure/World
- total playtime
- Worlds visited/completed
- championships and badges
- Pokédex seen/caught totals
- milestones

It should feel like an in-universe trainer card/profile rather than account settings.

## Hall of Fame

Hall of Fame is a long-term archive of completed Adventures. An entry may contain:

- World and Adventure
- completion date
- playtime at completion
- final party of up to six Pokémon
- optional screenshot and notes
- source: automatically derived or manually entered

Manual confirmation/editing remains valid even if automatic extraction exists later.

## Pokémon Center / maintenance

Pokémon Center is optional branding for service features and is not required to be a primary L1/R1 page.

Potential functions:

- backup/restore
- state/session management
- sync status
- storage and integration status
- relinking missing Adventure content

Destructive actions must be explicit and safe.

## System menu

`Start` opens a system menu from any primary section. It may expose:

- TrainerOS settings
- controller mapping
- visual/accessibility preferences
- Pokémon Center services
- Adventure management
- Android settings
- explicit exit/restart actions

## Input and accessibility

- normal use must not require touch
- every interactive element needs a visible focus state
- D-pad and left stick must work consistently
- `A` confirms, `B` goes back/closes
- no state should be communicated by color alone
- avoid tiny text/targets
- controller mapping should become configurable

## Persistence

Persist at minimum:

- current primary section
- useful per-page UI state/focus
- configured Worlds and Adventures
- Trainer profile
- recent Continue entries
- Hall of Fame history
- local Pokédex progress
- settings and mappings

External application save data remains external source data; TrainerOS should reference and manage it safely rather than assuming its own database is authoritative.

## Early non-goals

Do not block the first UI milestone on universal save parsing, universal state resume, cloud accounts, social features, online trading/battle features, perfect Pokédex coverage, custom Android firmware, or hardware modification.

## Product success test

A Pokémon fan should be able to pick up the device and understand it as a dedicated personal trainer terminal without needing to know which Android applications are underneath the interface.
