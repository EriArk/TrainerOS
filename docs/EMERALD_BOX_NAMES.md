# Emerald box names

Exact English Emerald only. In Pokemon / Boxes, **X - Rename box** opens the
controller keyboard with the current name. Apply shows old and new names;
A confirms, B returns to editing, and B from the keyboard cancels without writing.
The action also works on empty boxes. Names use 1-8 game-supported characters.
All fourteen boxes share the same proven encoding; FireRed remains unavailable.

## Protected operation

`BoxNameChange` binds a box index and name to the observed save SHA-256.
`renameEmeraldBox` requires the exact ROM allowlist and two valid save banks,
encodes through the factual Emerald character table, rejects EOS/control bytes,
and writes the name plus 0xff terminator into the selected nine-byte name slot.
ASCII apostrophe maps to native 0xb4. Unused suffix bytes stay intact.

Only the current bank's sector 13 name bytes and checksum may change. Other
names, wallpapers, current box, Pokemon records, bank counters and the older
bank remain byte-identical. No game or save revision is inferred from platform.
The writer rereads the candidate before returning it.

The existing host transaction checks owner/runtime/policy/staleness, creates
verified protection, atomically replaces and reads back the save. Signed local
history accepts the bounded `box-name` operation; it does not assert verified
native gameplay. Center lists the protection as **Before renaming box** and
restores it through the ordinary guarded restore flow. No database migration.

## Source and evidence - 2026-09-27

Pinned [pret pokeemerald](https://github.com/pret/pokeemerald/tree/5eff78649e7170a877b961ef0b3da13b81a16038):
`include/pokemon_storage_system.h` defines eight visible characters and names at
storage 0x8344; wallpapers start at 0x83c2. `src/naming_screen.c` uses that length.
`charmap.txt` defines the glyphs and EOS, consistent with the existing reference
character table. All name slots lie in logical sector 13 (offset 0x744 onward).

- Synthetic tests cover every box under all fourteen sector rotations, full and
  shortened names, suffix preservation, exact allowed deltas, no-op, unsupported
  glyphs/builds, stale hashes and damaged banks.
- Controller/keyboard tests cover empty boxes, confirmation, edit/cancel, invalid
  input, repeated input during writes, fixed box selection and owner cancellation.
- Storage and lineage tests cover read-only policy, stale tokens, runtime/source
  races, failed protection, signed history and byte-exact rollback.
- On a private Flip copy, SDL controller events renamed BOX1 to BOX1A. An
  independent decoder verified the complete allowed delta. Emerald loaded it,
  saved normally and returned; the new name and all Party/Box records survived.
  Center then restored the original file byte for byte. Personal saves remained
  unchanged. This is normal-game load/save proof, not a claim that its native PC
  naming screen was navigated during this check.

The reusable [adapter copy](adapters/implementations/gen3/README.md) includes
this operation and exact profile evidence; host file protection stays outside it.

Delivery checks: Windows native build and all 43 CTest entries passed; the
standalone exported adapter built independently and knowledge/export validation
passed. Release ARM64 build (tests disabled) installed on Flip; one supervised
shell, no active emulator, schema 14, 829 Adventures and 3 Trainers retained.
Installed keyboard/confirmation screenshots were captured with controller input;
the personal-save preview was cancelled. Device returned to Home.
