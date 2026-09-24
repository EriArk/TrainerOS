# Emerald Party and Storage: first real reader

## Delivered boundary — 2026-09-24

The exact English Emerald build already identified by [Game Progress](GAME_PROGRESS.md)
now supplies the Center's six Party positions and fourteen thirty-slot boxes.
This is an ordinary-save observation, not live emulator memory. FireRed still
supplies its existing badges/counts only; other titles and hacks gain no implied
Party support. The subsequent [Emerald Center](EMERALD_HEALING.md) adds the
independently guarded first Party healing writer. Box movement and exchange
remain unavailable; read support alone never enables writes.

The reader supplies species/form, nickname, level, types, ability, nature, held
item, six stats and four moves with PP. Party also has current HP and condition.
Storage has no current-HP row or health meter: only its calculated maximum HP
among the stats. Eggs hide their species; empty slots retain their positions;
an invalid individual becomes unreadable without inventing a replacement.
Japanese nicknames fall back to the species name. Shiny identity is read, but
this increment does not deliver shiny-specific artwork or personal history.

The current Pokémon Adventure and immutable Trainer save context own the
observation. Multiverse Home selection cannot replace that source. Entering
Center refreshes through the existing worker service; launch, backup activity,
source replacement and owner changes invalidate stale data. Repeated reads of
the same source preserve box focus; a different source starts at its saved box.
Missing/unsupported reads never fall back to development individuals.

## Format and reproducibility

The existing ROM SHA-256 gate, 128 KiB size check, rotating complete-slot
selection, section signatures/counters/checksums, fallback and counter wrap
remain in force. Party uses the validated SaveBlock1; Storage concatenates
sections 5–13. Each encrypted individual additionally passes its own checksum,
header/egg consistency, species/level/PP and Party stat bounds. The reader
handles all twenty-four personality-based substructure permutations.

Format facts come from the pinned pret Emerald revision
`5eff78649e7170a877b961ef0b3da13b81a16038`:
[individual layout](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/pokemon.h),
[save blocks](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/global.h),
[Storage layout](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/pokemon_storage_system.h)
and [level/stat rules](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon.c).
Storage records start at offset 4 after alignment; boxes are not packed against
the one-byte current-box field. Emerald Deoxys uses its Speed-form stats in
ordinary non-link summaries; Unown form follows personality bits.

`tools/build-emerald-reference.py --source-dir <pinned-checkout>` reproduces the
bundled factual ID/name/base-stat/PP/encoding tables. Input hashes and revision
are recorded in `data/emerald-reference-source.json`. No game code, descriptions,
ROM/save bytes or art enter the repository. Existing private illustration/sprite
providers remain independent. Derived observations stay in memory; no schema
migration or new resident service is needed.

## Verification

- Synthetic fixtures exercise every permutation, last box/slot, zero Party HP,
  unknown Storage current HP, PP Ups, held-item naming, Egg secrecy, corrupt
  individuals, impossible counts and incorrect block sizes.
- Presentation checks cover source/Trainer replacement, checking states,
  fourteen-box wrap and remembered focus; badge-only observations do not enable
  Party. Existing worker tests retain stale-result and unchanged-source checks.
- An optional private `TRAINEROS_EMERALD_SAMPLE` checks a real full save without
  committing it. All six Party entries and all 420 box positions decoded without
  unreadable records. The native suite passed 42/42; affected checks were rerun
  after the item-table and Storage presentation corrections.
- On Flip, the installed build displayed the real team and boxes. Emerald's
  own menus confirmed all six names/levels/HP and Blaziken's six stats, Charcoal
  and four moves/PP. The first comparison used the retained legacy route without
  an in-game save; original legacy and owner save SHA-256 values were unchanged
  after exit. Subsequent populated UI captures use isolated data/configuration
  and copied saves, including an explicitly isolated legacy route.
- Device controller events exercise the installed Party/Storage flow. The
  two-shell test setup required a targeted emulator SIGTERM to close the game;
  this is not fresh physical Home/guarded-exit acceptance. Source saves were
  verified unchanged. Development processes are closed after capture.
- Final ARM64 production SHA-256:
  `c7aabdd4b571c7c4bbcc5d01ba6571ab4d0684375faa0f74f6c61114320283e3`.
  Program/database backups precede installation; schema 13, 829 registrations
  and three Trainers are preserved. Final captures show Charcoal and Storage
  without current HP. The final game-progress/private-save and rendered SDL
  diagnostics checks passed 2/2 after the full 42/42 run.

Selected-save species progression now has its [first Emerald route](SAVE_POKEDEX.md).
Broader exact-build support, real Party
Playroom and independently guarded writers remain in ROADMAP row 7. This reader
does not certify save authenticity, individual legality, history or achievements.
