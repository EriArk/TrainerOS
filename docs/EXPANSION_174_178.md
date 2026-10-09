# Issues #174–178 and adaptive Home — 9 October 2026

**Implementation update, 9 October 2026:** the owner subsequently authorized the
UX-02 implementation. Current behavior, handheld delivery and retained acceptance
are recorded in [UX_FRAMEWORK](UX_FRAMEWORK.md). Dated planning-only and compact-menu
statements below describe the prior baseline.

Live issues were reviewed on 9 October. The owner's subsequent instruction adds
**adaptive Home** and sets the order: document the complete contract, then repair
the shared framework/UI/controls, then resume multiplayer and the retained queue.
This pass updates documentation only. No new experience, UI or runtime is installed.
[UX map](UX_OPTIONS_MAP_RU.md) owns interactions; [ROADMAP](ROADMAP.md) owns order.

## Accepted scope

| Source | Requirement | Boundary and evidence still needed |
| --- | --- | --- |
| [#174](https://github.com/EriArk/TrainerOS/issues/174) | One encapsulated game Experience Package supplies game-specific UI, actions and progress | Compose franchise presentation and narrow exact-build providers; host retains navigation, authorization, saves, launch, accounts and communication. No game-title switch trees in core shell. |
| [#175](https://github.com/EriArk/TrainerOS/issues/175) | Resolve labels, faces and contextual actions for two game-oriented primary slots | Selected Home Adventure drives shell experience; preserve five semantic slot identities, per-Trainer/game/module face state, capability gates and late-result invalidation. |
| [#176](https://github.com/EriArk/TrainerOS/issues/176) | Extract existing Pokémon screens and feature glue into a built-in experience | Preserve actual Emerald flows, exact-build read/write limits, save lineage and existing visuals. Moving presentation does not authorize save mutation. |
| [#177](https://github.com/EriArk/TrainerOS/issues/177) | Useful generic fallback and cross-experience acceptance | Metadata/ordinary launch/observed history/verified RA, no dead Pokémon screens or invented progress. Two nonproduction RPG/racing fixtures prove routing only, not Diablo/NFS integration. |
| [#178](https://github.com/EriArk/TrainerOS/issues/178) | Separate universal person/account profile from game persona/progression | User name/PIN/ownership/accounts and real play history remain host-owned. Pokémon favorites/badges/Champion are game-specific projections. Stable global profile access survives game/package changes. |
| Owner follow-up in this conversation | Home adapts to the selected game's experience too | Home retains its name/route and common controls; its actual game content, widgets, actions, presentation and verified progress adapt through the same package. More than replacing a background or title. |

Related #89/#90/#92/#111 and #167/#168/#169/#170/#172 have been updated to adopt
this architecture. Their older fixed Pokémon labels and pack-scoped Home/Y are
historical. The owner's adaptive-Home follow-up extends their earlier wording
about stable Home: **the shell role stays stable, the game content adapts**.
No GitHub issue was edited or closed by this documentation pass.

## One composed experience, bounded host services

Resolve from Trainer + selected Adventure + verified content/build + compatible
installed experience. A collection is a view, not a second game identity or
an authority to grant semantic capabilities. Franchise presentation may be shared;
exact-build readers, writers, runtime capabilities and unsupported states remain
independent. Ordinary runnable games never require a deep semantic adapter.

The experience contributes:

- Home presentation/widgets and capability-gated game actions;
- labels, icons, faces and trusted view identifiers for slots A/B;
- game-specific Select descriptors and live Game Options extensions;
- semantic/progress providers, provenance and exact-title overrides.

The host owns layout/input/focus, safe mounting, profile/accounts, actual history,
notifications, Social/GameParty/calls, runtime ownership and protected save commits.
RA service identity stays host-owned; a game can display only compatible real data.
Start and universal Shell Options do not gain adapter-specific menus. The global
profile entry inside Options is a host action, not an experience contribution.

First use trusted compiled/bundled presenters with versioned declarative descriptors.
Do not introduce a dynamic native ABI or executable downloaded QML. Future #92
packaging can compose UI declarations/assets and bounded semantic adapters, but
requires its own varied-game, versioning, trust and installation acceptance.
The new fixtures do not satisfy #90's real non-Pokémon semantic proof or #92 stability.

## Three contexts that must never be confused

| Context | Used for |
| --- | --- |
| Explicitly selected Home Adventure | Adaptive Home and game-oriented slots A/B, their faces and actions |
| Captured focused card/object | That object's Select/Properties/invitation; browsing a different library card alone does not replace Home selection |
| Verified actual live session | Game Options, live invite/settings, Return/Minimize/Exit, even while shell selects another game |

Actions bind owner, Adventure, experience/version, target and context generation;
revalidate before execution and after asynchronous work. Late results cannot fill
the new Home with old data or redirect a write into another save. Unsupported or
removed packages fall back without changing game IDs, media, saves or history.

## Scheduling and acceptance

This is part of **UX-02's shared framework outcome**, not deferred post-multiplayer
polish. Establish the experience/context contract, integrate adaptive Home, extract
Pokémon presentation, separate universal identity and supply the generic fallback;
then complete the shared Options/Select, live-game, invite/notification work as one
coherent UX. Dependencies are detailed in the [map](UX_OPTIONS_MAP_RU.md).
Only after this framework/UI/control block return to MP-02 and later work.

Acceptance covers Pokémon → generic → RPG fixture → racing fixture → Pokémon,
one game in multiple collections, a different minimized live game, missing/partial/
disabled/incompatible packages, stale callbacks, legacy routes, Trainer switching,
restart and stable focus. Home must change real content alongside both contextual
slots, while global recents, direct launch, profile, Social/call/party and save
protection stay correct. Verify existing Emerald flows and generic native playback
on Flip/Odin with controller/touch, plus real layout adaptation for Handheld/TV.
Fixtures prove composition only. Deferred physical/audio/network/TV acceptance and
future runtime proof remain explicit; they do not authorize endless repeated tests
or falsely close #135, full TV-01, #90/#92 or multiplayer families.
