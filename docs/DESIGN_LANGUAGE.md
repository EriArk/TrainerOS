# TrainerOS Design Language

Collection cards include an original hardware silhouette and short platform label seated in a recessed badge within the raised card. Missing editions use neutral grey material and retain a full-contrast focus outline; greying does not disable focus or the file-link action. This is the user's 2026-09-11 extension; see [collection catalogue](COLLECTION_CATALOGUE.md).

## Shared chassis geometry — delivered 2026-09-13

Issue [#21](https://github.com/EriArk/TrainerOS/issues/21) established one panel/tab geometry and a narrow contact shadow. Shared Theme geometry owns the panel bounds, tab overlap, content insets and footer. Primary pages and frame-mounted services use the same safe top inset; keyboard/system overlays stay attached to the frame and render above the tabs. The later lip adjustment below preserves the original tab silhouette and controller actions.

At 960×540 logical size the first increment moved the panel top from 68 to 49. The owner's follow-up raises it again to **36**, the beginning of the inactive tabs' 13-unit bevels. Inactive tabs still end at 49; the active tab keeps its full height and ends at 63. Both cast a contact shadow, with stronger depth on the active tab. Safe content still begins at 73, clear of the overhang. Footer, bottom-mounted keyboard and Continue remain fixed; palette and reduced motion do not change geometry.

The further owner refinement enlarges the TRAINER / OS title from 17 to 22 units on a 206-unit-wide **extension of the upper chassis**, ending at the same 63-unit line as the active tab with an 18-unit diagonal right cut. The owner explicitly rejected a separate plaque. `ChassisFrame` supplies one continuous body around a single closed, beveled screen aperture that follows this extension into the left side. Upper, side and lower edges share one material gradient; the lower hints sit directly on the body without a transverse seam. Continue uses that same spatial gradient and an open lower edge. Primary pages and full-screen services share the aperture instead of stacking independent frames. No controller subtitle occupies the title area; only exceptional controller loss appears in the footer. Start/keyboard scrims extend across the full upper chassis so their dimming never cuts through the enlarged title; their mounted tray geometry stays fixed.

Verification: the six existing rendered SDL/persistence scenarios passed on Windows/UCRT64 and native ARM64 on Flip (6/6 each), covering 960×540, 1920×1080, letterboxing, focus, menus/text entry, themes and restart. The production build was installed with a previous-binary/database backup and byte verification. InputPlumber controller events exercised all five pages, Start/Back and Continue on the running Flip; Gamescope captures were inspected. This does not close the separate header/pattern/status/content-polish work or U12's performance/calibration tasks.

## Rounded chassis refinements — 2026-09-13

The next owner review softens the exposed diagonal corners while retaining the organizer silhouette and the active tab's extra overhang. Five equal 145.2-unit tabs, spaced by 4 units, span from the 206-unit title extension to the recessed panel's right edge at 948, leaving the 12-unit right sidewall clear (owner's final alignment correction). Home meets the title side directly, closing the former notch. The title reads **TRAINER OS**, without a slash. Bundled [Fredoka](../assets/fonts/README.md) provides rounded display lettering: 24 units for the title and 19 for tabs; ordinary content keeps the UI font.

Continue's closed body is 292 units wide and reaches the left sidewall. Its yellow label inset follows the outer diagonal, with softened corners and a plain printed **Y**, seated inside the chassis rather than mounted as a separate rectangular button. Its lower edge joins the footer; its upper diagonal has softened transitions, as does the keyboard tray. Normal opening expands width for 180 ms and then rises for 220 ms; closing lowers then narrows, with interruptible animation. Reduced Motion still completes geometry immediately. The owner has switched Reduced Motion off on Flip to see this behavior. Each saved-moment card uses its screenshot as a cropped, 18%-opacity background across the card face; missing previews retain quiet original geometry. A colored ribbon carries the Adventure title. A lightly frosted field improves the lower metadata's readability: only its cropped background is blurred, with a cached capture while the drawer is open; the text stays sharp. Software rendering and missing previews retain the light field without a shader. Battery icon/percentage/charging and critical cues sit directly on the lower body, without an outer gauge plate.

The owner's further material correction deepens all chassis palettes slightly while preserving white content and distinct colored controls. A narrow, continuous outer contour catches light at the upper/left edge and darkens at the lower/right edge; the broad body rolls into a darker foot. The aperture reverses that lighting, with a shaded upper/left slope, brighter lower bevel and a thin contact shadow against the paper. Continue shares the body's spatial gradient and leaves its outer perimeter visible. These static gradients supply depth without adding a full-screen blur or continuous rendering effect.

The yellow Continue inset now follows the drawer's animated width, preserving the seated left edge and matching right diagonal. The dark `Choose for Home` legend is printed on the right of that yellow surface at its final horizontal position; the growing inset reveals it through a bounded clip, without a separate slide or opacity animation. Closing covers it in reverse. Acceptance: inspect closed, partly widened, raised and interrupted/reversed states, with real/sample copy and Reduced Motion; no text may extend beyond the yellow inset.

The next owner correction seats both Continue shoulders at the screen bevel. The actual left edge of the panel and its yellow inset have a shallow diagonal cut; the screen bevel above stays straight, without an added notch or protrusion. The right diagonal ends directly at the lower lip when closed and at the side lip when raised. Preserve this owner-approved right contact, without a separate fillet. The expanded body ends at 937, leaving the side bevel exposed. A short layered contact shadow follows the upper edge and both cuts, with no bottom seam or animated blur. At the mounting points, graduated shadows along the existing bevel soften its transition into the drawer; no second bevel is added. The body and highlight share one outline. The outer right chassis corners are square, removing the tiny radius at the physical screen edge. Acceptance: inspect both contacts, exposed bevels, outer right corners, yellow inset/legend containment, all three card faces/focus and intermediate/reversed motion at native size and in enlarged crops.

Start's right-mounted body narrows from 480 to 376 units. Notice text and confirmation controls adapt to that width; menu order, modal focus and Back remain unchanged. Acceptance: inspect both active/inactive Home-to-title joins, every tab label, the rightmost edge, both Continue states and intermediate/reversed motion, ribbons and lower metadata with/without previews, GPU blur and software fallback, the continuous shaded body, long menu labels and confirmations, battery states, palettes and handheld scaling. All changes preserve the continuous frame and fixed A/Y/L1/R1 behavior.

## Tab seams without a backing rail — 2026-09-13

The owner's next clarification preserves the tabs' existing faces: equal width, rounded diagonal outline, colors, Fredoka labels, active indicator and full active overhang. Only their mounting treatment changes, with old music-center keys as the depth reference. The screen aperture continues above the visible viewport behind the tab bank, removing the common horizontal chassis rail that previously appeared between/beneath the tabs. This supersedes the visible upper-lip treatment above; the content bounds and 73-unit safe content start do not move.

Each tab casts a short contour-following contact shadow and a lighter outer shadow. Narrow shaded seams sit between the tabs, at Home's direct join with the TRAINER OS extension and where Hall of Fame meets the right sidewall. The owner's further correction requires actual edge-to-edge contacts: every aperture contour uses the same upper boundary at x=206 on the left and x=948 on the right, so the frame's inner bevel cannot creep beneath a tab face. The title bevel turns below the left contact; the right bevel turns inward below the key's lower diagonal, tracking its 49/63-unit height with the same 130 ms motion (immediate with Reduced Motion). The side/title/footer bevels remain connected; no separate backing plate or floating control rail is introduced. Acceptance: inspect both end joins with their tab active and inactive, the spaces below the lower bevels, all labels and unchanged faces, and Start/keyboard overlay priority. L1/R1 and all page actions retain their behavior.

## Recess contour audit — 2026-09-13

The owner's close review identified a hooked right return, a pastel-colored slope and a missing left cut on Home. Home again uses the same softened left diagonal as its peers; its upper contact with the title remains. Tab faces, labels, colors and widths are otherwise unchanged. The right return now uses a cubic transition with vertical tangents at both ends, following Hall's height. Its shadow stops at the sidewall and the two outer contact seams fade out at the tab bevels, so they do not paint over the exposed rim.

Every screen slope is shaded from chassis colors; the brighter rim color is confined to narrow edge highlights. This applies to the main chassis and the shared `Panel` used by menus, text entry, account, launch and recovery surfaces. Nested corner radii use the same centers, preserving bevel thickness. The shared panel paints its open-center rim above mounted fills, preventing those fills from cutting off its inner corner/stroke. The recovery tray stays inside that rim, and account content clears its inner corners. Primary/service content has a 12-unit inset, one unit clear of the 11-unit screen slope; the safe content top is now 75. These small clearances replace the former 10/73 geometry without moving the tabs, title, Continue or footer.

Acceptance: inspect the right joint active/inactive at native size and magnified, Home's restored cut, title-to-left-wall transition, both lower corners, exposed slopes on every primary page, and menu/keyboard/recovery panel rims. Check five palettes, content/focus clearance and controller navigation. The magnified crops are diagnostic evidence; final layout approval must also use a full handheld frame.

The owner's subsequent correction keeps the entire right slope fixed: it belongs to the chassis and must not be cut back or follow the active tab. The five tabs now share a 143-unit width and end at x=937, the inner edge of the 11-unit screen bevel. Their rightmost seam and shadow stay inside that boundary. This supersedes the height-following return and x=948 tab endpoint above; the sidewall and its full bevel continue vertically to the upper edge. Home's left contact/cut and the existing tab faces/active overhang remain. Acceptance: inspect Hall active/inactive and its transition, checking unchanged slope width, readable labels and no tab/shadow covering the bevel.

## Continue placement and stronger cast shadows — 2026-09-14

The complete Continue assembly shifts left by 3 logical units (6 physical pixels on Flip), including its face, yellow inset, labels and cards. Its expanded width compensates for that offset so the approved right diagonal still reaches the side bevel. Mounting-point shadows remain aligned with the chassis. The top and both cuts now have a denser contact shadow fading through six contour bands over 11 units; the bevel contact gradients are stronger too. Tabs keep their faces, spacing and active overhang, with a darker near shadow and a five-band fade extending 10 units below the face instead of 6. The final tab shadow still stops before the right bevel. These are static contour layers that follow existing interruptible motion, without another blur or frame timer. Acceptance: inspect closed/open Continue, partial/reversed motion and first/last card focus, active/inactive end tabs, shared content clearance, intact bevels and the shadow fade at both logical and native size. Controller actions and Reduced Motion behavior remain unchanged.

## Upper chassis lip over the tabs — 2026-09-14

The fixed upper lip now covers the tops of the existing tab buttons. It has the same 12-unit thickness as the right outer body, joins that sidewall and continues into the TRAINER OS extension with matching chassis gradients and edge lighting. An 8-unit downward fading shadow falls onto the buttons from beneath this lip. There is still no backing rail below the tabs. The buttons keep their original x/y, width, height, silhouettes and active overhang; only their labels shift down by half the lip height to center within the remaining visible face. The lip stays below keyboard/system overlays and introduces no input target. Acceptance: inspect continuous title/top/right joins, active and inactive end tabs, equal label clearances, the two separate shadows above/below each key, unchanged lower contacts and modal dimming/priority in all themes at handheld size.

## Continue card title hierarchy — 2026-09-14

Each Continue card starts with its Adventure title on the colored ribbon, 8 units below the card's top. The ribbon is 16 units shorter overall, inset 14 units on both sides, with the same type size and bounded title elision. World and resume-point kind follow below; the frosted timestamp/summary field stays at the bottom. Card dimensions, screenshot coverage and controller focus/selection remain unchanged. Acceptance: inspect titles with and without previews, long-title elision, World/kind/metadata separation and first/last card focus at handheld size.

## Shared visual pass — 2026-09-13

The owner requested the available visual work together, including the deeper tab overhang above. Issues #22/#23/#33 now share these primitives:

- `PageHeader` owns title/breadcrumb/subtitle/trailing-context geometry, elision and optional two-line service status. Worlds, Pokédex, Trainer, Hall and service/editing screens reuse it. Home stays distinct. Headers take 48 units without a subtitle or 64 with one; eyebrows and two-line status add only their required space.
- Worlds uses the reclaimed height for 90-unit region caps and four complete 64-unit Adventure rows, including focus outlines. Missing Adventures remain grey and focusable for file attachment.
- `ShellBackgroundPattern` draws curved field lines once per size/palette change. Primary pages share the panel's cached texture; service overlays reuse the component. It has no input/domain dependency or frame timer. Future media must sit below the ornament, using the panel surface or an explicit Home pattern with the underlying one disabled.
- The compact accessible battery gauge retains percentage, charging bolt, critical exclamation and unknown dash. Only controller loss shows a lower-chassis warning. Detailed diagnostics, provenance and errors remain available.
- `MountedPanel` supplies shared molded tray lips. `CapButton` supplies sockets, highlights, contact depth, static focus rings, grey disabled material and optional warning marks. An observational Confirm cue depresses the focused cap before navigation without delaying actions; foreground/neutral gating stays centralized. Reduced motion removes interpolation, not the visible state.
- Start groups field tools separately from device/session exits. Settings adds a mounted palette preview. Service/account/save trays, printed controller keycaps and tab highlights share the physical treatment.

This covers **existing** #27 chrome and #34 feature surfaces. Future Power, Multiverse, onboarding, media and earned-state work keep their own acceptance gates. No account, save or launch ownership changes are implied. See the [visual review](VISUAL_REVIEW.md).

### Future background motion — owner request, 2026-09-13

In a later increment, probe whether Flip 2 exposes a usable gyroscope through the current ArmadaOS input/sensor stack. If verified, map gently filtered, bounded tilt to background-only parallax. If absent or inaccessible, design a calm autonomous background movement instead. Keep text, controls, focus and mounting panels stationary. Reduced Motion disables both variants; pause motion while hidden or an Adventure is active. Validate comfort, frame pacing and power cost on Flip before enabling it by default. Presence, calibration and API are unverified; this visual pass deliberately keeps its ornament static. Track this work in P5 alongside physical feedback capabilities, not as a prerequisite for today's polish.

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
- The outer chassis remains a continuous loop on every primary page and full-screen service, including beneath the title extension and around the lower corners. No footer seam cuts across the sidewalls; Continue joins the lower body in both states.
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
