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

TrainerOS is intended to be the **primary device interface**, not a secondary launcher app. The desired normal lifecycle is:

**Power on / wake → TrainerOS → Adventure → TrainerOS**

Android's stock launcher and system UI are maintenance/escape paths, not part of ordinary use.

It is not:

- a custom Android ROM
- a hardware mod
- a generic emulator launcher with a Pokémon theme
- a touch-first mobile app
- a ROM manager organized by console/platform

The intended illusion is a dedicated trainer terminal. Android and emulator details should normally stay hidden behind the experience.

## Confirmed product decisions — do not regress

These decisions are explicit and should be treated as requirements:

- **TrainerOS is the primary handheld interface.** Production builds should support being selected as the Android Home/Launcher where the target firmware permits it.
- Exiting a supported Adventure/emulator should return the user to TrainerOS rather than a stock Android launcher.
- Access to stock Android/system settings must remain possible, but through a deliberate maintenance/system action.
- If reliable Android Home replacement is restricted by firmware, preserve the same dedicated-device flow through a robust full-screen fallback rather than redesigning TrainerOS as a secondary app.
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
- Keep Android Home/Launcher integration, immersive/full-screen behavior, and return-to-TrainerOS lifecycle concerns behind dedicated platform/service boundaries rather than mixing them into feature UI.
- Model Worlds, Adventures, Save States, Trainer progress, Pokédex progress, and Hall of Fame as domain objects rather than UI-only state.
- Persist user/product state locally.
- Use mock/fake adapters before coupling the first UI milestone to real emulator quirks.

### Post-mock development strategy

The mock is a **prototype and UX probe**, not a frozen design or throwaway toy.

After the first controller-navigable mock works:

- Develop **modularly and top-down**.
- Take one user-visible feature at a time from UI/interaction → domain/use-case layer → repository/service boundary → adapter/integration → persistence/device behavior.
- Prefer complete vertical slices over building every UI first, then every backend, then every integration.
- Keep module boundaries explicit so individual integrations, data providers, and visual components can be replaced independently.
- Do not couple a screen directly to emulator packages, filesystem quirks, or save formats.
- Revisit and refactor abstractions when real device/integration behavior proves the mock assumptions wrong.
- **Visual design is intentionally not frozen after the mock.** Layout, hierarchy, component shapes, motion, density, and even whole screen compositions may be redesigned repeatedly while the product matures.
- Preserve product invariants and controller behavior while allowing aggressive visual iteration.
- Do not protect mock code or mock visuals merely because they already exist. Replace them when a better implementation is clear.

A healthy post-mock slice should leave behind a reusable module boundary rather than another special case.

### Controller-first implementation

- Test every screen with keyboard/gamepad key events, not only touch/mouse preview.
- Focus order must be intentional and stable.
- Never rely on hover.
- Scrolling must retain a focused item.
- Modal/drawer focus must be trapped correctly and restored on close.
- L1/R1 switching should preserve sensible per-page focus/state.

### Device-shell behavior

Treat dedicated-device behavior as a product requirement, not optional polish:

- production mode should support Android Home/Launcher role where possible
- start/wake/relaunch paths should converge on TrainerOS
- returning from Adventures should restore TrainerOS state and focus cleanly
- system bars/navigation chrome should be hidden during normal use where platform APIs allow
- app/process recovery must avoid stranding the user in an unrelated launcher when practical
- provide an explicit, discoverable maintenance path to Android settings / stock environment
- never require rooting, a custom ROM, or physical modification solely to make TrainerOS usable

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
- Treat visual components as replaceable until they have survived real-device testing; implementation cleanliness must not become an excuse to freeze weak UX.

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
