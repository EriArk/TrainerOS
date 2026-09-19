# Trainer card and local overview

**Accepted target, not delivered — #9/#20/#42:** counters below describe the existing single-Trainer/manual baseline. Future owner-scoped, source-aware projections follow the shared Adventure where relevant, with Y as the common selector and no duplicate context capsule. Do not relabel manual totals as verified current-save data. [Ownership and provider acceptance](EXPANSION_42_62.md).

The Trainer page combines the persistent identity with a cached summary of local records. It remains one peer section: A opens profile editing, L1/R1 switch sections, Start opens the system menu. Static counters do not acquire directional focus.

## What the counters mean

- **Adventures:** unique linked library registrations. Grey catalogue-only entries are excluded; linking a file does not certify that it is currently readable or launchable.
- **Worlds:** distinct primary/additional Worlds represented by those registrations, including fan-created Worlds. This is not a count of regions visited in games.
- **Seen / Caught:** positive, manually recorded species marks from the offline field journal. Unknown marks are not counted as positive or turned into negative records.
- **Favorite marks:** species starred in the Pokédex. The single favorite on the Trainer card is a separate profile choice.
- **Memories:** Hall of Fame archive entries. Several entries may describe different playthroughs of the same Adventure; this is not an inferred completed-game count.
- **Recorded time:** available completed session durations observed by TrainerOS for currently linked Adventures. It is not game-save playtime and does not reconstruct time from interrupted sessions. No recorded durations displays an em dash.

`TrainerOverview` reads cached repository/provider contracts. It excludes missing reference catalogue Adventures, deduplicates identities, preserves unavailable reference/archive counts as unknown, and never opens external saves or requests account data. The page refreshes these projections on entry and after library updates. Home retains its selected-Adventure scope; these library-wide counts do not replace its unknown game progress.

## Favorite selection

The old three-name sample cycle is replaced by `SpeciesPicker`, backed by the same reference provider as the Pokédex. Normal runs offer all 1025 species. A selects for the profile draft; X opens the shared controller keyboard for a name or exact National number. Search ignores accents and punctuation. Up/down browse and left/right jump eight entries, keeping the focused row visible. Y clears the draft's choice. B retains the current choice and returns to the profile form.

The profile must still be saved before its choice persists. B or a section change discards unsubmitted edits. A field-guide failure keeps the existing favorite and leaves search/back/clear reachable. Empty search results keep visible focus on Search. Selecting a companion never changes Pokédex favorites, Seen/Caught marks, games or saves.

## Acceptance

- Complete-reference tests cover the last species, exact-number/accent-insensitive search, bounded jumps, cancel/clear and unavailable-reference behavior.
- Aggregates exclude catalogue-only rows, duplicate identities and unavailable durations; unavailable guide/archive reads remain unknown.
- Real SQLite tests preserve the selected species across reopen while keeping journal marks and favorite stars independent.
- SDL scenarios exercise the scrolling picker, keyboard, draft choice, save/restart, cancel handling, section changes and the static Trainer action at landscape sizes.

On 2026-09-13, the Flip production build displayed its 686 linked Adventures, 30 represented Worlds and observed duration while the empty personal journal/archive stayed zero. Controller number entry found species #1025, Pecharunt; the profile choice survived application restart without changing the Trainer identity or journal marks. The original profile favorite was restored after verification. Ubuntu and ARM passed all 23 tests; Windows passed the suite with the updated legacy profile scenario verified in the final four affected checks.
