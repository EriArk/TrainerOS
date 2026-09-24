# Party and Storage presentation

## Split view — 2026-09-24

The Center now keeps the selected Pokémon's summary beside the team/box instead
of opening a separate detail page. Party has six slots in two columns; Storage
has a six-column, thirty-slot box. The development fixture exercises known,
fainted (zero HP), Egg, empty and unreadable records. It is not a game save.

The right pane shows illustration, level, types, HP, ability, nature, held item,
six colored numeric stats and four move slots. Missing values remain unknown;
Eggs never reveal an inferred species. Optional existing sprite assets supply
small slot previews, with illustrations as fallback. Only the focused sprite
animates; modal/hidden/inactive/reduced-motion states stop it. No assets are
bundled by this change, and existing provider attribution remains intact.

X switches Party/Storage, preserving their independent selection. Up from the
first Storage row focuses its box header; Left/Right cycles boxes, Down or B
returns to slots. Each box retains its selection. Horizontal slot edges stop
rather than unexpectedly moving to another row or box. Down from the last row
reaches Activities. L1/R1 and L2/R2 retain shell navigation.

A opens a compact actions panel without losing the inline summary. Close is the
safe initial selection; Adventure backups opens the existing ordinary-save
service, and B returns to the exact slot. Move and Heal are visibly disabled:
this increment introduces no external-save writes. All key legends remain in
the shared bottom chassis. Start and Choose Adventure retain shell priority.

Production now uses the [exact English Emerald reader](EMERALD_PARTY.md) for
real Party and fourteen-box Storage. Other titles retain the unavailable view
with working backup and Activity routes. Storage omits current HP, retaining
maximum HP among the six stats. Sample individuals are
only available through the isolated development composition (`--ephemeral` or
rendered checks), never persisted or shown as the active Trainer's real team.
Every provider must retain unknown versus zero and invalidate all individuals
when its Adventure/Trainer/save source changes.

## Acceptance

- Controller focus updates the inline detail, including zero HP.
- Thirty-slot Storage, box header, remembered slots, actions, backups, Activities
  and global section/paired-face return work without pointer input.
- Source/context changes reset stale selections; production exposes no samples.
- Rendered checks and actual Flip captures cover the split layout and action
  panel. Populated development captures must be identified as test data.
- The first exact Emerald read is delivered separately; more builds, mutations,
  living Party and two-device Link remain row 7 work in ROADMAP.

## Verification — 2026-09-24

Windows build and all 42 tests passed. After the final modal-shade/footer text
polish, the three affected interaction/rendered checks passed again. The SDL
diagnostics scenario covers Party, zero HP, Storage header/box change, unreadable
slot, backup return, Activities return and global/paired navigation.

The ARM64 Release build was installed on Flip after binary/SQLite backups.
Schema 13, 829 Adventure registrations and three Trainer owners were preserved;
SQLite integrity passed. The same installed executable was also launched in
isolated ephemeral mode with the device's existing private art providers for
populated Party/Storage/actions captures. Controller events were injected through
the device's virtual controller; screenshots were captured from its Gamescope
output. These are real handheld renders of development records, not real-save
Party proof or an owner's physical-button acceptance. The preview was then closed
and the ordinary personal shell restored.
