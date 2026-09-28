# Installation to everyday use — audit, 2026-09-28

TrainerOS has a working prepared-device experience, but not yet a reproducible
new-user experience. The main gap is the preparation between installation and
the first playable game. More explanatory labels or a wizard alone will not
close it. This audit records gaps and acceptance, not implemented behavior.

Scope: current source, tracked deployment tools and read-only checks of Flip
and Odin. No clean flash, factory reset, new network pairing, update/rollback or
new achievement earning was performed. Existing device data is preserved.
ROADMAP remains the execution queue; this is a user-journey acceptance map.

## Observed baseline

- Both devices have the native app, dedicated session, exit/network helpers and
  schema 14. Flip has the TrainerOS boot override; Odin intentionally retains
  its original Steam default. Do not turn this difference into an automatic
  installer overwrite of the owner's preference.
- Flip and Odin use different library layouts; Odin's default ROM path is an
  alias to its SD card. Integration JSON files and runtime setup were prepared
  through maintenance scripts. Their presence is not fresh-install proof.
- Odin's bounded library preparation verified 20 mainline Pokémon ROMs and a
  full Emerald test save. Launch, return and save-backed Home/Party worked.
  That test save, private artwork and account data are not product defaults.
- Existing evidence: [Odin](ODIN2_BRINGUP.md), [Trainer entry](TRAINER_ACCESS.md),
  [save ownership](TRAINER_SAVES.md), [network](DEVICE_NETWORK_PLAN.md),
  [session recovery](SESSION_PROTOTYPE.md), [RA](RETROACHIEVEMENTS.md).

## User journey and gaps

| Step | What works now | Gap to close / acceptance |
| --- | --- | --- |
| 1. Obtain/install | Native CMake installation and a separate additive Armada session installer. | One reproducible development installation must prepare dependencies, application, helpers, permissions and device-specific bindings, with preserved prior state and repeat/update behavior. A fresh supported Armada host must not depend on ignored private scripts. Consumer image and OTA remain #70/#71. |
| 2. First boot/input | Profile welcome, controller keyboard, profile creation, chooser and optional PIN; device-specific input/session work exists. | A resumable device setup must check usable controls/display and reuse profile creation. Optional Wi-Fi/account setup can be skipped. Do not require all emulators, artwork, Internet or a game to enter the shell. Upgrade must preserve completed choices. |
| 3. Choose storage/add games | Batocera-style folders, discovery, metadata/media and refresh; a standalone folder preparation script. | Select/reuse internal or SD library storage in Settings, create the compatible tree, show where files go and rescan. Handle an absent/read-only/full card and changed mount path without creating a misleading empty replacement or losing identities. Settings must work after onboarding too. No formatting by default. |
| 4. Discover/classify | Region-first Pokémon and platform-first Multiverse; empty systems hidden, missing Pokémon editions linkable; contextual edits. | Fix repeated/existing-World registration failures seen on Odin. Improve matching of the existing normalized filename/hash and annotated DS names without conflating hacks/editions. Repeated scans must converge without duplicate rows, SQL repair or manual per-game registration. |
| 5. Prepare a platform | Shared ARM64 catalogue, installed-core binding and specific native/Flatpak adapters; prepared devices launch normally. | Resolve actual installed emulators and prerequisites, prepare controller/BIOS/save/exit routes once per platform, and offer a real repair action for each missing prerequisite. “Setup” currently leads to the Adventure/file editor, which cannot install a missing emulator or resolve its BIOS. Reuse existing emulator configuration/data; avoid global replacement. |
| 6. First game/return | Empty Home leads to Worlds; short A launches a ready wheel selection; Y chooses context; guarded exit and recent media work on proven routes. | End-to-end clean-profile acceptance: copy a ROM, discover it, launch, save normally, cancel one exit, exit, reopen and load that save. Preparation must not require an exact semantic adapter for ordinary play. Missing progress support must not block launching or generate fictional Party/badges. |
| 7. Another family member | Trainer creation/switching/PIN/reset and personal shell records; verified GBA/mGBA save separation. | Supported runtime save ownership must be prepared together with launch. Existing-save adoption/import and a new playthrough need an explicit, safe path; creating a Trainer alone must not imply isolated saves on every emulator. Prove two-Trainer save/load on each claimed personal route. Portable Trainer export/restore remains #73. |
| 8. Accounts/media | RA login/read/cache/current-game return refresh; local gamelist images/video; optional private art/sprite providers. | Connect supported emulator earning to the active Trainer's RA identity and verify one newly earned unlock/return/offline/switch flow. RA shell login is not emulator earning. ScreenScraper stays paused for owner access; private packs do not make a clean installation visually equivalent. Final art-pack installation/Studio/Credits retain their final slot. |
| 9. Everyday system use | Volume/brightness, power, session transitions, Wi-Fi/Bluetooth actions and diagnostics. | Complete physical pairing/radio recovery, minimal controller calibration/remapping, and meaningful readiness/repair paths. Existing sound/RGB placeholder rows are not finished features. Sleep stays disabled until its separate tests pass; no first-boot promise of working wake. |
| 10. Reboot/update/recover | Local SQLite persistence, narrow protected save transactions/backups, session supervisor and maintenance fallback. | Prove cold boot and repeated session transitions; restore a Trainer onto a fresh install; deliver compatible image/database update and rollback; recover without developer SSH. A local save backup is not a complete device/Trainer backup. |

