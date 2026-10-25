# TrainerOS — active agent / developer contract

**Current owner-approved direction: 21 October 2026.** This is the short, **active** entrypoint for Codex and other development agents. The previous 84 KB collection of dated requirements was preserved in full (with relative links rebased) in [historical AGENTS archive](docs/archive/AGENTS_2026-10-14.md). It contains valuable implementation evidence and older decisions, but its superseded page names, priorities and button assignments are **not active instructions**.

## Current working mode — owner correction, 21 October 2026

**Owner Backend correction, 25 October 2026:** prepare the complete shared
server architecture before implementing/polishing individual working scenarios.
Use [Backend architecture v1](docs/TRAINEROS_BACKEND_PLAN_RU.md#architecture-v1):
all module/data/API/SDK/trust/operations boundaries, not a golf-first vertical
slice. Golf is a later integration example. This is a scoped Backend instruction,
not a restored global execution queue. **Later owner correction, 25 October:**
implement, build and test the Backend directly on the existing server; do not
accumulate local databases, downloaded runtimes or build environments. Server
installation is now authorized in this chat. Keep the live Lobby bot independent;
handheld tests and GitHub publication remain outside this increment. Architecture
is not runtime implementation or server acceptance.

**Owner account clarification, 25 October:** sign in through Fluxer, with no
separate TrainerOS username/password. A stable internal TrainerOS ID binds to
the verified Fluxer subject, never its display name. Use a separate OAuth app
named TrainerOS, owned by the owner's permanent account; preserve the existing
Lobby Directory app. Keep provider credentials outside games/SDK. Server source
and isolated provider tests do not prove live owner consent or shell integration.

Global plans, milestones and mandatory execution queues are withdrawn. Work
by the readiness of individual features, using [the readiness map](docs/FEATURE_READINESS.md).
Keep unfinished requirements and unchecked acceptance; there is no deferred category.
Older sequencing in issues, plans and reports is historical, not an active queue.
Record real dependencies beside the feature. Do not create a replacement global plan.
The owner made both handhelds available again on 21 October 2026. Connections,
bounded runtime checks and delivery to Odin/Flip are authorized. Preserve each
device's profiles/saves/settings; physical and different-network acceptance remain separate.

**Owner structural map, 21 October 2026:** use [STRUCTURE_MAP_RU](docs/STRUCTURE_MAP_RU.md)
as the canonical map of console-owned responsibilities, package roles and cross-system
contracts. Each readiness row links its primary owner and affected systems. Before a
change, identify the owner, entry/exit contract, consumers and lifecycle consequences;
use the map's F01–F12 end-to-end flows and reverse impact table to trace affected paths;
reuse its current implementation. Trace issue coverage through
[REQUIREMENTS_COVERAGE_RU](docs/REQUIREMENTS_COVERAGE_RU.md) and stable readiness R-IDs;
retain superseded decisions separately from unmet requirements. Update the map when ownership/contracts change,
and readiness/evidence when implementation or checks change. This is not a global
execution plan or an instruction to create new managers/microservices. Public SDK,
backend targets and installed built-ins remain distinct evidence levels.

## Read before working

**Owner library size correction, 21 October 2026:** exclude Xbox 360 and PS3
from the current handheld copy and final matched libraries. Stop their transfers
and remove only this job's copied/partial payloads; preserve source files and
personal saves. The active selection is now 314 games across 24 families; the
original 321-entry request remains historical evidence, not the current copy plan.
The owner cancelled the lengthy full SD checksum pass: successful transfer,
file presence and expected sizes are sufficient for this delivery. Do not restart
full disk hashing. The completed old-game comparison is retained; both Sacred
installer copies and the matched library/12 genre collections are now applied.
The owner additionally requested thematic genre covers on both devices. Preserve
the installed library composition and each owner's saves/history while applying art.

1. [Current product](docs/PRODUCT_ARCHITECTURE_RU.md) and [structural map](docs/STRUCTURE_MAP_RU.md) — **what TrainerOS is, which component owns each function, and how they connect**.
2. [Documentation index](docs/README.md) — where to find the appropriate current feature contract, implementation evidence and historical source.
3. [Feature readiness](docs/FEATURE_READINESS.md), [CURRENT_TASKS](docs/CURRENT_TASKS.md) and the **specific GitHub issue** — feature state, task scope and outstanding acceptance.
4. [DEVELOPMENT_WORKFLOW](docs/DEVELOPMENT_WORKFLOW.md) and [DEVELOPMENT](docs/DEVELOPMENT.md) — reproducible builds, review, tests, device and publication rules.
5. Read **only relevant** technical docs for the feature. Examples: [ARCHITECTURE](docs/ARCHITECTURE.md), [DATA_MODEL](docs/DATA_MODEL.md), [DESIGN_LANGUAGE](docs/DESIGN_LANGUAGE.md), [ARMADA_PLATFORM](docs/ARMADA_PLATFORM.md). Read [adapter knowledge](docs/adapters/README.md) and [src/adapters/AGENTS.md](src/adapters/AGENTS.md) before adapter/save work.

The short [PRODUCT_SPEC](docs/PRODUCT_SPEC.md) records accepted screen decisions. Removed long UX/expansion documents remain in Git history; the old [AGENTS](docs/archive/AGENTS_2026-10-14.md) is **dated history**. They provide retained requirements and evidence, not permission to revert newer owner decisions.

**Conflict rule:** newer explicit owner decision and its current feature issue → [current product map](docs/PRODUCT_ARCHITECTURE_RU.md) → feature readiness and dependencies → implementation/evidence of the actual build → historical text. If a recent issue conflicts with the summary, **update the summary**, not the working issue. A newer *target* does not mean it has been implemented. Never silently drop uncompleted acceptance.

## Product identity: an OS-level environment, not a ROM frontend

TrainerOS is a **controller-first native Linux shell / operating environment** with its own UI, games, Social, Apps, files and extensibility. First physical integration is ArmadaOS on supported ARM handhelds, notably Odin 2 / Flip 2; SteamOS/Deck and Bazzite are later target hosts. Do not build a new distro kernel, remove Steam or force ordinary tasks into the KDE maintenance desktop. The source is **not** a publicly released ready-to-flash OS yet.

### Current target navigation

**Exactly five shell primaries:** `Home | Games | GameHub | Social | Apps` (L1/R1).

- **Games:** collections + platforms. **GameHub:** My Story across the library / RetroAchievements / optional game Adapter / universal Lobby (L2/R2), including Timeline and Gallery.
- **Social:** Messages / Communities / Discover. The social conversation and GameHub Lobby remain distinct, linked surfaces.
- **Apps:** Apps / Crates / Browser / Files. Ordinary Linux apps remain ordinary apps; Crates is a curated opt-in source, not a second package manager.
- **X** globally toggles Quick Apps dock and **Y** globally toggles Recent Games/Favorites in the **shell**; do not steal them in inner pages. Select targets the focused object; Browser/Files toolbar exceptions and long-A context action follow their specific issue contracts.
- Physical **Home** opens the current-activity menu (game, notifications and game settings); gameplay controls belong to the **actual running session**. Selected game, live PID/session and foreground/input owner are different facts.
- Physical **Start** in the shell is the unified widget dashboard with quick actions, Settings and Power. Native Start during unmodified gameplay belongs to the game. Home → Game Options → Start opens the same dashboard in-game. Game settings belong to Home; system Settings/Power belong to Start.
- Opening Home alone is not auto-pause, auto-minimize or Exit. Preserve explicit Minimize/Return, input leases, neutral barriers, a predictable focus return and protected exit/save workflow.
- The **only** ordinary multiplayer destination is **GameHub Lobby**. Special game-specific semantic interactions can remain in the Adapter; do not create a parallel Social/GameParty or NetPlay/Remote Play UI.

**Source of truth for these targets:** #111, #168, #170, #179, #206, #207, #216, #224, #227, #233, #234 and their accepted later amendments. Delivery is recorded by build/date in PROJECT_STATUS; SDK targets are not installed API claims.

## Current owner constraints

- Picker #252–254 and PC #246–251 retain their unfinished criteria in the readiness map. The completed [library delivery](docs/MULTIPLAYER_SELECTION_DELIVERY.md) is evidence, not a new transfer task.
- Earlier delivery scope, retained as requirements rather than execution order: PC Home/exit without repeating passed checks. Make the two visible libraries identical: the approved selection plus old games confirmed present on both handhelds; copy both supplied Sacred distributions. Replace the old collection views with genre collections while keeping each owner's saves, profile and history. Remove the temporary PC input fixture, its library cards and temporary folders from both devices at the end. Copy verification is not installer integrity or gameplay compatibility.
- Keep one live room and separate scheduled rooms. All Ready enables the host's explicit Start; never auto-launch. Offline Lobby, private PSP and the full pass-controller catalogue remain unfinished features in the readiness map.
- Couch means independent simultaneous pads. Do not manufacture PS2 couch networking with two local game copies. Reuse upstream networking and existing Fluxer signaling.
- First beta is an installer over Armada; images and SteamOS/Bazzite come later. SDK/backend/account targets do not automatically block it.
- Preserve both devices' own profiles/saves/settings. The owner permits removing Steam games and other ROMs when needed for the agreed selection; preserve server originals and record exact removals privately.
- Do not test a sleep cycle: no suspend/resume, Power short-press or lid-close test. Prior Power implementation and untested sleep acceptance remain separate.

## Architecture & engineering rules

- C++20 / Qt Quick / QML / CMake, SQLite behind repositories, platform and emulator behaviors behind service boundaries. Keep the normal UI user-oriented; no distro commands or save offsets in QML.
- **Reuse code and real providers** before introducing abstractions: existing Fluxer worker/Social, Lobby, emulator NetPlay, remote-play transport, Files/File Picker, global Downloads, launch/exit service, device integrations. Avoid second backends, duplicate state ownership and speculative plugin microkernels.
- **Game-specific Experience modules** reside under `src/adapters/<id>/`, composed via `src/core/experience/` rather than scattered title switches in shell/QML. An adapter's presentation compatibility does **not** grant exact-save write authority.
- **Ordinary local games work offline without semantic adapters.** ROM/save/BIOS contents, keys, personal media, private research and credentials must never be committed or silently redistributed. Protect exact-game save writes through existing owner, build, source-revision, backup, atomic replacement and readback gates; no silent mutation while a game owns the save.
- Keep game process, conversation, call, Lobby and Downloads lifecycles independent where the product requires it. Avoid fake support, silent placeholder success or equating passed unit tests with physical player acceptance.
- Device host capabilities are **reported and gated**, not guessed. Respect gamepad, touch, handheld/TV layout, stable focus and short purposeful animations; never make normal use require a mouse/keyboard.
- Prefer fixing a known scope end-to-end rather than replacing stable components with fashionable technology. Respect settled architecture and reuse passed tests unless the change directly affects them.

### Platform SDK / Crates — bounded local API, wider contracts remain targets

Local protocol v1 preview now implements discovery, real context/events and a
host-owned text widget with callback/cleanup; see [author entry](sdk/README.md)
and [evidence/remaining contracts](docs/PLATFORM_SDK.md). Wider SDK/SPI targets
and handheld delivery remain separate. Extend actual owners through this bridge,
not placeholder successful APIs.

Native activity / two-peer reliable record integration now uses the existing
LobbyRuntime and secure record helper; see [the app contract](sdk/NETWORK.md).
Keep local TLS/process evidence separate from live Fluxer accounts, internet/NAT,
offline admission, unreliable channels, five-player and handheld acceptance.

[#235 — TrainerOS Platform SDK & Network Fabric](https://github.com/EriArk/TrainerOS/issues/235) and #236–244 define optional service/contribution contracts for games, apps, media/file providers, tools and system modules. They extend existing #231 (system module SPI), #92/#207 (semantic adapters), #179/#216 (Lobby), #233 (Start), #229 (Files) and #144–152 (Crates); **do not implement parallel systems**.

**Freedom by default:** no mandatory new account, special SDK approval, Crates publication, custom server, large manifest or blanket permission prompt just to run an ordinary Linux program, access one's own data/network or show a normal approved widget. Check real trust boundaries: protected saves, other profiles/private messages, voice/capture, device privilege and irreversible operations. Host owns trusted surfaces; untrusted downloaded QML/native code cannot arbitrarily execute inside shell.

**Network Fabric:** LAN/nearby/direct P2P, existing Fluxer identity/invitations/signaling and **only genuinely supported** provider messaging/relay as selected by workload. [#105](https://github.com/EriArk/TrainerOS/issues/105) owns Fluxer transport research. Do not assume a voice/video connection provides unlimited arbitrary realtime game packets, guarantee connection through difficult NAT or force a centralized packet proxy into each frame. Distinguish low-rate semantic events and realtime gameplay. The system should make a tiny Tetris/Tanks app easy to integrate.

No new SDK epic/child task becomes a blocker for the first early public beta merely because it exists. Keep each release requirement and its actual dependencies in the readiness map.

## Retained owner delivery preferences

- For a **new substantial work increment**, announce the intended user-visible slice and a suitable reasoning-effort recommendation before beginning. Preserve the owner's earlier six-position scheme when explicitly using it: **1/6 Низкое, 2/6 Среднее, 3/6 Высокое, 4/6 Очень высокое, 5/6 Максимум, 6/6 Ультра**. If changing the previously agreed level, explain briefly and wait for the owner's continuation. Do not turn a scope discussion into implementation permission.
- Prefer a **complete useful feature chain** (provider → UI → relevant tests → device → commit) to a pile of isolated internal microtasks.
- When deploying an actual build, update **both Flip 2 and Odin 2 when reachable**, preserve separate user state/boot/session preferences, and report deferred devices. Never claim Windows/Linux host tests as device proof.
- Game/save adapter edits also follow [adapter knowledge](docs/adapters/README.md): update factual per-title evidence and run `tools/export-game-adapters.py` / `tools/check-adapter-knowledge.py` as applicable.
- Respect original [GPL and third-party code/art license boundaries](LICENSING.md), and keep screenshots/reports free of private accounts, downloaded ROMs, saves, keys and unapproved artwork.

## Development delivery loop

1. Confirm task and accepted scope. Questions, informal brainstorming and proposals **do not authorize** new code/issues/repo edits. When explicitly authorized, work on a coherent outcome without repeatedly asking permission for ordinary commit/push.
2. Inspect branch/HEAD, repository state, active issues and relevant **existing** implementation; preserve unrelated user changes. When using a remote GitHub-only editor, check the current file SHA, update carefully and verify the resulting remote content/commit.
3. Implement the **smallest complete usable path**. Preserve existing UI state, saved data, input and backward compatibility. A test/migration/protection is part of that feature, not a separate invented milestone.
4. Verify appropriately: native build/focused tests for code; broader regression for shared input/storage/process behavior; real controller+touch and device screenshots for UI. A mock or local host test **is not proof** of functioning Odin/Flip hardware. Documentation-only changes need reviewed text, links and diff, **not unrelated device deployments**.
5. Update relevant current feature docs, actual evidence and acceptance; for meaningful delivered changes/decisions also update [PROJECT_STATUS](PROJECT_STATUS.md) and [CHANGELOG](CHANGELOG.md), **without rewriting historical dates or reporting planned features as installed**.
6. Commit and push the inspected work; verify the final remote commit. GitHub Actions are deliberately disabled as a release gate — do not recreate workflows or await billing/CI. Report what changed, what was checked and which acceptance conditions remain.
7. If real hardware, extra accounts/networks or a user decision are required, record the precise remaining dependency, complete safe independent work, then stop; don't manufacture tests to appear busy.

Owner reporting preference: end every final report with an explicit
**Доделано / Недоделано** status for the requested scope and what comes next.

Never force-push shared history, destroy others' work, suppress failing tests or expose secret account data, ROMs/BIOS, private saves, copyrighted assets or debug support bundles.

## Documentation discipline

- **README.md is a human-facing product showcase**, not architecture reference, TV advertising copy or a list of internal component names.
- Prefer **one clear active contract per responsibility**, with links from the index and short supersession banners on older still-linked texts. Preserve technical evidence and old build dates in their historical documents.
- Keep **target, source implementation, installed device build, real test and remaining work** separate. A closed issue or screenshot does not prove general compatibility.
- The current product/SDK map is short; feature issues specify behavior; code docs explain *how*. Do not recreate 100 KB specifications or copy the same policy into 20 issue bodies.
- Validate local Markdown links and actual GitHub issue paths where touched. Older literal terminology such as Worlds, OddCrate, Companions/Trainer primaries or an old Start menu must be clearly dated or corrected when presented as the **current target**.

**Historical appendix:** the full original agent memo, including early Android retirement, Pokémon-first UX, deprecated priorities, detailed per-game research and individual hardware reports, remains available at [docs/archive/AGENTS_2026-10-14.md](docs/archive/AGENTS_2026-10-14.md). It is evidence/history rather than an alternate current plan.
