# Pokémon FireRed — English original and Rev 1

## Exact identities

- Original SHA-256: `3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac`.
- Rev 1 SHA-256: `729041b940afe031302d630fdbe57c0c145f3f7b6d9b8eca5e98678d0ca4d059`.

GBA, verified RetroArch/mGBA ordinary-save route, 131072-byte raw save.
ROM length alone never selects a provider. LeafGreen, other languages and hacks
are unknown targets, not aliases.

## Format differences and capability evidence

The bank/footer/individual structures follow the bounded Gen III container in
[Emerald](emerald-en.md), but section 0 length is **0xf24**, section 4 **0xee8**;
SaveBlock1 is 0x3d68 bytes. Party count/records are **0x34/0x38**.
Dex duplicated seen fields are 0x5f8/0x3a18. Badge flags 0x820–0x827
relative to flags at 0xee0. Deoxys uses Attack-form reference stats.
No Emerald shops, currency offsets or storage writer are inherited.

Canonical [delivery/evidence](../SAVE_POLICY_FIRERED.md) records synthetic
both-layout checks, private reads, protected HP/status/PP transformation,
Flip treatment, allowed-byte checks and rollback. **Normal in-game healing
readback was proven on Rev 1 only**, not both ROM revisions. Original is
integrated with a remaining exact-build physical readback gate.

Primary upstream: [pret/pokefirered](https://github.com/pret/pokefirered),
`include/global.h`, `include/pokemon.h`, `src/pokemon.c`, `src/save.c`.
The earlier research used upstream source without retaining an exact revision;
do not invent a pinned citation. Pin and hash newly consulted inputs before
extending this adapter. Runtime full-ROM gates and private evidence remain
independent from that source-provenance limitation.

## Dated findings and unknowns

- 2026-09-27: reconciled implemented Party/Boxes/Dex/healing with the per-build
  evidence above. The first attempted private Rev 1 save was all 0xff, which
  explained its read failure; it was not evidence of an incompatible layout.
- Box edits, shops, transfers, Link and exact original-build game readback stay
  open. Similarity to Emerald is a research lead, never write authorization.
