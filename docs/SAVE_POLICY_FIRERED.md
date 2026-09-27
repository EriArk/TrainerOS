# Save policy and second exact reader — 2026-09-27

## Device-wide read-only saves (#75)

Settings → Saves owns one persistent policy for every Trainer. Enabling it blocks
TrainerOS healing, shop purchases/lessons and backup restoration at the common
storage transaction boundary, not merely in QML. Read-only observations and
copy-only backups continue. Ordinary saving inside games is unaffected.

`backups/save-policy.json` is outside Trainer-specific shelves. Policy changes
share the mutation lock and use atomic, synchronized replacement. Missing policy
preserves the existing write-enabled default; malformed/unreadable policy fails
closed. Transactions check before preparing and again before committing. Future
mutators must use this boundary; no feature may create its own save writer.

Tests cover each existing entry point, copy-only backup, restart, damaged policy
and lock contention. Physical controller verification enabled the policy, restarted
the app, attempted FireRed healing and proved the target stayed byte-identical.
After disabling it explicitly, the same clinic completed treatment.

## Exact English FireRed (#82, bounded R6 slice)

The existing exact-ROM identity and mGBA owner resolver now feed Party, fourteen
Boxes and species Seen/Caught into the existing consumers. Six Party records,
encrypted substructure ordering/checksums, stats, moves/PP, identities and all
three Seen replicas are validated. National species coverage remains 386;
species flags never establish ownership of every form. FireRed Deoxys uses its
Attack form, independently of Emerald's Speed form.

FireRed's Party offset is 0x38 (count 0x34), rather than Emerald's 0x238. Its
SaveBlock1 length and duplicate Dex flags also differ. Shared record decoding
does not turn platform or generation membership into compatibility evidence.
The exact English original and Rev 1 hashes are allowlisted; other revisions,
languages, LeafGreen and hacks remain unsupported by this semantic provider.

Protected treatment restores only Party HP/status/PP and required checksums,
preserving Eggs, Boxes, story, money and identities. It requires two verified,
unambiguously ordered save banks, an unchanged source revision and an automatic
protection copy, then verifies the resulting bytes. Emerald shops remain Emerald
only; no FireRed merchant support follows from this change.

The recorded unreadable Rev 1 test file was entirely 0xff, not a supported save.
No checksum relaxation was introduced. A separate valid private FireRed save
was read successfully: six members and all box slots verified without unreadable
members. Synthetic tests cover all fourteen sector rotations, both bank layouts,
replica disagreement, exact-build rejection and allowed healing changes.

On Flip, an isolated copy bound to the installed exact Rev 1 ROM was deliberately
given a burned Charizard with 1/285 HP and 0/24 Flamethrower PP. The clinic restored
it, created exactly one byte-exact protection copy and preserved unrelated bytes
and original personal saves. Normal FireRed loading displayed the healed Party,
285/285 HP and Flamethrower at 24/24 PP in the game's summary screens.
This physical check uses Rev 1; original-revision semantic support has reader and
synthetic proof, not a separate physical playthrough. Test achievement submission
was disabled. Private ROMs, save files and screenshots stay outside Git.

Primary format references: [pret/pokefirered global structures](https://github.com/pret/pokefirered/blob/master/include/global.h),
[save sectors](https://github.com/pret/pokefirered/blob/master/src/save.c),
[Pokémon records](https://github.com/pret/pokefirered/blob/master/src/pokemon.c).

## Delivery checks

Windows native build and all 42 tests passed; after the final capability/RA review,
all four affected achievement, input and QML checks passed again. ARM64 release
built and replaced the installed executable with a retained binary/database
rollback. Database schema 13, all 829 Adventure rows and three Trainers survived.
The installed release SHA-256 is
`2a8f5a43b7c5e7f5abd2a96ce963d540c4c0d7eaf70af3ee9f83e834a6dc45b3`.

## Remaining work

This delivers the requested bounded next three slices with the RA return work in
[RETROACHIEVEMENTS](RETROACHIEVEMENTS.md). It does not close all R4–R6 acceptance.
Party/Box editing, exact Journey/immutable Champion history, practice, two-device
Link, remaining per-game services and actual newly earned live RA proof retain
their existing roadmap gates. The next coherent slice remains meaningful Emerald
Party/Boxes management on the protected mutation boundary, at 3/6 High.
