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

## Meadow and social reactions — 2026-09-24

The owner-requested follow-up uses an original tile-style meadow viewed from
above at the sprite set's oblique angle: stepped tree crowns, wooden fence,
grass tufts, flowers and an open gate/path. A small static Canvas is scaled
without smoothing; it has no animation repaint timer. Actors retain the native
relative pixel scale at a smaller size. The enlarged field adapts its fence,
path and vegetation without stretching the pixel tiles.

A calls the selected member forward; X greets it. Select starts a short ball
game with the next awake member, or solo when there is no eligible partner.
Sleeping/fainted members respond by resting and are never recruited into play.
Occasional neighbor greetings and curious responses accompany idle/walk/rest.
These are temporary presentation reactions, not saved friendship or game events.

Normal/Happy portrait choices come from the same exact-form SpriteCollab source
as the optional animation. A bounded speech bubble above the actor shows the
emotion; missing Happy falls back to Normal, then a simple heart/note/question
symbol if no portrait can load. Portraits do not replace full-body animation or
the primary Pokédex illustrations. Actor-list notifications are separate from
selection/reactions so pressing a button cannot rebuild the entire scene.
Switching owner/save clears the actors, bubbles and ball. Menu/page/inactive
and Reduced Motion stop ambient motion; explicit reactions remain readable.

Sources: [Qt Canvas](https://doc.qt.io/qt-6/qml-qtquick-canvas.html) informs the
cached scenery boundary; private portrait provenance retains the pinned
[SpriteCollab](https://github.com/PMDCollab/SpriteCollab) revision and credits.

Optional local clips provide Idle, horizontal/vertical Walk and Sleep; missing actions
fall back to Idle, missing sprites to the existing illustration, then the actual
member's name. Native cropped sprite dimensions use a common bounded pixel scale
instead of making every species equally tall; this is artwork-relative sizing,
not a physical-height simulation. Actors move freely across the shared floor in any direction, including diagonals,
inside the fence and clear of the lower-left Adventure tab. Destinations avoid
occupied landings and nearer actors draw in front. Neighbor greetings use
proximity; the selected member can approach the front gate when called. One shared behavior timer chooses a visitor;
native animation and finite pause timers implement walking/resting and greeting.
Reduced Motion, hidden pages, application inactivity and modal UI stop motion.
Fainted or sleeping members rest rather than implying healing. No HP, friendship,
items, EXP or other save fields can change through this presentation.

Checking, failure, missing source or a different selected Adventure clears the
room. Replacing the same game's Trainer/save context also resets actor selection
and reactions. The scene never retains the previous team's members under a new
source. The explicit development sample remains isolated from production.

This is the first #52/#54 real consumer, not completion of all acceptance:
additional portrait/social scenelets,
additional exact editions and long-run performance measurements remain planned.
Practice and Link Counter do not acquire capabilities from this read-only scene.

## Home and compact scene layout — 2026-09-24

Home now reuses the selected Adventure's observed actors without opening Center
or adding a provider. Up to six bounded companions quietly idle, walk or rest
beside the Y tab. The same cropped native scale keeps relative artwork sizes;
missing clips retain the illustration/name fallback. Eggs and unreadable slots
remain absent. A still launches immediately and Y still chooses an Adventure.
Menus, hidden/inactive pages and Reduced Motion stop animation. Changing the
Adventure or Trainer clears stale actors through the shared Party observation.

The Playroom's lower information dock is removed. Its selected member name is
in the compact header beside the Adventure; the field uses the full remaining
height. Shared page headers place secondary context beside the title. Pages no
longer reserve a full-width strip for Y; local lists/actions avoid its actual
lower-left footprint while right-hand details and scenery extend downward.

## Initial delivery verification

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

## Meadow follow-up verification

The Windows build and complete 42-test suite passed. Added checks prove that
reactions do not notify/recreate the roster, sleeping members are excluded from
ball play, solo play stays usable, owner replacement resets the scene, and
portrait keys retain exact-form identity separately from animation clips.
SDL exercises Select in Playroom without entering backups and keeps actor focus.

Production ARM SHA-256:
`803e1f45ca510551559a903537aa2dff39f30f7d5f7a27e0c8bea80accf7bd74`.
The installed Flip build has six animated Emerald members and eleven private
Normal/Happy portraits from the same pinned revision. Linoone uses Normal when
Happy is absent upstream. Exact credit/source records and downloaded originals
remain outside Git; the existing public import policy is unchanged.
The final row-spacing adjustment passed the rendered SDL scenario again.
Actual Flip captures were inspected for the six-member meadow, greeting and
two-member ball play. Start/Back, member navigation, A/X/Select and L1/R1 were
exercised by controller injection; the preview log contained no QML warnings.
After closing the isolated validation session, the personal shell remained
running, database integrity passed and both original saves retained their hashes.

## Free-roaming layout and Home verification

The Windows build and all 42 tests passed. Home's rendered SDL scenario now
checks the six-member consumer before visiting Center, clearing unrelated save
observations, menu pausing and unchanged launch/focus behavior. The Playroom
scenario verifies movement on both axes while controller selection stays stable;
it passed again after the final uniform destination-selection adjustment.
Existing long-title and shared-navigation scenarios cover the compact headers.

Installed ARM production SHA-256:
`6cc862c2b3ebdf520023ae133e537bd3b10b31a8bb356f13cbd60f05ebd328fd`.
Actual Flip captures were inspected for Home, the expanded meadow and Pokédex,
using an isolated copy of the Emerald ordinary save. Twelve private vertical
walking clips from the same pinned SpriteCollab source preserve source/author
records outside Git. The validation log contained no QML warnings. After closing
validation, the personal shell remained running, its schema-13 database passed
integrity checking and both original save hashes remained unchanged.
