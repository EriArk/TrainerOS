# Pokémon Emerald — exact English GBA build

## 2026-09-28 nearby Link and ROM-specific Dex

[Link contract and device evidence](../EMERALD_LINK.md): local Wi-Fi pairing,
full-team friendly battles and protected exact Emerald exchanges/sales run
on Flip/Odin. Actual ordinary-game readback passed on both; reconnect recovered
an interrupted exchange. Sales/gifts and twelve native trade/item evolution rules
are implemented; gifts also completed between the devices, while individual
evolution readback is recorded in the Link evidence. Full movesets are kept,
without a move-replacement chooser.
Wider Link remains partial (no Egg/Mail,
Unown/Spinda, cross-edition or competitive-trust claim). Pure `EmeraldLink` semantic
conversion/candidate generation is included in the portable copy; host pairing,
protection and bilateral durable commit are separate.

`gen3PokedexScope` declares the Ruby/Sapphire/Emerald-compatible National range
001–386 with native Gen III forms/types/base stats. Later species and forms are
excluded; other adapters retain full-catalogue fallback. This is ROM capability
metadata, independent of whether a verified save already exists. Current delivery
supersedes older dated Link-open checkpoints below only within this scope.

### Portrait battle workspace and protected stakes

The exact adapter exports eligible individuals from Party and all fourteen Boxes,
reads the supported eight-medicine Bag subset, and generates verified battle
settlement candidates: money/Pokemon win/loss, draw/cancel and consumed medicine.
It preserves older banks and ordinary Party HP/PP. Host reservations/checkpoints
and two-sided commit remain platform services, not portable adapter powers.
The pure functions and exact profile are included in the synchronized reusable
copy. See the Link record for real money/medicine and Box-stake device evidence.

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

## Journey / Champion research and delivery (2026-09-27)

Exact English Emerald only. Research covered ordinary-save playtime, semantic
League progress and separate Hall-of-Fame team storage. No current-Party
substitution or fabricated victory dates. Archival deduplication is integrated;
authenticated lineage retains its later independent gate.

Source findings (pret/pokeemerald pinned `5eff78649e7170a877b961ef0b3da13b81a16038`):
- `include/global.h`: SaveBlock2 name/gender/TID at 0/8/10, playtime at
  0x0e/0x10; SaveBlock1 flags 0x1270 and XOR-keyed gameStats 0x159c.
- `src/overworld.c::GetGameStat`: full 32-bit XOR with SaveBlock2 key 0xac.
- `src/hall_of_fame.c`: 20-byte members, six per team, up to 50 teams;
  packed 9-bit species / 7-bit level; rolls oldest off. No victory dates.
- `src/save.c`: special sectors 28/29, each 3968 bytes, signature 0x08012025;
  their checksum is stored at footer **0xff4**, not ordinary 0xff6.
  Normal save writes do not rewrite Hall sectors. Entry count stat 10 caps at 999.
- Archive gate must check current game-clear flag, unsaturated count and matching
  contiguous team count. Never use current Party or current Dex totals as a
  historical team/progress snapshot. TID/name/gender is a bounded save identity,
  not the future #93/94 authenticated lineage; preserve conflicting teams separately.

Sources: https://github.com/pret/pokeemerald/tree/5eff78649e7170a877b961ef0b3da13b81a16038

Integration: see [Emerald Journey](../EMERALD_JOURNEY.md) for the read-only
capability, immutable owner archive, device findings and lineage/count limits.
The configured portable source copy includes these models and parser code.

## Held items - 2026-09-27, integrated

Exact English Emerald only. Protected Bag-to-Pokemon give/take and atomic
replacement for Party/Boxes, reusing the protected file transaction. Mail, eggs,
unknown items and other editions remain outside the writer until proven.

### Held-item source findings

Pinned pret/pokeemerald `5eff78649e7170a877b961ef0b3da13b81a16038`:
`src/item.c` (AddBagItem/RemoveBagItem), `src/item_menu.c` (GiveToParty/GiveToPC),
`src/party_menu.c` (SwitchItemsFromBag) and `src/data/items.h` establish:
- Holdability is item importance and pocket policy, not a nonzero hold effect.
  Ordinary medicines, balls, TMs and berries can be held. Key items and HMs cannot.
- Quantities use the low half of SaveBlock2's encryption key. Berries stack to
  999; other supported pockets to 99. TM/berry stacks must remain unique.
