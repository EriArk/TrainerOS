# Emerald Shops & Traders

## Accepted scope

[Issue #68](https://github.com/EriArk/TrainerOS/issues/68) adds merchants whose
availability follows the selected ordinary save. The owner's subsequent
2026-09-24 clarification supersedes its read-only restriction: these are usable
shops, with explicitly confirmed purchases that debit currency and add owned
items. Browsing and discovery never change the game save.

The first delivered slice covered eleven ordinary town/city Poké Marts: Oldale, Petalburg,
Rustboro, Slateport, Mauville, Verdanturf, Fallarbor, Lavaridge, Fortree, Mossdeep
and Sootopolis. Center → Activities → Shops & Traders opens the shop list.
Highlighting a discovered shop immediately previews its stock and prices; no
greeting screen intervenes. A moves into its stock (or a building's departments).
Up/Down selects shops/items; Left/Right changes quantity. A on an item opens the price and
remaining-balance confirmation, then A confirms. B cancels/unwinds; L1/R1 remain
global pages. All physical legends stay on the bottom chassis. Money and stock
refresh after a purchase. The room and clerk are original QML/Canvas art.

## Emerald facts and boundary

The exact English Emerald fingerprint and two-intact-slot write gate are shared
with [healing](EMERALD_HEALING.md). `EmeraldShops` consumes verified logical
SaveBlock1 and SaveBlock2, never raw paths. Money is XORed
with the full 32-bit key; Bag quantities use its low 16 bits. Normal Items, Poké Balls and TMs/HMs pockets, plus the eight decoration
inventories, have verified writers. The shop catalogue never offers HMs or key
items. TM quantities stop at 99 in one slot; they cannot spill into another slot.
Decorations use one byte per owned copy in their own category, not Bag slots. Existing occupied entries
must match their factual pocket and game stack bounds. A corrupt pocket, unknown
ROM, bad checksums or ambiguous save slots cannot enable a purchase.

Canonical data in `data/emerald-shops.json` is separate from save-derived state.
`tools/build-emerald-shops.py --source-dir <flattened-cache>` reproduces factual
IDs/names, prices, pocket IDs, shop stock and visited/expansion flags. Input
hashes and the pinned pret revision are in `emerald-shops-source.json`.
No upstream game code, dialogue, descriptions or graphics are bundled.

Sources at revision `5eff78649e7170a877b961ef0b3da13b81a16038`:

- [Shop scripts](https://github.com/pret/pokeemerald/tree/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps): per-shop lists and Oldale/Petalburg/Rustboro expansion conditions.
- [Item facts](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/data/items.h): prices and pockets.
- [Bag rules](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/item.c): existing stacks before empty slots, 99 per ordinary stack.
- [Purchase rules](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/shop.c): maximum 99 per purchase and one Premier Ball for 10+ Poké Balls if there is room.
- [News conditions](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/tv.c): Energy Guru discounts do not apply to ordinary town marts.

Purchases preserve unrelated counters, TV records, story, Party, boxes and the
older save slot. They do not simulate interacting with an in-game NPC or alter
the player's location. Current Battle Pyramid floor/top/interior saves keep
known shops browsable but unavailable; its separate Bag is not supported.

## Discovery and transaction

Visited flags reveal ordinary merchants; additional story flags select their
current stock. Hidden entries project only `???` / `Undiscovered`, with empty
identifiers, location and stock. Unknown providers expose no merchant data.
Known/unavailable remains distinct from undiscovered. Merchants carry their own
currency and observed balance: money, coins, Battle Points, ash or Berry Powder.
The stock, header and confirmation all use the selected merchant's balance.

A small local discovery cache is scoped to Trainer, Adventure, exact ROM, save
path and in-game Trainer identity. Its first scan establishes a quiet baseline;
later newly discovered shops produce a lightweight notice without repeating on
rescans. This cache never unlocks a shop and can be removed safely.

The existing storage worker handles both healing and shopping through one
protected replacement function: owner/context token, emulator exclusion,
operation lock, verified backup before writing, re-resolution before commit,
atomic replacement, synchronization and exact byte readback. Currency and items
are changed in the same image, never as separate writes. **Before purchase**
copies use the existing restore flow. Context changes discard stale UI results;
busy gates prevent repeated confirmation or unsafe launch/exit.

## Verification — 2026-09-24

The Release ARM64 build was installed on Flip 2 / ArmadaOS. Its SHA-256 is
`475919996a2be091436faf133028aa262ed316b3d71b342b7b4ec2a2264e1c5d`.
Physical-controller events exercised hidden shops, quantity, confirmation/cancel,
Start overlay/return, blocked target changes, purchase and backup restoration.
On an isolated Emerald save, buying ten Poké Balls changed money from 5000 to
3000 and added ten Poké Balls plus one Premier Ball. Independent byte comparison
confirmed only the intended fields/checksum changed and the protection copy was
exact. A normal mGBA game launch displayed those items and 3000 on the Trainer
Card. Restoring **Before purchase** through the installed UI recovered the exact
pre-purchase bytes. Personal originals and the 829-Adventure/three-Trainer database
were preserved. Handheld screenshots remain in private validation output.

Windows builds and the 42-test suite completed: 41 passed initially, with one
Center backup-focus assertion failing during parallel process/render checks.
That test passed two subsequent standalone runs. Final affected checks for
progress, backups, controller interactions, process persistence and QML passed.
New synthetic fixtures cover slot rotations/keys, counter wrap, unlock/stock
gates, malformed saves, full pockets, insufficient money, stack/bonus rules,
exact write boundaries, stale sources, protection/undo and scoped discovery.

## Money merchants expansion — 2026-09-24

This increment expanded the catalogue to **36 distinct counters**, including the original 11:

- Lilycove Department Store: ten counters across floors 2–5, rooftop vending and
  the temporary clear-out sale. The building appears as one destination with a
  second list of separate departments; B returns through that hierarchy.
- Slateport Market: Energy Guru, dolls, Secret Base decorations and Power TMs.
- Fortree's desk/chair counters; Lavaridge herbs; Pretty Petal plants.
- Pokémon League, Battle Frontier and Trainer Hill ordinary money marts.
- Mt. Chimney's Lava Cookie stall and Seashore House's paid Soda Pop.

The script inventory was checked across all 468 map script files. Money-to-coins,
ash and Berry Powder were deferred to the currency block below. Museum/Safari
entry fees are paid access, not persistent merchandise, and remain excluded.

Required story/landmark flags and hidden-NPC flags gate special merchants.
Trainer Hill retains its pre/post-Champion inventories. Saved Poké News state
controls the Energy Guru's half-price sale and rooftop clear-out availability;
this projects the chosen merchant's context without moving the player. Sootopolis
weather states 1–3 close both rooftop services. Reading does not advance the
in-game clock or event counters: availability reflects the last ordinary save.

Normal shops offer up to 99 items where money/space permit. Decorations and
single-item NPC/vending purchases are one per confirmation. Vending reproduces
1/64 odds of an extra drink, then 1/64 odds of a third, only if space remains.
TrainerOS supplies those draws independently; it does not rewrite Emerald's RNG.
The pure transformer accepts a draw for deterministic verification; production
supplies fresh draws at the protected purchase. Decoration IDs and item IDs carry
separate kinds so identically numbered objects cannot cross storage namespaces.

Additional primary inputs at the same pinned revision:

- [Decoration facts](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/data/decoration/header.h),
  [inventory rules](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/decoration_inventory.c)
  and [save layout](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/global.h).
- [Rooftop vending/sale](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps/LilycoveCity_DepartmentStoreRooftop/scripts.inc)
  and [TM/HM identities](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/constants/tms_hms.h).

### Expansion verification

The final Windows native build passed **all 42 tests**. Synthetic cases exercise
every offer across all 36 counters, department navigation, TM single-stack limits,
decoration category capacity and identity separation, news/weather gates and
deterministic vending bonuses. Cross-sector writes also run across every save-slot
rotation with both encryption-key fixtures. Regenerating the factual catalogue
from the pinned inputs reproduced both JSON files byte-for-byte.

The installed Flip build has SHA-256
`136ac0f8420ca499729dbba763e8e4de1f6a2430eaca87ad567c96a6ca7f0e35`.
Actual controller input bought TM38 Fire Blast, a Pichu Doll and Fresh Water in an
isolated test profile: 50000 became 41300. Independent byte comparison verified
only money, the intended slots and their checksums changed. Normal Emerald/mGBA
readback displayed Fire Blast and Fresh Water. A separate disposable readback
copy retained those purchases at the fixture's original saved position; normal
Fly/walking reached the bedroom PC, which displayed **Pichu Doll, 1/40** in its
decoration menu. No in-game save was made during readback. The installed Center
restored the exact pre-purchase save from its automatic protection copy. The two personal save
originals and schema-13 personal database remained intact.

## Currency exchanges — 2026-09-26

There are now **46 counters**. Ten new counters contain **58 offers**, all one
unit or coin bundle per confirmation, matching their individual NPC exchanges:

- Mauville Game Corner: 50/500 coins for money, five TM prizes, three starter dolls.
- Frontier Exchange: two decoration counters, vitamins and held items for BP.
- Glass Workshop: five flutes and two furnishings for collected ash.
- Slateport Market's Berry Powder trader: eleven medicine/vitamin offers.

Game Corner requires a carried Coin Case. Glass needs the Soot Sack and workshop
state 2; an already paid, uncollected in-game order blocks further exchanges
until collected in-game. Powder requires its received-jar flag and the jar.
Discovery alone does not bypass these conditions or create the required item.

Currency bytes in the verified logical save blocks:

| Currency | Location | Representation / limit |
| --- | --- | --- |
| Money | SaveBlock1 `0x490` | 32-bit XOR key / 999999 |
| Coins | SaveBlock1 `0x494` | 16-bit XOR low key / 9999 |
| Ash | SaveBlock1 `0x142c` (`VAR_ASH_GATHER_COUNT`) | Plain 16-bit / 9999 |
| BP | SaveBlock2 `0xeb8` | Plain 16-bit / 9999; lifetime/card points untouched |
| Berry Powder | SaveBlock2 `0x1f4` | 32-bit XOR key / 99999 |

The pure transformer produces both candidate blocks. The existing exact-build
writer replaces only changed sectors in the chosen slot and recalculates their
checksums; the protected transaction still commits one complete save. Currency
rewards have their own kind, so buying coins cannot add an identically numbered
Bag item. Balances and capacity are checked again from the fresh source.

Additional pinned primary inputs:

- [Game Corner](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps/MauvilleCity_GameCorner/scripts.inc)
  and [coin encoding](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/coins.c).
- [Frontier offers](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps/BattleFrontier_ExchangeServiceCorner/scripts.inc)
  and [BP operations](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/field_specials.c).
- [Glass orders](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps/Route113_GlassWorkshop/scripts.inc),
  [ash collection](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/field_tasks.c),
  [Slateport offers](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps/SlateportCity/scripts.inc)
  and [powder encoding](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/berry_powder.c).

### Currency verification

The Windows native build passed all **42 tests**, including all 58 new offers
with two encryption keys and cross-block writes at all 14 sector rotations.
The final confirmation-layout adjustment also passed `qml_smoke`. Regenerating
the catalogue reproduced both JSON files byte-for-byte.

The final ARM64 Release build installed on Flip has SHA-256
`cca64aba7ebf246ee09a3ec12e71ab41f7cad1b28035797f93691ef4dae446da`.
Actual controller input in an isolated test profile completed eight purchases:
50 coins, Double Team, Treecko Doll, Kiss Poster, Protein, Blue Flute, Pretty Chair
and Energy Powder. Independent full-save comparison verified exactly the reward,
wallet and checksum bytes; the other save slot and unrelated data stayed intact.
Final balances were money 49000, coins 2550, BP 283, ash 2750 and powder 4950.
The automatic pre-purchase protection copy matched its source byte-for-byte.

Normal Emerald/mGBA readback displayed the three Bag items, TM32, Coin Case 2550
and Powder Jar 4950. This increment did not separately navigate the in-game
Frontier BP display or decoration PC; their exact storage deltas were checked,
and the earlier decoration PC readback above remains evidence. No in-game save
was made. On the final installed build, Center restored the complete exact
pre-purchase save and retained the purchased state in its before-restore copy.
Personal save hashes and the schema-13 library remained unchanged.
The installed UI was captured on Flip, including immediate highlighted-shop
stock preview and the currency confirmation layout.

## Categories and Frontier lessons — 2026-09-27

The 48 discovered counters are split into Poké Marts, Stores, Specialists,
Exchanges and Services. Unknown counters stay in one neutral Undiscovered group;
their type and stock are not revealed. Left/Right cycles categories at the place
list, Up/Down selects a place and immediately previews its stock. Existing grouped
departments remain together. A enters stock; B returns one level. L1/R1 keeps
switching primary pages and all local controller legends stay in the footer.

Both Battle Frontier tutors now offer their complete **20 BP lessons**. Choose a
lesson, an eligible saved Party member and a move slot, then confirm the named
replacement and BP cost. Eggs, damaged records, incompatible species, already
known moves and HM replacement are refused. An empty slot is filled first.
The request carries the selected record's fingerprint; fresh eligibility and
funds are checked again inside the protected transaction.

The writer changes only the selected move, its base PP and PP-Up bits, the
individual checksum, BP and affected sector checksums. Other moves, held item,
condition, training data and friendship remain intact, matching this tutor path.
It reuses the existing exact-ROM identification, exclusive ordinary-save route,
automatic verified backup, source-revision check and atomic replacement.

Factual compatibility and lesson prices come from the same pinned source as the
existing catalogue, with input hashes retained in `emerald-shops-source.json`:

- [Both tutors and BP prices](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/data/maps/BattleFrontier_Lounge7/scripts.inc).
- [Species compatibility](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/data/pokemon/tutor_learnsets.h).
- [Move selection, HM protection and PP bonuses](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/party_menu.c),
  [move-slot PP initialization](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon.c).

### Lesson verification

The native suite passed all **42 tests**. The new cases exercise all 20 lessons
at all 24 Pokémon substructure orders, eligibility/refusal paths, empty slots,
selected-slot PP Ups, stale identity, insufficient BP, exact unrelated-byte
preservation and both save keys at all 14 sector rotations. Controller tests
cover categories, immediate preview, grouped places, recipient/move selection,
confirmation and busy/modal gates. Both generated JSON files reproduce exactly.

On Flip, controller input in an isolated test profile taught Blaziken Mega Punch
and Thunder Punch through the two tutors. An independent full-save comparison
proved only the permitted changes: BP 300 → 228, the two moves and PP, their
PP-Up bits and checksums. Normal Emerald/mGBA readback showed Mega Punch 20/20
and Thunder Punch 15/15 alongside the unchanged Sky Uppercut and Blaze Kick.
No in-game save was made. Center restored the exact pre-lesson save and retained
the learned state in its before-restore copy. Personal saves were unchanged.

The final ARM64 Release installed build has SHA-256
`abc3b9dfb5f1c64c175a5665aa798bcd4f82517a31e707dd22b95c8e53c8f908`.
Final category refresh/message-layout polish passed the affected progress,
interaction and QML smoke checks again. Installed screenshots verified the
category/stock view and lesson confirmation after that polish. Personal library
integrity remained valid (schema 13, 829 Adventures and 3 Trainers).

## Remaining #68 acceptance — retained

- Remaining special services/item payments, notably Heart Scale move relearning
  and the shard/stone and Shoal ingredient exchanges, still need their own exact
  eligibility, debit/reward and in-game proof. Frontier BP lessons are delivered;
  this does not enable arbitrary move editing or another game's tutor format.
- Optional item search across discovered merchants and location filters. The
  category browser above supplies the requested separation by shop/service type.
- Any remaining meaningful currency-to-owned-content sources found during those
  integrations; transport-only payments and pure paid entry stay excluded.

Other Emerald consumers, Party/Storage operations, other exact games, two-device
exchange and the final pack/credits work retain their roadmap gates.
