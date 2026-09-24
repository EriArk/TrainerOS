# Emerald Pokémon Center

## Scope and controls

The owner's 2026-09-24 increment completes the first real healing route for the
verified English Emerald ROM already used by the Party reader. Platform identity
does not establish save-format compatibility: FireRed, Ruby/Sapphire and hacks
remain separate provider work. This intentionally advances Emerald's protected
writer alongside its Center consumer; all other roadmap commitments remain.

In Center's Party view, A opens member actions. **Visit Pokémon Center** enters
an original Hoenn-inspired pixel room with a nurse, recovery machine and team.
A confirms treatment; B leaves without changing anything. Start may overlay the
room, and L1/R1 retain global navigation. Y/paired triggers cannot change the
target during this visit. While writing, Back cannot interrupt replacement;
the existing service gate blocks launch and drains writes before exiting.

Treatment restores every verified non-Egg Party member's current HP to its stored
maximum, clears battle status and refills all four moves including PP Ups.
Eggs, boxes, identity, experience, items, stats, Pokédex and story flags stay
unchanged. An entirely healthy team creates no redundant backup or write.
Unsupported, corrupt, missing or ambiguous data cannot enable treatment.
This first clinic heals without charging in-game money. The earlier paid-service
acceptance remains deferred until a separately verified money writer exists.

## Verified format boundary

ROM SHA-256: `a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af`.
The raw save must be 128 KiB with two complete, individually verified rotating
slots and unambiguous different counters, including wraparound. Unlike the
read-only recovery path, a damaged redundant slot is not accepted for writing.
Only the newest slot's section 1 is patched; the older slot, padding, section
order, counters and extra sectors are retained byte-for-byte.

Each non-Egg member must pass the existing individual reader. Its 48-byte
encrypted substructures are decrypted and located using personality modulo 24.
Only current move PP changes inside that record; its checksum/encryption and
the containing sector checksum are recomputed. Current HP/status are separate
Party fields. The output is read back through the normal reader before it can
be committed. This does not certify legality or provenance of the user's save.

Facts were checked against pret's pinned Emerald revision
`5eff78649e7170a877b961ef0b3da13b81a16038`:
[HealPlayerParty](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/script_pokemon_util.c),
[PP and individual checksums](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/pokemon.c),
[save sections](https://github.com/pret/pokeemerald/blob/5eff78649e7170a877b961ef0b3da13b81a16038/src/save.c).
Existing factual move/PP tables are reused; no upstream game code or nurse art
is bundled. The nurse and room are original QML/Canvas drawings.

## Protected write and recovery

The pure exact-build transformer is injected into `LocalSaveBackupService` at
composition. QML and Center receive capability/status projections, never paths
or format offsets. The existing worker, owner-bound resolver, operation lock,
emulator exclusion and service busy gate apply to healing as well as backups.

Confirmation binds the displayed save revision. Before replacement, an atomic,
synchronized and read-back-verified protection bundle preserves the original.
The resolver rechecks content/context/owner/path and current bytes before and
immediately after staging. Any failure before commit cancels the write. A
post-commit storage verification error explicitly reports that a write occurred.
The **Before healing** backup uses the existing restore/undo flow; old bundles
remain readable and require no schema migration.

The ordinary-save provider invalidates during the operation and refreshes when
it finishes. Party, Home and Playroom consume that new observation. The next
normal emulator launch loads the treated ordinary save; no savestate is used.

## Acceptance

- Synthetic records cover all 24 permutations and 14 sector rotations, counter
  wrap, PP Ups, empty moves, status/HP recovery, unchanged Eggs/boxes/other bytes,
  idempotence, wrong ROMs, damaged sectors and ambiguous counters.
- Storage checks cover backup-before-write, exact undo, unchanged healthy saves,
  stale tokens/context, provider rejection, failed protection and concurrent work.
- Controller checks cover explicit acceptance/cancel, busy Back, discarded late
  results after context replacement and Party's entry action. Rendered SDL checks
  cover actual focus, Start/Back, Y/trigger priority and L1/R1.
- Real-device treatment and subsequent normal in-game Party/PP inspection are
  required before reporting device completion. Test damage is restricted to
  isolated copied saves; personal originals are hash-checked afterward.

## Delivered verification — 2026-09-24

The Windows suite's 42 entries passed across the full run and the affected
`persistence_process` rerun after updating its healing/undo scenario. ARM64
production compilation passed and the final installed executable SHA-256 is
`34f609260adc5d8ba46a8d0c0fb7f271fa9522c32b11c987f456f1ebd49df75a`.

On Flip, isolated copied saves supplied a team with depleted HP/PP and burn.
Controller treatment produced exactly the independently reconstructed expected
bytes and an exact pre-treatment backup. A normal Emerald launch accepted the
save: the game's Party menu showed all six full HP values, and Blaziken's move
page showed 32/32, 24/24, 16/16 and 16/16 PP. Center's controller restore then
recovered the complete pre-treatment bytes; a second treatment passed again.
The final room and the in-game screens were captured from the actual handheld.
The installed preview produced no QML warnings. Both personal save originals
retained their hashes, the personal database passed integrity/schema checks,
and the isolated preview was closed with the normal personal shell running.
