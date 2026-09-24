# Party Playroom

The first real scene consumes the same verified English Emerald Party snapshot
as Center. It has no save resolver or writer of its own. Up to six known Party
members appear, preserving slot order and distinct nicknames even for duplicate
species. Eggs, empty slots and unreadable individuals do not become invented
actors. Storage does not supply the room.

Center → Activities → Party Playroom opens the garden room. Left/Right cycles
the stable member index; A calls that member closer and X gives a greeting.
B restores Activities, then the previous Party/Storage selection. L1/R1,
L2/R2, shared Y and Start retain the shell's normal priorities.

Optional local clips provide Idle, directional Walk and Sleep; missing actions
fall back to Idle, missing sprites to the existing illustration, then the actual
member's name. Native cropped sprite dimensions use a common bounded pixel scale
instead of making every species equally tall; this is artwork-relative sizing,
not a physical-height simulation. Each actor stays in a bounded floor lane and
clear of the attached name dock. One shared behavior timer chooses a visitor;
native animation and finite pause timers implement walking/resting and greeting.
Reduced Motion, hidden pages, application inactivity and modal UI stop motion.
Fainted or sleeping members rest rather than implying healing. No HP, friendship,
items, EXP or other save fields can change through this presentation.

Checking, failure, missing source or a different selected Adventure clears the
room. Replacing the same game's Trainer/save context also resets actor selection
and reactions. The scene never retains the previous team's members under a new
source. The explicit development sample remains isolated from production.

This is the first #52/#54 real consumer, not completion of all acceptance:
sliding portrait panels, richer social scenelets, the quieter Home consumer,
additional exact editions and long-run performance measurements remain planned.
Practice and Link Counter do not acquire capabilities from this read-only scene.

## Verification

Native checks cover known/egg/unreadable/empty slots, duplicate species, cycling,
repeat calls, context/owner replacement and unavailable snapshots. Rendered SDL
checks cover call/greet focus, page/face/Back and retired journal shortcuts.

Windows native build and all 42 tests passed, including the full SDL scenarios.
The production ARM build was installed on Flip with binary/database rollback
copies; installed SHA-256:
`d0df212b594e57248095a76c4be25c4d8a3ebd532cc63c0cf00e781f058e75b4`.
Actual Gamescope captures show six real Emerald Party members, walking and
greeting, the journal-free Dex and the Trainer overview without manual totals.
Controller injection exercised member selection, A/X, Start, Back and L1/R1.
The installed production binary used an isolated copy of the full Emerald save
for these populated captures. Missing-art fallback was also inspected on Flip;
the final run loaded all six members' sprites without QML warnings.
After verification the personal shell was restored, its schema-13 database
passed integrity checking and both original save hashes remained unchanged.

Twenty-four exact base-form clips were added only to the owner's private device
catalogue, preserving the previous index and each source/artist. These are
original game assets from the pinned SpriteCollab source, not represented as
community-licensed contributions; no files or private import manifest enter Git.
The public community importer and its existing attribution gates are unchanged.
