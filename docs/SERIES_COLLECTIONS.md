# Game collections — #114

## Collections and global recent games — 9 October 2026

This supersedes the original series-scoped Home/Y and Pokemon region-card journey
below. Earlier installed evidence is historical, not the current navigation contract.

- **Collections** is the library primary. Pokemon opens its games directly across
  regions/platforms, using the same browser as other collections. **All games**
  retains platform browsing and includes games also present in series collections.
- Home has one chosen Adventure across the library, with no L2/R2 collection
  controls. Y is **Recent games**, one latest actual launch per game across all
  domains. It chooses the Home game without launching; A retains direct launch.
  When available, the latest observed resume point keeps its identity and the
  existing compatibility checks when that game is selected.
  Repository launch order survives clock changes; no per-domain 100-game cap.
  Missing selected games remain explicit rather than silently choosing another.
- Manual collections hold stable Adventure IDs. A game may belong to several;
  removing membership or a collection never removes/moves a ROM or save.
- Automatic views: Recently played, Favorites (gamelist favorite metadata), Not
  played yet, Multiplayer games (catalogue counts, not online eligibility).
  Personal dynamic collections combine title substring, platform, genre,
  publisher, developer, minimum players (2/4), played and favorite rules.
  Missing metadata does not match a metadata-dependent rule. Library refresh and
  new scraping metadata update the views; creating collections never starts scraping.
- New collection is available by touch in the grid and with Select. Select edits
  a selected personal collection; its ellipsis is the equivalent touch action.
  Game Options -> Collections changes manual membership. Draft edits can be
  canceled. Settings -> Library -> Collections replaces the retired Edit Worlds toggle.
- Definitions use an atomic, versioned JSON file under local state `collections/`,
  keyed by the active Trainer's hashed ID. They are separate from navigation/launch
  checkpoints and external files. Unreadable/unsupported files are kept and editing
  fails visibly. Explicit prior Home choices migrate without changing game IDs.
- Historical region data remains available to game-specific progress/history.
  The subsequent common UI is now planned in [UX-02](UX_OPTIONS_MAP_RU.md), not
  implemented by this library delivery. Existing series art and
  README screenshots are preserved; MP-02's timer changes remain outside this work.

UX-02 Select will resolve the focused game versus collection/header explicitly;
the current collection-management shortcut above is the installed baseline.
The existing library game-management menu is distinct from the new live-session
Game Options: only the latter controls a running process/minimize/Exit.

Reference: Batocera's [collection settings](https://wiki.batocera.org/game_collection_settings),
using manual membership and dynamic rules. This does not claim `.cfg`/`.xcc`
import/export or reuse its retired command-line helper.

### Verification and installation

- Windows native build and five focused suites passed: core, interactions,
  worlds, series and worlds_qml_smoke. The first rendered run exposed a missing
  host SVG image plugin; installing matching Qt 6.11.1 UCRT64 QtSvg resolved it,
  and the unchanged screen test then passed without QML warnings.
- ARM64 build succeeded. Thirteen affected suites passed across the bounded
  verification runs: history, core, interactions, worlds, pokedex, hall, storage,
  ownership, series, library, qml_smoke, downloads_qml_smoke and worlds_qml_smoke.
  Changed behavior was rechecked after fixes, including the final resume-identity
  change. New checks cover 121 unique recent games, clock changes, owner isolation,
  collection persistence/deletion/cancel, metadata rules, legacy Home migration,
  corrupt-file preservation and modal controller priority.
- Rendered SDL scenarios passed at 960x540 and 800x450 with no QML warnings.
  The exact delivered binary passed the isolated collections scenario on both
  Flip 2 and Odin 2. Fixture screenshots are not owner gameplay acceptance.
- Delivered binary SHA-256:
  `ac5427a5dc50aee56e704b9e28eb26d4edaa2c073d862d199310d70a94316c94`.
  The build includes the previously retained, uncommitted MP-02 timer work in
  `RuntimeMultiplayer.cpp`; that file is unchanged and excluded from this commit.
  This is not additional MP-02 acceptance.
  Flip's live process was restarted and its executable hash verified. Remote
  controller/pointer checks and actual 1920x1080 captures covered the collection
  grid, direct Pokemon list, creation/automatic-rule draft, platform rule picker,
  Start over the editor, Home without trigger cycling, and a recent-games drawer
  containing Pokemon and other systems together. The temporary draft was canceled.
- Odin's installed executable has the same hash. Its existing Steam Gaming Mode
  session was preserved; the new production TrainerOS UI applies on its next
  launch. The isolated on-device render passed, but a foreground Odin UI walkthrough
  is not claimed. No game, scraping job, ROM/save path or boot preference was changed.
- Private test logs/captures remain outside Git under `work/research/collections-*`
  and `work/research/ux01-flip-collections-*`; README screenshots remain unchanged.
  Broader UI redesign and owner physical-control acceptance remain separate.

## Historical series delivery

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

The original illustrations were replaced on 2026-10-07 with independently
conceived environments, removing explicit franchise-inspired props. Full prompts
and scope are in [the asset record](../assets/series/README.md). Collection IDs,
names, navigation, game media and saves remain unchanged. Historical captures
below show the earlier artwork; they are not evidence for the replacement.

Replacement delivery: ARM64 compilation succeeded and all ten PNGs were checked
at 1536 x 1024, with hashes added to the generation record. Binary SHA-256
`3eb498e8521ba3a95795765521b0a500c0a56e8a7e6668d691fd9b66ef729fed`
was installed on Flip and Odin. Flip's existing shell process restarted and its
running executable matched the hash; both grid positions were visually reviewed
on the actual display. No asset/QML errors appeared in the bounded current-process
journal check. [Unedited captures](../screenshots/series-2026-10-07/README.md)
show all ten cards. Odin was in Steam with TrainerOS closed: the installed file
was verified, its session was preserved, and visual acceptance there is not claimed.
No input/library/save behavior changed and unrelated multiplayer tests were not
repeated. Resume MP-02 after this asset replacement.

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
