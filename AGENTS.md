# AGENTS.md — TrainerOS

**Owner scheduling amendment, 2026-09-23:** generic artwork-pack installation,
selection, verification/update/removal and Pack Studio all move to the final
artwork delivery stage. Existing private illustrations remain in both the Dex
list and selected entry. Early #51 sprites/portraits use a separate bounded,
read-only local provider; its necessary validation is not a generic pack manager.
The next order is sprites/portraits, P2 ownership, then P3/P4 library binding.
All earlier acceptance remains; ROADMAP is the execution queue.

**Owner interface clarification, 2026-09-23:** no separate sprite/portrait
inspector in normal Pokédex use. Keep the illustration and animated companion.
Defer author-entry tooling and consolidated Credits to the final pack/Pack Studio
stage. Preserve per-asset authors, sources and terms in the underlying data now;
do not add technical asset panels to ordinary screens.

This file is the working contract for Codex and other coding agents in this repository.

**Owner delivery clarification, 2026-09-23:** implement real behavior instead of
substituting labels, explanations or disclaimers for missing functionality.
Keep ordinary screens free of unnecessary technical/ownership commentary.
In particular, do not add a "shared save" label as the solution to Trainer save
isolation: deliver the verified per-Trainer save route in the upcoming increment.
Keep unfinished work in ROADMAP; necessary actionable errors and honest
unavailable states must still reflect actual capabilities.

## Read first

Before implementing or changing product behavior, read:

1. `README.md`
2. `docs/PRODUCT_SPEC.md`
3. `docs/UX_NAVIGATION.md`
4. `docs/DESIGN_LANGUAGE.md`
5. `docs/ARCHITECTURE.md`
6. `docs/ARMADA_PLATFORM.md`
7. `docs/DATA_MODEL.md`
8. `docs/ROADMAP.md`

If a task conflicts with these documents, do not silently invent a new direction. Preserve confirmed decisions and document intentional changes.

## Product identity

TrainerOS is a **software-only, controller-first Linux shell/session** for a Pokémon-focused handheld, initially targeting a Retroid Flip-class device running ArmadaOS.

TrainerOS is the **primary user interface of the handheld**, not a secondary desktop app.

Desired lifecycle:

**Power on / wake → TrainerOS → Adventure → TrainerOS**

The underlying KDE Plasma desktop exists as explicit **Desktop / Maintenance Mode** for configuration, recovery, files, package management, and development. It is not part of ordinary use.

TrainerOS is not:

- an Android app
- an APK
- a custom Android launcher
- a custom Linux distribution at this stage
- a replacement for ArmadaOS low-level device support
- a hardware modification
- a generic emulator frontend with a Pokémon skin
- a touch-first or mouse-first desktop app
- a Pokémon library organized by console/platform; the accepted non-Pokémon Multiverse extension is a separate system-organized context inside Worlds

## Confirmed platform decision — do not regress

The former Android/Kotlin/Jetpack Compose plan is retired.

Current platform direction:

