# Series collections — #114

Owner priority, 4 October 2026: deliver this library/navigation increment before
returning to MP-02. It is not a replacement for the remaining #90 experience-pack
and exact-game adapter work.

## User journey

- Worlds opens the available series grid. Pokémon opens its region cards; other
  populated series open their game wheel directly across platforms. Multiverse
  retains the system browser for games without a supported series classification.
- A on an installed game launches immediately through the existing adapter.
  Held A retains Rename / Move / Delete / Properties.
- B returns from a series wheel to the collection grid and keeps its focus.
  Returning from another primary page starts at the grid, not the old game wheel.
- Home L2/R2 cycles the same collections, including Pokémon and Multiverse.
  Each remembers its own chosen Adventure; Y only lists that collection's games.
  The large A button keeps the normal direct launch path.

## Boundaries and persistence

`SeriesCatalog.h` declares the initial series and conservative name matchers:
Mario, Zelda, Sonic, Final Fantasy, Metroid, Castlevania, Kirby and Metal Slug.
Unmatched names and recognized crossovers stay in Multiverse. Only populated
non-Pokémon series appear. This is a presentation classification over existing
registrations, not a filesystem move, new Adventure ID or new save format.

The existing per-Trainer navigation state now contains independent collection
selection/search/filter/focus state. Legacy explicit choices are retained in the
matching collection. Device library, emulator settings, file paths, media,
achievements, Adventure history and save ownership remain unchanged. Switching
collections uses the cached registration snapshot; it does not rescan ROMs.

Non-Pokémon collections currently provide library/Home/launch. This pass does
not fabricate species, Party, shops or save semantics for other franchises.
Companions retains its existing Pokémon Adventure context. #90's fully adaptive
experience faces and two shared-title/one override adapter proof remain open.

## Artwork and private content

Ten original environment illustrations are bundled under `assets/series`.
They contain no copied game sprites/logos; titles are rendered by the shell.
Generation prompts and tool provenance are in `generation.json`. Source games
and scraped game media are private device data and must not enter the repository.

## Delivery evidence

Verified 2026-10-04 on Retroid Pocket Flip2 and AYN Odin 2:

- Both run ARM64 binary SHA-256
  `e36cf6779986e63c36e4ef4da9a28c39a060d4c3ca8855dc23bcc312076dbbf8`.
  Installed atomically, restarted only TrainerOS, checked the running executable.
- Actual controller navigation/captures show the ten collection cards, automatic
  grid scrolling, scoped wheels, Home cycling and the scoped Y drawer. A from
  Flip's Sonic wheel launched Sonic Battle through ordinary mGBA, reached its
  animated title scene and returned through physical Home / Exit / Leave. This
  is a representative launch, not compatibility proof for every copied title.
- Native Windows and ARM64 builds passed `series`, `interactions`, and `library`
  CTest suites (3/3 on each). Coverage includes classification/crossover fallback,
  unchanged identity/paths, independent selection/search/focus, restart and owner
  isolation, legacy choice migration, Back/root re-entry and direct launch.
- Final bounded current-process journal scan on each found no ReferenceError,
  TypeError, binding-loop, assignment or load diagnostics. This is not a full
  performance benchmark. Collection switching uses cached data and bounded art
  decoding rather than filesystem scans.
- Screenshots are real handheld captures in
  [screenshots/series-2026-10-04](../screenshots/series-2026-10-04/README.md).

### Private library delivery

The existing collection was reused, then the owner's server supplied **6 new
files on Flip and 13 on Odin**. Existing matching copies were preserved. The
representative set covers Mario & Luigi / Mario Golf, Zelda (Minish Cap and
A Link to the Past), Sonic Battle, Final Fantasy Tactics Advance, Metroid
(Fusion/Zero Mission), Castlevania (Aria/Harmony), Kirby (Nightmare/Amazing Mirror)
and Metal Slug Advance. Broader pre-existing series libraries on Flip remain.

Copies were hash-verified. Three initially selected source files (Mario Kart
Super Circuit and Sonic Advance 1/2) had null GBA headers; a bounded launch exposed
that source-data problem. Only the newly added bad copies were removed through
ordinary library Delete on both devices. Valid-header Sonic Battle and Mario Golf
replaced them. Final audit verified all remaining added files and absence of those
three bad copies. The server originals, old device content and saves were not
changed. New individual game metadata/media remain dependent on available local
gamelists; generated series art does not pretend to be scraped game screenshots.

The owner-prioritized library/navigation delivery is complete. Resume MP-02;
keep the deeper #90 adapter and adaptive-face acceptance open.
