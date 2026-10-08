# Issues #136–160: guest content, OddCrate, software updates and TV

Reviewed against live issue bodies/comments on 8 October 2026, including changed
#104/#107/#113/#119 and the owner's later content-responsibility correction.
These are accepted targets, **not installed capabilities**.

[Full Russian product vision](PRODUCT_VISION_RU.md) consolidates the experience.
[ROADMAP](ROADMAP.md) owns execution; [CURRENT_TASKS](CURRENT_TASKS.md) owns remaining
work. MP-02 stays next; preserve its uncommitted timer work. Compact warm Social
is installed; future-route and human acceptance remain open.

## Superseding decisions

- Personal guest files: known **official releases/regions/revisions** form the
  prohibition catalogue. No ROM-hack/transformed-image catalogue, automatic origin
  classifier or per-file licence-evidence requirement. User contractual responsibility
  replaces that prerequisite. Unknown is not automatic deny or legal clearance.
  Invalid/missing/stale signed policy still disables transfer; consent, integrity,
  runtime compatibility, isolation, revocation and save protection remain mandatory.
- OddCrate publications still require operator review of binary/asset/media rights.
- #113: 7200 eligible seconds of completed actual gameplay OR verified exact-game
  completion with meaningful local play. Imported completion alone is insufficient.
  Required 1–5 stars, optional text up to 800 characters, legacy v1 compatibility,
  one effective provider-account/game review; old Emerald-only proof is retained.
- I (prepared image) or H (external host), S (TrainerOS Software), G (OddCrate games)
  have separate update ownership. Related independent I/S manifests share component
  ownership. Policies use S only; policy-only releases need no Linux/binary upgrade.
- TV requires real Auto/Handheld/TV layouts and D-pad-only navigation on every
  surface, not an enlarged 960×540 canvas; dock transitions preserve user state.
- Private ROM Policy Studio source/binary/project/plaintext/keys/correspondence
  must never enter **any GitHub repository** or public attachment/artifact, including
  prototypes. Public verifier/schema/synthetic fixtures and compiled signed encrypted
  policy are separate. The agreement remains a draft; AbyssTail is currently the
  individual's brand in Israel, not an already registered company.

## Acceptance and dependencies

| Issue | Required outcome | Dependency / retained boundary |
| --- | --- | --- |
| #136 | Temporary invited content across technically supported platforms. | #137–143; small licensed first fixture is not an age/platform restriction. |
| #137 | Exact identity; ready / needs-guest-content / unavailable; opaque source. | Existing GameParty; no remote path/CLI or fake indexed game. |
| #138 | Bounded binary transfer, progress/cancel, backpressure, stable source/hash/size, partial cleanup. | Consent + valid policy; reviewed multipart contract; no BIOS/keys. |
| #139 | Supervised isolated lease, handles/process revocation on exit/expiry/crash/Trainer switch. | Legitimate Home/voice/notification overlays survive; no recent/cache/dump/swap leak; personal saves preserved by mode. Not root-proof DRM. |
| #140 | Signed encrypted per-platform/family official-release prohibitions and monotonic freshness. | Nintendo first-party seed, not all Nintendo-platform games; unknown is not deny, invalid policy is. |
| #141 | Real paired missing/mismatch transfer/play/overlay/cleanup and fault journey. | #137–140/#143; not screenshots or handshake alone. |
| #142 | Local private authoring/evidence/conflict review/sign/export. | No GitHub editor/plaintext/secrets; no per-homebrew whitelist or hack catalogue. |
| #143 | Ordinary Software S policy delivery and policy-only point releases. | #119/#153/#154; rollback floor, no instant offline revocation. |
| #144 | Complete curated OddCrate experience. | #145–152; permanent install distinct from guest lease. |
| #145 | My Collection / OddCrate in Multiverse, equal-scale cards, handheld/TV. | Five primaries preserved; #156–159. |
| #146 | Signed operator-hosted catalogue/packages/media, stable IDs, architectures, offline cache. | Rights review, bounded URLs/paths, no catalogue-supplied commands. |
| #147 | Approved upstream recipes, isolated ARM64/x86_64 builds, validation/promotion, last-good. | No private services/tokens/signing keys accessible to untrusted builds; package format still a choice. |
| #148 | Ordinary library install/update/remove, no duplicates, preserve saves/history/metadata. | Active game defers update; compatible rollback; explicit install can resume same party. |
| #149 | Creator submission, owner review, rights evidence, deliberate publication/unlist. | No auto-publish; unlist does not erase installed copies. |
| #150 | Verified creator URL and locally generated scannable QR. | Handheld/TV; no in-app money handling. |
| #151 | Top Rated from FluxerReviews, counts/average and sample-aware ranking. | Revised #113; curated version mapping; no invented legacy stars. |
| #152 | Server→handheld/TV install/play/update/remove/QR/review proof. | #144–151, #113/#156–159, 7199/7200 boundary. |
| #153 | Separate I/H, S and G ownership/update paths. | Supersedes monolithic #119 reading, not a second ownership database. |
| #154 | Signed cross-distro Software updater, changed components, atomic compatible apply, LKG. | Game/protected writes defer apply; rollback obeys data compatibility. |
| #155 | Separate Software/Image/Games update UI. | Host owns distro updates; shell restart differs from reboot. |
| #156 | Real Auto/Handheld/TV tokens/layouts, readability, density, safe area, override. | Shared backend; no resolution-only TV inference. |
| #157 | Recompose every primary/Settings/wizard/keyboard/modal/QR/notification/in-game Home. | #156; no handheld regression or TV feature fork. |
| #158 | D-pad/remote focus, visible navigation, text entry, accessibility. | Actual CEC/remote events; preserve focus through async changes. |
| #159 | Safe dock/output transitions with retained state and override. | No host display takeover or forced game restart. |
| #160 | Actual image/SteamOS/Bazzite updates and handheld/TV 1080p/4K/high-DPI matrix. | #125/#152/#154–159 and physical acceptance; resized screenshots insufficient. |

