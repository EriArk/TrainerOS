# Emerald Party and Box management

The exact English Emerald build in the [adapter record](adapters/emerald-en.md)
supports protected Party reorder, moves to empty box slots, Party/Box transfers
and explicit occupied-slot exchanges. FireRed and other read-capable providers
do not inherit these writes.

## Controller flow

Select a readable hatched member with A, choose **Move Pokémon**, select Party
or a named box, then select a position and confirm once. B returns to the previous
step without writing. No cursor movement writes a save. Team reorder swaps with
the selected occupied position. Occupied Box/Box and Party/Box destinations
open a two-member exchange confirmation; empty destinations remain ordinary moves.
Withdrawal uses the first empty team position, avoiding holes. Party/Boxes retain
their normal L2/R2 relationship outside the modal. During the move picker the
source stays fixed; global navigation/Start/Choose Adventure cannot replace it.
An in-flight write finishes before its result can be dismissed.

The grid and illustrations use the existing local artwork provider. All contextual
controller hints remain in the footer. A smaller confirmation panel avoids a
nearly empty full-screen question. Inspection and save refresh preserve box focus.

## Protected behavior

The feature sends semantic positions and the displayed save SHA-256 through
`SaveBackupService::movePokemon`. QML has no paths, offsets or emulator commands.
The existing exact owner/runtime resolver, service exclusion, persistent read-only
policy, verified protection bundle, repeated stale-token checks, atomic file
replacement and readback all apply. The separate raw-save digest prevents a fresh
service inspection from silently replacing the older observation the user chose.
Copies are labelled **Before moving Pokémon** and use Center's existing restore.

Party reorder preserves complete 100-byte records, including status, HP and Mail
association. Deposit compacts remaining members and zeros the last position;
withdrawal appends a reconstructed 100-byte record. PC placement restores PP;
withdrawal initializes status, HP, level, stats and MAIL_NONE according to the
game. Identity, experience, held items, Pokédex, flags, currencies, unrelated
individuals, names/wallpapers, older bank and counters remain unchanged.
Only changed section checksums are rebuilt. The output passes the normal reader.

Do not remove the last living non-Egg team member. Never discard an occupied
box slot or silently exchange a full team member with a stored Pokémon.
An explicit exchange prepares both replacements together, as described below. Eggs
cannot be selected for movement in this first writer; they can remain in a
reordered team. A member carrying Mail must have it removed inside the game before
storage. Release and held-item management are delivered separately. Box renaming and
two-device Link remain separate roadmap work.

## Verification

`GameProgressTests` covers exact output, all 24 individual permutations and all
14 sector rotations, Party reorder, deposit, last-box/last-slot movement,
withdrawal, full HP and PP, cached Deoxys stats, stale observations, wrong build,
invalid banks, last-able-member refusal, Mail, Eggs and occupied destinations.
`SaveBackupTests` covers policy, stale tokens, exact protection/undo, unsupported
targets and failed backup storage through the same transaction. Interaction tests
cover confirmation/cancel, repeated A while busy, exact source request, owner
replacement and box focus across observation refreshes.

2026-09-27 device checkpoint: actual controller events on Flip performed Party
reorder, deposit and a transfer from Box 14 to Box 13 on an isolated copied save.
An independent Python reconstruction matched the complete resulting file after
each operation, checked sector checksums, exact pre-edit protection and unchanged
personal-save hashes. Normal Emerald accepted the result and displayed the five
remaining team members. Withdrawal, restore and final installed-build evidence
are recorded below when completed; this checkpoint alone does not claim them.

### Final delivery — 2026-09-27

Withdrawal to Party slot 6 completed through the same controller UI. Normal
Emerald loaded the result: Swellow appeared last with 305/305 HP and the other
five members intact. Center restore then returned the original copied save
byte-for-byte; personal-save SHA-256 hashes stayed unchanged throughout.
The existing isolated runtime helper closed the validation emulator; this run
does not claim fresh physical Home-button exit proof.

Windows full suite: **42/42**, final focus/layout checks: **8/8**. ARM64 production
build is installed, with matching installed/running SHA-256
`94f7122a7a0fa95bf70ac6c5ea722cfffca2c551d46e28c49b1b3294c1ec9389`.
Database integrity passed: schema 13, 829 Adventures, three Trainers. Actual
installed Flip captures show the movement destination grid and compact final
confirmation. The final installed check cancels before a personal-save write.

[#91 knowledge and reusable source](adapters/README.md) record exact game bounds,
primary-source findings, known exclusions and separately buildable code/profiles.

Protected release is delivered separately; see [release and rollback evidence](EMERALD_RELEASE.md).

## Occupied swaps - 2026-09-27

A known hatched Pokemon can exchange places with another readable hatched
Pokemon in a box or Party. This permits replacing a member of a full six-member
team without a separate deposit. The same Move picker shows both portraits,
names and current locations, then requires **Swap places**. B returns to the
same destination without writing. Contextual A/B hints remain in the footer.

The pure request carries explicit `exchangeOccupied` intent, false by default.
Without it, the old empty-slot requirement remains. The provider advertises
`canSwapOccupied` only for verified English Emerald; other read providers do not
inherit it. Both converted records are prepared before altering the candidate.
The Party count and positions of other members remain unchanged. Native PC
placement restores PP; withdrawal reconstructs HP/status/stats while retaining
boxed PP, identity, held items and other individual data. The resulting Party
must remain readable with an able non-Egg member. Egg, Mail and damaged targets
are refused; empty slots are not exchange destinations.

Source: pinned [pret/pokeemerald storage implementation](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon_storage_system.c),
`SetShiftedMonData`, `SetPlacedMonData`, `CanShiftMon`. This is an atomic composed
swap, not the game's intermediate cursor-held state. The existing owner/runtime
lock, read-only policy, source digest, verified backup, atomic write/readback,
lineage and Center restore cover it. No new persistence schema is introduced.

Validation: all 24 individual permutations and 14 save-sector rotations,
sector-crossing occupied records, full-Party exchange in either direction,
explicit intent, capability gating, stale source, Eggs, Mail, damaged targets,
confirmation cancellation and repeated input. Actual Flip controller input swapped
Blaziken/Bulbasaur between Party and Box 1, then swapped Blaziken/Ivysaur within
Box 1. Independent checks compared the whole output file with the expected
allowed delta and verified sector/Pokemon checksums. Normal Emerald displayed
Bulbasaur first with 20/20 HP; an ordinary in-game save retained all six Party
and all Storage records. Center restored the copied baseline byte-for-byte.
Original personal save hashes remained unchanged. Test game was closed through
the existing owned-window helper; no new physical Home-button proof is claimed.


Delivery checks: the 43-check native suite was run; its one interaction-fixture
failure was corrected by supplying a new observation revision when changing test
data. The final affected interaction/rendered checks passed **8/8**. No production
cache behavior was weakened. The portable Qt Core adapter builds independently;
source/profile drift and exact-build knowledge checks pass.

Production ARM64 installed/running SHA-256:
`c2d62d1a9db8a299db6cc90f7b17a1a96dee311fdd047b931dd4762bd76c56dc`.
Installed Party/Box and Box/Box confirmation captures and controller cancellation
were checked on Flip without changing personal saves. Database integrity passes
at schema 14, 829 Adventures and three Trainers; final shell returned to Home.
