# Emerald selected-form collection

Delivered 2026-09-27 for the allowlisted English Emerald build. The combined
Pokédex keeps species registration and current form ownership separate:

- Seen/Caught remains a National species flag, including when a different
  reference form is selected.
- Party and Boxes count current hatched individuals of that exact selected form
  in the ordinary save. Eggs are excluded. Zero means a complete verified area
  contains none; a dash means the area could not be read completely.
- Reference forms, illustrations, stats and personal favorites remain available.
  Modern reference data does not become the game's individual stats or rules.

## Evidence and meaning

The pinned pret revision `5eff78649e7170a877b961ef0b3da13b81a16038`
stores National flags in [GetSetPokedexFlag](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokedex.c).
Its [Pokedex structure](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/global.h)
also holds first-seen Unown/Spinda personalities; these cannot prove historical
ownership of all forms. No form-history checklist is inferred from them.

Reuse the [validated individual reader](EMERALD_PARTY.md): PID identifies Unown
letters; the exact Emerald edition identifies Deoxys Speed; stored Castform is
the normal form, not a persistent weather transformation. Spinda spot patterns
are not separate entries in the current reference catalogue. This slice does
not add readers for other games, held Day Care occupants or historical forms.

`SavePokedex` has two optional form-count maps. Party and Boxes fail independently
if any record in that area is unreadable. An absent key in a complete map means
zero. The existing source/Trainer/content checks govern the entire observation.
Checking clears current values; a failed refresh may retain a labelled last
verified snapshot only after the same identity is revalidated. A successful
older save replaces counts rather than unioning them. Missing/replaced sources
clear the previous snapshot. Nothing is written to the game save or SQLite.

## Validation

- Synthetic complete saves cover all 14 sector rotations, distinct Unown forms,
  duplicates, Eggs, independent Party/Box corruption and empty areas. Deoxys and
  Castform assertions separate stored forms from reference alternatives.
- Presenter checks cover form switching without changing species flags, stale
  refresh, rollback, partial corruption, changed owner and out-of-game species.
- An independent private decoder verified 6 Party members and 386 boxed members:
  Blaziken is in both areas, Unown A is boxed, as are Deoxys Speed and Castform.
  Private input, manifests and captures remain outside Git.
- Windows build and full suite: 43/43. The additional edition-form assertions
  and final compact-layout rendered Dex scenario pass separately.
- Native ARM Release build is installed on Flip. Controller events exercise
  search, form selection, Back and shoulder navigation; captures show the real
  installed screen. This is injected controller input, not an owner physical trial.
  Unown A shows Party 0 / Boxes 1; switching to B shows 0 / 0 while the species
  remains Caught. The final layout retains the family line without clipping;
  the installed process journal has no QML errors.
- Production SHA-256:
  `fd436e20633cffc03e045d3cd06d8987f76a18def99372c358db9ae421291d45`.
  Schema 14, 829 Adventures and 3 Trainers remain intact; personal-save hashes
  are checked before/after. No new writer or game-save readback claim is made.

The permanent [adapter copy](adapters/implementations/gen3/README.md) includes
the maps, aggregation code and exact-game capability profile. All remaining
R4 scenes/practice/Link and broader-title gates stay in [ROADMAP](ROADMAP.md).
