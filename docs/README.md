# TrainerOS documentation

> **Read [the current product & architecture map (Russian)](PRODUCT_ARCHITECTURE_RU.md) first.** It records the owner-approved direction as of **19 October 2026**: TrainerOS is a game-first operating environment; five root spaces are **Home / Games / GameHub / Social / Apps**, with Start, system/game Options, Crates and the planned Platform SDK/Network Fabric. **Target ≠ shipped.** The main-branch build still has some earlier UI structure; do not infer implementation from issues or mockups.

## Pick the right source

| Question | Read |
| --- | --- |
| **What is TrainerOS supposed to become?** | [Current product architecture](PRODUCT_ARCHITECTURE_RU.md), then the relevant [GitHub feature issue](https://github.com/EriArk/TrainerOS/issues) |
| **What is installed and genuinely verified?** | [PROJECT_STATUS](../PROJECT_STATUS.md), [CHANGELOG](../CHANGELOG.md), and dated feature/device evidence |
| **What should be worked on next?** | [ROADMAP](ROADMAP.md) plus [CURRENT_TASKS](CURRENT_TASKS.md) and accepted current issue scope; a historical "next" is not today's priority |
| **How should Codex/developers work?** | [AGENTS](../AGENTS.md), [DEVELOPMENT_WORKFLOW](DEVELOPMENT_WORKFLOW.md), [DEVELOPMENT](DEVELOPMENT.md), [CONTRIBUTING](../CONTRIBUTING.md) |
| **How is the code organized?** | [ARCHITECTURE](ARCHITECTURE.md), [DATA_MODEL](DATA_MODEL.md), [LOCAL_PERSISTENCE](LOCAL_PERSISTENCE.md), [DESIGN_LANGUAGE](DESIGN_LANGUAGE.md) |
| **What was agreed earlier?** | Historical [product vision](PRODUCT_VISION_RU.md), [PRODUCT_SPEC](PRODUCT_SPEC.md), [UX_NAVIGATION](UX_NAVIGATION.md), [UX_OPTIONS_MAP](UX_OPTIONS_MAP_RU.md), expansion registers, [original Codex brief](CODEX_START.md) |

[Documentation audit and remaining reconciliation plan — 19 October 2026](DOCUMENTATION_AUDIT_20261019_RU.md) records the completed inventory, safe cleanup and items that must wait for actual code/device delivery.

**Authority / conflict rule:** explicit later owner decision and the current issue contract win over earlier target text; [the current architecture map](PRODUCT_ARCHITECTURE_RU.md) summarizes those decisions. Code and evidence, not target language, determine what is actually implemented. Do not silently discard still-open acceptance when old UI wording is superseded.

## Feature references (current owner + technical history)

### Shell, input and applications

- **Navigation / overlays / lifecycle:** [#111](https://github.com/EriArk/TrainerOS/issues/111), [#206](https://github.com/EriArk/TrainerOS/issues/206), [#168](https://github.com/EriArk/TrainerOS/issues/168), [#170](https://github.com/EriArk/TrainerOS/issues/170), [#171](https://github.com/EriArk/TrainerOS/issues/171), [#227](https://github.com/EriArk/TrainerOS/issues/227); historical [UX Framework](UX_FRAMEWORK.md), [Home menu](HOME_MENU.md), [UX navigation](UX_NAVIGATION.md).
- **Apps → Apps / Crates / Browser / Files:** [#224](https://github.com/EriArk/TrainerOS/issues/224), [#144–152](https://github.com/EriArk/TrainerOS/issues/144), [#229](https://github.com/EriArk/TrainerOS/issues/229), [#230](https://github.com/EriArk/TrainerOS/issues/230). **Start QuickActions/Extensions:** [#233](https://github.com/EriArk/TrainerOS/issues/233). **Settings:** [#234](https://github.com/EriArk/TrainerOS/issues/234). These recent UI changes are primarily **target** specs, not shipped screens.
- **Platform SDK and peer network (approved target):** [#235 epic](https://github.com/EriArk/TrainerOS/issues/235), [#236–244](https://github.com/EriArk/TrainerOS/issues/236), existing system modules [#231](https://github.com/EriArk/TrainerOS/issues/231), semantic adapter packaging [#92](https://github.com/EriArk/TrainerOS/issues/92). Developer simplicity, normal Linux freedom and protection only at genuine trust boundaries are explicit requirements.

### Games, data, multiplayer and Social

- [Library / collections](SERIES_COLLECTIONS.md), [ROM platforms](ROM_PLATFORMS.md), [emulators](emulators/README.md), [ScreenScraper](SCREENSCRAPER.md), [game progress](GAME_PROGRESS.md), [adapter research](adapters/README.md).
- **GameHub adapters** [#207](https://github.com/EriArk/TrainerOS/issues/207); existing code / adapter evidence [UX_FRAMEWORK](UX_FRAMEWORK.md), [ADAPTER_AUDIT](ADAPTER_AUDIT.md).
- **Social** [#128](https://github.com/EriArk/TrainerOS/issues/128), [#217–223](https://github.com/EriArk/TrainerOS/issues/217); integration/evidence [SOCIAL](SOCIAL.md), [SOCIAL_UX](SOCIAL_UX.md), [Fluxer capability audit](FLUXER_CAPABILITIES_RU.md).
- **One GameHub Lobby and normal multiplayer** [#179](https://github.com/EriArk/TrainerOS/issues/179), [#216](https://github.com/EriArk/TrainerOS/issues/216), [LOBBY_EXECUTION_PLAN_RU](LOBBY_EXECUTION_PLAN_RU.md), [REMOTE_PLAY_LOBBY](REMOTE_PLAY_LOBBY.md), [multiplayer working matrix](MULTIPLAYER_WORKING_MATRIX.md).
- **Fluxer packet/data relay is conditional, not assumed:** existing [#105 transport research](https://github.com/EriArk/TrainerOS/issues/105), then [#238 Network Fabric](https://github.com/EriArk/TrainerOS/issues/238).

### Server-side infrastructure (planning, not shipped)

- **[TrainerOS Backend plan](TRAINEROS_BACKEND_PLAN_RU.md)** — one optional VPS for the existing Fluxer bot and potential shared services: event/API intake, scores, tournaments, rewards, clans, operations and scale. This plan does **not** move gameplay, voice, video or saves to the server.
- **[Hosted Crates data & install recipes](CRATES_SERVER_STORAGE_RU.md)** — small signed/verified catalog metadata, revisions, install recipes and compatible configuration; **GitHub Releases remains the binary/package host**. Existing implementation owners: [#144](https://github.com/EriArk/TrainerOS/issues/144), [#146](https://github.com/EriArk/TrainerOS/issues/146), [#147](https://github.com/EriArk/TrainerOS/issues/147), [#148](https://github.com/EriArk/TrainerOS/issues/148).

### Device and delivery

- [Armada platform](ARMADA_PLATFORM.md), [device baseline](ARMADA_DEVICE_BASELINE.md), [first run](FIRST_RUN.md), [native build guide](DEVELOPMENT.md), [public readiness](PUBLIC_READINESS.md), [security](../SECURITY.md), [licensing](../LICENSING.md), [third-party notices](../THIRD_PARTY_NOTICES.md).

## How to maintain these documents

1. Change the **current owner** of a decision (issue / canonical product map) and the **affected implementation doc**; add short supersession pointers to older active docs instead of copy-pasting an entire new spec across them.
2. Keep **TARGET**, **CODE**, **INSTALLED**, **TESTED** and **OPEN** separate. Record device name, build/commit and test boundary when claiming evidence. Planning-only docs do not imply shipped behavior.
3. Preserve historical reports, release/evidence chronology and older decisions as dated history. Do not erase contradictory old paragraphs without leaving a clear migration pointer.
4. Update [PROJECT_STATUS](../PROJECT_STATUS.md) and [CHANGELOG](../CHANGELOG.md) with *meaningful delivered changes or accepted scope changes*, but don't rewrite old dates to look like new checks.
5. Check affected relative Markdown links/anchors and issue references. Documentation-only tasks need document/link/diff review, not unrelated full builds or handheld redeployment.

Older [bootstrap guide](archive/DEVELOPMENT_BOOTSTRAP.md) and [Codex Start](CODEX_START.md) are reference history, **never the active execution queue**.
