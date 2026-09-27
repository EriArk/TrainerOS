# Protected Emerald release

The exact English Emerald build listed in [the adapter record](adapters/emerald-en.md)
is the only release target. Movement and release have separate capabilities.
FireRed, hacks and unknown revisions remain unavailable for this operation.

## Behavior and protection

Party/Boxes → A → Release Pokémon opens a compact identity confirmation with
nickname, species, level, slot, illustration and held item. **X releases; B keeps**.
A repeats, held A and ordinary navigation cannot confirm. The shared operation
modal blocks page/face/Home/Start/Adventure changes, and owner/context changes
cancel a pending confirmation. The action is absent for Eggs, unreadable/empty
slots, samples and device-wide read-only mode.

An exact raw-save revision binds the selected individual. The existing protected
transaction resolves the current Trainer/runtime route, rejects a running game,
locks the service, checks source stability, validates candidate bytes, creates
and rereads a pre-release backup, checks again, atomically replaces and rereads.
Failed protection prevents the write. The Center labels these backups separately.

Native rules come from [pret/pokeemerald storage source](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon_storage_system.c):

- No Egg or Mail release; another able non-Egg remains in Party.
- At least two individuals remain across Party and Storage.
- Another Pokémon must know each restricted move held by the released individual:
  Surf/Dive everywhere, Strength/Rock Smash at League 1F/2F (maps 16/10, 16/14).
- Held items leave with the Pokémon, without a Bag refund. Confirmation says so.
- Party compacts and decrements count; box slots stay in place; Dex history stays.

TrainerOS additionally refuses unreadable collection records and inconsistent
Party count/padding. It does not guess whether an unreadable Pokémon can satisfy
a traversal safeguard. Only the verified latest bank's affected payload/checksum
bytes change; the old bank, unrelated records, Bag, Pokédex, counters and extra
sectors are preserved. This is an exact-build operation, not a generic slot eraser.

## Verification

Synthetic tests cover all 24 substructure permutations, 14 sector rotations,
Party compaction, first/last and cross-sector box slots, full-byte expected output,
wrong ROM, stale bytes, damaged bank, Eggs/Mail, last able member, collection
minimum and location-dependent restricted moves. Service tests cover read-only,
unsupported/runtime-changing routes, stale source, failed protection and exact
backup restoration. Controller tests prove separate X confirmation, A repeat,
B cancellation, in-flight duplication and owner/Adventure cancellation.

### Actual Flip, 2026-09-27

The production ARM64 candidate ran with an isolated data/save directory. Printed
Switch-position SDL events exercised the action menu, identity confirmation,
repeat A, Home/Start/shoulder/trigger/Adventure modal gates, B cancellation and X
release. Swellow was removed from Party slot 2 and Bulbasaur from Box 1 slot 1.
Independent byte construction matched the complete 128 KiB candidate after each
operation, including checksums, older bank, other Pokémon and held-item semantics.
Both pre-release bundles matched the respective full source SHA-256.

A normal Emerald launch displayed the five retained Party members. An explicit
in-game save generated a new valid bank: all five retained 100-byte records and
the entire Storage block matched the candidate, including the empty Bulbasaur
slot. This run did not navigate the game's PC UI. The existing isolated-runtime
helper closed the emulator; no new physical Home-button exit proof is claimed.

Controller-driven Center restore returned the full initial copied save exactly,
including both released Pokémon. Personal-save hashes stayed unchanged throughout.
Windows full suite: **42/42**. The standalone exported adapter also rebuilt and its
read-only example verified the private exact ROM/save. The ARM64 production build is installed and its running/installed SHA-256 matches:
`d7fc4ad7ae2f1d2911efb3a33d7a691cf9d5799ca2766588cfdbc760f85da67a`.
Database quick-check passed: schema 13, 829 Adventures and three Trainers.
Actual installed Flip captures show the action menu and identity confirmation;
this final check cancelled before any personal write and returned to Home.
