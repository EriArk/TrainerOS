# Home progress from in-game saves

**Accepted expansion, not delivered — #42/#50:** extend this exact-build read-only baseline with small semantic Party/Storage/Pokédex/Journey/Champion providers and independent guarded writers. No generation-wide support or editor capability follows from current badge/count reads. [Provider/research gates](EXPANSION_42_62.md#exact-save-providers-and-research). #9 will supply one shared context; #46/#47 projections follow proven fields. Old moment comparisons below explain legacy behavior superseded by #49.

The user's 2026-09-13 save trial exposed a missing connection: imported saves loaded in the Adventures, while Home's badge/caught fields still had no real provider. This increment introduces the first bounded automatic-progress module on the existing library, launch/return and verified save-resolution foundations.

## Supported observations

**2026-09-24 extension:** the exact English Emerald reader also supplies
[real Party and fourteen-box Storage](EMERALD_PARTY.md) to Center. FireRed
Party, selected-save species progression and all save writers remain separate.

Home reads the eight badge flags and the count of registered National Pokédex species from **English Emerald and FireRed (original and Rev 1)** through the verified RetroArch/mGBA save resolver. A complete ROM SHA-256 match is required. Names, catalogue entries and original ROM headers do not establish compatibility. Other editions, languages, hacks, Diamond and Colosseum retain unknown progress with an explicit explanation; being launchable is a separate capability.

Home displays the badge count, eight original geometric badge indicators and Caught. Its source caption says **Last in-game save · National Pokédex**. This describes the ordinary cartridge save, including when an older emulator moment is selected. It does not claim live RAM/state progress, regional-only counts, party size, completion, or achievement unlocks. Recorded time remains TrainerOS's observed process duration.

The local trainer journal, manual Pokédex, Hall of Fame and RetroAchievements remain separate records. Reading a downloaded save does not award achievements or invent a capture/victory history. No game assets or save files are distributed with this module.

## Boundaries and refresh

Badge presentation uses an exact semantic `badgeSet` (`kanto-frlg` or `hoenn-rse`) emitted only by the matching verified build provider. A separate asset layer maps ordered badge identities to credited vector-derived artwork, independently of display names or emulator IDs. Unearned badges are subdued; unknown identities and missing sets use neutral mounts rather than another game's badges. The informational tray adds no focus stops. [Artwork provenance and later in-game comparison](../assets/badges/README.md).

`GameProgressProvider` exposes one identified observation to the shell. `GameProgressService` performs worker-thread I/O through an injected save resolver. `Gen3Progress` only interprets immutable bytes after content identification. QML sees normalized values and a source/availability message; it never opens files or knows binary offsets.

Reads run on startup, when the shared current Pokémon Adventure changes, when entering Home or Center, after library changes, after Adventure launch/return, and after save-service activity. Multiverse Home does not replace this Pokémon source. In-flight results are discarded after selection changes, save operations or launch. Shell actions and launch remain usable while a read runs.

The source is reopened and compared after reading; ROM/integration identity is revalidated before publishing. Files must be ordinary 128 KiB raw saves. Additional containers and RTC trailers are not inferred. Both rotated fourteen-sector slots are checked for unique section IDs, consistent counters, signatures and edition-specific checksums. The newest complete slot wins, including counter wrap; if only one slot is intact it supplies the observation. Ambiguous equal counters with different payloads are rejected.

Missing, unsupported or unverifiable observations show unknown fields and a message. Previous values are cleared when checking a replacement rather than presented as its current progress. Observations retain provider/version, content hash, save hash and observation time in memory, and are rebuilt from external saves after restart. They are derived data; no SQLite migration or save write is required.

## Format evidence

The independently implemented reader uses format facts from pret's matching game decompilations: [Emerald save structures](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/global.h), [save-sector implementation](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/save.c), [Emerald badge flags](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/constants/flags.h), [FireRed structures](https://github.com/pret/pokefirered/blob/master/include/global.h), and [FireRed badge flags](https://github.com/pret/pokefirered/blob/master/include/constants/flags.h). It copies no game implementation or assets.

