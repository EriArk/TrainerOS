# Pokémon Emerald — exact English GBA build

## Identity and existing evidence

SHA-256 `a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af`.
Runtime: verified RetroArch/mGBA ordinary-save route. Raw save: 131072 bytes.
ROM length is not an identity gate; the full digest is. Other languages,
Ruby/Sapphire, hacks and FireRed do not inherit this writer.

Canonical evidence: [progress](../GAME_PROGRESS.md),
[Party/Boxes](../EMERALD_PARTY.md), [Dex](../SAVE_POKEDEX.md),
[healing](../EMERALD_HEALING.md), [shops](../EMERALD_SHOPS.md),
[device-wide policy](../SAVE_POLICY_FIRERED.md).
These dated documents retain their original delivery boundaries.

## Technical map

Two 14-sector save banks start at 0 and 0xe000. Sectors are 0x1000 bytes;
footer section ID/checksum/signature/counter offsets are 0xff4/0xff6/0xff8/0xffc.
Signature is 0x08012025. Section payloads: 0=0xf2c, 4=0xf08, 13=0x7d0,
others=0xf80. Sum little-endian 32-bit words, then fold to 16 bits.
Sector order rotates; resolve by section ID, never physical position.
Reads can recover one valid bank. Writes require both valid, unequal,
unambiguously ordered counters (including modular wrap). Preserve older bank,
counters, padding and extra sectors.

SaveBlock1 concatenates sections 1–4 (0x3d88 bytes): party count 0x234,
six 100-byte party records from 0x238. Storage concatenates sections 5–13
(0x83d0): current box byte 0; 14×30 80-byte records from **4**, box names
from 0x8344 (9 bytes each). Padding after the current-box byte matters.

Individuals: PID 0, original trainer ID 4, nickname 8, language 18,
flags 19, checksum 28, encrypted 48-byte payload 32. XOR each word with
PID XOR trainer ID; four 12-byte logical blocks are permuted by PID modulo 24.
Checksum sums clear 16-bit words. Growth contains species/item/experience/PP Ups;
attacks contain move IDs/current PP; EV and miscellaneous blocks hold EVs,
IVs, Egg/ability bits and other identity. Party adds status at 80, level 84,
mail 85, current HP 86, max HP and five stats 88–99.

Dex caught/seen: SaveBlock2 0x28/0x5c, duplicate seen fields in SaveBlock1
0x988/0x3b24. Badge flags 0x867–0x86e relative to flags at 0x1270.
Do not infer individual ownership/history from aggregate Seen/Caught.

The canonical shop map is `data/emerald-shops.json`, generated from pinned
source by `tools/build-emerald-shops.py`; currency/inventory/discovery/write
rules are documented in [shops](../EMERALD_SHOPS.md). Do not copy stock into
another registry. Factual species/stats/moves/encoding are
`data/emerald-reference.json`; both `*-source.json` files record input hashes.

## Sources and dated findings

Primary source: pret/pokeemerald revision
`5eff78649e7170a877b961ef0b3da13b81a16038`:
[layouts](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/pokemon.h),
[individual conversions](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon.c),
[storage behavior](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon_storage_system.c),
[save format](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/save.c).

- 2026-09-27 audit: existing reads and protected healing/shop transactions have
  synthetic, private copied-save, Flip, normal-game and rollback evidence in
  the documents above. Read support alone never enabled box edits.
- 2026-09-27 movement research started: three bounded capabilities—party reorder,
  box-to-box movement, party/box transfer. Release, item manipulation and Link
  remain separate gates. Consult exact game conversion rules before coding.
- 2026-09-27 primary-source finding: `BoxMonToMon` copies the 80-byte individual,
  clears status/current/max HP, sets MAIL_NONE and recalculates stats. Storage
  refuses to remove the last living non-Egg party member. These are external
  source findings at this point, not yet local or physical movement proof.

## Open gates

