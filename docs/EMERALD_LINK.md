# Emerald Link — implementation and evidence

Owner requested a substantial two-handheld Link increment on 2026-09-28,
ahead of remaining RA work. Preserve that work and all roadmap acceptance.

Delivered: exact English Emerald pairs over local Wi-Fi on Flip 2 and Odin 2.
Bluetooth, cross-edition conversion, trade evolution, gifts/sales and verified
competitive policy remain separate gates. Casual sessions may use imported saves;
they do not acquire verified provenance.

References: pret/pokeemerald `include/pokemon.h`, `src/trade.c`,
`src/pokedex.c` and existing pinned adapter reference in emerald-en.md.
The native trade exchanges individuals, retains OT/personality and registers
the received species. The TrainerOS boundary transports named semantic fields,
never complete save images. Unsupported Mail/eggs/evolution cases must fail
before a proposal can be confirmed.

Required proof: semantic round-trip; exact candidate allowed delta; stale-source
rejection; bilateral confirmation; durable prepared/commit receipts; idempotent
reconnect; interruption before and after each local replacement; two-device
operation and ordinary-game readback. No success until both receipts exist.
An unresolved transaction must block play/save services instead of guessing
whether a remote write happened. No automatic one-sided rollback.

## Using Link

Open **Pokémon → Center → X Link** on both consoles on the same Wi-Fi. Select
the other console and confirm the matching code on both. Choose Friendly battle
or Trade Pokemon, select one Party member each, and confirm the proposal.
Contextual controls remain in the footer. Existing Center/Playroom are preserved.

Friendly battle is **one Pokémon per side**, using the existing pinned Showdown
bridge. Each console chooses its own moves; HP, PP, turn results and winner
synchronize. It uses copied, battle-ready individuals; no save damage, XP,
currency, achievements or rewards are written.

Trade exchanges actual saved Party members, retaining individual identity and
held items, resetting friendship to native trade value 70 and registering the
received species in the native Pokédex. Both saves receive verified protection.
Only hatched English individuals without Mail are accepted. Trade-evolution
species and the additional first-seen-PID cases Unown/Spinda are refused for now.

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
not transport complete save images. The writer changes the selected Party record,
Dex flags and affected checksums, preserving the older bank, counters, boxes and
special sectors. Both valid save banks and a remaining able Party member are
required. Signed local history records `link-trade`; recovery after a write that
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

## Evidence, 2026-09-28

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

This is bounded proof, not an exhaustive power-loss/network-fault matrix.
Bluetooth, full teams/switching, cartridge rule parity, evolution, eggs, Mail,
Unown/Spinda, gifts/sales, cross-edition conversion and trust-sensitive #93/#94
policy remain open. Retain all earlier R1–R18 and RA work.

Primary references at the existing pinned pret/pokeemerald revision:
[native trade](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/trade.c),
[individual layout](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/include/pokemon.h),
[Pokédex](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokedex.c).
See [exact-build record](adapters/emerald-en.md) and its portable adapter copy.
