# Home progress from in-game saves

The user's 2026-09-13 save trial exposed a missing connection: imported saves loaded in the Adventures, while Home's badge/caught fields still had no real provider. This increment introduces the first bounded automatic-progress module on the existing library, launch/return and verified save-resolution foundations.

## Supported observations

Home reads the eight badge flags and the count of registered National Pokédex species from **English Emerald and FireRed (original and Rev 1)** through the verified RetroArch/mGBA save resolver. A complete ROM SHA-256 match is required. Names, catalogue entries and original ROM headers do not establish compatibility. Other editions, languages, hacks, Diamond and Colosseum retain unknown progress with an explicit explanation; being launchable is a separate capability.

Home displays the badge count, eight original geometric badge indicators and Caught. Its source caption says **Last in-game save · National Pokédex**. This describes the ordinary cartridge save, including when an older emulator moment is selected. It does not claim live RAM/state progress, regional-only counts, party size, completion, or achievement unlocks. Recorded time remains TrainerOS's observed process duration.

The local trainer journal, manual Pokédex, Hall of Fame and RetroAchievements remain separate records. Reading a downloaded save does not award achievements or invent a capture/victory history. No game assets or save files are distributed with this module.

## Boundaries and refresh

Badge presentation uses an explicit verified `badgeSet` (Kanto or Hoenn), independent of emulator/provider identifiers and display-name matching. Eight original vector crystal cuts have individual colors and beveled facets. They sit in shaped recesses in a wider tray; unearned badges expose the empty recess. Unknown data does not render eight unearned badges. No ripped badge sprites are packaged. The larger crystals are decorative, so Home retains its fixed A action and Y selector without adding focus stops.

`GameProgressProvider` exposes one identified observation to the shell. `GameProgressService` performs worker-thread I/O through an injected save resolver. `Gen3Progress` only interprets immutable bytes after content identification. QML sees normalized values and a source/availability message; it never opens files or knows binary offsets.

Reads run on startup, when Home's selected registration changes, when entering Home, after library changes, after Adventure launch/return, and after save-service activity. In-flight results are discarded after selection changes, save operations or launch. Shell actions and launch remain usable while a read runs.

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
