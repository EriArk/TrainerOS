# Emerald Journey and Champion history — 2026-09-27

Bounded #47/R4 delivery for the verified English Emerald build from the
[adapter registry](adapters/registry.json). FireRed, hacks and other formats do
not inherit these capabilities. This reader never changes an external save.

## Real data and presentation

Journey shows current ordinary-save hours/minutes, existing badges, validated
Seen/Caught counts and provider-defined First partner / Pokédex received / Hoenn
Champion flag states. Unknown time or unsupported milestones remain absent/unknown;
there is no percentage, invented order of badge acquisition or victory date.

A/X opens Champion records. Left/right cycles preserved teams; A/B returns to
Journey. L2/R2 retains Journey/Hall/RA route and selection; Y follows the shared
Adventure. Hall memories remain independently editable; RA remains external truth.
The Champion gallery uses existing installed illustrations, six colored mounts,
level and shiny status. Missing artwork is an honest placeholder.

## Exact source facts

Sources pinned to pret/pokeemerald
[`5eff78649e7170a877b961ef0b3da13b81a16038`](https://github.com/pret/pokeemerald/tree/5eff78649e7170a877b961ef0b3da13b81a16038):

- `include/global.h`, `include/constants/flags.h`: SaveBlock2 time at 0x0e/0x10;
  SaveBlock1 flags 0x1270, system flags 0x860, 0x861 and 0x864.
- `src/overworld.c::GetGameStat`: 32-bit gameStats at 0x159c XOR key at
  SaveBlock2 0xac. `GAME_STAT_ENTERED_HOF` is index 10.
- `src/hall_of_fame.c`: 50 x six x 20-byte historical members. OT ID,
  personality, 9-bit internal species / 7-bit level, ten-byte nickname.
  New records append, then shift the oldest record out when full.
- `src/save.c`, `include/save.h`: Hall sectors 28/29 each carry 3968 bytes.
  Signature 0x08012025 and folded-word checksum stored at **0xff4**, unlike
  ordinary save sectors. Both special sectors must validate.

The latest valid ordinary bank establishes live progression. Archive import also
requires game-clear, entry count 1–998, exactly min(count,50) contiguous valid
teams and valid species/levels. New-run old special sectors, corrupt/incomplete
Hall data and the saturated count 999 are not imported. Current Party is never a
fallback. No historical Dex totals, current playtime or exit screenshot is passed
off as belonging to an old victory. An existing corrupt Hall may be reinitialized
by the game with a mismatching counter; this bounded reader declines that import.

## Durable ownership and limits

SQLite schema 14 adds `champion_records`, separate from manual `hall_of_fame`.
The owner/build/Adventure/record key is immutable on duplicate observation; the
first observation time and source-save hash remain intact. All batch records
commit atomically through the existing worker. Profile deletion includes its
Champion rows; library edits and external-save rollback do not delete them.

The bounded save identity hashes canonical player name, gender and full Trainer
ID, excluding changing warp flags, playtime and encryption key. A record hashes
that identity, victory number and the original occupied Hall member bytes. A new
Trainer identity is separate; conflicting teams remain separate. Ordinary rolling
50-team history deduplicates by victory number, not changing storage index.

This is **not authenticated playthrough lineage**: copied saves are the same
identity, and independently fabricated runs with identical identities/teams cannot
be distinguished. #93/#94 still own that trust gate before Link. There is no claim
that an imported save proves when or how the user earned a victory. At saturated
count 999 new imports stop rather than merging distinct wins under guessed numbers.

## Verification

- Windows native suite: all 42 tests pass after the updated gallery navigation
  smoke scenario; affected parser/controller/UI checks rerun for final changes.
- Synthetic Emerald partial/current and completed cases; XOR counter, real Hall
  checksums, invalid second sector, mismatched/saturated counter, fresh-run stale
  Hall, current Party mutation, Trainer identity, mutable warp flags and 50-team rollover
  identity checks.
- Durable store/controller tests: repeated observations, owner isolation,
  independent connection read, Adventure switching, saved-team immutability and
  current-save rollback while history remains; manual memories remain separate.
- ARM64 production build on Flip, isolated copy of the existing ordinary save:
  live time 39h58m, 8 badges, 386 Seen/Caught; one historical six-member team.
  Its final Poochyena level 2 differs from today's Party, proving the gallery
  consumes Hall records rather than substituting current individuals.
- Final production binary installed and its running hash matched; database
  migrated to schema 14 with 829 Adventures / 3 Trainers retained. Personal and
  isolated save hashes remain unchanged. Binary/database rollback copies retained.
- Actual SDL navigation through Champion/Journey, Hall and RA, shared Y/back;
  actual handheld captures inspected. No external save writes are needed.

Full broader story/post-game milestones, authenticated lineage, other builds and
first-victory playtime are still separate research/acceptance work. #47 is not
closed wholesale by this bounded Emerald increment.