The local full-file SHA-256 values below were paired with the matching SHA-1 references in the [Emerald](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/README.md) and [FireRed](https://github.com/pret/pokefirered/blob/master/README.md) build manifests:

| Edition | SHA-1 | SHA-256 |
| --- | --- | --- |
| Emerald English | `f3ae088181bf583e55daf962a92bb46f4f1d07b7` | `a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af` |
| FireRed English | `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc` | `3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac` |
| FireRed English Rev 1 | `dd5945db9b930750cb39d00c84da8571feebf417` | `729041b940afe031302d630fdbe57c0c145f3f7b6d9b8eca5e98678d0ca4d059` |

## Acceptance

- Synthetic, original fixtures cover both layouts, zero versus unknown, rotated slots, counter rollover, checksum failure, fallback, duplicate IDs, inconsistent counters, ambiguous slots, truncated/oversized files and wrong layouts.
- Filesystem/service tests verify unchanged original bytes, full fingerprint gating, unsupported hacks/adapters, missing/replaced sources, registration changes, stale worker completion and decreasing progress after a save replacement.
- Optional private examples are read through `TRAINEROS_PROGRESS_SAMPLE_DIR`; no commercial saves enter Git. FireRed and Emerald examples must both yield eight badges and 386 registered species, matching their in-game menus.
- The rendered SDL Home scenario exercises badge/caught projection, source caption, per-badge indicators, unrelated/unsupported observation rejection, fixed A/Y behavior and global navigation.
- Device acceptance requires the installed build to display these real values on Home, refresh after return and preserve ordinary launch/navigation. Host tests alone do not satisfy that boundary.

### Delivered validation — 2026-09-13

The production build was installed on Flip 2. Controller selection displayed **8/8 badges and 386 caught** on Home for both English FireRed Rev 1 and Emerald. Emerald was opened normally from Worlds, loaded from its in-game Continue menu, returned to TrainerOS, selected for Home with Y/A, then resumed with Home's fixed A action. Home refreshed to the same verified values after return. Unsupported Ruby displayed the explicit availability message. The six imported save targets retained their original checksums throughout this validation; the existing FireRed backup was verified again.

The initial progress increment passed all 29 Windows tests and all 30 tests on both the Ubuntu/Qt 6.4.2 build server and the ARM64 Flip. Windows and ARM64 production builds with testing disabled also passed. Rendered SDL scenarios were inspected at handheld and full display sizes. Reports, device captures, downloaded examples and installation/database backups remain private and excluded from Git.

The subsequent crystal presentation update passed seven affected checks on each of Windows, the Ubuntu server and ARM64: game progress, persistence/process scenarios, and five rendered QML/controller scenarios. Visual inspection covered both complete badge sets, mixed earned/unearned badges and an empty tray at 960×540 and 1920×1080. The production ARM64 build was installed and both FireRed and Emerald sets were inspected on Flip, including Y/A selection and shoulder navigation.

Party, map labels, per-species journal import, automatic historical events, DS/GameCube readers and ROM-hack profiles remain separate validated increments.

### Badge asset presentation - 2026-09-19

The existing exact-build readers now identify `kanto-frlg` or `hoenn-rse`; their verified flag offsets/order and supported ROM fingerprints are unchanged. The independent `BadgeAssets` layer resolves ordered identities and credited art; Home uses recognizable vector-derived badges instead of original crystals. Unsupported sets/count-only/unknown values never choose another game's artwork. No new save support or schema migration is implied. See [delivery and remaining in-game comparison](EXPANSION_63_64.md#first-badge-delivery---2026-09-19) and [asset provenance](../assets/badges/README.md).

## Per-Trainer save follow-up - 2026-09-23

The first [GBA/mGBA per-Trainer ordinary-save route](TRAINER_SAVES.md) binds
launch, existing progress observations and backup shelves to the active Trainer.
Original legacy saves/backups remain in place; new Trainers start fresh. Other
emulator save routes remain shared until verified separately. This supersedes
earlier statements that all external saves are still shared.
