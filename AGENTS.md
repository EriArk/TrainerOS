# TrainerOS — active agent / developer contract

**Current owner-approved direction: 19 October 2026.** This is the short, **active** entrypoint for Codex and other development agents. The previous 84 KB collection of dated requirements was preserved in full (with relative links rebased) in [historical AGENTS archive](docs/archive/AGENTS_2026-10-14.md). It contains valuable implementation evidence and older decisions, but its superseded page names, priorities and button assignments are **not active instructions**.

## Read before working

1. [Current product and architecture](docs/PRODUCT_ARCHITECTURE_RU.md) — **what TrainerOS is and which components own what**.
2. [Documentation index](docs/README.md) — where to find the appropriate current feature contract, implementation evidence and historical source.
3. [ROADMAP](docs/ROADMAP.md), [CURRENT_TASKS](docs/CURRENT_TASKS.md) and the **specific GitHub issue** — accepted sequence, task scope and outstanding acceptance.
4. [DEVELOPMENT_WORKFLOW](docs/DEVELOPMENT_WORKFLOW.md) and [DEVELOPMENT](docs/DEVELOPMENT.md) — reproducible builds, review, tests, device and publication rules.
5. Read **only relevant** technical docs for the feature. Examples: [ARCHITECTURE](docs/ARCHITECTURE.md), [DATA_MODEL](docs/DATA_MODEL.md), [DESIGN_LANGUAGE](docs/DESIGN_LANGUAGE.md), [ARMADA_PLATFORM](docs/ARMADA_PLATFORM.md). Read [adapter knowledge](docs/adapters/README.md) and [src/adapters/AGENTS.md](src/adapters/AGENTS.md) before adapter/save work.

The long [PRODUCT_VISION_RU](docs/PRODUCT_VISION_RU.md), [PRODUCT_SPEC](docs/PRODUCT_SPEC.md), [UX_NAVIGATION](docs/UX_NAVIGATION.md), [UX_OPTIONS_MAP_RU](docs/UX_OPTIONS_MAP_RU.md), old expansion registers and old [AGENTS](docs/archive/AGENTS_2026-10-14.md) are **dated specifications/history**. They provide retained requirements and evidence, not permission to revert newer owner decisions.

**Conflict rule:** newer explicit owner decision and its current feature issue → [current product map](docs/PRODUCT_ARCHITECTURE_RU.md) → active queue → implementation/evidence of the actual build → historical text. If a recent issue conflicts with the summary, **update the summary**, not the working issue. A newer *target* does not mean it has been implemented. Never silently drop uncompleted acceptance.

## Product identity: an OS-level environment, not a ROM frontend

TrainerOS is a **controller-first native Linux shell / operating environment** with its own UI, games, Social, Apps, files and extensibility. First physical integration is ArmadaOS on supported ARM handhelds, notably Odin 2 / Flip 2; SteamOS/Deck and Bazzite are later target hosts. Do not build a new distro kernel, remove Steam or force ordinary tasks into the KDE maintenance desktop. The source is **not** a publicly released ready-to-flash OS yet.

### Current target navigation

**Exactly five shell primaries:** `Home | Games | GameHub | Social | Apps` (L1/R1).

- **Games:** collections + platforms. **GameHub:** Overview / RetroAchievements / optional game Adapter / universal Lobby (L2/R2).
- **Social:** Messages / Communities / Discover. The social conversation and GameHub Lobby remain distinct, linked surfaces.
- **Apps:** Apps / Crates / Browser / Files. Ordinary Linux apps remain ordinary apps; Crates is a curated opt-in source, not a second package manager.
- **X** globally toggles Quick Apps dock and **Y** globally toggles Recent Games/Favorites in the **shell**; do not steal them in inner pages. Select targets the focused object; Browser/Files toolbar exceptions and long-A context action follow their specific issue contracts.
- Physical **Home** opens **Shell Options** outside gameplay, **Game Options** for the **actual running session**. Selected game, live PID/session and foreground/input owner are different facts.
- Physical **Start** in the shell is the widget dashboard **QuickActions / Extensions**. Native Start during unmodified gameplay belongs to the game. Home → Game Options → Start opens the same dashboard in-game. Shell Settings/Power/launcher icons belong to Shell Options, **not** Game Options.
- Opening Home alone is not auto-pause, auto-minimize or Exit. Preserve explicit Minimize/Return, input leases, neutral barriers, a predictable focus return and protected exit/save workflow.
- The **only** ordinary multiplayer destination is **GameHub Lobby**. Special game-specific semantic interactions can remain in the Adapter; do not create a parallel Social/GameParty or NetPlay/Remote Play UI.

**Source of truth for these targets:** #111, #168, #170, #179, #206, #207, #216, #224, #227, #233, #234 and their accepted later amendments. Some targets are **not in main yet**. Do not label the old main UI as a completed new design.

## Architecture & engineering rules

