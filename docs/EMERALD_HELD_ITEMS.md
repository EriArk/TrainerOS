# Emerald held-item services - 2026-09-27

Bounded R4 Party/Boxes give, take and replacement for the exact allowlisted
English Emerald ROM. No FireRed, ROM-hack or other-platform write permission.

## Behavior

Open a known Party/Box Pokemon's actions, then Held item. The Bag lists verified
available quantities of eligible items. Take returns its current item. Giving
a different item previews both the recipient and the old item's return before
one explicit confirmation. Cancel preserves the selection and changes nothing.
A single protected transaction debits one new item, returns one old item and
updates the individual. No generated items, purchases, HP or PP treatment.

Mail/message links, eggs, unreadable individuals, unknown/important items,
key items, HMs, dynamic Enigma Berry and Battle Pyramid Bags are excluded.
Full pockets and missing stock refuse the whole candidate. The device-wide
read-only policy, runtime ownership, source/context revisions, automatic verified
backup, atomic replacement/readback and Center restore apply below the UI.

## Source and format

[pret/pokeemerald pinned source](https://github.com/pret/pokeemerald/tree/5eff78649e7170a877b961ef0b3da13b81a16038):
`src/item.c` Bag quantities/capacities; `src/item_menu.c` holding eligibility;
`src/party_menu.c` native remove-new/return-old order; `src/data/items.h` facts.
The [knowledge record](adapters/emerald-en.md) tracks findings and exclusions.

Latest checksum-valid unambiguous bank only. Party and Box encrypted Growth
held-item field and Pokemon checksum change; no identity, moves, stats, status,
other Pokemon, older bank or extra sectors change. Bag quantities use the
SaveBlock2 encryption key, normal stacks at most 99 and berries at most 999.
TM/berry stacks are unique. Only affected sector checksums are resealed.

The portable adapter snapshot includes the real byte transforms and factual
holdability table. Host-owned locks, confirmations and file protection remain
the receiving application's responsibility, as before.

## Verification

Synthetic coverage includes 24 Pokemon permutations, rotated sectors, Party and
Box give/take/swap, exact allowed-byte output, conservation, encrypted quantities,
berry limits, full Bag refusal, last-unit replacement, stale/unverified targets,
Mail/Egg rejection, protected restore and runtime/source changes during a write.
Actual Flip controller UI gave Cheri Berry to Blaziken (returning Charcoal), then
gave Charcoal to Bulbasaur in Box 1. Independent byte/protection checks passed.
Normal Emerald loaded the changed save, displayed Cheri Berry on Blaziken, and
wrote an ordinary in-game save. Independent readback verified all Party/Storage
records and Bag quantities in its newer valid bank. Taking the boxed Charcoal
returned it to Bag. Center restored the initial copied save byte-for-byte.
Personal save hashes remained unchanged throughout.

The full native suite passed (42/42). After the backup-shelf refresh correction,
all nine affected interaction/storage/QML checks passed again. On Flip the
new copy appeared immediately and restored successfully without manual refresh.
The portable Qt Core adapter builds separately and export drift checks pass.
The installed production build uses the existing schema 14 and preserves all
829 Adventures and three Trainers. Installed SDL picker/confirmation/cancel and
clean return to Home were checked; personal saves remained byte-identical.