- Replacement removes the new item first, then returns the old item. Failure to
  return it must leave the entire source untouched.
- Held item is Growth substructure offset 2; its update changes only that field
  and the Pokemon checksum, preserving encrypted ordering, identity, PP and HP.

This bounded writer excludes Mail/message associations and dynamic Enigma Berry.
Battle Pyramid locations use their own Bag and are rejected. No other editions
inherit this capability.

[Held-item contract and evidence](../EMERALD_HELD_ITEMS.md): all-permutation
synthetic checks, protected service tests, actual Flip controller give/take/swap,
independent byte/quantity checks, normal Emerald load and in-game save, and exact
Center rollback passed. Personal saves remain unchanged. Entering Center backups
refreshes the shelf after a Party/Box operation. The portable code, tables and
exact-game profile are synchronized with the implementation.

## Occupied swaps - 2026-09-27, integrated

Exact English Emerald only: occupied Box/Box and Party/Box exchange, with
explicit two-member confirmation. Native storage swap conversion and last-able
member constraints are covered by source review, byte tests and normal-game proof.
Other editions and unreadable/Egg/Mail transfers remain outside this scope.

Source review: pinned pret/pokeemerald `pokemon_storage_system.c` functions
`SetShiftedMonData`, `SetPlacedMonData` and `CanShiftMon` show atomic composition
of two placements: storage placement restores PP, withdrawal initializes Party
HP/status/stats, and the resulting Party must retain an able non-Egg member.
TrainerOS prepares both records before any write and requires explicit occupied
exchange intent; it excludes Egg/Mail and damaged records in either direction.

