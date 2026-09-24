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
Up/Down selects shops/items; Left/Right changes quantity. A opens the price and
remaining-balance confirmation, then A confirms. B cancels/unwinds; L1/R1 remain
global pages. All physical legends stay on the bottom chassis. Money and stock
refresh after a purchase. The room and clerk are original QML/Canvas art.

## Emerald facts and boundary

The exact English Emerald fingerprint and two-intact-slot write gate are shared
with [healing](EMERALD_HEALING.md). `EmeraldShops` consumes verified logical
SaveBlock1 and its SaveBlock2 encryption key, never raw paths. Money is XORed
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
Known/unavailable remains distinct from undiscovered. The generic currency enum
contains money, coins and Battle Points; only money purchases are verified now.

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

The catalogue now contains **36 distinct counters**, including the original 11:

- Lilycove Department Store: ten counters across floors 2–5, rooftop vending and
  the temporary clear-out sale. The building appears as one destination with a
  second list of separate departments; B returns through that hierarchy.
- Slateport Market: Energy Guru, dolls, Secret Base decorations and Power TMs.
- Fortree's desk/chair counters; Lavaridge herbs; Pretty Petal plants.
- Pokémon League, Battle Frontier and Trainer Hill ordinary money marts.
- Mt. Chimney's Lava Cookie stall and Seashore House's paid Soda Pop.

The script inventory was checked across all 468 map script files. Money-to-coins
belongs with the next Game Corner block. Museum/Safari entry fees are paid access,
not persistent merchandise, and are excluded. Ash and Berry Powder are distinct
future currencies; retain those merchants in the remaining acceptance.

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

## Remaining #68 acceptance — retained

- Game Corner coins/prizes, including the money-to-coins counter.
- Frontier BP vendors and their exact owned-item/service writers.
- Glass Workshop ash and Berry Powder merchants with verified currencies/storage.
- Optional item search across discovered merchants and location/type filters.
- Any remaining meaningful currency-to-owned-content sources found during those
  integrations; transport-only payments and pure paid entry stay excluded.

Other Emerald consumers, Party/Storage operations, other exact games, two-device
exchange and the final pack/credits work retain their roadmap gates.
