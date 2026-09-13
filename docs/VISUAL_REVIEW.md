# Shared visual review — 2026-09-13

Scope: the owner's combined visual pass, #22/#23/#33, existing surfaces from #27/#34, and the follow-up #21 tab overhang. [Design decisions](DESIGN_LANGUAGE.md#shared-visual-pass--2026-09-13) and [execution order](ROADMAP.md) remain authoritative. Future motion is recorded separately; the current background is static.

## Review matrix

| Surface | Change / review target |
| --- | --- |
| Chassis and Home | Panel lip at 36, inactive tips at 49, unchanged active tab and enlarged diagonal upper-chassis extension at 63, safe content at 73. One closed beveled screen aperture follows the title into the sidewall; upper, side and lower body share a continuous material without a footer seam. Continue joins the lower body. Services use the same aperture. Shared quiet field lines, compact battery, fixed A launch and Y selector. Crystals/progress data are unchanged. |
| Worlds | Larger three-column region cards; four complete game rows with focus rings, including grey missing editions. Shared contextual headers and larger centered record emblem. Search/filter/back remain on the attached lower tray. |
| Pokédex | Compact title/trailing context, mounted filter/action rails, shared detail heading. List focus, zero results, form/journal/favorite actions and absent artwork retain explicit behavior. |
| Trainer | Common heading, tighter identity block, larger readable statistic labels, mounted profile controls and palette-independent emblem. Long names elide; empty/edit/error states remain bounded. |
| Hall | Shared title and molded local rail/action tray. Archive, account, cached/offline/loading/error and unknown records retain source information. Earned-state redesign #24 remains separate. |
| Start and confirmations | Right-edge module with field-tools/session groups, raised warning exit cap, same focus/order/Back. Existing mode and save confirmations use common trays; future #26 Power is not implemented by this pass. |
| Settings / device / diagnostics | Compact headings, larger Settings controls with palette preview, molded trays, original diagnostics/status and volume/brightness behavior. |
| Library / account / Center / editors | Shared headers and attached action surfaces; file paths, account errors, save provenance/protection copy and manual journal caveats remain visible. |
| Controller entry / scaling | Existing right-side numeric keyboard, modal priority, L1/R1, fixed Home A, Continue selection and focus restoration. Themes and Reduced Motion preserve geometry; 960×540 logical, 1920×1080 and letterboxed rendering. |

## Evidence

Host validation: Windows/UCRT64 native build and full 29-test suite; the new four-row geometry check initially used QObject parentage, then was corrected to walk the actual ListView visual tree. All 28 other tests passed; the corrected Worlds rendered SDL scenario passed separately. The check still requires all four real controls and their full focus outlines to fit the viewport. Diagnostics tests verify that press feedback respects foreground/neutral gating and an SDL Confirm depresses the focused cap without delaying the action. Existing battery scenarios render zero/low/charging/unknown states; charging-bolt and healthy-controller visibility are checked.

Native ARM64 validation: **30/30 tests passed**, including rendered SDL, input, persistence and Linux session checks. Subsequent visual-only changes extend the scrims and replace stacked frame pieces with one chassis aperture shared by pages/services. The final frame received a fresh ARM production build and **6/6** Windows rendered SDL/persistence rechecks (98.89 seconds); the earlier full ARM suite is not presented as a rerun on that later frame. Input and domain behavior did not change during these follow-ups.

Device delivery: production installed with a previous-binary/SQLite backup, source-manifest and installed-byte verification. SQLite quick-check/schema 6 and all 686 personal Adventure rows were preserved. InputPlumber events exercised all five pages, a missing game, Settings/device/diagnostics/Center/library, mode confirmation/Back and Continue. Final Gamescope frames also verify four complete real game rows with platform badges, the enlarged integrated title, keyboard, Start scrim and return to Home. Screenshots show the current device setting, Reduced Motion on; host fixtures cover both motion modes and all palettes.

After the continuous-chassis refinement, a second InputPlumber pass captured and visually reviewed all five pages, expanded Continue, Start, Settings and return to Home at 1920×1080. The title-to-sidewall corner, both lower corners, open drawer base and shared service aperture remain connected; no independent footer seam remains. Installed production bytes were verified again after this final frame build.

Captures, binaries, source manifests, private data and reports stay outside Git. Representative host renders cover every existing primary page, grouped Start and theme variants; empty/account/save/controller states use the existing rendered scenario fixtures rather than personal saves. GitHub Actions was not used, per the owner's instruction.

## Remaining acceptance

This does not close the whole of #27/#34: their future Power, Multiverse Home/library, onboarding, media, Caught and achievement-earned surfaces still require their own review. No new art, game-save parsing or account semantics are claimed. Device frames can be inspected remotely, but owner comfort at handheld viewing distance and quantitative frame pacing/power profiling remain physical follow-ups. No sleep test is part of this increment.

## Follow-up: rounded tabs and integrated Continue

The owner's next review asks for softer corners, equal-width tabs meeting the title and right edge, larger playful lettering, a narrower Start menu, a compact unfolding Continue with a fitted diagonal yellow inset, card-wide translucent screenshots and no outer battery plate. [Geometry and motion](DESIGN_LANGUAGE.md#rounded-chassis-refinements--2026-09-13) and [font provenance/license](../assets/fonts/README.md) record the final treatment.

Validation: Windows native build and six rendered SDL/persistence scenarios passed (98.45 s). Subsequent card/battery/keyboard changes passed both persistence-process and general rendered scenarios (41.89 s); the final inset/Home corner passed the general rendered SDL scenario again (17.64 s). ARM64 production was rebuilt and installed with source/byte verification and a previous-binary/SQLite backup; schema 6 and 686 Adventure rows were preserved. This follow-up did not rerun the earlier full ARM suite. Font bytes were verified against the recorded upstream SHA-256; the license is embedded alongside the font.

On Flip, InputPlumber events exercised every primary page, text entry, service panels, Start and mode confirmation/Back. Final frames were visually reviewed for the title/Home join in both tab states, the rightmost label, the fitted yellow inset, screenshot-backed cards and battery. Independent timed Y cycles captured partial width expansion, initial upward movement and the fully opened state; interrupting close with Y reopened correctly and B returned to closed Home. Reduced Motion was off, as changed by the owner; existing host scenarios retain its immediate-geometry check.

Visual assessment: the equal tabs and rounded display face give a softer, more consistent upper hierarchy; all tab and long menu labels fit. The yellow insert follows the drawer contour with a visible body margin, and the 18% screenshot layer leaves World/title/save text legible on the inspected real saves. These are inspected frames and interaction checks, not a frame-pacing benchmark or a guarantee for every possible user screenshot. Captures and operational reports remain private and outside Git.