[Occupied swap delivery](../EMERALD_MOVEMENT.md#occupied-swaps---2026-09-27):
24 Pokemon permutations, 14 sector rotations, full Party, sector-crossing slots,
explicit-intent/missing-capability gates, stale/Egg/Mail/corrupt rejection and
controller cancel/repeat checks. Real Flip exchanged Party/Box and occupied
Box/Box records; independent exact-delta checks passed. Normal Emerald loaded
Bulbasaur in the first Party position with 20/20 HP and saved all changed
Party/Storage records unchanged. Center restored the copied input byte-for-byte.
Personal saves remained unchanged; portable source/profile copy refreshed.

## Box names - 2026-09-27, integrated

Exact Emerald English only. Controller entry and protected native names are
integrated; [contract and evidence](../EMERALD_BOX_NAMES.md).

Pinned pret `5eff78649e7170a877b961ef0b3da13b81a16038`:
`include/pokemon_storage_system.h` defines BOX_NAME_LENGTH=8 and fourteen
nine-byte names at storage 0x8344, followed by wallpapers at 0x83c2.
`src/naming_screen.c` uses that limit; `charmap.txt` defines native glyphs and
0xff EOS. These names all belong to logical sector 13. Encode through the
existing reference table, disallow EOS/control characters as input, preserve
unused slot bytes and update only that sector's checksum in the current bank.
ASCII apostrophe aliases the game's 0xb4 right quotation mark.

All fourteen name slots/rotations pass synthetic preservation checks. Flip copy
rename, normal native load/save readback and byte-exact Center restore passed;
personal files stayed unchanged. No native PC naming-screen UI claim.

## Selected-form collection - 2026-09-27, integrated

Pinned pret `src/pokedex.c` GetSetPokedexFlag stores National species flags.
`include/global.h` Pokedex has one first-seen Unown/Spinda personality, not a
complete form-history checklist. Current form ownership must come from verified
Party/Storage records, not Caught flags or that first appearance. Reuse existing
individual PID-based Unown and edition-specific Deoxys readers. Exclude Eggs;
invalid records make the affected area's exact total unknown. Never union
observations across rollback or Trainer/source changes. This slice adds read-only
current-form counts; no save edits or per-form historic capture claims.

[Delivered form-count scope and checks](../EMERALD_DEX_FORMS.md). The reusable
adapter exposes independent optional Party/Box maps; no database migration or
writer capability was added. FireRed does not inherit this Emerald-only slice.

## Practice feasibility - 2026-09-27, partial

Evaluate the accepted pkmn/engine and Pokemon Showdown candidates for the exact
English Emerald first. Check generation coverage, rule differences, required
individual fields, licensing, ARM64 cost and isolated execution. Existing
presentation records do not yet prove a complete battle input. No practice
capability is enabled in the handheld by this research.

[Completed engine/field feasibility](../EMERALD_PRACTICE.md): pkmn lacks Gen III;
pinned Showdown 0.11.11 runs on ARM64. Six private Party stat reconstructions
match, but one PP maximum differs. IV/EV, friendship, gender, numeric moves and
PP initialization need an explicit semantic reader/bridge. Practice stays
disabled; no exact full-battle, native rules or production consumer claim. Owner
approves small Emerald-specific rule modules where demonstrated gaps require
them. Current generic Party presentation and save writers are unchanged.

### 2026-09-27: read-only practice bridge verified on Flip

The actual C++ reader now preserves optional IV/EV, friendship, PID-derived
M/F/N gender, nature/ability IDs and four move IDs/PP Up counts for Emerald.
Pinned factual species tables add gender ratios/ability IDs; the source revision
is unchanged. The portable copy includes the pure exact-game pair projection.

Five native ARM64 copied-pair battles cover all six members of the private Party;
all individual fields/stats match the independent decoder. A stale save revision
cancels/reaps the child; originals stay byte-identical. PP default mismatch is
resolved before battle start. Sparse/locked/Transform/Struggle choices and child
failure/timeout/oversize cleanup have checks. See [bridge evidence](../EMERALD_PRACTICE.md).

Practice remains partial: the native screen/source bindings, production runtime
packaging and cartridge rule comparisons are still open. No save writes/rewards
or general battle-equivalence claim follows from the process bridge.

### 2026-09-27: native practice consumer delivered, rule coverage partial

Two-side controller play now consumes the verified pair bridge. The installed
offline Node/Showdown bundle starts only for a battle. Read-only source checks
run before starting/turn submission and periodically while active; source or
navigation changes discard the copied match. No save writers or rewards.

Flip completed a real-Party match through the installed shell, returned to
Playroom, and cancelled another with Home. The engine process was reaped;
personal saves stayed byte-identical. UI uses ordinary battle text and exact
HP event updates, suppressing the engine's duplicate public split-log lines.
The Gen III simulation label distinguishes bounded supported practice from a
full Emerald cartridge-rules claim; native damage/status/item control cases
remain the next gate. Updated [evidence and runtime recipe](../EMERALD_PRACTICE.md).

### 2026-09-28: ordered battle presentation annotations

The pinned bridge now annotates log events with stable roster identity and move
type. It does not change simulator action order or save transformations. The
native consumer plays exact intermediate HP/status changes in order and applies
generic effects. Speed, priority and KO response tests pass; actual two-device
Link captures verify switch/attack/damage order. The portable engine copy is
refreshed. [Bounded evidence](../EMERALD_LINK.md#ordered-turn-playback-and-effects-2026-09-28);
full cartridge parity and per-move animation coverage remain open.

### 2026-10-01: existing exact-pair adapter exercised over Bluetooth

No adapter/source/profile behavior changed. Both installed handhelds completed
the protected Alakazam/Swellow Party exchange over RFCOMM, then loaded the received
individual in ordinary Emerald. A subsequent full-team battle completed normally
at turn 20; both protected settlements applied the mutually approved money stake
and Odin's one used Full Restore. Independent before/after decoding showed the
entire ordinary Party unchanged during the battle. No active transaction remained.

This adds transport-specific device evidence, without promoting the partial
Link capabilities to universal/native/competitive support. Bluetooth sales/gifts,
other exact pairs, cartridge rule parity, extended fault recovery and #93/#94
remain open. [Evidence and actual-device limits](../NEARBY_PLAY.md#bluetooth-emerald-activities--2026-10-01).


### 2026-10-01: public-route exchange and one-sided commit recovery

The unchanged exact-pair adapter completed a real Flip/Odin Party exchange over
public Fluxer messages. Ordinary Emerald displayed the received individual on
both consoles. A reverse exchange was interrupted with Flip committed and Odin
prepared; explicit reinvitation recovered the same durable transaction. Both
full save hashes then matched their pre-test originals and no active journals
remained. No adapter/profile/portable source changed. Both handhelds shared one
home internet, so separate-internet proof remains open alongside broader fault,
trust and exact-pair gates. See [bounded evidence](../ONLINE_LINK.md#protected-public-route-exchange-and-interruption-2026-10-01).
