# AGENTS.md — TrainerOS

## Read first

- Read [current task](docs/CURRENT_TASKS.md), its technical record and live related
  issues before implementation. [Roadmap](docs/ROADMAP.md) owns order;
  [product contract](docs/PRODUCT_SPEC.md) owns current UX decisions.
- Latest explicit owner decisions override conflicting older issues/documents.
  Issues own the backlog; commits and CHANGELOG own history. Dated evidence is
  not a new instruction to repeat a completed step. Do not append another queue
  or paste issue bodies into AGENTS/ROADMAP.
- Verify actual checkout, branch, upstream and WIP before editing. Preserve
  unrelated changes. Ask on a material UX/architecture fork; continue independent
  authorized work while awaiting the answer. A question alone is not permission
  to implement. Explicit audit/planning requests can be read-only or docs-only.

## Product and scope

- TrainerOS is a native C++20/Qt Quick Linux shell over Armada, not an Android app.
  Five roots: Home / Games / GameHub / Social / Apps. Restore each root's last place.
- Preserve the warm, colourful handheld visual language, material depth, readable
  scale and compact layouts. Review actual rendered output, including empty states.
  Follow [design](docs/DESIGN_LANGUAGE.md); no default desktop widgets as shell UI.
- Full controller and touch operation share actions/state. Semantic navigation,
  stable focus/Back/modal restoration, normalized repeat/deadzones; no hover-only UI.
  Controller recognition/glyphs must reflect actual labels, with manual override
  and unknown-device fallback; do not claim that all devices have passed #77.
- Ordinary A launch prepares an approved route automatically, not an editor.
  Keep emulator paths, core setup and commands out of ordinary player flows.
- Current launch scope is all 106 catalogue IDs. Owner added PS3/Switch on
  18 October: test those two on Odin using small homebrew, without claiming
  commercial-game compatibility or reopening their multiplayer scope.
  This expands the shared list, not their priority: PS3/Switch stay at the end
  of the oldest-first queue. Do not treat device availability as a queue override.
  Current sweep checks only launch from TrainerOS UI and controller response.
  Once both pass, close normally and advance. Do not add save/reopen, deep
  gameplay or repeated lifecycle tests without a concrete failure requiring them.
  Those broader acceptance checks remain deferred, not certified by this sweep.
  Reuse launcher mechanisms in batches, then test oldest first. See
  [platform register](docs/LAUNCH80_PLATFORM_REGISTER.md); installed cores are not
  proof of launch, performance, input or multiplayer compatibility.
- Multiplayer must reuse existing emulator networking. Pin runtime/core/build,
  protocol and sync settings per platform/mode; validate peers before starting,
  no arbitrary discovered fallback. See [standard](docs/EMULATOR_STANDARD.md).
  This is a requirement, not a claim that every admission gate is implemented.
- Ordinary PSP is sufficient; private PSP parties and full pass-controller
  catalogue are deferred. Owner's couch request means simultaneous separate pads.
  PS2 native online remains required; shared-console Remote Play is a separate route.
  Do not run two PS2 game copies on one handheld to manufacture couch networking.
- Fluxer supplies social, voice, spectator streaming and signaling. Remote Play
  gameplay uses its own low-latency P2P path. No heavy owner-hosted TURN requirement;
  difficult NAT and offline Lobby are deferred. Never equate LAN/forced relay with
  distinct-internet acceptance. Do not hide implemented routes behind global dev opt-in.
- First beta is an installer over Armada, not an image-first release. Target
  requirements and open gates: [installer](docs/EARLY_BETA_INSTALLER_RU.md).
  Crates system modules/SPI are fully deferred: do not invent infrastructure for them.

## Boundaries and data protection

- Features use shared repositories/services; platform details stay behind platform
  interfaces. QML contains no distro paths, emulator commands or credentials.
- Game experiences belong in `src/adapters/<id>/` with their own presentation,
  actions and navigation. Shared contract: `src/core/experience/`; explicit
  composition registration only. No Pokemon/title branches in the generic shell.
  Read `src/adapters/AGENTS.md` and [knowledge base](docs/adapters/README.md).
