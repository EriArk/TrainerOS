# Installation to everyday use — audit, 2026-09-28

TrainerOS has a working prepared-device experience, but not yet a reproducible
new-user experience. The next work starts at power-on: compose the complete
first-run journey and connect its real preparation services as needed, through
the first playable game. A wizard with nonfunctional steps does not close these
gaps. This audit records acceptance, not implemented behavior.

Scope: current source, tracked deployment tools and read-only checks of Flip
and Odin. No clean flash, factory reset, new network pairing, update/rollback or
new achievement earning was performed. Existing device data is preserved.
ROADMAP remains the execution queue; this is a user-journey acceptance map.

**Follow-up:** [native first-run implementation](FIRST_RUN.md) now connects the
core welcome/control/network/storage/Trainer/Home journey and durable resume.
Its refinement adds illustrated setup and persistent storage selection in Settings.
The subsequent discovery correction reuses existing Worlds, recognizes DSi
Enhanced filename annotations and separates equal relative names across storage
roots. Existing classifications are preserved; stable changed-mount identity
and fresh-image launch/update acceptance remain open.
The table below records the audit baseline and remaining target acceptance;
language/time controls, image preparation and update gates are still open.

[Standalone Home exit](STANDALONE_HOME_EXIT.md) subsequently adds guarded
DS/GameCube return and repeatable session-helper preparation. These were tested
on the existing Flip installation, not a clean image or upstream emulator update.

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

Keep device setup short and ordered from the user's first power-on:

1. Welcome and language selection, using actual available translations.
2. A brief physical-control check with the correct A/B mapping.
3. Optional Wi-Fi and date/time setup; offline use remains possible.
4. Internal/SD library storage: reuse existing games or prepare compatible folders.
5. Create/select a Trainer, with appearance and optional PIN. If PIN is chosen,
   integrate the existing family reset-code prerequisite into that path.
6. Optional accounts such as RetroAchievements, skippable until later Settings.
7. Home: choose an existing Adventure or directly open library setup when empty.

Reuse Settings and Trainer components rather than introducing duplicate forms
or a deep setup menu tree. Back and skip must preserve completed choices;
restart resumes the unfinished step. Subsequent boots use the existing Trainer
chooser/PIN rules and enter Home. Upgrades preserve existing owners' completed
setup and must not force fresh-install setup or reset their boot-session choice.
Optional media setup can happen later in Settings.

