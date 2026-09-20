# P1 handheld UI audit

## Forms and navigation pass - 2026-09-20

This is an acceptance record for the delivered UI, not delivery of the later
owner/save/media/runtime providers. ROADMAP remains the execution queue.

| Flow | Exercised coverage | Boundary still open |
| --- | --- | --- |
| Home and shared Choose Adventure | Selection versus launch, exact card focus, Start/Back, missing pictures, interrupted drawer and Reduced Motion | Per-Trainer/domain binding and other adapter evidence remain in their functional phases |
| Worlds / Multiverse | Region/detail/search/filter/link routes, missing editions, paired-face focus and sample selection | Real Multiverse ownership, library and launch binding remain P2-P4 |
| Pokedex / Center | Journal/filter/detail/art alternatives, picker, paired face, Party/Storage unavailable and sample states; real 1025-entry list and Center return reviewed | Individual Party and semantic writes remain P8; physical comfort/reconnect acceptance remains open |
| Hall / Journey / RA | Peer face/back/detail, earned/locked/unknown states, sample Champion availability | Live semantic snapshots and selected-Adventure/account projections retain P6/P8 gates |
| Trainer / Settings | Inline validation returns to Name, keyboard draft survives Start/Back, keyboard cancel discards draft, account sign-out cancellation restores the exact row | Real separate Trainers, PIN and ownership remain P2 |
| Start / System / Controller | Sliders, safe Power default, category/detail Back, unavailable capabilities and diagnostic return | Physical reconnect/remapping and boot durability retain U12/platform gates |
| Center activities | Playroom, practice and link rehearsals, production unavailable states and modal priority | Real animation, rules and transfer work remain P5/P8 |

The shared Settings status area now reserves three lines below the controls.
Profile errors only appear while editing the Trainer; preference errors belong
to Appearance; device errors belong to Appearance, Sound or System. Async global
failure notices are unchanged. Long setting titles are bounded before their
value/chevron. Compact rows retain a visible gold focus edge and do not overlap
the status area. No provider, database schema or external save changed.

The extended SDL scenario checks validation, keyboard draft/Start priority,
account sign-out cancellation, exact Back focus and focused-row/status geometry.
Existing rendered scenarios cover the other flows above; reference their exact
fixtures rather than treating a sample screen as a working feature.

Validation: all seven rendered QML scenarios plus library, interaction and
cross-process persistence checks passed on Windows and ARM (10 affected checks
on each). The extended diagnostics scenario also passed under the Flip's native
Gamescope compositor with SDL events and no QML warnings; its 1920x1080 captures
were reviewed. These are software-injected controller checks, not an owner's
physical-button acceptance test.

The production build was installed on Flip after a binary/database backup;
installed hash, schema 7, unchanged Trainer profile and all 686 Adventure records
were verified. No save migration was required.

## Acceptance reconciliation - 2026-09-20

The bounded P1 presentation pass is complete for the current synchronous screen
contracts and existing services. This is permission to advance the roadmap's
early P7 runtime-art slice, not a claim that every represented feature works.
Physical acceptance, future provider states and whole-product #34 acceptance
remain open. No GitHub issue is closed by this record.

