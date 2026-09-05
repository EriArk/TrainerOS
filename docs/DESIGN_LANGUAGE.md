# TrainerOS Design Language

## Design thesis

TrainerOS should look like a **real trainer terminal from the Pokémon world**, but not imitate one specific game's Pokédex pixel-for-pixel.

The UI needs enough original identity to stay coherent across Kanto through Paldea and across very different game generations.

Think: modern dedicated handheld system with region-aware atmosphere, not Linux/KDE/Steam with a Pokémon theme.

## Overall composition

- landscape-first
- strong central content area
- minimal permanent chrome
- no desktop window frames, taskbars, panels, or title bars in normal mode
- no phone-style bottom navigation bar
- no giant grid of identical launcher tiles
- page-to-page movement is primarily horizontal and driven by `L1/R1`
- information density should be moderate: richer than a console launcher, calmer than a PC dashboard

## Primary page identity

The five primary pages should have distinct personalities while sharing components and spacing.

### Home — living overview

Home is intentionally asymmetric and editorial rather than a menu grid.

A good first composition:

```text
┌──────────────────────────────────────────────────────────────────┐
│ HOME                                              22:41  78%     │
│                                                                  │
│  Active World                                                    │
│  HOENN                          Trainer activity / featured area  │
│  Pokémon Emerald                latest milestone                 │
│  5 / 8 badges                   recently caught (later)          │
│  Dex 84 / 202                                                    │
│                                                                  │
│  small utility/status strip                                      │
│                                                                  │
│  [ Continue Adventure affordance ]                               │
└──────────────────────────────────────────────────────────────────┘
```

The exact arrangement can change. Important constraints:

- Continue is visible/discoverable but not dominant
- the page has breathing room
- selected World/featured Pokémon can influence atmosphere
- system status remains compact

### Continue drawer

The drawer should feel like opening a quick layer of recent moments.

Example card rhythm:

```text
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ screenshot   │ │ screenshot   │ │ screenshot   │
│              │ │              │ │              │
├──────────────┤ ├──────────────┤ ├──────────────┤
│ Emerald      │ │ Platinum     │ │ SoulSilver   │
│ Hoenn        │ │ Sinnoh       │ │ Johto        │
│ Route 119    │ │ Jubilife     │ │ Ecruteak     │
│ 2h ago       │ │ yesterday    │ │ 3d ago       │
└──────────────┘ └──────────────┘ └──────────────┘
```

Prefer 3–5 visible/reachable recent cards depending on available width. Do not turn them into full-size game covers.

### Worlds — places, not files

Worlds should feel like choosing destinations.

Region cards can use:

- abstract terrain/topography shapes
- region accent palettes
- progress/status
- short atmospheric labels
- subtle map-like motifs created for TrainerOS

Do not require official region-map artwork.

Opening a World should feel like entering that destination, with Adventures listed inside.

### Pokédex — dense but elegant instrument

Pokédex can be the most data-dense page while retaining strong focus and whitespace.

Suggested structure:

- focused list/grid of entries
- compact filters
- detail pane/page
- types as small semantic accents
- Seen/Caught state via iconography + text/shape, not color only

Avoid a tiny phone-style list with dozens of narrow rows.

### Trainer — personal identity

Trainer should feel like a premium trainer card expanded into a full page.

Prioritize:

- trainer identity
- favorite/featured Pokémon
- a few meaningful aggregate stats
- milestones
- current Adventure/World

Do not make it an account-profile form.

### Hall of Fame — archive

Hall of Fame can be more ceremonial than other pages.

Use:

- timeline/archive composition
- date and World hierarchy
- team-of-six presentation
- subtle trophy/champion motifs designed for TrainerOS

Avoid fake gold everywhere; restraint makes completed entries feel more special.

## Page indicator

The user needs a subtle cue that `L1/R1` moves between pages, but not a giant permanent navbar.

Possible pattern:

```text
L1   HOME · WORLDS · DEX · TRAINER · HALL OF FAME   R1
```

Only the current page needs strong emphasis. The indicator may fade/reduce prominence after input if desired.

This is navigation feedback, not an app dock.

## Focus language

Controller focus is one of the most important visual elements.

Recommended direction:

- thin luminous/bracket outline
- slight scale or lift
- small directional marker/cursor
- quick 80–140 ms response

Avoid default Qt desktop focus rectangles, KDE selection visuals, or generic TV-interface glows.

The same focus treatment should work on cards, rows, buttons, and menu items.

## Color

Use a stable neutral base and apply Pokémon flavor through accents.

Layers:

1. neutral global background/surfaces
2. active World accent
3. Pokémon type accents where semantically relevant
4. semantic status colors

Do not flood the full UI with a different saturated color for every World.

## World atmosphere

Worlds may influence:

- accent color
- background gradient
- subtle texture/pattern
- motion motif
- small ambient illustration/geometry

They should **not** change:

- navigation rules
- component locations unpredictably
- typography system
- focus behavior

This creates regional personality without making nine unrelated themes.

## Typography

- prioritize readability at handheld distance
- use a distinctive display face only for large labels if licensing permits
- UI/body text should be extremely legible
- avoid excessive all-caps paragraphs
- numeric progress should be easy to scan

Do not depend on proprietary Pokémon fonts.

## Motion

Motion explains structure:

- `L1/R1`: horizontal page travel
- opening World/Pokédex/Hall entry: depth/panel transition
- Continue: slide-out drawer
- system menu: overlay
- Adventure launch: quick clean handoff without desktop minimize/window animations

Animations should be short, interruptible, and pleasant during rapid navigation.

## System status

Time, battery, network, storage/backup state belong in the interface but remain quiet.

Do not expose the normal Plasma panel/system tray as the shell status treatment. TrainerOS owns its own compact status language.

Complex system configuration can intentionally hand off to Desktop / Maintenance Mode instead of reproducing an entire desktop control center inside TrainerOS.

## Desktop / Maintenance Mode visual boundary

TrainerOS and Plasma should feel intentionally separate.

TrainerOS does not need to visually imitate KDE or hide the fact that Desktop Mode is a different environment. The transition should simply be deliberate and clearly labeled.

Normal TrainerOS screenshots/mockups should never include Plasma panels, desktop wallpaper, window decorations, or taskbars.

## Rotom-style notifications — later

A small Rotom-inspired personality layer may eventually make system events charming, for example:

- backup completed
- new Hall of Fame entry
- new Pokédex milestone
- integration needs attention

This should remain **small, optional, and non-chatty**. Do not turn TrainerOS into a chatbot or let a mascot obscure navigation.

## Placeholder asset policy

Until final assets exist:

- use original gradients, geometric motifs, generated silhouettes, and text
- use neutral Pokémon slots labeled by number/name where needed
- keep asset loading replaceable through an asset/provider layer

Do not commit copied official UI screens, official character art, or game audio as placeholders.

## Visual iteration policy

The first complete mock is a **conversation with the real device**, not a style contract.

At any vertical-slice milestone, it is acceptable to replace:

- whole page composition
- component shapes
- spacing/density
- typography hierarchy
- motion
- focus treatment
- World atmosphere treatment

Do not keep weak visuals merely to preserve already-written QML. Domain/adapters should make visual replacement cheap.

## Anti-pattern check

If a mockup resembles any of these, reconsider it:

- KDE Plasma with a Pokémon theme
- Steam/EmulationStation library organized by platform
- Netflix-style endless cover rows
- Qt Widgets desktop application
- PC dashboard full of tiny statistic widgets
- mobile app stretched sideways
- generic console launcher with Pokémon wallpaper

TrainerOS should read as a **purpose-built trainer handheld interface**.
