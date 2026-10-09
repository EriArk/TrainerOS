# Issues #161–173 — acceptance and dependencies

**Later owner extension, 9 October:** [#174–178 and adaptive Home](EXPANSION_174_178.md)
now belong to UX-02's foundation, before completing Options/Select/live-game work.
The five semantic slots stay stable; Home game content and the two former fixed
Companions/Trainer slots adapt through an experience. Universal Shell Options does
not. Complete the whole framework/UI/control block before MP-02; keep this register's
runtime, notification, DS and alpha-art acceptance.

Reviewed 9 October 2026 against live GitHub issues and the owner's request to
map the experience and correct documentation **before implementation**.
This register adds requirements, not installed capabilities. No issue is closed.
The [precise UX map](UX_OPTIONS_MAP_RU.md) owns screen/action/session transitions;
[ROADMAP](ROADMAP.md#active-execution-queue--2026-10-09) owns execution order.

## UX-02: common controls and live-game multitasking

| Issue | Accepted outcome | Dependency / acceptance |
| --- | --- | --- |
| [#167](https://github.com/EriArk/TrainerOS/issues/167) | Five primaries, direct A, contextual Select, universal physical Home/Options, system-only Start; parallel touch/controller and warm material style | Parent of #168–173. Exact-origin return, stable focus, input ownership, real Handheld/TV composition. Home primary remains Home; Options is a working overlay name. |
| [#168](https://github.com/EriArk/TrainerOS/issues/168) | Shell Options occupies 85–95% of safe area with notifications, Social shortcuts, independent call/party state and Return to live game | Same capabilities from every page; no selected-game Properties/filter menu, duplicated settings or second messenger. Restore exact route/focus. UX-02A. |
| [#169](https://github.com/EriArk/TrainerOS/issues/169) | Select and visible ellipsis open compact actions for the focused game/person/message/collection/record | Shared action model and dispatcher; capture and revalidate target/owner. Direct A and useful X/Y/hold shortcuts remain. B/Select dismiss. UX-02A. |
| [#170](https://github.com/EriArk/TrainerOS/issues/170) | Large Game Options with live-game artwork/title, Continue, Minimize, Together, supported settings and separated Exit | Bind actual live session, never Home selection. Opening alone does not pause/minimize/exit. Preserve #49 fresh clean capture, applicable save question and graceful close. UX-02B. |
| [#171](https://github.com/EriArk/TrainerOS/issues/171) | Minimize a supported game, use shell/Social, return to the same process/session | Separate alive/foreground/paused state; proven solo pause only, netplay timing continues. Exclusive input, neutral gates, save/owner protection, honest time/audio, recovery and no phantom Return. Real solo and already supported multiplayer route on Flip/Odin. UX-02B. |
| [#172](https://github.com/EriArk/TrainerOS/issues/172) | One Play Together journey from Home/library game, DM/group and live game | Prefill known game/recipient; live and selected context stay distinct. Reuse GameParty/admission/consent/capacity and independent group call; no duplicate transport or lobby. UX-02C. |
| [#173](https://github.com/EriArk/TrainerOS/issues/173) | Passive shell/gameplay toasts and one inbox shown in both Options surfaces | No focus/input/pause changes. Real compositor proof, honest inbox fallback, event IDs/expiry/read state, DND/privacy/dedup/burst handling. Verified new achievements reuse #25; hide every UI layer for #49 capture. UX-02C. |

UX-02A establishes the shared interaction and platform contracts; UX-02B delivers
the full live-game/Social/return cycle; UX-02C connects contextual invitations and
notifications; UX-02D integrates the journey and records residual acceptance.
These are whole dependent outcomes, not permission to substitute test-only turns.
Each implementation delivery includes UI/integration, bounded checks, both
available handheld installations and commit/push. Current work is docs-only;
implementation starts after the owner's next continuation, then returns to MP-02.

Updated #25, #112, #134 and #157 explicitly adopt this direction. Their older
compact Home/menu and achievement presentation are historical; provider, call,
save/capture and accessibility boundaries remain. #135 stays open for future
runtime/Hotseat/native activities and deferred owner checks. The new surfaces
need true Handheld/TV composition, but do not silently complete all TV-01 routes.
External TV/audio/distinct-network gates stay explicit; missing future routes
do not prevent validating the interface against implemented routes.

The owner's newer library decision is also binding: **Collections** replaces
Worlds; Pokémon opens games without region cards; personal manual/dynamic and
automatic collections coexist. Home has global recents via Y and no L2/R2
collection cycling. Older issue titles retain their original names for lookup.

## MP-06: native Linux DS Local Wireless over internet

| Issue | Accepted outcome | Dependency / acceptance |
| --- | --- | --- |
| [#162](https://github.com/EriArk/TrainerOS/issues/162) | TrainerOS-owned Linux melonDS route for deterministic internet Local Wireless | Clarifies MP-06 after older families. Neither Android APK, game WFC, shared pads nor user VPN is the requested route. No implementation is claimed. |
| [#163](https://github.com/EriArk/TrainerOS/issues/163) | Port/reuse GPL DetMP and mirrored machines with reproducible native core changes | Pin upstreams and patches; deterministic wireless ordering, input/replay, lag, JIT/render compatibility, rebase/rollback. Each client may emulate its own DS plus peer mirrors; measure 2/4-instance budgets. Preserve upstream/GPL, ENet MIT and zstd BSD notices as applicable. |
| [#164](https://github.com/EriArk/TrainerOS/issues/164) | Local compatible BIOS/firmware, private saves and bounded mirror synchronization | Review/replace upstream package transfer assumptions before network enablement. No automatic commercial BIOS/ROM transfer; minimal ephemeral state, explicit scoped consent where needed, authenticated identity, replay/parser/decompression limits and cleanup. Guest transfer remains governed by #136–143. |
| [#165](https://github.com/EriArk/TrainerOS/issues/165) | Native runtime joins existing GameParty and secure automatic direct/relay transport | Depends on core and data boundary. Match exact game/build/profile; own save namespaces, existing Options/voice, common Software S ownership. Two-player first; no separate Social/lobby or manual IP/VPN journey. |
| [#166](https://github.com/EriArk/TrainerOS/issues/166) | Two real handhelds perform meaningful wireless gameplay, loss/desync recovery and safe return | LAN is intermediate, distinct internet remains separate proof. At least ten minutes meaningful gameplay; performance/input/save/crash evidence. Three/four-player availability needs actual peer-count and workload proof, not configured capacity. |

Issue #163 proposes WatermelonDS revision
`9b603e53d4d098a0fcee3401ffa5e9c9992f01a3` (`NetplayAndroid.cpp`) and core revision
`85fdb7596011a3d0e679a6f1f88c312e77ff4cc5` (`DetMP.cpp`) as research inputs.
They are issue references, not a fresh source audit or adopted shipping pins.
Source/license inspection belongs to MP-06 before importing code. Retain related
#76/#106/#107/#108/#115/#121 runtime, transport, redistribution and release gates.
DS ordinary launch already exists; the new DS multiplayer adapter does not.

## ART-01: alpha creature fallback

[#161](https://github.com/EriArk/TrainerOS/issues/161) requests original cuboid/
geometric creature portraits and sprites so the first alpha is not visually
empty without a personal pack. This is a bounded fallback, not full Pack Studio.
Start with a representative visual sample and owner review before expansion;
retain recognizable allusions only through original design, not tracing or
recolouring official assets. This does not establish legal clearance of the
entire artwork inventory; #116 remains open.

Keep entity/game IDs and gameplay unchanged. Record provenance, coverage and a
coherent generic fallback for uncovered entities. Preserve existing user-pack
priority per asset family. A bundled fallback is not a user-selected compatible
pack and must not disable optional ROM extraction. Series environments and
README screenshots are a different asset scope and remain unchanged.

Schedule ART-01 after accepted multiplayer/guest work and before alpha packaging
and publication. Pack Studio and optional ROM assets stay in the post-alpha
queue under the 8 October milestone; #161 does not restore the older distribution
order. This review does not generate images or modify private artwork.
