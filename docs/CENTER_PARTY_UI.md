# Center Party and Storage presentation

P1 #44/#53 extends the existing Pokédex companion with practical Party/Storage views. Existing ordinary-save backup/restore remains real. Individual Pokémon reading and all semantic writes remain late P8; no new parser or save editing is introduced here.

## Controls

- L2/R2 switches Pokédex/Center even when the save service is unavailable. L1/R1 remains primary-page navigation.
- Center opens Party initially. X switches Party/Storage. D-pad selects fixed slots; A opens a read-only detail. A/B closes detail; B on the grid never flips the companion face.
- In the sample Storage grid, left/right at the outer column changes between two explicitly sample boxes and remembers each box's selection. Box counts and slot counts are presentation fixtures, not a universal Pokémon storage format.
- Select on a grid opens the existing ordinary-save shelf. Inside that shelf Select still creates a backup and X checks again. B first cancels restore confirmation; when idle it returns to the prior Party/Storage grid. While a backup operation is busy, the existing service gate owns those local actions.
- Shared Y stays available above Party/Storage, including detail. Cancel restores the route; choosing a different Adventure clears the detail, slot focus and sample box. It never launches or writes a save. Restore confirmation keeps priority over Y and paired triggers.
- Start → Pokémon Center opens the same companion, retaining its useful local section. Page/face visits preserve view state in memory; new Party/Storage routes are not persisted as owned records before P2.

## Honest data boundary

`PartyPresentation` is a transient, read-only presentation object. It has no repository mutation, save-file access, parser or write capability. Only the existing non-editable development library enables its clearly labelled records. The fixture covers known HP including zero, unknown moves/PP and held item, an egg without invented species/hatch progress, empty slots and an unreadable record. Neutral number medallions are original UI, not Pokémon artwork or animation sprites.

The real library shows no individuals or invented box counts. No selection, unsupported reading and an unlinked Adventure are distinct messages. The empty action opens ordinary-save backups, whose actual capability/loading/error/confirmation behavior remains in `SaveCenterController`. This synchronous presentation has no pending reader and does not fabricate loading or a successful observation. A future P8 snapshot must carry exact source/build/revision and field availability before populating real cards.

Heal, reorder, transfer, release and related save modifications are unavailable. Viewing a card or changing a box cannot rewrite a save. Playroom, Practice and Link Counter remain separate later P1 presentations and P8 integrations.

## Acceptance

Check sample Party → detail → Storage → second box → unreadable/empty detail, slot restoration after Back and page visits, and the existing shelf's backup/restore/cancellation flow. Check production empty states and shared context changes without sample leakage. SDL rendered scenarios cover the new views; save-service scenarios continue to compare fixture bytes before/after backup and guarded restore. Native Flip rendering and injected device input are separate from a physical-button acceptance by the owner.

2026-09-20 verification: Windows build and all 35 CTest entries passed; ARM64 build on Flip passed 38 entries plus the separate persistence process scenario. The native Gamescope SDL scenario passed without QML warnings. Party, Storage and detail captures were visually inspected at 1920×1080, with host coverage at 960×540. The fixture includes zero HP and unreadable records rather than treating them as healthy or empty.

The production build was installed with a prior binary/database backup. Injected InputPlumber events verified the real Party → Storage → existing FireRed backup shelf → B back to Storage path. Production captures show no sample individuals. Database integrity, the Trainer profile and all 686 Adventure registrations were preserved. Physical-button acceptance remains with the owner; no real save was restored during delivery checks.
