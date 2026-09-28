# Agent approach audit — 2026-09-28

This is a correction of agent decisions against the owner's requirements, not
a replacement roadmap or a claim of a full-device feature audit.

## What I got wrong

I turned internal emulator preparation into a user task. Commit 1c9a798 added
an A → setup popup → Check again → Play sequence even though the platform
folders and one-A launch contract already define the solution. The extra file
chooser implied that the user had to explain a ROM that was already discovered.
Passing tests and a neat screenshot only proved that the wrong flow worked.

I also delivered a narrow diagnostic UI while announcing broader emulator
readiness work. Native/Flatpak discovery and automatic prerequisite preparation
remained undone. I should have implemented the dependency or reported its exact
remaining limit, rather than presenting another screen as the next useful step.

## Product understanding to preserve

- TrainerOS is the normal controller-first handheld experience on ArmadaOS.
  The desktop is explicit maintenance/recovery, not a routine step before play.
- Its core idea is continuity outside the game: the real Party, Pokédex,
  Center, shops and history continue the Adventure in the shell where verified
  integrations support it. It is not merely an emulator launcher with a skin.
  Pokémon is the first experience; later franchises may supply different pages
  and concepts through the accepted experience-pack direction.
- The library follows Batocera folders and metadata. `roms/gba` supplies GBA;
  the appropriate installed emulator is selected automatically. A user does not
  register or bind each ROM. Catalogue matching adds Pokémon organization and
  information; it must not be required for ordinary ROM launch.
- Pokémon Worlds stay region-based; Multiverse is platform-based. These are
  presentations of the library, not competing file-management schemes.
- A launches from Home/game wheels. Y selects the Home Adventure. Hold A opens
  explicit management. L1/R1 and L2/R2 retain primary/secondary navigation.
- The custom physical-looking shell and existing Playroom are deliberate design
  choices. Do not redesign them as a side effect of backend work.
- New functionality should remove user work. A modal is justified by a real
  choice or protection (purchase, deletion, guarded save/exit), not by exposing
  implementation phases. Do not stack menus to narrate background operations.
- Real capabilities matter: installed emulators, functioning controls, normal
  save/return and truthful errors. A label is not an implementation; a detected
  executable is not proof of firmware, controls or protected save support.
- Emulator defaults belong in the future Armada image and updates must preserve
  preferences/data. No per-game setup wizard substitutes for those obligations.

## Concrete findings and corrections

| Finding | Correction/status |
|---|---|
| Per-title preparation popup adds two confirmations | Removed; worker preparation continues directly to normal launch after the original A |
| Choose game file appears for an already discovered ROM | Removed from the normal launch path; missing catalogue titles report absence and use folder discovery |
| Home/footer advertise Set up | Changed to the actual Play action |
| Existing prepared games already launch directly | Preserve that fast path; no blanket extra scan/check before every launch |
| Folder discovery already supplies platform IDs | Reuse it; regression checks an arbitrary filename in `gba`, without title matching |
| Previous tests rewarded a popup rather than the user's outcome | Replace with single-request launch, duplicate-press suppression, missing-file and custom-route preservation checks |
| Active docs still prescribe setup-on-A | Correct AGENTS, UX, startup audit and roadmap; mark old popup evidence superseded |
| Broad emulator discovery/preparation is still incomplete | Keep it open in the existing startup lane; do not claim this correction implements it |
| Source retains historical manual registration/detail routes | Do not expand them; normal wheel/Home A must not enter them. Explicit management is separate |

The audit deliberately does not mark the remaining emulator readiness work as
complete. `Main.cpp` still loads per-device integration snapshots; broad automatic
native/Flatpak discovery and safe refresh after upstream updates remain actual
dependencies. The correction uses the existing folder scanner and launch adapters
instead of inventing a second library or asking the user to compensate for that gap.

Device evidence for the correction: one controller A on the selected Kirby Super
Star Ultra in an isolated Flip library reached its actual title screen, with no
intermediate preparation UI. The test began with its internal runtime record
unconfigured. Existing ROM/title identity remained intact. A separate regression
discovers an arbitrary filename under `gba` and verifies selection of mGBA without
catalogue matching. Five affected CTest targets passed. This is launch proof, not
a new save/exit compatibility claim; the isolated test did not load the production
Home-overlay integration and its owned title-screen process was stopped separately.

## Working rules for subsequent increments

Before implementation, state the intended user outcome and action count; find
the existing service that already owns the behavior. For normal play the result
must remain **select a game → A → game**. If a proposed screen asks the user for
information already present in folders/configuration, eliminate that screen.

Review the result against the owner's contract before writing assertions about
the implementation. Use one representative end-to-end device check plus relevant
regressions; avoid repeated broad testing without a new concern. Report concrete
delivered behavior and remaining dependencies separately. Commit/push and device
delivery remain required; GitHub Actions stays disabled.

Keep the accepted library → Settings → system → RetroAchievements priorities,
all deferred work and safety gates in ROADMAP. This correction does not authorize
new save research, an emulator framework rewrite or unrelated visual redesign.
