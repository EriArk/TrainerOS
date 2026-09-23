# World chronology — P3 / #63

The read-only collection projection orders Adventures by runtime platform era,
then known edition release year, explicit catalogue order, case-folded title and
stable Adventure ID. Platform IDs break equal-era ties; unknown platforms follow
known ones in stable ID order. Availability is not a sort key: Linked/Missing and
search are subsequences of the same list. World membership is unchanged.

`data/catalogue.json` owns optional platform `chronology` ranks and edition
`releaseYear`, `releaseSource` and `order`. Ranks express relative hardware order,
not invented dates. There are 13 dated platform families and 96 sourced edition
years in the 216-entry reference. Undated editions remain after dated editions
within their platform, using explicit catalogue order. PC/Android have no assumed
single console era. Their unknown rank does not remove existing entries.

The year describes the first release of the represented edition, not the local
ROM revision, translation or reissue. The separately catalogued Japanese Red/Blue
and western Red/Blue retain distinct years. Later ports require their own evidence;
they never inherit a date merely because their title resembles an older game.
ROM hacks do not inherit their base edition's year/order, even when an imported
record carries that catalogue ID. A mismatched platform cannot borrow an edition's
date. No filename, plot region or emulator executable implies release chronology.

## Sources reviewed 2026-09-23

- [Pokémon official game catalogue](https://www.pokemon.co.jp/game/) supplies
  the year groups for the dated main series and spin-offs. Each dated edition
  records its source URL. Unchecked titles/ports remain undated.
- [Serebii Red/Blue release record](https://www.serebii.net/rb/) distinguishes
  the western pair (1998) from the separately catalogued Japanese originals.
- [Nintendo hardware history](https://www.nintendo.com/en-gb/Hardware/Nintendo-History/Nintendo-History-625945.html)
  anchors console order. In particular N64 predates Game Boy Color, and GBA
  precedes GameCube. The rough order in #63 is not used as a factual date source.
- [SEGA company chronology](https://www.sega.jp/history/companyTimeline/en/)
  places Pico in 1993. [Pokémon mini release history](https://en.wikipedia.org/wiki/Pok%C3%A9mon_Mini)
  places its first release in November 2001, after GameCube and before DS.

## Storage and UI boundary

No schema migration, personal metadata rewrite, rename, file move, save read or
history merge. The existing World controller retains selection by Adventure ID
through refresh, return and persisted navigation. Filters that remove the chosen
entry keep the existing deterministic first-result fallback. Four visible rows,
platform badges and controller shortcuts remain unchanged. Development fixtures
retain their explicitly scripted order; the real collection uses this projection.

Multiverse keeps separate ordering. Per the owner's follow-up, its P3/P4 registry
must cover every platform with a credible Flip/Armada runtime route, not only the
six old preview cards. Production no longer shows those cards with no ROMs.
Real system visibility must be derived from available content, independently of
the local search/filter; disconnected content must not delete library/history.
The complete registry, enumeration, per-Trainer/domain choices and launch binding
remain the next connected increment, with emulator/BIOS/four-ROM-per-system
provisioning in P4. Presence alone does not certify a working emulator.

## Verification

Collection tests exercise mixed eras, remakes, hacks, platform mismatch, missing
metadata, year/order/title ties, availability-independent order and insertion of
a new platform through metadata alone. Controller tests cover search/filters,
refresh and identity-based navigation restoration without creating personal rows.
The production Multiverse empty state handles all directions without a negative
focus range.

Windows passed 39/39 checks; ARM64 passed 43/43, including controller-rendered
Worlds and persistence scenarios. The non-testing build was installed on Flip
with a paired binary/database backup and SHA-256 readback:
`3db575805b069816c1723ad6a1028d092c37d031c0a621a7b8504f33db7ece0a`.
InputPlumber button events and released virtual trigger events exercised the
installed Gamescope session: Hoenn retained the previously selected Emerald
despite its new position; Linked/Missing kept platform order; L1/R1 and L2/R2
restored the chosen Omega Ruby card. Four full rows and the GBA → GameCube → 3DS
transition were inspected in actual device captures. Empty production Multiverse
stayed empty under direction/A/B input and returned through its paired trigger.
The device was left on Pokémon Home. These are injected controller checks, not
a new physical-button acceptance by the owner.

Database integrity passed at schema 10, with the Trainer profile and all 686
personal Adventure rows identical to the pre-install backup. No games were
launched during this check. Screenshots and machine-specific helpers stay private.