## Integration into the existing queue

| Block | Added obligation; immediate priority unchanged |
| --- | --- |
| MP-02 → MP-07 | Existing runtime families first; retain independent saves and deferred internet proof. |
| HS-01 | Live Hotseat follows those families. |
| GUEST-01 | #136–143 follows runtime/party foundations, before claiming missing-game invitations. The alpha queue places it after native Link and before REL-01/distribution. |
| LINK-04 | Native online Link retains #93/#94 and bilateral acceptance. |
| REVIEW-05 · after alpha | Revised #113 rating/eligibility/migration plus residual communication/#135. |
| Remaining R/U/P · after alpha | ODD-01 (#144–152) and TV-01 (#156–159) join retained product scope. Their mutual order is not owner-ranked; neither overrides MP-02. |
| R18 → R18a/R18b | Pack Studio/art/sprite packs, then optional ROM assets after alpha, before the full-product artifact refresh. |
| REL-01 | #119/#153 independent I/S manifests and shared ownership; design S policy/update contracts earlier when a dependent feature needs them. |
| R15 + INSTALL-01 | Image and SteamOS/Bazzite installation use compatible S baseline; alpha follows full multiplayer implementation, ahead of remaining product/assets work. |
| R16 + MAINT-01 + SW-01 | #71 image OTA, #123 host repair, #154 Software updater, #155 UI. |
| R17 | #125/#141/#152/#160, remaining #135 and all earlier physical/legal/build gates. |

The later [owner alpha milestone](ROADMAP.md#alpha-milestone--2026-10-08) moves
initial distribution before Pack Studio. This table maps scope, not a competing
execution order. Implement cross-cutting components when a dependent feature
requires them; partial work never closes the parent outcome. Full Software
updates, repair and final acceptance remain after alpha except necessary safe
delivery/guest-policy prerequisites. No issue is closed here.

## Evidence limits

This is a documentation/issue-contract pass. Existing evidence remains in
[SOCIAL_COMPACT](SOCIAL_COMPACT.md) and [HANDHELD_MULTIPLAYER](HANDHELD_MULTIPLAYER.md).
Guest leases/policies/private editor, OddCrate, universal reviews, Software updates
and true TV layouts require implementation and their own device/release proof.
[Agreement](USER_AGREEMENT_DRAFT_RU.md) is not yet effective service terms.

## Reconciliation delivery — 8 October 2026

The product vision includes all 158 issues returned by the #1–160 review; #66/#67
are absent from that issue inventory. Updated issue bodies were read back from
GitHub: #68, #101, #104, #107, #129, #130, #133, #135, #136–138 and #140–142.
Corrections cover protected Shops transactions, compact warm Social, official-only
prohibitions/user responsibility, Software policy delivery and permitted overlays.
No issue was closed and no issue comment was posted.

README/docs index, AGENTS, product specification, active roadmap/task register,
architecture/data-model notes, design/navigation, reviews and agreement delivery
wording now point to the same current contract. Historical implementation evidence
is explicitly retained. Documentation checks cover relative links/vision anchors,
unique complete issue mapping and whitespace. This pass neither builds nor installs
runtime code; MP-02 timer changes, private work/share and screenshots are preserved.
Next implementation task remains MP-02, not another documentation-only loop.