| Screen/state group | Concrete regression evidence | Remaining acceptance and owner phase |
| --- | --- | --- |
| Home / Choose Adventure | [Home scenario](../tests/ui/HomeSmokeScenario.cpp): fixed A, Y selection without launch, missing media, known zero versus unknown, clean-exit picture and choice after reopen | P2 owner/context isolation; P4 real Multiverse choices/media; P8 additional observed progress |
| Worlds / Multiverse | [Worlds scenario](../tests/ui/WorldsSmokeScenario.cpp): list/detail/Back, empty search/filter recovery, long titles, modal priority, independent paired routes; real 506-row Fan Worlds check below | P3 chronology/catalogue/identities; P4 real system library, linking, media and playable runtime collection |
| Dex / manual journal / species picker | [Pokedex scenario](../tests/ui/PokedexSmokeScenario.cpp): load failure/retry, combined filters, no results, failed writes, journal cancellation and fully visible selection after Center; [persistence scenario](../tests/ui/PersistenceSmokeScenario.cpp): picker and committed profile/journal after restart | P2 personal ownership; P7 separate Caught projection; P8 selected-save and individual records |
| Illustration review / credits | [Artwork scenario](../tests/ui/ArtworkSmokeScenario.cpp): complete reference, no-pack fallback, alternatives, exact forms and source panel; [Flip bootstrap](ARTWORK_BOOTSTRAP.md) records real geometry/derivatives | Early P7 runtime validation/Settings and separate sprite provider; 142 unresolved forms remain explicit; final Pack Studio stays last |
| Center / ordinary backup shelf | [Diagnostics scenario](../tests/ui/DiagnosticsSmokeScenario.cpp): sample Party/Storage/detail, zero HP, unreadable record, slot return and empty-shelf/search hints; [Center scenario](../tests/ui/CenterSmokeScenario.cpp): existing guarded backup/restore fixture flow | P8 real Party/Storage, independent healing/write capabilities and additional exact-format evidence; existing working backups remain available |
| Playroom / Practice / Link | [Diagnostics scenario](../tests/ui/DiagnosticsSmokeScenario.cpp): actor focus, setup/preview, interrupted proposal and management return; production unavailable routes checked on Flip | P5 motion provider; P8 actual individuals, rules and durable two-device transaction. A preview interruption does not prove recovery |
| Hall / Journey / Champion / RA | [Hall scenario](../tests/ui/HallSmokeScenario.cpp): empty archive, errors, offline/account states and pair restoration; Home scenario: Journey zero/unknown/source isolation; actual RA row restored on Flip | P2 accounts/ownership; P6 selected-Adventure matching and earning; P8 durable Champion and new semantic snapshots |
| Trainer editor / registration / PIN | [Diagnostics scenario](../tests/ui/DiagnosticsSmokeScenario.cpp): inline validation, keyboard draft and cancellation, account sign-out cancellation, labelled registration/keypad/chooser rehearsal | P2 real creation loading/failure/retry, startup unlock, PIN protection/recovery, owner switching and stale-result isolation. These are not tested by the synchronous rehearsal |
| Start / Settings / Power / Controller | [Diagnostics scenario](../tests/ui/DiagnosticsSmokeScenario.cpp): category/detail return, sliders, safe Power Cancel, capability fallbacks and bounded status; [persistence scenario](../tests/ui/PersistenceSmokeScenario.cpp): durable settings | P5 media/feedback; P7 pack management; P10 session/software delivery; U12 physical volume/face-button/reconnect checks after Armada changes |
| Exit overlay / storage recovery | [Exit scenario](../tests/ui/ExitSmokeScenario.cpp): compact captured background, neutral gate, cancellation to same process, waiting, explicit autosave confirmation; persistence scenario: failed storage traps navigation | Per-runtime launch/input/return proof and boot/access durability remain platform/P9 work; storage and migration recovery rechecked with each P2/P10 change |

The source links identify checks, not proof of a fresh test run. The recorded
delivery checkpoints are `7318d72` (forms/status), `7c54ffb` (library layout) and
`f300d92` (Center/Hall hints and Dex return), with dated Windows/ARM and native
Flip results below and in the linked module documents. The initial library
heading assertion failed before the font-metric fix; its subsequent final
Worlds rerun passed. Do not treat that initial log or an old CTest failed-test
list as the final result, or describe cumulative checks as one fresh full suite.

### Gates carried forward

- U12/P12: physical Switch A/B/X/Y, L1/R1, L2/R2 and Home behavior; reconnect,
  foreground recovery, physical volume keys, sustained readability/performance
  and all-theme/reduced-motion acceptance on the handheld. Injected events and
  earlier owner observations do not certify every later build or Armada update.
- Each functional phase: extend the same screen tests for its actual async
  loading/cancellation/failure/retry and identity changes. Synchronous unavailable
  views must not pretend to have a pending provider. Retain production sample
  isolation and the existing real save protections.
