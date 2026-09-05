# AGENTS.md — TrainerOS

This file is the working contract for Codex and other coding agents in this repository.

## Read first

Before implementing or changing product behavior, read:

1. `README.md`
2. `docs/PRODUCT_SPEC.md`
3. `docs/UX_NAVIGATION.md`
4. `docs/ARCHITECTURE.md`
5. `docs/DATA_MODEL.md`
6. `docs/ROADMAP.md`

If a task conflicts with those documents, do not silently invent a new product direction. Preserve confirmed decisions and document any intentional change.

## Product identity

TrainerOS is a **software-only, controller-first Android frontend for a Pokémon-focused handheld**, initially targeting Retroid Flip / Retroid Flip-class devices.

It is not:

- a custom Android ROM
- a hardware mod
- a generic emulator launcher with a Pokémon theme
- a touch-first mobile app
- a ROM manager organized by console/platform

The intended illusion is a dedicated trainer terminal. Android and emulator details should normally stay hidden behind the experience.

## Confirmed product decisions — do not regress

These decisions are explicit and should be treated as requirements:

- The user-facing library section is called **Worlds**, never “Games”.
- Worlds are organized by Pokémon **region first**, not emulator/platform first.
- Top-level sections are full-screen pages.
- **Home is only one top-level page. It is not permanently visible behind other pages.**
- `L1` and `R1` switch top-level pages and must not be repurposed for local screen actions.
- Home is a special trainer dashboard / living overview, not a giant Continue screen.
- Continue Adventure is a **compact slide-out panel/drawer** on Home.
- Continue uses **small save-state cards**, ideally with live screenshots and metadata.
- Normal navigation must work entirely with physical controls.
- Settings are a service/system function and should not consume one of the main L1/R1 pages unless the product spec is intentionally changed.
- No physical modification of Retroid hardware belongs in scope.

## UX invariants

Always preserve:

- `L1/R1` = previous/next top-level section
- D-pad / left stick = focus navigation
- `A` = confirm/open
- `B` = back/close
- `Start` = system menu
- visible, deterministic focus at all times
- no required touch interaction for normal use
- no desktop mouse metaphors
- no exposed emulator jargon in primary UI

Secondary shortcuts are allowed only if they do not conflict with the above. Keep them remappable where practical.

## Engineering rules

### Architecture

- Prefer Kotlin + Jetpack Compose.
- Keep feature UI independent of emulator implementations.
- Put emulator-specific behavior behind adapter interfaces.
- Model Worlds, Adventures, Save States, Trainer progress, Pokédex progress, and Hall of Fame as domain objects rather than UI-only state.
- Persist user/product state locally.
- Use mock/fake adapters before coupling the first UI milestone to real emulator quirks.

### Controller-first implementation

- Test every screen with keyboard/gamepad key events, not only touch/mouse preview.
- Focus order must be intentional and stable.
- Never rely on hover.
- Scrolling must retain a focused item.
- Modal/drawer focus must be trapped correctly and restored on close.
- L1/R1 switching should preserve sensible per-page focus/state.

### Emulator integration

- Do not hard-code the entire app around one emulator package.
- An emulator adapter may support capabilities independently: launch, resume state, enumerate states, state screenshot, save backup, save parsing.
- UI must degrade gracefully when an adapter lacks a capability.
- Never delete or overwrite a user's save/state without an explicit safe operation and backup strategy.

### Content / IP hygiene

Do not commit:

- commercial ROMs
- BIOS/firmware dumps
- encryption keys
- ripped game assets
- proprietary emulator files
- copyrighted Pokémon artwork/audio copied from games or official media without an appropriate license

Use original placeholders and clearly separated user-provided asset/data paths.

## Implementation style

- Keep the visual system custom and cohesive; avoid exposing default Material components unchanged.
- Prefer a small number of strong components over many one-off widgets.
- Build responsive landscape layouts; do not hard-code a single pixel resolution unless a hardware-specific layer requires it.
- Keep animation short and functional. Top-level page transitions should communicate horizontal section movement, not delay input.
- Avoid visual clutter. Pokémon flavor should come from hierarchy, type/world accents, data, and motion — not wallpaper overload.

## Documentation discipline

When a task introduces a meaningful product or architecture decision:

- update the relevant doc in the same change
- add or update acceptance criteria
- keep README high-level; put implementation detail under `docs/`

## First implementation target

Unless a newer issue/task says otherwise, follow `docs/CODEX_START.md` and `docs/ROADMAP.md`.

The first milestone is a **real controller-navigable UI prototype with mock data**, not deep save-file reverse engineering.

## Definition of done for UI work

A UI task is not done until:

- it is usable without touch
- focus behavior is deterministic
- `L1/R1` top-level navigation still works
- back behavior is correct
- empty/loading/error states are considered
- no platform/emulator jargon leaks into normal user-facing copy
- the screen still reads clearly on a small landscape handheld
