# Worlds collection catalogue

**2026-09-23 — #63:** the real collection now uses data-driven platform/edition
chronology instead of linked-first alphabetical order. Search and Linked/Missing
preserve that order; unknown years remain unknown. [Sources and boundaries](WORLD_CHRONOLOGY.md).

NES has a platform badge for user-provided unofficial releases and ROM hacks. It has no official Pokémon checklist editions. These records remain separate from official GB/GBC/GBA entries with similar names; local variants retain their own file and Adventure identities.

The 2026-09-11 user decision extends Worlds from an installed library to a collection checklist. Platform badges are explicitly requested on Adventure cards; region-first navigation and controller invariants remain unchanged. The user has also confirmed comfortable physical navigation, successful Adventure launch and profile creation on Flip 2.

## Accepted catalogue revision — planned 2026-09-13

Issues [#28](https://github.com/EriArk/TrainerOS/issues/28)/[#30](https://github.com/EriArk/TrainerOS/issues/30) intentionally revise the broad historical checklist described below. The target is a complete collection of **substantial Pokémon games with credible Flip/controller routes**, plus a separate **Multiverse** system browser for appropriate non-Pokémon/general crossover content. Pokémon remains region-first; eligible missing editions remain grey/linkable. Preserve useful spin-offs, translations, meaningful revisions and ROM hacks rather than treating their groups as a blacklist.

Review each candidate's role and runtime evidence before retaining, moving, hiding or excluding it. Promotional/browser fragments, distribution/accessory/test material, trivial utilities and unsupported/closed service clients do not belong in ordinary Pokémon Worlds merely to increase the catalogue count. Empty/weak groups can disappear only after their contents are reconciled. Unknown hack geography is not evidence that the hack belongs in Multiverse.

This changes reference eligibility/navigation, **not private data ownership**: keep excluded owned records, file links, saves/states/media/history and maintenance access. #18 separately removes only verified redundant personal files through its manifest/relink/protection workflow. U4 still audits every requested archive, later transfer and meaningful variant, including reasons for intentional exclusions. Existing edition counts and broad groups below describe the delivered baseline; no catalogue JSON or installed collection was changed by this replan. See [P3/P4 and preservation map](ROADMAP.md#unified-execution-order--existing-work-and-new-issues) and [detailed curation acceptance](EXPANSION_PLAN.md#library-domains-multiverse-and-catalogue-curation).

## Reference data and ownership

`data/catalogue.json` is a bundled, offline, versioned reference dataset. It contains title/platform editions and World relationships, not copyrighted game assets, ROMs, installation paths, saves or personal progress. `CollectionRepository` combines this reference with the personal repository; it does not seed hundreds of fictitious owned records into SQLite. The original sample preview remains isolated.

- A missing edition is grey but focusable. A opens its details; Link a file opens the existing controller file picker with title, edition, platform and Worlds prefilled.
- Save returns directly to the Adventure's detail. B discards the form and restores that action. L1/R1 retains its global meaning, including inside the picker and keyboard.
- A linked edition keeps a stable `catalogueId`. Multiple personal revisions can refer to it; the missing reference card disappears while all owned variants remain visible.
- Names are presentation metadata. Linking or renaming never moves or modifies external game/save files.
- A linked file and a working launch setup are separate facts. Unsupported setups remain disabled; they never become successful mock launches.
- Reference limitations are shown on details. No progress, badges, story completion or save location is invented.
- New custom Adventures and ROM hacks still use Manage Adventures. Platform and edition can be chosen there; a catalogue edition cannot silently change platform or become a different ROM hack.

The file picker accepts plausible platform file formats, while the persistence worker verifies a readable regular file. An archive or installable package can be registered without promising that an adapter can execute it. Catalogue format checks are not ROM identity verification; users remain responsible for choosing the correct edition.

## Coverage and device feasibility

### Browsing a large personal collection

Inside a World, X opens the shared controller keyboard to search titles, version labels and platform names/badges. Search is case- and accent-insensitive, and every space-separated term must match. Y cycles All → Linked → Missing; ownership filtering does not imply that a linked file is playable. Left/right jumps eight rows, clamped to the list ends; up/down remains single-row navigation. L1/R1 still switches primary sections.

The query, filter and selected Adventure are retained separately for each World and restored on restart. Cancelling the keyboard keeps the previous search; switching sections discards an unfinished draft. Empty results keep X/Y available and focus the mounted Back control. Filtering and searching never launch, edit, rename or remove an Adventure. Search/filter buttons are mounted in the bottom panel and use fixed physical-button actions rather than adding more stops to list focus.

Acceptance covers a 700-record library, additional World relationships, version/platform/accent matching, linked/missing intersections, clamped jumps, stale selection recovery and restart restoration. The rendered SDL scenario types a query using controller events, cancels drafts, exercises empty results and menu priority, switches sections and checks visible focus after jumps.

### Importing personal archives

On-device collection preparation keeps a private manifest with source names, checksums, file sizes, paths and separately editable presentation metadata. Exact duplicate files are omitted; N64 byte-order variants can be identified without modifying their bytes. Official reference matches and unverified/modified files remain distinguishable. Different releases of a ROM hack retain their own identities and version labels. Unknown settings stay in Fan Worlds until verified; the original game's region is insufficient evidence for a hack's setting.

The 2026-09-11 archive pass installed and verified 641 additional files on the existing microSD, bringing the personal library to 686 records before the separate crossover transfer. This is a file/release count, not 686 distinct games or a claim of universal compatibility. Patch-only releases, explicitly bad dumps, accessory tests and promotional material remain outside the playable library. User archives, manifests, ROMs, patches, BIOS and saves stay outside Git.

Region checks include [RainbowDevs](https://rainbowdevs.com/games/) for Brown/Prism, [Crystal Clear's documentation](https://github.com/ShockSlayer/ccdocs/blob/master/docs/Documentation.md), and [Gaia's guide and creator attribution](https://romhackguides.com/hacks/gaia/). Version-specific secondary regions require separate review; later releases do not establish the contents of an older beta.

The initial catalogue contains 216 title/platform editions: main-series releases, local spin-offs, Pokémon mini, selected substantial crossovers, educational PC/Pico titles, official Virtual Console packages and Android titles. Versions/regions/revisions of the same release remain variants, except genuinely distinct original Japanese Red/Green/Blue editions. Subscription emulation collections do not create a separate title for every subscription service. Expansions are part of their parent game, not independent launch targets.

This is a maintained checklist, not a claim that every historical promotional browser mini-game, language dump or community ROM hack has been exhaustively identified. Add omissions with a source and a plausible runtime route. The user explicitly excludes closed online games and prioritizes titles that can plausibly run on Flip 2. Do not add Switch 2 exclusives, arcade hardware without a suitable emulator, unreleased games, distribution-only packages, BIOS/keys, or unsupported historical online clients simply to increase the count.

ArmadaOS on the device provides Waydroid 1.6.3 and dedicated controller integration helpers, but its Android image is not initialized. Android cards therefore describe a runtime route, not verified app/account/sensor/controller compatibility. Online requirements, cameras and location services can prevent individual titles from working. Switch, PC and Pico entries also retain explicit device-validation limitations; none gets an enabled launch from its platform label alone. No user Android app is installed or executed by linking a package.

Worlds with known settings use those settings: Orre, Fiore, Almia, Oblivia, Ransei, Hisui, Lental, Ferrum, Pasio and Aeos alongside the nine main regions. Hisui also belongs under Sinnoh; Johto's paired adventures also appear in Kanto. PokéPark, Mystery Dungeon and Toy Pokémon get their own groups. Puzzle Corner, Pokémon Playroom, Card Club and Crossover Arena are organizational groups rather than invented canonical regions.

## Sources and review

Names are factual references, descriptions and hardware silhouettes are original. Cross-check title/platform editions against:

- [Official Japanese release index](https://www.pokemon.co.jp/game/) and [Nintendo's Pokémon Friends listing](https://www.nintendo.com/en-ca/store/products/pokemon-friends-switch/).
- [Pokémon game-series index](https://en.wikipedia.org/wiki/List_of_Pok%C3%A9mon_video_games) for international naming and series coverage; verify unusual editions against their individual sources.
- [Pokémon mini catalogue](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9mon_mini), [PokéROM disc editions](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9ROM), and [Sega Pico releases](https://bulbapedia.bulbagarden.net/wiki/Sega_Pico).
- [HarmoKnight's official description](https://www.nintendo.com/en-gb/Games/Nintendo-3DS-download-software/HarmoKnight-727424.html), [Blue Rescue Team's Wii U edition](https://www.nintendo.com/en-gb/Games/Nintendo-DS/Pokemon-Mystery-Dungeon-Blue-Rescue-Team-272387.html), and [Rumble U](https://www.pokemon.com/uk/pokemon-video-games/pokemon-rumble-u).
- [Official mobile index](https://www.pokemon.com/us/app), [TCG Live platforms](https://www.pokemon.com/uk/pokemon-video-games/pokemon-trading-card-game-live), [Magikarp Jump support](https://support.pokemon.com/hc/en-us/articles/18769981645076-Pok%C3%A9mon-Magikarp-Jump-Frequently-Asked-Questions), and [TCG Online closure](https://support.pokemon.com/hc/en-us/articles/4406895467668-Pok%C3%A9mon-TCG-Online-Sunset-Information).
- Runtime routes: [ArmadaOS](https://github.com/armada-os/armada), [Waydroid](https://github.com/waydroid/waydroid), [PicoDrive's ARM/Pico support](https://docs.libretro.com/library/picodrive/). These establish infrastructure, not per-game performance.

## Persistence and acceptance

Schema 3 adds non-null `platform_id`, `catalogue_id`, `variant` columns with empty defaults and a catalogue lookup index. Migration preserves profile identity, favorites, existing Adventure IDs/file paths/configuration and navigation. New reference Worlds are persisted only when a personal registration uses them. Catalogue updates never rewrite personal names or file references.

Acceptance checks cover reference ID/relationship integrity, absent-card actions, attaching and reopening, multiple variants without false duplicates, wrong-platform rejection, extra reference Worlds, missing external content and unchanged file bytes. A rendered SDL-controller scenario covers grey cards, platform silhouettes, file linking, Save/Cancel focus, scrolling and shoulder navigation at landscape handheld resolutions. Actual installation requires a fresh personal-database backup and hardware validation.
