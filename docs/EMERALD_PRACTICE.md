# Emerald practice: native read-only sparring

2026-09-27, R4 / #55. **Native two-side practice is implemented for the exact
English Emerald reader.** Enter from Playroom with Up, choose two real Party
members and their moves, advance battle events with A, or leave with B. The
selected engine is Pokemon Showdown's offline Gen III simulator. The page labels
the mode "Gen III simulation"; complete cartridge equivalence is not claimed.
No battle result is written to a save or TrainerOS history.

## Candidate decision

| Candidate | Pinned evidence | Decision |
|---|---|---|
| pkmn/engine | `9b88fd6c5467f703c38951d5b2e8a660314d410b`, MIT | Its current work covers Gen I/II; Gen III/IV are a later stage. No Emerald engine to benchmark or integrate now. |
| Pokemon Showdown | npm `0.11.11`, npm gitHead `739a5e1fee432ad80ff7136d70cca993be358b59`, MIT | Gen III works on Flip ARM64 in an independent offline process. Continue with a bounded exact-game bridge. No server/browser UI. |

Primary sources:
[pkmn status](https://github.com/pkmn/engine/blob/9b88fd6c5467f703c38951d5b2e8a660314d410b/README.md#status),
[simulator protocol](https://github.com/smogon/pokemon-showdown/blob/739a5e1fee432ad80ff7136d70cca993be358b59/sim/SIMULATOR.md),
[Gen III rules](https://github.com/smogon/pokemon-showdown/blob/739a5e1fee432ad80ff7136d70cca993be358b59/data/mods/gen3/scripts.ts),
[individual initialization](https://github.com/smogon/pokemon-showdown/blob/739a5e1fee432ad80ff7136d70cca993be358b59/sim/pokemon.ts),
[MIT terms](https://github.com/smogon/pokemon-showdown/blob/739a5e1fee432ad80ff7136d70cca993be358b59/LICENSE).
The npm package differs from today's repository HEAD. The reproducible probe
pins the package and dependency hashes, not a moving branch. Undocumented
internal APIs need that pin and a small replaceable bridge.

**Owner clarification:** if the existing engine differs from Emerald, add
bounded game-specific battle modules after verifying the rules; a complete new
battle engine is not the default. Existing turn processing and supported rules
should be reused. New moves/abilities/edge cases get independent vectors and
native-game comparisons before being declared exact. This records the fallback,
not a claim any Emerald correction module is already implemented.

## Required individual data

Reference is the already pinned pret Emerald source
`5eff78649e7170a877b961ef0b3da13b81a16038`:
[record layout](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/pokemon.h),
[species facts](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/data/pokemon/species_info.h),
[stat/damage logic](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon.c).
Reuse the existing checksum/PID/block-permutation validation. Do not pass raw
save chunks, paths or write-service access to the battle process.

| Field | Reader/bridge after this increment | Remaining boundary |
|---|---|---|
| Exact game/context and revision | Frozen owner/Adventure/content/context/save identity; exact Emerald gate | Native consumer forwards source invalidations and rechecks the actual save before start/each turn and every 3 seconds while running. |
| Species/form, level, ability, nature | Present | Explicit Gen III identifiers and edition form policy; preserve validation failures. |
| IVs/EVs | Optional numeric battle traits, HP/Atk/Def/SpAtk/SpDef/Speed | Not inferred for other readers or fixtures. |
| Friendship | Exact Growth byte 9, including zero | Actual friendship initializes Return/Frustration. |
| Gender | PID low byte and pinned species ratio; M/F/N | Fixed gender and threshold boundary tests; no random engine default. |
| Moves | Four numeric IDs and PP Up counts, including empty slots | Numeric/name mapping validated; dynamic locked/Transform/Struggle choices tested. |
| Held item | Gen III factual ID/name mapped to an existing Gen III effect | Unknown effects and save-customized Enigma Berry rejected. No item debit/write. |
| HP and persistent status | Fresh healthy copies at full HP and actual maximum PP | Not a saved-battle resume. Saved current HP/status/PP are never changed. |
| Six stats | Compared before any switch-in or first request | Mismatch rejects the pair rather than replacing the save's values. |

Showdown fills omitted IVs, gender and friendship itself. It also initializes
moves with maximum PP Ups. These conveniences cannot silently replace the save's
individual values. A semantic field map alone is not a production adapter.

## Rules boundary

The bridge uses `gen3customgame`, singles, one member per side, explicit PRNG
seeds. Fresh copies start with full HP, healthy status and full PP computed from
their actual saved PP Ups. The pre-start hook updates base PP and PP Ups before
switch-in abilities or requests, including later Transform behavior.
The public simulator accepts teams without full validation. Its research custom
format permits much broader inputs than this handheld feature should accept.

Gen III support does not establish full Emerald cartridge equivalence. The UI
therefore identifies simulation mode under #55's approximate-rule allowance.
The intended practice
context is a fresh Link-style duel: no trainer badge bonuses or obedience, no
bag commands, no switching in this two-individual slice. Emerald's
`ShouldGetStatBadgeBoost` excludes Link/Frontier contexts but modifies some
ordinary player battles. Obedience, PRNG consumption, rounding/order, switching,
status counters, move edge cases and edition-specific forms retain native-proof
gates. A matching seeded Showdown replay is not matching Emerald's RNG stream.

## Original feasibility results (before the bridge)

[Original probe and locked dependencies](../tools/research/emerald-practice/README.md).
No copyrighted game assets, raw saves or proprietary fixtures are committed.

- Windows x64 and Flip ARM64: all probe assertions pass. Gen III Physical Shadow
  Ball / Special Bite, Normal Clefairy, Speed Deoxys reference, known stat/nature
  vectors, Hidden Power IV extremes, Return friendship extremes and invalid move
  rejection are exercised. This is bounded rule coverage, not all moves/items.
- Replay hash matches across both platforms after excluding wall-clock markers:
  `47af2b7f8ab3ffc29603c5f4b9d95e2bd2e03c85da2ad9f671b865193ba241d0`.
- Independently decoded private Emerald Party: all six members' six stats match
  the simulator reconstruction. One move's maximum PP differs from Showdown's
  default. Two copied members complete deterministic practice without changing
  their source input. This private decoder is research, not an installed reader.
- Representative Flip run, Node 24.18.0 Linux ARM64: module load about 105 ms;
  first battle construction about 13 ms after Gen III data loading; short-battle
  median 2.3 ms, p95 4.7 ms over 100 battles. Separate process ready in about
  620 ms. About 173 MiB RSS at first battle, 224 MiB process peak during the run.
  Startup/module/first-battle timings have different boundaries; do not sum them
  as a total startup measurement. Longer battles and UI frame pacing are unproven.
- A separate stdio child is killed and reaped while awaiting a player choice.
  No Node process remains afterward. No Showdown server/listening port is started.
  The portable Node runtime exists only in the private research directory.
- Original-save hashes, database integrity and the installed TrainerOS binary
  remain unchanged; schema 14, 829 Adventures, 3 Trainers. No app deployment or
  new visual screen belongs to this feasibility increment.

## Bridge increment evidence (before the native consumer)

- `PokemonRecord::battle` is optional, populated only by the Emerald reader.
  Synthetic records cover all 24 encryption block permutations, IV/EV order,
  numeric attacks and PP Ups, friendship, gender thresholds and fixed genders.
  Eggs, corrupt records and unsupported readers do not gain battle facts.
- `emeraldPracticePair` freezes two different known Party slots from exact
  available Emerald progress. The reusable adapter export includes this pure
  projection; it carries no save path, raw record or writer.
- `PracticeSession` is a separate host-owned QProcess with a 5-second response
  deadline, bounded JSON, one in-flight request and child cleanup on completion,
  cancellation, failure or source change. Trainer, Adventure, content, context
  and save revisions are all compared. This API is tested but **not yet wired
  to the shell's practice screen/source signals**.
- The worker accepts start/turn/cancel only, not raw simulator commands or eval.
  Two individuals, four move slots, level 1-100, bounded IV/EV/friendship and a
  maximum of 200 turns constrain work. Unsupported input fails closed. No Node
  server is started; runtime/dependencies remain in the private development
  directory. Node is a normal process, **not an OS filesystem sandbox**; its
  protocol exposes no file/save operation. Production runtime packaging remains
  part of the native consumer delivery.
- The pinned engine's defaults are replaced before start, not after its first
  request. Zero friendship, mixed PP Ups, sparse slots, multi-turn locks,
  Transform's temporary moves and exhausted-PP Struggle have executable checks.
  No broad Emerald rule correction is claimed from this initialization work.
- Flip ARM64: native C++ reader/process service completed five battles spanning
  all six actual Party members. IVs/EVs, friendship, gender, move IDs/PP Ups and
  six stats matched an independent private decoder. A changed save revision
  reaped the child and cleared the state. Source saves remained byte-identical.
- Windows and ARM64 bridge replay match:
  `ae4cfeef13f009ca97fc95b90e6da71c6bb346ae1eb9c9736d359e07bf38db9e`.
  Synthetic child tests also exercise missing runtime, timeout, crash, malformed
  and oversized output, and cancellation during startup. Dependency checks are
  separate from native tests; skipped integration tests must not imply proof.
- Delivery verification: full host suite 44/44, followed by the affected lifecycle
  test after the final completed-result owner invalidation fix. Both standalone
  Qt Core adapter targets build; the portable copy includes the actual bridge,
  worker and dependency lock. Installed/running Flip application SHA-256:
  `281091a149bd2980a738aa2b43a5d49a702112db0ea2be9dbdab0afc484ab2e9`.
  Database remains schema 14, 829 Adventures and 3 Trainers, with integrity checks
  passing. The engine is still exercised through the opt-in native probe, not
  enabled as a user-facing practice capability. There is no visual change here.

Reproduce with the locked dependency installation documented beside the original
probe, then `node tests/EmeraldPracticeEngineTests.cjs`. Qt tests are
`game_progress` and `practice_session` (the latter uses the pinned dependency
when installed). The optional `TRAINER_BUILD_PRACTICE_PROBE=ON` builds
`trainer_practice_probe ROM SAVE NODE WORKER ENGINE_ROOT`; it verifies the ROM
hash, reads the ordinary save, runs copied pairs and checks original bytes.
Its detailed report is private test data and must stay outside Git.

## Native consumer and offline runtime

`PracticeController` freezes both actual Party presentations, validates the pair,
collects both sides' move choices and translates bounded engine events into
ordinary battle messages. Private/public split events are displayed once; exact
HP/status updates occur with their events. Selection excludes the same slot on
both sides. Source state and saved HP/PP remain untouched. There is no AI claim:
the owner controls both sides, as accepted in #55.

`GameProgressService::verifySnapshot` reads on its existing worker without
replacing the public snapshot or triggering a Checking/Available loop. The host
also checks active Trainer, Adventure, emulator and protected service activity.
A failed or late check cannot start a closed screen. Leaving a primary/secondary
page or using Home cancels the child; ordinary Start retains modal priority.
The runtime exists only while a battle needs it, with no network listener.

The explicit [installer](../tools/install-practice-runtime.py) installs verified
Node 24.18.0 plus the locked 0.11.11 dependency, worker and factual reference in
the application data directory's `practice/emerald-v1`. It stages replacement,
keeps a previous bundle and retains dependency licenses/file hashes. The app
does not fetch packages or silently fall back to another engine.

Tests cover two-participant selection, Back, same-slot rejection, late async
checks after leaving, owner changes, actual turns, source mismatch and immutable
input. Snapshot verification additionally covers changed/malformed saves without
changing the public observation. Existing timeout/crash/malformed child checks
remain. Installed Flip controller proof covers selection, a complete two-turn
battle, returning to Playroom and cancelling another battle with physical Home;
the child was reaped and personal saves remained byte-identical. Device renders
are captured from the production shell, with no development preview or fixture.

Validation: Windows native build and the 44-test baseline were exercised. Its
transition-notification and obsolete preview-focus failures were fixed; the
affected interaction, practice lifecycle and two QML suites subsequently pass
(4/4, including zero diagnostics QML warnings). The other 42 baseline checks
passed. The pinned engine replay still matches the bridge evidence above;
portable adapter verification passes. Final installed ARM64 application SHA-256:
`c07274ef5fdfffd1db0979d2e806f74002ffe73203169fb723c13c98653f0bc3`.
Schema 14, 829 Adventures and three Trainers are preserved with DB integrity
checks. The source-state tests include unavailable input with a focused Back
action; no preview fixture can enable a real battle.

## Remaining gates

1. Expand exact rule coverage with native-game comparisons and add only demonstrated
   Emerald corrections. Keep ordinary save writers and trust-sensitive Link separate.
   Existing reference/PP/stat vectors and private reader agreement are not a
   complete cartridge battle comparison; approximation remains explicit.
2. Add more battle presentation/effect coverage only where supported; no claim of
   every cartridge animation/message, battle switching, AI or saved-battle resume.

All other R4/R1-R18 commitments remain in [ROADMAP](ROADMAP.md).
