# Emerald practice: engine feasibility

2026-09-27, R4 / #55. **Research complete; practice is not enabled.** The current
handheld application and saved games are unchanged. The selected candidate is
Pokemon Showdown's offline Gen III simulator, subject to the gates below.

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

| Field | Current Party presentation | Required bridge work |
|---|---|---|
| Exact game/context and revision | Resolved by progress service | Bind a frozen pair to owner/content/save revision; reject unsupported games and changed context. |
| Species/form, level, ability, nature | Present | Explicit Gen III identifiers and edition form policy; preserve validation failures. |
| IVs/EVs | Decoded to calculate stats, then discarded | Export six numeric values in a declared order; necessary for stats and Hidden Power. |
| Friendship | Not exported | Growth block byte 9, 0-255; affects Return/Frustration. Never default to maximum. |
| Gender | Not exported | PID low byte plus exact species ratio; fixed genders/genderless need their own cases. |
| Moves | Display names/current and max PP | Export numeric move IDs and stable slot mapping. Retain PP Up count from Growth byte 8. |
| Held item | Numeric ID and name present | Validate the Gen III effect and reject unsupported mappings; no item debit/write. |
| HP and persistent status | HP plus a display string | Decide fresh-practice initialization explicitly; preserving saved status needs raw sleep/toxic distinctions, not the display label. |
| Six stats | Present | Compare the engine reconstruction against verified game values before starting. |

Showdown fills omitted IVs, gender and friendship itself. It also initializes
moves with maximum PP Ups. These conveniences cannot silently replace the save's
individual values. A semantic field map alone is not a production adapter.

## Rules boundary

The probe uses `gen3customgame`, singles, one member per side, fixed PRNG seeds.
Fresh copies start with full HP, healthy status and the engine's maximum PP;
the PP difference is deliberately detected, not silently accepted as final UX.
The public simulator accepts teams without full validation. Its research custom
format permits much broader inputs than this handheld feature should accept.

Gen III support does not establish full Emerald cartridge equivalence. Choose
and document the practice battle context before enabling it: Emerald's
`ShouldGetStatBadgeBoost` excludes Link/Frontier contexts but modifies some
ordinary player battles. Obedience, PRNG consumption, rounding/order, switching,
status counters, move edge cases and edition-specific forms retain native-proof
gates. A matching seeded Showdown replay is not matching Emerald's RNG stream.

## Reproduction and observed results

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

## Next gate

1. Extend the exact Emerald read-only record with the missing battle fields,
   with synthetic and private/native comparisons. Keep the reusable adapter copy
   synchronized. Do not make Pokémon fields mandatory for other franchises.
2. Build the pinned semantic bridge with correct PP/individual initialization,
   minimal bounded process I/O, cancellation and deterministic rule vectors.
   Add only demonstrated Emerald corrections. No filesystem/save capability in
   the battle worker; the host retains all ownership and recovery controls.
3. Then deliver the native controller-first 1v1 presentation using two real Party
   copies, validate on Flip against game situations, and prove saves byte-identical
   after finish/cancel/failure. No HP/PP/item/EXP/money/friendship/evolution rewards.

All other R4/R1-R18 commitments remain in [ROADMAP](ROADMAP.md).