Bounded move semantics and normal-game readback are now verified below.
Mail/Egg transfers are deliberately refused. Protected release is verified below.
Held-item edits, rename, Link/two-device exchange, battle practice and immutable
Champion history remain separate gates without proof from this record.
No save parser certifies provenance, authenticity, legitimacy or achievements.

### 2026-09-27 movement implementation findings

Pinned `SetPlacedMonData` refills boxed PP, `BoxMonToMon` clears battle status,
sets mail to 255 and computes full HP/stats. **Deoxys correction:** its displayed
Speed-form stats differ from the cached base-species stats written by the game;
the withdrawal helper must use base-species cache calculations. Box transfers
require an empty destination in this bounded implementation. Party reorder swaps
whole 100-byte records; box moves preserve individual identity and refill PP.
Eggs and Mail transfers stay unavailable; no implicit release or replacement.
Synthetic exact-output, rotations/permutations, stale-save, last-able, mail,
occupied destination, bank corruption and protection/undo tests added. Device
and normal-game movement verification still pending at this checkpoint.

### 2026-09-27 copied-save device result

Actual Flip controller operations completed reorder, deposit (Party 1 to Box 14
slot 30), box transfer (Box 14 slot 30 to Box 13 slot 30), and withdrawal to
Party slot 6. Independent reconstruction matched each entire output, including
the original Swellow cached stats on withdrawal, and found a byte-exact protection
bundle before each operation. Personal originals retained their hashes. Normal
Emerald loaded the intermediate output and displayed five remaining members.
Normal Emerald also loaded the final round trip: Swellow appeared in Party slot
6 with 305/305 HP, alongside the other five correct members. UI restore remains
the last transaction check at this checkpoint.

### 2026-09-27 completed movement delivery

Center UI restored the initial copied save byte-for-byte after all four movement
operations. Every original personal-save hash remained unchanged. The final
production build is installed on Flip; controller checks covered its destination
grid, compact confirmation and cancel without changing the personal save.
The full Windows suite passed 42/42; final layout/focus checks passed 8/8.
ARM64 production build and installed/running SHA-256 match:
`94f7122a7a0fa95bf70ac6c5ea722cfffca2c551d46e28c49b1b3294c1ec9389`.
See [movement evidence](../EMERALD_MOVEMENT.md) and the
[reusable adapter](implementations/gen3/README.md). Raw ROM/save evidence remains
private. The Emerald input ROM length was measured as 16,777,216 bytes.

### 2026-09-27 release research started

Register a separate release capability before implementing it. Verify native
release restrictions (Egg/Mail, last able member and traversal moves), held-item
semantics and exact removal/compaction. Candidate writes must use the existing
protected transaction and include an independently confirmed source revision.
Movement capability alone must not enable release. Device/game proof is pending.

Native source findings: `pokemon_storage_system.c` at pret revision
`5eff78649e7170a877b961ef0b3da13b81a16038`, `MENU_RELEASE`,
`PurgeMonOrBoxMon`, `InitCanReleaseMonVars`, `RunCanReleaseMon` and
`IsRemovingLastPartyMon`: reject Egg/Mail, keep another able Party member and
at least two total individuals; preserve a second known Surf/Dive holder,
also Strength/Rock Smash at League 1F/2F. `map_groups.json` places those maps at
16/10 and 16/14; `global.h` places location group/number at SaveBlock1 +4/+5.
[Map IDs](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps/map_groups.json),
[save layout](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/global.h).
Release removes held items with the individual; it does not return them to Bag.
Party compacts and decrements count; boxes keep positions. Dex history remains.
TrainerOS additionally rejects unreadable collection records and inconsistent
Party padding/counts; no write decision is inferred from movement support.

### 2026-09-27 release integrated

[Release contract and evidence](../EMERALD_RELEASE.md): separate provider flag,
Party/Box release with deliberate X confirmation, protected transaction and Center
rollback. Synthetic all-permutation/rotation/boundary tests, real Flip SDL controls,
independent full-byte delta checks, normal Emerald load + in-game save and exact
Center restore passed. Personal saves remained unchanged. No FireRed/hack release
capability follows from this proof. The portable source/profile copy is refreshed.
