# Emerald Party and Box management

The exact English Emerald build in the [adapter record](adapters/emerald-en.md)
now has three protected operations: swap occupied Party positions, move a
Pokémon to an empty slot in another/same box, and deposit/withdraw between Party
and Boxes. FireRed and other read-capable providers do not inherit these writes.

## Controller flow

Select a readable hatched member with A, choose **Move Pokémon**, select Party
or a named box, then select a position and confirm once. B returns to the previous
step without writing. No cursor movement writes a save. Team reorder swaps with
the selected occupied position; other transfers require an empty destination.
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

Do not remove the last living non-Egg team member. Do not overwrite an occupied
box slot or silently exchange a full team member with a stored Pokémon. Eggs
cannot be selected for movement in this first writer; they can remain in a
reordered team. A member carrying Mail must have it removed inside the game before
storage. Release, held-item management, box renaming and two-device Link remain
separate roadmap work, not hidden capabilities of this operation.

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
