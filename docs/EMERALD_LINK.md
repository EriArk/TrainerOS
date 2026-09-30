# Emerald Link — implementation and evidence

**Current pairing UX:** [Nearby play](NEARBY_PLAY.md) supersedes the same-router,
both-pages-open and matching-code instructions below. Bluetooth discovery uses
Trainer names and acceptance popovers. The older proof remains historical;
the protected transaction/battle engine boundaries below are unchanged.

## Bluetooth activity evidence — 2026-10-01

The exact English Emerald pair on installed Flip 2/Odin 2 has now completed a
mutually confirmed Alakazam/Swellow exchange over Bluetooth RFCOMM. Both protected
receipts completed, and ordinary Emerald loaded the received individual on each
device. The same accepted connection survived both game launches/guarded exits
and carried a subsequent full-team battle to a normal turn-20 victory.

Both players approved stakes of 1000 in-game money each. Flip's final balance
increased by 1000 and Odin's decreased by 1000; Odin's one used Full Restore
was charged exactly once. Both current save hashes matched their completed
battle receipts, no active transaction remained and both entire Party records
were byte-identical to the battle's protected originals. Copied battle HP/PP
were not written to ordinary saves.

These were controller-injected actual-device runs using the existing production
binary and adapters, not new parser/rule work or owner manual-button acceptance.
Wi-Fi remained enabled for SSH; the application Link sockets were loopback-only
Bluetooth bridges. This covers this pair's bounded exchange/battle settlement,
not Bluetooth sales/gifts, other radios/games, cartridge parity, extended fault
recovery or competitive authentication. See
[transport proof and remaining gates](NEARBY_PLAY.md#bluetooth-emerald-activities--2026-10-01).

## Portrait workspace, Bag and stakes ? 2026-09-28

Link now keeps one activity rail and one working area, with portrait offers and
Party/Box selection. Battles retain the arena while the lower action area switches
between attacks, team and medicine; no separate full-screen Bag/team route.

Select on the host's review cycles no stake / in-game money / one Pokemon each.
Money changes directly with arrows. Both players see and approve the same proposal;
a terms change revokes previous acceptance. Pokemon can come from Party or any of
the fourteen Boxes of the selected verified Emerald save. Unsupported records,
last able Party removal, funds/capacity overflow and full receiving storage fail
before the match. This is not cross-game staking.

X opens teammates, Y opens the actual saved Bag. Supported medicine: Potion,
Super/Hyper/Max Potion, Full Restore, Full Heal, Revive and Max Revive. Select a
medicine and a target in the same lower panel. Using one consumes a turn and one
real item; it does not also attack or spend attack PP. These are TrainerOS casual
battle rules inspired by Emerald's medicine behavior, not cartridge Link parity.
Other battle items remain unsupported. Copied battle HP/PP/status and damage are
not written to the ordinary save; only the agreed stake and used medicine are.

Before starting, both sides create verified protected reservations. The host
persists each completed turn before publishing it; checkpoints retain monotonic
consumption and request IDs. Terminal results become bilateral prepared/commit
receipts using the existing protected save service. B asks to concede inside the
arena; confirming awards the agreed stake to the opponent. An unfinished network
interruption requires the same peers to reconnect: it voids the stake but charges
medicine in the last durable host checkpoint. A durable finished result is
completed on reconnect. This is trusted casual play, not an anti-cheat system.

Medicine references: [native item use](https://github.com/pret/pokeemerald/blob/master/src/item_use.c)
and [battle utilities](https://github.com/pret/pokeemerald/blob/master/src/battle_util.c).
The pinned Showdown bridge remains the battle foundation, with a bounded custom
trainer-item action; exact native rule equivalence remains open.

## Expanded Emerald increment — 2026-09-28

Full saved-team battles, paid sales, gifts and native trade evolutions extend
the existing exact Emerald pair. FireRed Rev 1 is a researched next pair, not
implicitly supported. Bluetooth/native parity retain their own evidence gates.

Owner requested a substantial two-handheld Link increment on 2026-09-28,
ahead of remaining RA work. Preserve that work and all roadmap acceptance.

Delivered: exact English Emerald pairs over local Wi-Fi on Flip 2 and Odin 2.
Bluetooth, cross-edition conversion and verified
competitive policy remain separate gates. Casual sessions may use imported saves;
they do not acquire verified provenance.

References: pret/pokeemerald `include/pokemon.h`, `src/trade.c`,
`src/pokedex.c` and existing pinned adapter reference in emerald-en.md.
The native trade exchanges individuals, retains OT/personality and registers
the received species. The TrainerOS boundary transports named semantic fields,
never complete save images. Unsupported Mail/eggs/first-seen-PID cases must fail
before a proposal can be confirmed.

Required proof: semantic round-trip; exact candidate allowed delta; stale-source
rejection; bilateral confirmation; durable prepared/commit receipts; idempotent
reconnect; interruption before and after each local replacement; two-device
operation and ordinary-game readback. No success until both receipts exist.
An unresolved transaction must block play/save services instead of guessing
whether a remote write happened. No automatic one-sided rollback.

## Using Link

Open **Pokémon → Center → X Link** on both consoles on the same Wi-Fi. Select
the other console and confirm the matching code on both. Choose Friendly battle,
Trade Pokemon, Sell Pokemon or Give a Pokemon. Choose the lead/offer and confirm.
Contextual controls remain in the footer. Existing Center/Playroom are preserved.

Friendly battle uses **the saved teams, up to six per side**, with the existing
pinned Showdown bridge. X switches between moves and available teammates;
fainted members require a replacement. Each console chooses its own moves;
HP, PP, turn results and winner synchronize. It uses copied, battle-ready
individuals. Optional agreed stakes and consumed medicine follow the protected
transaction described above; no battle damage, XP or achievements are written.

Trade exchanges actual saved Party or Box members, retaining individual identity and
held items, resetting friendship to native trade value 70 and registering the
received species in the native Pokédex. Both saves receive verified protection.
Only hatched English individuals without Mail are accepted. Unown/Spinda remain
refused until their additional first-seen-PID handling is implemented.

**Sales and gifts:** the initiating console offers one Party or Box member. Sales use
ordinary in-game money; Up/Down changes the price and Left/Right changes its step.
Both players see the amount, before/after balances and receiving Party/Box slot
before confirming. A full Party sends the received member to the first empty Box
slot. Insufficient money, wallet overflow, completely full storage and removal of
the seller's last able member are rejected. Gifts use the same transaction at
zero price. Currency and the Pokemon move together, with protected candidates,
durable receipts and reconnect; no one-sided rollback.

**Trade evolution:** all twelve Gen III trade/item rules are implemented, including
Everstone prevention, consumed evolution items, native stat recalculation, default
nickname changes, friendship, both Dex entries and the native evolution counter.
Current-level moves fill empty slots; a full moveset is retained, rather than
silently replacing an attack. A move-replacement chooser is not implemented.

This is a casual **trusted local network** feature. The comparison code confirms
the selected peer/session; it is not authenticated remote identity or encrypted
transport. UDP 47846/TCP 47845 listen only while Link is open. Frames, discovery
entries, buffers and timeouts are bounded. The test devices have no active firewall
blocking these ports. No cloud service, automatic firewall changes or background
discovery outside Link is installed.

## Durable save transaction

Both confirmations bind the exact proposal and observed source revisions.
`LinkSaveStore` prepares the protected candidate and durable journal before either
save is replaced. A commit decision is durable before atomic replacement/readback.
Each peer retains its journal until both matching completion receipts exist.
Retries are idempotent. A prepared transaction can be cancelled only while its
source remains unchanged and the other peer has no committed decision. Disconnect
after confirmation pauses the exchange; reconnect the same consoles to finish.
An unresolved transaction blocks ordinary play and other save writers across
application restarts. Device read-only mode and ownership/stale-source gates apply.

`EmeraldLink` validates named semantic fields and native record integrity; it does
not transport complete save images. The writer changes the relevant Party/Box
records, money, Dex/evolution data and affected checksums, preserving the older
bank, unrelated records and special sectors. Both valid save banks and a remaining
able Party member are required. Signed local history records `link-trade`,
`link-sale`, `link-gift` or `link-battle`; recovery after a write that
preceded a failed history append must not fabricate the missing event.

## Adapter-owned Pokédex

`gen3PokedexScope` declares National **001–386** for the allowlisted Emerald ROM:
all species representable in Gen III, including compatible transfers, rather than
only locally catchable Hoenn encounters. The family is Ruby/Sapphire/Emerald;
version exclusives are not removed. Only Gen III forms remain: Unown, Castform
and Emerald's Deoxys Speed form. Types and base stats come from the adapter's
pinned Gen III reference. Later species/evolutions, Mega and regional forms are
excluded. The scope works without a save; Seen/Caught still require verified data.

Other ROMs, including FireRed, keep the full catalogue until their own adapters
supply a scope. Ruby/Sapphire parsing and cross-edition trades are not delivered.

## Initial evidence, 2026-09-28

- Targeted Windows checks: semantic round-trip across all 24 record permutations,
  native allowed-byte delta, malformed/stale/Egg/Mail/evolution rejection;
  protected prepare/commit/recovery, read-only policy and signed history;
  pairing/frame limits, Dex scope/reset and QML loading. Practice regression passed.
- ARM64 build installed on Flip/Odin. Actual controller events paired them and
  completed a Blaziken/Swampert duel with consistent HP/PP and winner. Ordinary
  saves retained their pre-battle HP.
- Blaziken ↔ Swellow exchange completed. An initial history-operation omission
  interrupted completion after a local replacement; durable journals blocked play
  and reconnect completed both sides without replaying a write. The missing
  `link-trade` history whitelist is fixed and covered by the protected-service test.
- **Normal Emerald on both consoles** loaded the received members without save
  errors. A second, Odin-initiated exchange returned the original species to their
  slots; both final receipts are complete and match current save hashes.
  Both devices have a protected signed `link-trade` history record and no active
  pending journal. The portable adapter was refreshed and built independently.
- Real handheld screenshots cover pairing, proposals, battle/finish, normal-game
  Party readback and Emerald Dex. Private screenshots, saves and ROMs stay out of Git.
- Installed binary SHA-256:
  `073c190f1ad5d4cca73778768230d20d993964b8e531de009d935aa91ac614a3`.

## Expanded evidence, 2026-09-28

- Tests cover all twelve evolution rules, Everstone, nickname/HP/stat/Dex changes;
  sale/gift balance and allowed-byte changes, stale source, insufficient funds,
  overflow, last able member, Party-to-Box delivery and full storage rejection.
- Protected-service exchange/sale/gift tests interrupt after one committed side,
  reconnect, verify idempotence and signed operation kinds.
- The pinned engine completes six-member battles with voluntary switching and
  multiple forced replacements. Request IDs reject stale choices within a turn.
- Flip sold Swellow to Odin for **321**: seller money **499800 → 500121**, Party
  six to five; buyer money **499800 → 499479**, Swellow in **Box 13, slot 27**.
  Both peers completed the protected transaction. Normal Emerald loaded both
  saves; seller Party and buyer wallet were inspected in-game.
- Real controller inputs started a full-team battle between Flip/Odin and switched
  Flip from Blaziken to Linoone; the opposing attack, HP and next turn synchronized.
  The test battle does not write damage to ordinary saves.
- Odin also gifted Swellow to Flip: both balances stayed unchanged, recipient
  Party grew from five to six and donor Party shrank from six to five. Both
  protected receipts completed, with no pending transaction.
- Odin's Box 3 Kadabra was withdrawn through the protected movement UI and
  exchanged for Flip's Swellow. Both receipts completed. Normal Emerald on Flip
  loaded **Alakazam Lv.16, 48/48 HP** in Party slot 6 (Kadabra had 43 HP).
  All twelve rules retain automated evidence; this native readback covers Kadabra.
- Final ARM64 binary installed on both devices:
  `6aeb4221d8b691d8e6cdf5ac55d15deeb7ae73dcf8d852af478ea1026a8fa78f`.
- All 48 CTest targets pass across the full run and focused reruns. Three optional
  private-example cases were not supplied. The portable adapter also builds
  independently. Old library tests now enforce direct A launch/error instead of
  the retired ROM-binding screen; Link return no longer gives a hidden legacy
  activity button focus.

This is bounded proof, not an exhaustive power-loss/network-fault matrix.
Bluetooth, cartridge rule parity, evolution move replacement, eggs, Mail,
Unown/Spinda, cross-edition conversion and trust-sensitive #93/#94
policy remain open. Retain all earlier R1–R18 and RA work.

Primary references at the existing pinned pret/pokeemerald revision:
[native trade](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/trade.c),
[individual layout](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/pokemon.h),
[Pokédex](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokedex.c).
Evolution references: [rules](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/data/pokemon/evolution.h),
[trade evolution selection](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon.c),
[native evolution effects](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/evolution_scene.c).
See [exact-build record](adapters/emerald-en.md) and its portable adapter copy.

## Workspace/stakes evidence, 2026-09-28

- Full CTest run passed 47/48 targets; emulator_refresh exited without output under
  concurrent load and passed its isolated verbose rerun. All 48 targets therefore
  passed across the run/rerun; three optional private-example cases were skipped.
  Updated Link peer checks pass. Engine tests cover consumed-turn medicine,
  bench healing, revive, remaining-team count and invalid medicine rejection.
- Exact-save tests cover money outcomes, Party/Box stakes, native record delta,
  encrypted Bag consumption, fractional/overdraft rejection, reserved commit gates,
  monotonic checkpoints, terminal result preservation and idempotent settlement.
- A real twenty-turn Flip/Odin match used Full Restore and switched to Linoone.
  Odin won the 1,000 stake: Flip 500121 -> 499121, Odin 499479 -> 500479.
  Flip's Full Restore count changed 48 -> 47. Both ordinary Party blocks remained
  byte-identical, both receipts completed and matched the saves; no pending work.
- A second match staked Box individuals. Odin confirmed concession; its Box 1
  Ivysaur moved to Flip, while Flip retained its offered Bulbasaur. An independent
  decoder checked species/PID/OT: Odin's Ivysaur count 1 -> 0, Flip's 1 -> 2;
  the preexisting duplicate on Flip is from the test collection. Both ordinary
  Party blocks, money and medicine stayed unchanged; both receipts completed.
- Stakes are shown as the actual offered individuals, not the selected battle
  leads. Actual-device screenshots were inspected for the counter, portraits,
  Party/Boxes, arena, medicine and final result. Private captures stay outside Git.
- Updated pure adapter snapshot, exact profile and standalone build verified.
  Both devices run binary SHA-256
  `56153b7536c5e1e1cc97d2815c57ba33de004fe03bda7a2b5c5d1be721a853f0`.

This increment has native save-service readback and independent raw-save checks;
normal emulator readback of the new medicine/stake outcomes remains separate from
prior ordinary-game trade/sale proof. It does not claim exhaustive crash testing,
all medicine/items, cross-game stakes or cartridge battle parity.

## Ordered turn playback and effects, 2026-09-28

The Link screen previously exposed the completed turn's HP immediately and
triggered both attack gestures together. It now presents simulator events in
order: one action, its exact HP/status effects, then the next action. Selection
reopens only after playback; a single submitted choice still waits for its peer.
The next turn cannot be submitted during playback. Forced replacements and
terminal settlement retain their existing protected transaction gates.

The shared presentation parser also serves practice. Stable roster annotations
disambiguate duplicate species and bench healing; private/public split-log HP
lines are shown once. Attack lunges, type-colored impact bursts, healing/status
effects and miss/immunity feedback are presentation only. These are generic
effects, not a complete collection of cartridge move animations. Practice retains
its manual event stepping; Link playback advances automatically.

Focused engine checks cover speed order, priority overcoming speed and a KO
preventing the defeated partner's response. Playback checks cover intermediate
HP, split-log suppression, duplicate species and off-field healing. Native
Windows/ARM64 builds and link_peer/practice_session/qml_smoke tests pass.
Real Flip/Odin controller proof covers waiting for the second choice, two attacks,
then a voluntary Sceptile switch before Aerial Ace. Display captures show Sceptile
at 268/268 on entry, then 47/268 with the impact effect, before turn 3 opens.
Both devices received binary SHA-256
`ddf528a0acea2ea038411b6e01b8e0b7846d0de53052d5a115f4393c8c4862a8`
and the matching manifest-verified engine bundle. No save format or battle-rule
equivalence claim changes in this correction.
The no-stake proof match ended through confirmed concession. Both receipts are
complete with no pending transaction; money, Full Restore counts and ordinary
Party bytes are unchanged, and current saves match their completed receipts.