- C++20 / Qt Quick / QML / CMake, SQLite behind repositories, platform and emulator behaviors behind service boundaries. Keep the normal UI user-oriented; no distro commands or save offsets in QML.
- **Reuse code and real providers** before introducing abstractions: existing Fluxer worker/Social, Lobby, emulator NetPlay, remote-play transport, Files/File Picker, global Downloads, launch/exit service, device integrations. Avoid second backends, duplicate state ownership and speculative plugin microkernels.
- **Game-specific Experience modules** reside under `src/adapters/<id>/`, composed via `src/core/experience/` rather than scattered title switches in shell/QML. An adapter's presentation compatibility does **not** grant exact-save write authority.
- **Ordinary local games work offline without semantic adapters.** ROM/save/BIOS contents, keys, personal media, private research and credentials must never be committed or silently redistributed. Protect exact-game save writes through existing owner, build, source-revision, backup, atomic replacement and readback gates; no silent mutation while a game owns the save.
- Keep game process, conversation, call, Lobby and Downloads lifecycles independent where the product requires it. Avoid fake support, silent placeholder success or equating passed unit tests with physical player acceptance.
- Device host capabilities are **reported and gated**, not guessed. Respect gamepad, touch, handheld/TV layout, stable focus and short purposeful animations; never make normal use require a mouse/keyboard.
- Prefer fixing a known scope end-to-end rather than replacing stable components with fashionable technology. Respect settled architecture and reuse passed tests unless the change directly affects them.

### Platform SDK / Crates — approved foundation, not shipped API

[#235 — TrainerOS Platform SDK & Network Fabric](https://github.com/EriArk/TrainerOS/issues/235) and #236–244 define optional service/contribution contracts for games, apps, media/file providers, tools and system modules. They extend existing #231 (system module SPI), #92/#207 (semantic adapters), #179/#216 (Lobby), #233 (Start), #229 (Files) and #144–152 (Crates); **do not implement parallel systems**.

**Freedom by default:** no mandatory new account, special SDK approval, Crates publication, custom server, large manifest or blanket permission prompt just to run an ordinary Linux program, access one's own data/network or show a normal approved widget. Check real trust boundaries: protected saves, other profiles/private messages, voice/capture, device privilege and irreversible operations. Host owns trusted surfaces; untrusted downloaded QML/native code cannot arbitrarily execute inside shell.

**Network Fabric:** LAN/nearby/direct P2P, existing Fluxer identity/invitations/signaling and **only genuinely supported** provider messaging/relay as selected by workload. [#105](https://github.com/EriArk/TrainerOS/issues/105) owns Fluxer transport research. Do not assume a voice/video connection provides unlimited arbitrary realtime game packets, guarantee connection through difficult NAT or force a centralized packet proxy into each frame. Distinguish low-rate semantic events and realtime gameplay. The system should make a tiny Tetris/Tanks app easy to integrate.

No new SDK epic/child task becomes a blocker for the first early public beta merely because it exists. Follow the active agreed release queue.

## Development delivery loop

1. Confirm task and accepted scope. Questions, informal brainstorming and proposals **do not authorize** new code/issues/repo edits. When explicitly authorized, work on a coherent outcome without repeatedly asking permission for ordinary commit/push.
2. Inspect branch/HEAD, repository state, active issues and relevant **existing** implementation; preserve unrelated user changes. When using a remote GitHub-only editor, check the current file SHA, update carefully and verify the resulting remote content/commit.
3. Implement the **smallest complete usable path**. Preserve existing UI state, saved data, input and backward compatibility. A test/migration/protection is part of that feature, not a separate invented milestone.
4. Verify appropriately: native build/focused tests for code; broader regression for shared input/storage/process behavior; real controller+touch and device screenshots for UI. A mock or local host test **is not proof** of functioning Odin/Flip hardware. Documentation-only changes need reviewed text, links and diff, **not unrelated device deployments**.
5. Update relevant current feature docs, actual evidence and acceptance; for meaningful delivered changes/decisions also update [PROJECT_STATUS](PROJECT_STATUS.md) and [CHANGELOG](CHANGELOG.md), **without rewriting historical dates or reporting planned features as installed**.
6. Commit and push the inspected work; verify the final remote commit. GitHub Actions are deliberately disabled as a release gate — do not recreate workflows or await billing/CI. Report what changed, what was checked and which acceptance conditions remain.
7. If real hardware, extra accounts/networks or a user decision are required, record the precise remaining dependency, complete safe independent work, then stop; don't manufacture tests to appear busy.

Never force-push shared history, destroy others' work, suppress failing tests or expose secret account data, ROMs/BIOS, private saves, copyrighted assets or debug support bundles.

## Documentation discipline

- **README.md is a human-facing product showcase**, not architecture reference, TV advertising copy or a list of internal component names.
- Prefer **one clear active contract per responsibility**, with links from the index and short supersession banners on older still-linked texts. Preserve technical evidence and old build dates in their historical documents.
- Keep **target, source implementation, installed device build, real test and remaining work** separate. A closed issue or screenshot does not prove general compatibility.
- The current product/SDK map is short; feature issues specify behavior; code docs explain *how*. Do not recreate 100 KB specifications or copy the same policy into 20 issue bodies.
- Validate local Markdown links and actual GitHub issue paths where touched. Older literal terminology such as Worlds, OddCrate, Companions/Trainer primaries or an old Start menu must be clearly dated or corrected when presented as the **current target**.

**Historical appendix:** the full original agent memo, including early Android retirement, Pokémon-first UX, deprecated priorities, detailed per-game research and individual hardware reports, remains available at [docs/archive/AGENTS_2026-10-14.md](docs/archive/AGENTS_2026-10-14.md). It is evidence/history rather than an alternate current plan.
