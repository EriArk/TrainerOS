# P1 handheld UI audit

## Forms and navigation pass - 2026-09-20

This is an acceptance record for the delivered UI, not delivery of the later
owner/save/media/runtime providers. ROADMAP remains the execution queue.

| Flow | Exercised coverage | Boundary still open |
| --- | --- | --- |
| Home and shared Choose Adventure | Selection versus launch, exact card focus, Start/Back, missing pictures, interrupted drawer and Reduced Motion | Per-Trainer/domain binding and other adapter evidence remain in their functional phases |
| Worlds / Multiverse | Region/detail/search/filter/link routes, missing editions, paired-face focus and sample selection | Real Multiverse ownership, library and launch binding remain P2-P4 |
| Pokedex / Center | Journal/filter/detail/art alternatives, picker, paired face, Party/Storage unavailable and sample states | Individual Party and semantic writes remain P8; long real-library/device audit remains open |
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

## Remaining P1 audit

- Long real-library titles/list ends and empty/error/filter combinations on Flip.
- Consistent action hints and focus return across the remaining paired faces.
- Physical reconnect/foreground checks at the U12/platform boundary; injected
  SDL/InputPlumber observations do not certify a physical press or hotplug.
- Keep all provider-only limitations visible; no new save research or Pack Studio.

This record does not close P1 wholesale or remove any earlier roadmap item.