In the target Armada-based TrainerOS image (#70), supported emulators and their
baseline controller/launch settings are already included. First run must not
download or ask users to build that stack one
emulator at a time. User-supplied firmware remains separate from distributable
defaults; missing optional platforms or firmware do not block entering Home.

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

1. **First-boot journey first:** define and build the resumable flow above,
   reusing the real controller, Connections and Trainer services. Pull only this
   necessary #72/R14 slice forward. Develop the following dependencies within
   this user journey; do not ship placeholder steps or defer onboarding until
   after unrelated library expansion. Full Help remains late.
2. **Storage/discovery within setup:** persistent library location, folder
   creation and existing-library adoption, reusable from Settings. Fix the
   existing-World import failure and classification/idempotent refresh. Include
   copied/missing/reconnected storage and preservation of edits/history.
3. **Preinstalled platform readiness:** discover supported native/Flatpak routes,
   prepare controller/firmware/save/exit paths and offer actionable repair.
   Representative launches only; no per-ROM campaign. Ordinary play remains
   independent of semantic save support.
4. **Repeatable installation and update preservation:** compose those services
   into a tracked install/update/repair path under the policy below, preserving
   Steam, Plasma, preferences and libraries. This remains the foundation for the
   later Armada image, not a competing packaging framework.
5. **Fresh-install acceptance:** on a clean isolated user/data setup, exercise
   first boot, interrupted setup/resume, offline/empty and existing libraries,
   first launch/save/return and upgrade preservation. Retain later physical
   clean-image acceptance; prepared devices alone are insufficient proof.
6. **Family and RA completion:** finish supported save ownership/adoption and
   active-account emulator earning, with two-Trainer and offline acceptance.
   Finish installed Steam integration in the library lane without waiting for
   every optional emulator; it is not a prerequisite for first non-Steam play.

Then retain portable export/health, remaining device/recovery/sleep gates,
reproducible image, OTA/compatible rollback and final UX acceptance in their
existing dependency order. Final artwork/Pack Studio/Credits stay at the end.
Pokémon/Link/practice/franchise and all R1–R18, R7a/R18a/R18b acceptance remain;
this audit does not reopen deep save research ahead of the current priorities.

## Emulator defaults and ongoing updates — owner clarification, 2026-09-28

The Armada-based TrainerOS image (#70) includes supported emulators and usable
baseline settings before first boot. Native package/session deployment remains
the development foundation; it is not the intended consumer installation flow.
They receive ongoing upstream updates through the supported Armada/package,
Flatpak or native-runtime route. Do not freeze the whole stack indefinitely to
protect integration, or update running games. Exact unattended-update policy
and release channels remain to be chosen; this is not a delivered updater.

- **Separate ownership:** emulator binaries/resources are replaceable; TrainerOS
  launch bindings, controller/exit integration and save-location policy have
  their own maintained configuration. User preferences, accounts, ROMs, firmware
  and saves remain user data. An update must not reinstall factory defaults over
  them or silently move to a different save directory.
- **Use narrow overrides:** prefer supported per-launch/per-profile configuration
  where available. Seed defaults once. Where an emulator requires a shared file,
  patch only the necessary owned keys with a backup and preserve unrelated edits.
  A copied whole base configuration must not become an indefinitely stale fork;
  reconcile it with upstream changes when that route is updated. Never assume
  all emulators expose the same override or migration mechanism.
- **Keep integration compatible:** track the installed runtime identity/version
  and the relevant CLI, configuration, input, save-path and exit requirements
  in the existing adapter/platform registry. Refresh discovered executable/core
  locations after updates while preserving explicit user routes. Version changes
  do not by themselves invalidate unrelated platforms or imply verified support.
- **Bounded update checks:** check an affected route's launch, controller input,
  Home exit/cancel/return and ordinary save/load, including claimed Trainer save
  separation. Verify that user preferences survive and the same saves remain
  reachable. No retest of every ROM or semantic research campaign is required.
  Protected semantic writes retain their independent exact-build gates.
- **Recover incompatibilities:** preserve the last working integration settings
  before migration and the prior runtime when the update mechanism supports it.
  Restore a compatible runtime/configuration pair or offer a concrete repair;
  do not silently reset settings or roll back/delete newer game saves. Runtime
  downgrade must consider changed config/save formats. An affected emulator must
  not prevent TrainerOS, other games or maintenance mode from working.

These requirements extend platform preparation and later #71 update acceptance;
they do not claim arbitrary future emulator changes can never break integration.
Keep update work within existing adapters/platform services, without a new
generic plugin/update framework. Existing prepared-device settings are preserved
until a replacement route is verified.

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
- `ShellController.cpp`, `LaunchPreparation.cpp`: one A launches using the
  folder-derived platform; missing internal runtime records are prepared
  automatically, never through a per-title setup popup. Missing games/runtimes
   report errors. [Native/Flatpak discovery](EMULATOR_DISCOVERY.md) is delivered
   for the existing adapters; prerequisite installation and image/update proof remain open.
- `SettingsController.cpp`, `LibraryStorageController.cpp`: persistent storage
  selection/discovery is delivered and reused by the first-run flow; unavailable
  sound/feedback controls retain their separate acceptance.
- `AchievementAccountController.cpp`, `RETROACHIEVEMENTS.md`: shell account/read
  flow remains separate from verified emulator earning.
- README and older sections of PRODUCT_SPEC/UX_NAVIGATION still contain retired
  primary names, pair-only navigation and manual-registration directions. Dated
  overrides preserve decisions, but fresh-install documentation needs one current
  route alongside the repeatable installer. Historical evidence can remain in
  dedicated references; it should not be the newcomer's installation guide.