- Matching uses explicit alternative identity rules (provider/local IDs, exact
  hashes/codes, verified observations), not filename guesses or registration order.
  Presentation/catalogue matching does not authorize semantic save reads/writes.
  External downloadable adapters remain a separate #92 trust/installability gate.
- Preserve normal saves, per-Trainer state, firmware identity and user preferences.
  No silent overwrite/migration. Backup/copy/verify, exact-build guards and durable
  recovery are required for protected writes; never mutate an active game's save.
  Cemu/ARMSX2 Libretro bindings are real, not automatically invalid standalone aliases.
- Selection, live session, foreground/input owner and captured exit frame are
  different identities. Minimize is not pause, Exit, a save or another launch.
  Explicit Exit owns fresh clean capture and applicable save confirmation; cancel
  returns to the same game. Crashes never fabricate saved progress.
- Assets/packs remain separate from replaceable adapter code. A compatible active
  pack suppresses that family's extraction. Preserve exact mapping/provenance gates.
- Update adapter research and reusable exports with relevant code/data changes;
  use `tools/export-game-adapters.py` and `tools/check-adapter-knowledge.py`.

## Delivery and evidence

- Announce the whole player-visible result and recommended reasoning level before
  a new increment. Labels: 1/6 Низкое, 2/6 Среднее, 3/6 Высокое,
  4/6 Очень высокое, 5/6 Максимум, 6/6 Ультра. If the recommendation changes, pause
  for explicit continuation; never claim to change the owner's setting yourself.
- Complete authorized implementation, integration and relevant checks, then commit,
  push and verify remote SHA. Routine commit/push is authorized. No force-push or
  history rewriting. Stage explicit reviewed paths; do not clean unrelated WIP.
- Update Russian [PROJECT_STATUS](PROJECT_STATUS.md) and [CHANGELOG](CHANGELOG.md)
  in the same meaningful delivery/scope/status commit. Status is cumulative current
  state; changelog preserves dated history. Link evidence and relevant commits.
- Distinguish source, tests, installed artifact, observed device behavior and owner
  acceptance. Preserve pending checks; never certify a whole platform from one game.
  Repeat passed checks only after affected changes, failure or a named unresolved risk.
- UI work needs rendered/controller checks; lifecycle changes need real processes;
  persistence needs reopen/migration checks. Docs-only work needs content/link/diff
  checks, not device deployment. GitHub Actions is disabled; do not recreate it.
- Deliver both available handhelds when the increment includes deployment; check
  active sessions first, preserve each device's own data/boot preference, verify
  each installed version. Do not deploy during an active game/save transaction.
  Report an unreachable device or needed reboot promptly; prefer bounded recovery.
  Leave Odin volume at 0% after remote checks. Listening/headset/physical acceptance
  is deferred until the owner is available, not replaced by synthetic input/audio.
- Isolate `link_peer` protocol tests from a live shell's port 47845; no host-network
  test container against the user's listener. See [workflow](docs/DEVELOPMENT_WORKFLOW.md).
- End with result, evidence, remaining limits and proposed next step. Do not invent
  extra implementation to occupy a blocked acceptance stage.

## Content and privacy

- Original code is GPL-3.0-or-later; preserve third-party terms and LICENSING.md.
  Binary/source redistribution (#115), artwork (#116) and fresh setup (#117) are
  separate open acceptance. Keep existing README screenshots until agreed replacement.
- No secrets, private operational reports, ROM/BIOS/keys, proprietary binaries,
  personal saves or unlicensed ripped art in Git. Private ROM Policy Studio must
  never enter any GitHub repository, including a prototype.
- Owner authorizes test copies from their server ROM disk. Discover the actual
  mount; preserve originals and isolate test saves/configs from personal play.
- Guest-content policy covers known official releases; no mandatory ROM-hack
  classifier or per-file rights certificate. Unknown is not a prohibition.
  Contractual responsibility does not waive consent, integrity, signed policy,
  operator review or applicable law. Agreement remains a draft, not legal acceptance.