## Target first-run composition

Keep device setup short: welcome/control check → choose library storage →
create/select Trainer → optional network → Home. Reuse Settings and Trainer
components rather than introducing duplicate forms or a deep setup menu tree.
Show only relevant steps, with Back, skip for optional steps and durable resume.
Account, theme and optional media setup can happen later in Settings.

On an existing library, discover before asking the user to register anything.
A missing emulator/BIOS should open the corresponding actionable platform
preparation, then return to the same game. The normal route after preparation
is A → game, not A → setup on every launch. A completely empty device still has
a usable offline shell/Dex and a direct route to storage/library preparation.

Separate device readiness, platform launch readiness and exact-game companion
capabilities. “All supported” must never mean that every ROM was launch-tested
or that every save can be edited. Do not make users understand these internal
layers just to play; expose the specific missing action when it matters.

## Gap-filling order inside the current priority lane

1. **Library correctness first:** existing-World import, classification and
   idempotent refresh. Include copied/missing/reconnected storage and preservation
   of existing edits/history. This removes the actual Odin SQL workaround.
2. **Storage setup:** reusable persistent library location, folder creation,
   existing-library adoption and controller Settings flow. Preserve sources.
3. **Platform preparation/readiness:** broad installed-runtime discovery and
   bounded native/Flatpak routes, BIOS/controller/save/exit preparation and
   actionable repair. Representative launches only; no per-ROM compatibility
   campaign. Ordinary play and semantic save support remain independent.
4. **Repeatable development installation:** compose the proven services into
   a tracked install/update/repair path; preserve Steam, Plasma, prior settings
   and existing libraries. Validate the dependency chain on a clean isolated
   user/data setup before destructive device acceptance. This is not the final
   Armada image and must not become a competing packaging framework.
5. **Minimal first run:** connect the completed steps into the short resumable
   flow above, including useful empty states and return-to-game after setup.
   Pull only this necessary slice of #72/R14 forward. Full Help remains late.
6. **Family and RA completion:** finish supported save ownership/adoption and
   active-account emulator earning, with two-Trainer and offline acceptance.
   Finish installed Steam integration in the library lane without waiting for
   every optional emulator; it is not a prerequisite for first non-Steam play.

Then retain portable export/health, remaining device/recovery/sleep gates,
reproducible image, OTA/compatible rollback and final UX acceptance in their
existing dependency order. Final artwork/Pack Studio/Credits stay at the end.
Pokémon/Link/practice/franchise and all R1–R18, R7a/R18a/R18b acceptance remain;
this audit does not reopen deep save research ahead of the current priorities.

## Completion criterion

A person can install on a supported base, use physical controls to finish or
skip optional setup, add their own game, launch and save, return, reboot and
continue without developer SSH/SQL/JSON edits. Existing owners can upgrade and
recover their data; unsupported optional features do not block ordinary play.
The release path additionally proves clean image installation, restore and
compatible update/rollback. Until then, describe the current devices as prepared
development installations, not a finished out-of-box distribution.

## Code anchors / documentation debt

- `CMakeLists.txt`: current install rules ship the app/desktop entry and controller
  bridge; session/integration installation is separate.
- `packaging/session/install.py`: requires an already installed/validated app,
  root and Armada tools; `--default` is explicit. `client.py` supervises startup
  and orphaned Adventures; `control.py` retains maintenance recovery.
- `src/app/Main.cpp`: default `~/Emulation/roms`, optional CLI `--roms-dir`,
  per-installation integration JSON and distinct provider composition.
- `tools/prepare-rom-folders.py`, `tools/install-arm64-cores.py`: useful bounded
  building blocks, not user-facing installation/storage orchestration.
- `TrainerSetupPresentation.cpp`, `TrainerAccessController.cpp`: real profile
  entry and PIN flows, not whole-device onboarding.
- `BatoceraLibrary.cpp`, `CollectionRepository.cpp`, `SqliteLibrary.cpp`:
  filename identification and the multi-layer World creation/import boundary.
- `ShellController.cpp`, `LibraryManagementController.cpp`: current setup action
  opens the registration/file editor rather than platform preparation.
- `SettingsController.cpp`: real settings alongside unavailable sound/feedback
  rows; no persistent library-location or whole-device setup flow there.
- `AchievementAccountController.cpp`, `RETROACHIEVEMENTS.md`: shell account/read
  flow remains separate from verified emulator earning.
- README and older sections of PRODUCT_SPEC/UX_NAVIGATION still contain retired
  primary names, pair-only navigation and manual-registration directions. Dated
  overrides preserve decisions, but fresh-install documentation needs one current
  route alongside the repeatable installer. Historical evidence can remain in
  dedicated references; it should not be the newcomer's installation guide.
