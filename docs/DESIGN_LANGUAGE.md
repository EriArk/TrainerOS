# TrainerOS Design Language

Collection cards include an original hardware silhouette and short platform label seated in a recessed badge within the raised card. Missing editions use neutral grey material and retain a full-contrast focus outline; greying does not disable focus or the file-link action. This is the user's 2026-09-11 extension; see [collection catalogue](COLLECTION_CATALOGUE.md).

## Shared chassis geometry — delivered 2026-09-13

Issue [#21](https://github.com/EriArk/TrainerOS/issues/21): the panel now starts at the inactive-tab baseline, with the original active-tab silhouette overlapping its lip and a narrow contact shadow. Shared Theme geometry owns the panel bounds, tab overlap, content insets and footer. Primary pages and frame-mounted services use the same safe top inset; keyboard/system overlays stay attached to the frame and render above the tabs. Page contents and controller actions are unchanged.

At 960×540 logical size the panel top moves from 68 to 49, the active tab ends at 63, and safe page content begins at 73. The footer and bottom-mounted keyboard/Continue remain fixed. Scaling and letterboxing still belong to the viewport; palette and reduced-motion changes do not alter these dimensions.

Verification: the six existing rendered SDL/persistence scenarios passed on Windows/UCRT64 and native ARM64 on Flip (6/6 each), covering 960×540, 1920×1080, letterboxing, focus, menus/text entry, themes and restart. The production build was installed with a previous-binary/database backup and byte verification. InputPlumber controller events exercised all five pages, Start/Back and Continue on the running Flip; Gamescope captures were inspected. This does not close the separate header/pattern/status/content-polish work or U12's performance/calibration tasks.

## Accepted shared-system refinement — remaining work

Issues #22–23/#27/#33 build on the delivered geometry: share compact title variants and one subtle cached ornament. Healthy battery/controller status becomes quiet; important faults/provenance stay clear. Mounted Start controls and Power caps use the same recess/bevel/focus family. Special Home/onboarding/banner compositions remain intentional exceptions to the general heading template.

Issue #32 adds a **scoped exception** to the light-workspace rule: Multiverse Home may use a dark cosmic/portal inner field, with the same chassis geometry, physical controls and strong contrast. Pokémon Home and ordinary content retain their light recessed surfaces. Shell color themes stay user-selected; a context switch does not globally change them. New media/pattern layers cannot obscure focus or detach controls from the frame.

Issue #34 audits feature interiors after these shared foundations: meaningful larger cards/media, four readable game rows where targeted, balanced typography and deliberate empty/error states. Check each delivered screen at handheld scale and on physical Flip; do not redesign the chassis a second time or alter navigation/data ownership during polish. Original startup/audio assets reuse this identity without copying commercial logos, music or jingles. Detailed gates: [expansion acceptance](EXPANSION_PLAN.md).

## Design thesis

TrainerOS should look like a **real trainer terminal from the Pokémon world**, but not imitate one specific game's Pokédex pixel-for-pixel.

The UI needs enough original identity to stay coherent across Kanto through Paldea and across very different game generations.

Think: modern dedicated handheld system with region-aware atmosphere, not Linux/KDE/Steam with a Pokémon theme.

## Overall composition

### Material and contrast direction — September 2026

The target device is the turquoise Retroid Flip 2 with Snapdragon 865. The owner's device reference establishes this visual direction:

- Dark turquoise/teal structural panels, approximately matching the device's shell color.
- A white or near-white main content surface, with clearly darker text and strong contrast against the surrounding panels.
- Tangible depth through panel thickness, inset surfaces, bevels, contact shadows, and restrained reflections. Tinted polymer is a useful material reference.
- Every control has a visible supporting surface. Buttons belong to panels, trays, or rails; avoid detached controls floating above an arbitrary background.
- Panels slide from visible slots or behind supporting surfaces. Their movement preserves a readable connection to the surrounding structure.
- Small chips and wear may appear sparingly on edges and contact points. Keep text and main content clear; do not apply a uniform distressed texture everywhere.
- Subtle parallax is an option to explore inside recessed visual areas. Keep text, controls, and their supporting panels stable, and respect reduced motion.
- Focus should remain attached to its control through an edge treatment or inset indicator. Pressed feedback may depress the control into its panel.

The exact palette, material finish, amount of wear, depth, and page composition remain exploratory. A material study does not approve a final Home layout. These are software visuals, with no physical modifications to the handheld.

Acceptance checks for this direction:

- Primary content remains light and readable against the darker teal panel structure.
- Every actionable control has an identifiable supporting surface in both resting and animated states.
- Drawer motion shows where the panel comes from and where it returns.
- Decorative wear and parallax do not interfere with text, controller focus, or reduced-motion operation.

### Flip 2 screen and controller composition

Design against the actual horizontal handheld display: **Retroid Flip 2, 5.5-inch AMOLED, 1920×1080 (16:9), 60 Hz**. Manufacturer reference: https://www.goretroid.com/products/retroid-flip-2-handheld.

- Home and ordinary overview/service surfaces should fit inside one screen without whole-page scrolling.
- Drawers and overlays stay inside that fixed viewport. Continue must not extend the page downward or require a finger swipe to reach its cards.
- Worlds, Pokédex, and growing archives may scroll inside clearly bounded content areas. Controller focus drives that scrolling and remains visible; touch gestures are never required.
- Keep page controls and useful navigation hints stationary while a list moves. `L1/R1` remain reserved for primary pages.
- Fit content by prioritizing it, using horizontal space, and moving secondary details into another controller-accessible view. Do not solve overflow by shrinking text excessively.
- A 960×540 logical composition at 2× is a useful initial design study for the 1080p display. Validate final type sizes, margins, and density on the physical 5.5-inch screen; native pixel count alone does not determine readability.
- In conversational previews, preserve the 16:9 device geometry when the preview is scaled. Do not reinterpret a narrow conversation window as a portrait version of the handheld UI.

Acceptance: opening Continue or a system panel must not change the outer screen dimensions; moving past the last visible list item must reveal the next item using controller input alone. No essential actions may sit below the device viewport.

### Friendly character and interchangeable color themes

The material interface should have a friendly Pokémon adventure character. Use softer shapes, expressive progress badges, warm details, and the sense of a personal field companion. Avoid excessive industrial labels, serial numbers, stern monochrome styling, or a corporate/instrument dashboard mood. Decorative details still need a visible attachment or supporting surface.

Turquoise matching the owner's device is the default shell theme. The native shell also provides **red, green, blue, and orange** through Start → Settings, using the same interface:

- Centralize panel, edge, shadow, focus, and text colors as semantic theme tokens.
- A theme changes the palette/material finish, not layout, navigation, focus order, or interaction behavior.
- Keep the white/near-white main content surface and strong readable contrast across themes.
- Separate the chosen shell theme from World atmosphere and Pokémon type/status colors. Entering another World must not silently replace the user's shell theme.
- Use original geometric placeholders until suitable original or licensed assets exist.

Color and reduced-motion preferences persist locally. Chassis/bevel colors consume shared tokens; button accents and recessed screens retain their distinct roles. Reduced motion removes drawer, keyboard and tab transition durations without changing final geometry or focus behavior.

Acceptance: the same screen and controller path must work in every theme without content shifting or color becoming the only state indicator. The current mock's compositions remain exploratory.

### Organizer tabs, angled modules, and recessed screens

The owner's next design refinement adds diagonals and circles to soften the rectilinear composition:

- Primary page names sit on differently colored organizer-like tabs emerging from the upper frame. The active tab extends farther into the interface; color is supplemented by depth/position/focus.
- Continue rests as a small bottom-left frame module with a diagonal cut on its right side. It protrudes into the white workspace, expands horizontally on `Y`, then rises to reveal resume cards. Closing reverses those stages.
- Light screens sit visibly inside the casing. Use sloping bevels from the surrounding frame down to the content surface, as on older handheld instruments.
- Buttons and list rows have tangible thickness, contact shadows, and distinct colors. Their palette is separate from the shell theme so controls remain readable against both panels and white screens.
- Panels supporting controls must connect to a frame edge, extend a main panel, or visibly mount on an existing frame module. Do not suspend a control panel in the middle of the white workspace.
- Side modules attach to the side/top/bottom frame; filter rails attach to the upper frame; keyboards and action trays attach to the lower frame.
- The keyboard has a letter/editing block on the left and a distinct three-column numeric block on the right, with a wide zero below `1–9`. Both sit in the same lower-frame tray; numeric keys use a contrasting cap color.
- Use circles for recessed lenses, avatar wells, indices, and button caps; diagonal cuts for module joins. These details must not reduce readable content or controller target clarity.
- Controller hints belong to the casing: embossed/debossed legends and raised colored button caps, with quiet shadows and a clear physical relationship to the bottom frame.

Acceptance: check Home, Continue, filter/search panels, profile editing, and keyboard states at the same 16:9 dimensions. Every control-supporting panel has a visible connection to the frame; no layout depends on touch, floating panels, or whole-screen scrolling.

### General layout rules

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

The 2026-09-11 refinement gives the right frame-mounted module a large circular Adventure button: recessed mounting ring, warm contrasting cap, thick lower edge, bevel and short pressed travel. It starts the Adventure selected on Home. The Y drawer chooses which Adventure Home describes without starting it. The button replaces the earlier companion placeholder in this composition; see [Home and play history](HOME_AND_HISTORY.md).

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
- an internal RetroAchievements area using the same frame-mounted controls and visual language
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
- an attached edge/inset treatment; any depth change must preserve contact with the supporting panel
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

## Home badge collection — 2026-09-13

Badges are individual colored, faceted crystals in a molded collection tray, with distinct silhouettes and separate Kanto/Hoenn sets. Use visible bevels, highlights and contact shadows; an unearned badge leaves a shaped recess. Keep the crystals large enough to distinguish on the handheld (about three times the first tiny diamond indicators), with all eight visible together. Unknown progress remains unknown rather than appearing as eight missing badges. These are original vector interpretations, not copied game sprites. The collection is informational and adds no controller focus stops.

## Placeholder asset policy

The accepted [media/artwork queue](ROADMAP.md) adds optional images without replacing this material language. Worlds' selected Adventure panel uses fitted wheel/marquee/boxart with a persistent text title and ownership state. Home's return screenshot is a faint layer beneath the existing ornaments/patterns, light workspace and controls. Neither adds focus stops. Pokédex species/form art uses a separate provider and deliberate missing-art treatment. Check wide/portrait assets, bright/dark screenshots and all themes at both logical/device resolutions; asset presence must not imply progress or playability.

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