- P9/P10: repeat physical Home/exit and input-access persistence after reboot or
  device re-enumeration before expanding the dedicated-session guarantee.
- P12: final #34 audit remains; UI-first delivery does not freeze visual design.
  Sleep stays deferred, full Help stays P11, Pack Studio stays final row 14.

## Library list and paired-face pass - 2026-09-20

Adventure detail headings now reserve two lines for long edition names in
Worlds and the Multiverse presentation. Worlds keeps its description below the
heading and its action strip fixed; list rows remain compact and elided.
This does not change catalogue identities, ownership, launches or saved choices.

The SDL Worlds scenario now also exercises independent empty searches on both
faces, exact Back focus after a face round trip, Start/Back above the empty
Multiverse result, and A resetting that search to the first result. A long-title
render assertion checks actual line count and truncation. Existing scenarios
retain detail/list return, unavailable launch, search cancellation, jump-to-end,
global shoulder navigation and Pokedex/Center and Hall/RA coverage.

All eight affected checks (seven QML scenarios and Worlds unit tests) passed on
Windows and ARM. The heading uses the actual wrapped text height rather than a
fixed two-line pixel estimate: platform font metrics differ. New sample searches
are scenario inputs only; normal controller keyboard entry remains covered by
the earlier SDL search steps.

The expanded Worlds scenario passed under native Flip Gamescope with no QML
warnings; the two-line heading was visually reviewed at 1920x1080. Production
was installed after a binary/database backup, with installed hash, unchanged
Trainer profile, schema 7 and all 686 Adventure records verified.

Installed-build InputPlumber checks traversed the actual Fan Worlds list to
506/506, opened detail without launching, restored the last row through Back
and the list's Back control, and exercised the empty Missing filter. Captures
confirmed visible gold focus at the clipped list edge and a reachable empty-state
Back action. The filter was reset and the handheld returned to Home. Resolve
regions from the displayed catalogue: its merged ordering differs from raw
SQLite world order. Physical button/hotplug acceptance remains separate.

## Center and Hall action hints - 2026-09-20

The Center Adventure browser labels X as Search and its on-screen button opens
the same search as the physical shortcut. Empty or unrestorable shelves no
longer advertise restoring a selected copy; Back distinguishes the companion
Party/Storage route from the standalone Adventure browser. The unavailable
backup control is disabled without changing backend save guards. Empty Hall
archives offer New memory without advertising Edit for a nonexistent record.

SDL checks cover the empty Center shelf, empty Adventure search, X keyboard
entry and cancellation restoring its opener, plus the empty Hall hint. Existing
Pokedex and Hall scenarios retain combined filters, write/read error recovery,
offline/loading/disconnected RA states, account isolation and paired navigation.
These UI changes add no save parser, writer, ownership or RA provider capability.

The three affected rendered SDL scenarios (Pokedex, Hall and diagnostics/Center)
passed on Windows and ARM. The installed pre-change Emerald shelf confirmed the
reported empty-copy hint problem against a real ordinary save; inspection did
not create or restore a backup. Physical reconnect/foreground acceptance and
future provider states still require their own verification.

Real-content review also found that returning from Center could leave the
selected Dex row partly below the viewport. Pokedex now reveals its current row
again on visibility, focus-ownership and viewport-height changes. The SDL test
perturbs the hidden list's scroll offset, returns through the paired trigger,
and checks both species identity and the complete focus outline. This does not
change journal data, filters or the selected species.

Native Flip Gamescope runs passed for diagnostics/Center and the final Pokedex
return regression. Installed-build InputPlumber checks confirmed that Machoke
remains fully visible after a Center round trip in the real 1025-entry Dex.
The Emerald empty-copy shelf has the corrected hint, and Hall/RA pairing retained
the selected achievement in the 91-record account collection. These are injected
controller checks, not a substitute for physical reconnect acceptance.

The final production binary was installed with a binary/database rollback copy.
Its hash, database integrity, unchanged Trainer profile, schema 7 and all 686
Adventure records were verified. No game save was created or restored during
the real-content audit; the device was returned to Home.
