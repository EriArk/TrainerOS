# Offline Pokédex and field journal

**Accepted target, not delivered — #46/#13/#51:** this document records the existing offline reference and manual journal. Primary current progression will use the shared Adventure's verified ordinary-save Seen/Caught; manual history/favorites remain separately sourced and survive rollback. Unknown is not false. L2/R2 pairs Dex with Center, shared Y changes Adventure. Classic illustrations remain primary long-list art; optional animated sprites/portraits serve detail. Follow [projection acceptance](EXPANSION_42_62.md#pokédex-journey-and-achievements) and the strict [#61 artwork sequence](EXPANSION_42_62.md#artwork-sequence), not a generic downloader-first plan.

Normal runs use a bundled factual reference containing **1025 species, 1579 named forms and ten regional collections** from a pinned [PokéAPI snapshot](https://github.com/PokeAPI/pokeapi/tree/4b82c204ddd19ecb8eda2ea044ccb59e222b721c/data/v2/csv). The application performs no network requests to browse it. Ephemeral and existing scenario fixtures remain separate.

## Reference scope

The guide includes names/numbers, types, height, weight, six base stats, named forms and evolution-family membership. Regional collections combine the regional Pokédex lists associated with each region in the source. They are **reference lists**, not a promise that every species or form can be caught in each Adventure. Regional membership belongs to the species; it does not infer individual form availability. National-only entries remain browsable without a regional filter.

Types and stats describe the selected form in this snapshot. They do not adapt to historical game rules, emulator cores or ROM-hack modifications. The named form list is not an enumeration of every procedurally varied appearance. Fan-created species require a separate future reference provider. Missing numerical facts display an em dash.

`tools/build-pokedex.py` generates `data/pokedex.json` from pinned CSV bytes, records input hashes in `data/pokedex-source.json` and preserves the upstream BSD-style notice in `data/licenses/PokeAPI.txt`. CMake includes the factual reference as a Qt resource and installs the license under `share/doc/traineros/licenses`. No game sprites, cries, flavor text or other extracted assets are included.

`OfflinePokedex` validates the bundled snapshot and caches it in memory. The existing provider contract remains replaceable. Duplicate identities, missing families, invalid regional references and broken form/stat records reject the load instead of producing a partial invented guide.

## Controller flow

- X opens search from the list; exact National numbers, names and form labels are supported. Search ignores case, accents and punctuation.
- Up/down move through the bounded list, left/right jump eight entries. Selection immediately updates the illustration, animated companion, facts and six colored numerical stat tiles beside the list; there is no second entry screen.
- A toggles the selected species favorite. B switches between the list and filter rail. Form and Artwork are the final two rail controls; type filtering initially selects a matching form where applicable. The chosen form survives section changes/restart without changing journal data. Legacy detail routes restore the same entry in the combined browser. Touching a row selects it without changing progress.
- Select opens the attached field journal; Y opens the shared Adventure selector. X remains search. L1/R1 and Start keep their global roles. All button legends live in the shell footer and follow the foremost active panel.
- In the journal, A cycles Seen/Caught through unknown, Yes and No, or opens the shared note keyboard. Y saves; B discards the draft. L1/R1 discard an unsubmitted journal and switch sections.

## Personal records

Seen/Caught and notes are explicitly **manual, species-wide trainer records**, shared across the local library. They do not represent a particular ROM, profile inside a game, form collection or automatic save parsing. Favorites remain an independent mark.

A caught mark requires Seen=Yes. Choosing Seen=No also makes Caught=No. Unknown is kept distinct from No; changing a field to unknown removes a conflicting positive/negative assertion where needed. Notes allow a single line of 160 characters. Unsaved edits never enter storage.

SQLite schema 6 adds `pokedex_records` with nullable boolean marks, note and revision, without seeding progress. Its transaction checks the previous revision and reads the committed projection before acknowledging Save. Favorites stay in their existing table and are merged only when presenting progress, so journal writes cannot overwrite concurrent favorite changes. Errors preserve the old record and retryable draft. Normal exit drains submitted writes. These operations never open, modify or reinterpret external game saves.

## Acceptance

- The pinned guide loads all species/forms and validates identities, facts and family/region relationships.
- Accent-insensitive search, alternate-form type filtering, numeric lookup and form restoration are checked against the actual bundled reference.
- Schema 5→6 migration preserves favorites; marks/notes survive database reopen and remain separate from favorites.
- Stale and inconsistent records are rejected; a locked writer preserves the editor draft while the event loop remains responsive.
- SDL controller scenarios exercise note entry, mark consistency, save/cancel, page switching and focus recovery when a changed mark removes a filtered entry.
- Device checks cover the full reference, form switching, controller journal entry and persistence in the production composition.

Verified on 2026-09-13: all 22 tests passed on Windows, Ubuntu Qt 6.4 and Flip ARM Qt 6.11. The production device displayed 1025 entries, switched Vulpix to its Alolan Ice form with the corresponding facts, and saved Seen/Caught plus a controller-entered note. The record survived restart; cancelling a changed draft preserved it. The temporary test record was backed up and removed, restoring the previously empty personal journal. All 686 registered Adventures and the existing archive survived migration.

## Combined-browser delivery — 2026-09-23

The installed Flip build shows four illustrated species rows beside Marshadow's
primary illustration, optional animated companion, types, height/weight and six
colored numerical stats. The footer changes for list, filters, journal, keyboard
and other shell surfaces. Windows' full run passed 39 of 40 tests; the remaining
Hall assertion still assumed the removed inline legend. It was updated to inspect
the actual footer's visual children and passed in the 3/3 Pokédex/artwork/Hall
rerun. Compact rail-label polish passed another Pokédex SDL scenario. No outstanding
test failure remains. ARM64 production compilation and installed-session controller
navigation were verified. Actual Flip captures also cover Pokémon/Multiverse
crystal wheels, Start and Settings; these are device frames, not host renders.
Installation preserves schema 11 and all 826 Adventure rows, with database and
binary rollback copies. New save readers and artwork-pack authoring remain deferred.