- **ArmadaOS is the system base.**
- **TrainerOS is a native Linux graphical shell/session on top of ArmadaOS.**
- **KDE Plasma remains installed and available as maintenance/recovery desktop mode.**
- **Preserve a verified Plasma maintenance/recovery environment.** The owner's 2026-09-13 clarification prefers Plasma Mobile only, if it can replace ordinary Plasma Desktop on Flip. Validate Mobile and rollback first; preserve shared KDE dependencies.
- **Steam retention is transitional.** Accepted issue [#11](https://github.com/EriArk/TrainerOS/issues/11) changes the target to reversible Steam Gaming Mode removal. Keep current working sessions until the reviewed migration and physical restore gates in [ARMADA_PLATFORM.md](docs/ARMADA_PLATFORM.md#planned-session-consolidation--2026-09-13) pass. This supersedes the earlier permanent three-mode requirement; it does not authorize blind package deletion.
- The first prototype must run safely as a normal full-screen application before TrainerOS is made the default session.
- Once crash/recovery behavior is proven, production mode should boot/login into the TrainerOS session by default.
- Exiting supported Adventures should return to TrainerOS.
- Never remove the last verified Plasma maintenance/recovery path merely to create the illusion of a dedicated device. Ordinary Desktop may be retired only after the accepted Mobile replacement is proven.
- Do not fork/rebuild ArmadaOS or create a custom image until the normal-package/session approach has been proven insufficient.

If current ArmadaOS internals differ from assumptions in these docs, adapt the platform integration layer and update the docs. Do not leak distro/session-specific quirks upward into feature code.

## Confirmed product decisions — do not regress

- The user-facing library section is **Worlds**, never “Games”.
- Pokémon Worlds are organized by **region first**, not emulator/platform first. Accepted #28 adds a separate **Multiverse** system browser within Worlds for non-Pokémon titles; it does not add a primary page or fake Pokémon regions. Its transient P1 UI is delivered; real library binding remains planned. [Boundary](docs/MULTIVERSE_UI.md).
- Top-level sections are full-screen peers.
- **Home is one top-level page, not a permanent background shell.**
- `L1/R1` switch top-level pages and must not be repurposed for local features.
- Home is a living trainer overview, not a giant Continue page or tile launcher.
- Planned updated #9: one shell-owned Choose Adventure drawer selects a shared per-Trainer `CurrentPokemonAdventureContext` across Pokémon-aware screens; Worlds retains local search/filter controls. No duplicate persistent Current Adventure capsule/chip. Shared Y and the Pokedex/Center and Hall/RA pairs are delivered. Schema 10 binds owner-isolated personal repositories, real profile creation/selection, TrainerOS RA accounts and optional PIN/startup gating with a parent-defined family reset code. Active-profile removal is delivered; the first proven per-Trainer RetroArch save route is next. Additional domain consumers remain planned. See docs/TRAINER_ACCESS.md. No universal master password; the owner requested simple family use, not enterprise account controls.
- Planned #49 supersedes all normal emulator savestate/ResumePoint creation, management and resume. Use ordinary saves/autosaves and recent Adventure cards with clean exit screenshots. Preserve legacy implementation evidence and migrate user data safely; no new state-resume expansion.
- Selecting a Continue card with A chooses the Adventure shown on Home; it never launches. Home has a large physical-style action button that launches the selected Adventure normally; the game loads its ordinary save/autosave. The explicit Home choice persists; latest launch is the default before a choice is made (2026-09-11 clarification).
- RetroAchievements achievements belong inside Hall of Fame, alongside the completed-Adventure archive, not on a separate primary page.
- The accepted plan is [ROADMAP.md](docs/ROADMAP.md), with [earlier acceptance](docs/EXPANSION_PLAN.md), [#42–62 target acceptance](docs/EXPANSION_42_62.md) and [#63–64 acceptance](docs/EXPANSION_63_64.md). Preserve its earlier steps 1–10, U1–U13 and optional/deferred commitments when replanning. The ROADMAP table controls execution order; P0-P12 are stable work-area IDs, not a numerical schedule; acceptance registers are not competing queues.
- Planned #19–20 introduce separate Trainers/PIN/onboarding: library/installations/shared media are device-wide; personal journal/history/Hall/Home and RA identities are Trainer-scoped. Global RA management belongs in Settings and serves the active Trainer. Shared external saves do not imply separate owned playthroughs.
- Planned #30 explicitly narrows the reference checklist to substantial playable Pokémon titles with credible Flip/controller routes. Preserve the complete eligible collection, missing/linkable editions and meaningful hacks/variants. Catalogue curation never deletes private content/history and remains separate from #18 duplicate-file cleanup.
- Adventure media and Pokédex art have separate identities; manual Caught marks must not fabricate individual Pokémon. New media/audio/haptics/RGB/Steam/boot features require their roadmap capability and recovery gates. Planned capabilities must not be described as already implemented.
- Normal use must work entirely with physical controls.
- Settings/service features should not consume a primary L1/R1 page without an intentional product change.
- Desktop/maintenance access must be explicit.
- No physical modification of Retroid hardware belongs in scope.

## Accepted target extension — 2026-09-19

- #44/#53 Center is a first-class Pokédex companion: practical Party/Storage, safe Heal/Backup/Restore, later #45 durable exact-pair Link Counter. Separate #54 Playroom and #55 read-only practice use proven Party data; no invented individuals or save rewards.
- #46 primary current Pokédex progression follows the selected ordinary save; preserve manual #14 journal/history separately. #47 live Journey and preserved Champion snapshots coexist with manual Hall; #48 RA remains external account truth.
- #51 sprites/portraits and #52 bounded native living-party animation are optional presentation. Classic #13 illustration art remains primary in long Pokédex lists. Keep per-asset attribution/source and no unlicensed redistribution.
- #61 runtime prerequisites remain #58 raw corpus -> #60 real Flip bootstrap/canonical map and measured profiles -> #57 required contract/shared validation. **Owner clarification, 2026-09-20: #59 Qt desktop Pack Studio moves to the very end**, after handheld UI and asset structure stabilize; it no longer blocks Settings polish. Keep early work limited to necessary runtime loading/validation; do not prematurely freeze the final authoring format. ROADMAP owns execution order. No resident downloader; early artwork remains independent from save writers. Classic illustrations remain primary list art; #51 sprites serve detail/scenes.
- #56 larger environmental World cards use original data-driven motifs and intentional small-group diagonal pairs without merging unrelated identities.
- These are accepted/planned changes, not a claim the installed build has implemented them. #62 reconciles docs only. Keep earlier unfinished/deferred tasks and mark explicit supersessions instead of silently dropping them.

## UX invariants

Always preserve:

- `L1/R1` = previous/next top-level section
- `L2/R2` = switch the delivered Pokedex/Center, Hall/RetroAchievements and P1 Worlds/Multiverse presentation pairs; preserve per-face routes and modal priority. Real Multiverse library binding remains planned. B never flips a pair.
- D-pad / left stick = focus navigation
- `A` = confirm/open
- `B` = back/close
- A/B physical shell positions follow Switch: right A confirms, bottom B goes back (owner clarification 2026-09-19). SDL's Xbox positional names are B/east and A/south; translate centrally, not per feature. North X is SDL Y and west Y is SDL X; translate those centrally too. This does not remap emulator gameplay.
- `Start` = TrainerOS system menu
- Physical `Home/Guide` returns to the Home page when no Adventure is running; during a supported Adventure it requests the guarded exit question. It never bypasses an active storage/service gate or directly kills a game.
- P1 #9 `Y` = shared Choose Adventure on Pokémon Home, Pokédex, Center, Hall/RA and Trainer; Worlds local Y and modal/keyboard/system/recovery priority remain. Profile switching now reconstructs these owner views; independent domain consumers remain planned. [Delivered boundary](docs/SHARED_ADVENTURE.md).
- `Select` owns labelled local journal/new-memory/achievement-refresh/save-backup shortcuts.
- Planned #43 `L2/R2` = paired faces Worlds/Multiverse, Pokédex/Center, Hall/Journey/RA; preserve local routes/focus, never use L1/R1 for companions or steal emulator input.
- #31: unobstructed Home `X` toggles Pokémon/Multiverse in the delivered P1 presentation. Durable per-Trainer/domain choices and real Multiverse launch remain P2/P3/P4; A/Y retain selection-versus-launch behavior. Samples never launch or enter the personal library.
- On unobstructed Home, `A` immediately invokes the large Adventure button, regardless of prior D-pad/stick input. `Y` opens the selector; `A` inside it selects for Home without launching.
- Prefer visible page-specific physical-button actions over moving focus between static modules. Directional focus belongs to lists, grids and open selection/editing panels; modal actions take priority over page shortcuts.
- visible deterministic focus whenever interactive content exists
- no required touch/mouse/keyboard for normal use
- no desktop-window metaphors in the normal shell
- no exposed emulator/core commands or configuration jargon in primary UI; requested edition platform badges and Multiverse system names are intentional exceptions

Secondary shortcuts are allowed only if they do not conflict with the above. Keep them remappable where practical.

## Engineering baseline

Preferred baseline:

- C++20
- Qt 6
- QML / Qt Quick
- CMake
- Qt Test and/or lightweight C++ tests where appropriate
- SQLite-backed persistence behind repositories
- Linux process/service/filesystem integration behind explicit platform abstractions

Do not introduce Kotlin, Jetpack Compose, AndroidX, Gradle, Android intents, Room, DataStore, or APK packaging unless a future task explicitly reverses the platform decision.

### Architecture

- Keep feature UI independent from emulator implementations.
- Put emulator-specific behavior behind capability-based Adventure adapters.
- Keep ArmadaOS/session/process/power/network/storage integration behind platform services.
- Model Worlds, Adventures, shared current context, ordinary-save observations, exit media, Trainer progress, Pokédex and Journey/Champion history as domain objects rather than QML-only state. Legacy Resume Points are migration inputs only under #49.
- Persist TrainerOS-owned state locally.
- External game saves/states remain external source data; reference/manage them safely rather than making TrainerOS metadata the sole source of truth.
- Use mock/fake adapters before coupling the first UI milestone to real emulator quirks.
- Avoid embedding distro paths, emulator paths, or shell commands directly in QML.

## Development order — foundations before integrations

The mock is a **prototype and UX probe**, not a frozen design and not disposable architecture.

The user's 2026-09-06 clarification supersedes the earlier top-down sequence. Develop **bottom-up in dependency order**:

1. Native project/build skeleton and module boundaries.
2. Shared interface/controller skeleton: peer pages, reusable panels, focus, drawers, text entry, theme tokens, and mock data.
3. Shared backend foundation: domain models, repository/service contracts, local persistence, and fake providers.
4. Individual functional modules on those foundations, with real integrations added one at a time after their prerequisites work.
5. Optional automatic progress providers only after the library, launch/return, persistence, and relevant feature modules are stable.

**Owner clarification, 2026-09-19:** finish the whole handheld UI/controller skeleton before new save integrations and heavy research. Build future screens on small presentation contracts and explicitly labelled development fixtures, while retaining existing real features and backups. Pokedex illustrations/sprites and #64 game-accurate badge assets belong early with UI; preserve #61's actual Flip/bootstrap/schema gates and asset source/credit requirements. New parsers, save editing/healing, battle-engine research and two-device transactions belong late P8. UI completion does not imply functional feature completion: no fictional progress or enabled unverified writes in production. Full #40 Help stays late P11. The ROADMAP table owns the detailed sequence.

Across all stages:

- keep the shared backend pragmatic; build the foundations required by the planned modules, not speculative infrastructure
- complete each module end-to-end when its turn arrives, rather than spreading unfinished integrations across every feature
- keep module boundaries explicit so integrations, providers, and visual components can be replaced independently
- refactor abstractions when real device/integration behavior disproves mock assumptions
- **visual design remains intentionally fluid**: layout, hierarchy, component shapes, motion, density, and whole compositions may be redesigned repeatedly
- preserve product invariants and controller behavior while allowing aggressive visual iteration
- do not protect mock code or visuals merely because they exist

Feasibility research does not authorize jumping ahead in the roadmap. Implement only verified capabilities; represent unavailable data honestly and do not promise universal save parsing or achievement coverage.

## Controller-first implementation

- Test every screen with actual controller/gamepad events, not only mouse/keyboard.
- Focus order must be intentional and stable.
- Never rely on hover.
- Scrolling must retain a focused item.
- Modal/drawer focus must be trapped and restored correctly.
- `L1/R1` page switching should preserve useful per-page focus/state.
- Input repeat/dead zones should be normalized centrally, not reinvented per feature.

## Session / device-shell behavior

Treat dedicated-device behavior as a product requirement, but add it safely:

1. First run TrainerOS as a normal full-screen Qt app in a development/Plasma session.
2. Prove controller navigation, process launch/return, persistence, crash behavior, and recovery.
3. Add a dedicated TrainerOS session entry.
4. Only then make that session the normal/default device experience.

Production expectations:

- startup/login converges on TrainerOS
- returning from Adventures restores TrainerOS state and focus cleanly
- Plasma panels/window chrome do not appear during normal TrainerOS use
- explicit system-menu action can enter Desktop / Maintenance Mode
- crash/restart paths must leave the user with a recoverable session rather than a black screen
- do not require removing Plasma

Do not assume a specific display manager, compositor, Gamescope arrangement, systemd unit, or ArmadaOS session layout without validating it on the target/current ArmadaOS build.

## Adventure integration

- Do not hard-code the entire application around one emulator.
- Target adapter capabilities include normal launch, process lifecycle, clean exit capture, verified title save policy and ordinary-save resolution/backup. Exact-build providers independently advertise semantic reads and protected writes (#42/#50); reading Party/money does not prove editing support.
- Planned #49 exit captures gameplay before its overlay, asks manual/unknown titles while the game stays alive, cancels back to that same process or confirms graceful exit; verified autosave also asks "Close this game?" (owner clarification, 2026-09-19). Target physical Home/Guide as the request instead of Start+Select after verifying platform interception. Start retains the shell system menu. Crashes never fabricate save confirmation. Prove compositor/input handoff on Flip before claiming support.
- The UI must degrade gracefully when an adapter lacks a capability.
- Process invocation and emulator-specific CLI/environment details belong in adapters, not feature UI.
- Never delete or overwrite a user's save/state silently.
- Prefer backup/copy/verify workflows for destructive operations.

Initial likely adapter targets may include RetroArch, melonDS, Azahar, and Dolphin, but support must be implemented one adapter at a time and validated on the actual ArmadaOS device.

## Content / IP hygiene

Do not commit:

- commercial ROMs
- BIOS/firmware dumps
- encryption keys
- ripped game assets
- proprietary emulator files
- copyrighted Pokémon artwork/audio copied from official media without an appropriate license
- commercial-game save files used as fixtures without permission

Use original placeholders and clearly separated user-provided asset/data paths.

### Authorized ROM source for testing — 2026-09-13

The owner explicitly permits freely selecting, copying and using ROMs from **their ROM disk attached to the server** for TrainerOS development and testing, including non-Pokémon titles and ROM hacks. Do not ask again for permission for each test title/copy/launch from that source. Locate the actual mounted source using the existing private operational notes or a read-only inventory; do not guess a mount path.

Preserve source originals. Stage test copies and test saves/configuration separately from personal playthroughs; keep content and private path/hash manifests outside Git. The owner also requested a later small selection of iconic games for each system; its scope and delivery slot are recorded in [ROADMAP.md](docs/ROADMAP.md#later-collection-deliverable--representative-classics).

## Implementation style

- Keep the visual system custom and cohesive; do not ship Qt default widgets as the product aesthetic.
- Prefer QML/Qt Quick visual components over desktop QWidget-style UI for the shell.
- Prefer a small number of strong reusable components over many one-off widgets.
- Build responsive landscape layouts; do not hard-code a single pixel resolution except inside a device-profile layer when genuinely needed.
- Keep animation short, interruptible, and functional.
- Avoid visual clutter; Pokémon flavor should come from hierarchy, accents, data, motion, and world atmosphere rather than wallpaper overload.
- Treat visual components as replaceable until they survive real-device testing.

## Development workflow — required

### Reasoning effort before each work increment — owner preference, 2026-09-19

Before each new stage/increment, recommend a reasoning-effort level with one short task-specific reason. Prefer announcing the next increment and its level at the end of the current turn, so the owner can change the setting before saying continue. For an unannounced new task, limit initial assessment to what is needed to recommend a level before substantial work. Do not silently start a different major increment in the same run before giving that recommendation.

Use the lowest adequate available level: Low/Light for clear text or small layout edits and routine operations; Medium for ordinary bounded implementation and tests; High for uncertain cross-module behavior, lifecycle, migrations or save integrity. Extra High/Max is exceptional for demonstrated difficult problems, not the default. These are project heuristics, not guaranteed quota savings. Raise or lower the recommendation as the actual uncertainty changes; required verification/data protection still applies at every level. Do not claim to change the owner's model/reasoning setting yourself or infer an exact remaining allowance.

Use the owner's six-position UI labels explicitly: **1/6 Низкое (Low), 2/6 Среднее (Medium), 3/6 Высокое (High), 4/6 Очень высокое (Extra High/xhigh), 5/6 Максимум (Max), 6/6 Ультра (Ultra)**. Recommend the ordinal and Russian label together; High is the third option, not the fourth.

**Pause on a level change - owner correction, 2026-09-19:** if the recommended effort differs from the previously agreed/recommended level, state the new ordinal/label and reason, then stop before substantive work. Wait for the owner to change the setting and explicitly continue; announcing a change is not permission to proceed. Same-level authorized continuation needs no repeated approval. Treat questions as questions: answering a question does not authorize implementation, document edits, commits or deployment. Do not claim to have changed the UI setting yourself.

### Commit and verification loop

**Increment size — owner clarification, 2026-09-19:** prefer a complete visible
feature chain in one work session (backend, UI consumer, checks, device delivery,
commit/push), rather than ending after each small internal mechanism. Keep the
work coherent and dependency-ordered; do not expand into unrelated unfinished
modules. Announce the larger slice and its reasoning level before starting.

The user has authorized routine commits and pushes for this project. A completed implementation increment must be checked, committed, pushed to GitHub and verified there; do not leave finished work only in the working tree or local commits. Do not ask for repeated approval for this routine workflow.

1. Start by inspecting the current branch, status, upstream and relevant issue/docs. Identify pre-existing changes and preserve work belonging to the user or another task.
2. Work in small, coherent increments in the confirmed dependency order. Finish the current slice before spreading changes across later modules. Keep ArmadaOS/device assumptions explicit.
3. Review the actual diff and run checks appropriate to the change. For native behavior, build and run affected tests; shared input/storage/lifecycle changes warrant the broader suite. For QML, exercise SDL controller events and inspect rendered layouts/focus. Documentation-only changes need content/link/diff checks, not unrelated test reruns.
4. Stage reviewed files explicitly. Inspect the staged diff, including new files, and run `git diff --cached --check`. Keep builds, reports, machine-specific helpers, research downloads, credentials and user game data out of Git. Never use an indiscriminate add to bypass review.
5. Commit every completed, validated increment with a concise message explaining the change. Update relevant documentation and acceptance criteria in that increment. Do not accumulate multiple finished stages waiting for a reminder.
6. Fetch before pushing and inspect divergence. Push the intended branch/upstream normally. Preserve remote work; never force-push, rewrite shared history or bypass branch protections as a routine fix. If the repository requires a PR, use its supported branch/PR workflow.
7. Verify the remote branch contains the intended commit. Per the user's 2026-09-13 instruction, GitHub Actions is not a delivery gate: do not poll it, wait for it, or troubleshoot its billing. Use the local Windows toolchain, Linux build server and actual Flip as appropriate to the change. Fix failures in these checks before delivery.
8. Before reporting completion, inspect status and upstream once more. Report the commit/push result, relevant checks and real limitations. State plainly if credentials, network or hardware blocks verification; never imply that an unverified step succeeded.

See `docs/DEVELOPMENT_WORKFLOW.md` for concrete commands and verification boundaries. Task-specific user instructions can override the normal workflow, such as a request to keep a change local or hold a push.

## Documentation discipline

When a task introduces a meaningful product/platform/architecture decision:

- update the relevant doc in the same change
- #40 in-app Help (viewer and product articles) is deferred to P11 until the main flows stabilize, per the owner's 2026-09-19 clarification. Keep engineering docs, runtime-capability facts and recovery notes current now; update matching Help articles once Help exists. Do not document planned behavior as currently available
- add/update acceptance criteria
- keep README high-level; implementation details belong under `docs/`

## Original first implementation target — historical

The original bootstrap is recorded in `docs/CODEX_START.md`; current work follows `docs/ROADMAP.md` and its accepted #42–62 amendments. Do not repeat the completed foundation or revive its superseded state-resume behavior.

The first milestone is a **native Qt/QML controller-navigable full product mock**, not session replacement, distro modification, or deep save-file reverse engineering.

## Definition of done for UI work

**Owner clarification, 2026-09-23:** when a change is visual, capture the installed
build on the actual handheld and show the relevant screenshots in the chat.
Host renders and internal test previews remain useful verification but are not
the requested delivery screenshots. If the device is unavailable, report that
limit explicitly instead of presenting host captures as handheld evidence.

A UI task is not done until:

- it works without touch/mouse
- focus behavior is deterministic
- `L1/R1` top-level navigation still works
- Back behavior is correct
- empty/loading/error states are considered
- emulator/platform jargon does not leak into normal user-facing copy
- the screen reads clearly on the target landscape handheld
- the implementation does not couple QML directly to emulator or ArmadaOS internals
