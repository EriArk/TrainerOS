# Documentation

**Final product:** [Complete product vision (Russian)](PRODUCT_VISION_RU.md)
describes the end-to-end target, detailed behaviour, safeguards, failure handling,
acceptance and issue coverage. [Issues #136-160](EXPANSION_136_160.md) reconcile guest
content, OddCrate, independent updates and TV. Targets and installed evidence are
explicitly separate; [ROADMAP](ROADMAP.md) retains the active execution order.


Start with the [project overview](../README.md) and [native build guide](DEVELOPMENT.md).
TrainerOS is under active development. Dated device reports describe what was
observed on those builds; they are not universal compatibility promises.

## Work on the project

- [Contributing](../CONTRIBUTING.md) — scope, changes, verification and issues.
- [Development](DEVELOPMENT.md) — dependencies, isolated build/run and checks.
- [Engineering workflow](DEVELOPMENT_WORKFLOW.md) — complete delivery and evidence.
- [Roadmap](ROADMAP.md) — accepted order; [current tasks](CURRENT_TASKS.md) — remaining acceptance.
- [Issues #118–135](EXPANSION_118_135.md) — installer/shared release, live Hotseat and Social/Together reconciliation.
- [Architecture](ARCHITECTURE.md) and [data model](DATA_MODEL.md) — boundaries and identity.
- [Security reporting](../SECURITY.md), [code license and scope](../LICENSING.md)
  and [third-party inventory](../THIRD_PARTY_NOTICES.md).

## Understand current features

- [Series collections](SERIES_COLLECTIONS.md), [ROM platforms](ROM_PLATFORMS.md),
  [standard emulators](EMULATOR_STANDARD.md) and [emulator maintenance](emulators/README.md).
- [Primary navigation](NAVIGATION_111.md), [physical Home menu](HOME_MENU.md),
  [first run](FIRST_RUN.md) and [startup-to-play acceptance](STARTUP_EXPERIENCE_AUDIT.md).
- [Social](SOCIAL.md), [compact interface and profiles](SOCIAL_COMPACT.md),
  [calls and notifications](SOCIAL_MEDIA_VOICE.md),
  [multiplayer experience](MULTIPLAYER_EXPERIENCE.md),
  [classic RetroArch profiles](RETROARCH_MULTIPLAYER_PROFILES.md) and
  [independent handheld link](HANDHELD_MULTIPLAYER.md).
- [Game-adapter knowledge](adapters/README.md) and [Emerald Link](EMERALD_LINK.md).
- [Public-readiness audit and follow-ups](PUBLIC_READINESS.md).
- [User agreement draft (Russian)](USER_AGREEMENT_DRAFT_RU.md) — proposed content responsibilities; not effective service terms.

## Reading historical records

Owner decisions and the active roadmap supersede older target descriptions.
Feature documents often contain dated checkpoints: distinguish implemented work,
observed behavior, and acceptance still waiting on hardware or human testing.
The [bootstrap development guide](archive/DEVELOPMENT_BOOTSTRAP.md) and
[original Codex bootstrap brief](CODEX_START.md) are historical, not onboarding.
Older acceptance remains open unless explicitly delivered or superseded; a new
document does not silently cancel it. Pack Studio and optional ROM-native assets
precede image/installer delivery, then updates/repair and final acceptance.
