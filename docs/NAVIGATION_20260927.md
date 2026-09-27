# Cyclic navigation and shop baskets — 2026-09-27

Installed primaries: Home / Worlds / Pokémon / Trainer / Journey. L1/R1 retains
primary routing. L2/R2 cycles secondary faces and wraps in either direction:
Home and Worlds each have Pokémon/Multiverse; Pokémon has Dex/Party/Boxes/Center/
Playroom/Shops; Journey has live Journey/Hall/RetroAchievements. Header cues show
these persistent controls; contextual actions stay in the footer.

Home X no longer changes domain. B unwinds local routes without changing face or
opening filters. Dex Select opens its filter rail. Party/Boxes retain their own
positions; Center owns backups/recovery and Link. Start contains quick controls,
Settings, Switch Trainer, system modes and Power; switching Trainer is not power.
Pending writes, storage confirmations and shop transactions gate navigation.

Short A on a configured playable wheel entry launches directly, preserving its
wheel/focus on return. Missing/unconfigured entries open setup; hold A management
and Y selection without launch are retained. Home A still directly launches its
chosen Adventure. Library Properties asynchronously shows verified capability
readiness, separate from runtime configuration.

## Shops

X searches discovered places/stock and Y filters known locations. Unknown places
never disclose names or stock through filters. Highlighting still previews stock.
Left/right selects quantity, A adds it to the basket, Select opens the basket.
There: directions choose/change quantity, X removes a line, A checks out once.
Currencies/material totals are separate. Lessons retain recipient/move confirmation.
The basket is scoped to the current Adventure/Trainer; navigation alone preserves
it. Re-resolution and source tokens prevent stale/double purchases. Any invalid
line rejects the whole candidate; one host transaction creates one protection
copy and commits the full basket.

## Validation

Native controller/QML regressions cover face order/wrap, local routes, modal gates,
Start/Settings, focus restoration and persistence. Pure Emerald tests cover save
sector rotations, encryption keys, invalid later lines and protected single commit.
Actual Flip SDL input exercised Shops, search/location/basket, all Pokémon faces,
Journey and Settings. A separate test save bought 20 Poké Balls and two Potions
for 4,600 from 50,000: normal Emerald Bag showed 20 Poké Balls, one Premier Ball and
two Potions. The protection copy matched the original exactly; restoration matched
both test sources exactly and preserved the purchased copy. Other Trainer and
personal save hashes were unchanged. Private captures/manifests stay outside Git.

The production package is built on ARM64 without test switches; atomic installation
keeps a previous binary and SQLite backup. 829 library records, three Trainers and
schema 13 are preserved. #75, remaining #74 capabilities, complete #90 pack registry,
actual Champion writes/practice/Link and other R4–R18 work remain separate gates.
