# Hall of Fame

**Accepted target, not delivered — #47/#48:** the manual archive below is preserved alongside planned live Journey and verified Champion snapshots. L2/R2 pairs Journey/Hall with RA; shared Y changes the same Adventure used by Home/Dex/Center. The shared Y selector is delivered; Select now owns the local new-memory/refresh shortcuts. The paired Hall/RA route is delivered; live Journey, Champion snapshots and selected-save RA projection remain pending. Historical team/victory facts require provenance; RA is independent account truth. [Projection acceptance](EXPANSION_42_62.md#pokédex-journey-and-achievements).

The local archive is a journal of completed Adventures. Entries are explicit trainer memories, independent of the current game save and of RetroAchievements. Normal runs start with an empty personal archive; sample memories and unlocks are limited to the ephemeral/test composition.

## Controller flow

**Delivered paired routing, 2026-09-19:** L2/R2 switches Archive and RetroAchievements inside the same Hall primary page. The header names the visible face and the chassis hint names the other. The old Archive/RA button row is removed. Each face remembers its list/detail, selected row and action focus across switching, L1/R1 and persisted restart. B only backs through the current face; at its root it stays there. Up on the first row stays on the visible list. Empty lists retain a visible recovery action; RA's root action refreshes its records without moving to the archive.

Y opens the shared Adventure drawer on either face without replacing the open record. A chooses for Home/Center without launching; B cancels. Editor/account/keyboard/system/error overlays and the open drawer block paired switching. Select remains New memory/Refresh, X remains Edit/Account. Async provider results and completion of a submitted memory do not switch the visible face; unavailable rows reconcile when returning, and wrong-account records remain hidden.

The existing navigation JSON keeps its active route fields and adds independent `archiveView`/`achievementView` values. Old rail focus migrates to a visible list/action; no database migration is needed. This routing does not turn historical memories or account achievements into current-save progress.

In Archive, **Select creates a memory** and **X edits the selected memory**, from either list or detail. The attached editor contains Adventure, completion date, playtime, note and champion team. A edits the focused field; Y saves the main form; B discards an unsubmitted draft. L1/R1 retain global page navigation and discard the transient editor. A submitted write finishes even if the page changes.

The Adventure picker contains owned library entries, including ROM hacks, with World and edition labels. X opens shared-keyboard search; left/right jump eight items; up/down move one item. Catalogue entries without an attached file cannot be archived as owned Adventures. An empty picker retains a visible Back action.

The team panel has six controller cells. A edits a name; X edits its level; B returns to the main form. Names allow 24 characters, levels 1–100. Clearing a name clears its optional level. Nicknames and fan-game creatures are allowed; they do not assert Pokédex ownership.

Completion date input uses eight numeric digits, YYYYMMDD, so it works with the existing number block. Display uses YYYY-MM-DD. Blank means unknown; today's date is not assumed. Playtime is optional manual minutes, 0–999999. Notes allow one line of 160 characters. This time is separate from observed process duration on Home.

## Persistence and ownership

`ArchiveEditor` owns transient drafts and consumes repository/library contracts. `HallOfFameController` owns archive presentation; QML has no SQLite or emulator commands. `LocalStateStore` implements `HallOfFameRepository` with an asynchronous worker and committed projections. `loadArchive()` keeps its API distinct from Trainer profile loading.

Schema 5 adds `hall_of_fame` transactionally, without creating memories. Entries store a stable ID, Adventure ID, historical title/World, optional UTC date and manual duration, six optional members, note, source and revision. New memories resolve title/World from the actual library. Editing the same Adventure preserves historical labels; explicitly choosing a different Adventure resolves its labels. Archive records have no cascading deletion relationship to library entries.

Create/edit and reading the resulting projection share one transaction. Updates compare the read revision. Stale writes fail visibly; storage errors preserve the draft for retry. Saves, states and game progress are untouched. Memories do not mark a current Adventure complete or infer badges, seen/caught totals or unlocks.

## Achievement boundary

RetroAchievements stays inside Hall of Fame. The normal composition uses a real provider with controller account entry, verified cartridge-file matching and account-scoped offline records. X opens Account and Select refreshes recently played supported Adventures. Unlocks are external account history, separate from local memories and current saves. Unsupported hash formats remain unavailable. See [RetroAchievements](RETROACHIEVEMENTS.md) for the implemented protocol, privacy, cache and earning boundaries.

## Acceptance

- Database migration/reopen preserves memory identity, unknown fields, team and edits.
- Stale revisions, absent Adventures, invalid members and locked writes preserve committed memories.
- Renaming an Adventure does not rewrite existing historical labels.
- Controller events exercise create, picker/search, text entry, numeric input, team editing, Save, Cancel, global page changes and focus restoration.
- Empty/loading/error states retain usable controls; saved values appear only after successful storage completion.
- Rendered checks cover the landscape composition; device verification and platform test results accompany delivery.

2026-09-13 delivery checks: Windows 21/21 (102.20 s), Ubuntu 21/21 (92.65 s), ARM64 21/21 (99.40 s), followed by affected checks for the final date-only presentation. On Flip, injected OS controller chords created a memory from the real library, entered name/level/note, saved it, reopened the app and edited the persisted record. The test memory was backed up and removed, restoring the previously empty personal archive. The production binary and pre-migration database were backed up before installation; all 686 library records survived migration. Physical comfort feedback remains with the owner. GitHub Actions currently cannot start jobs because of account billing, independently of these successful builds.


## Paired-route delivery evidence - 2026-09-19

The final source passed Windows 33/33 and ARM64 37/37 tests. Native cases cover independent active/inactive route restoration, legacy navigation, unavailable data, wrong-account isolation, pending Refresh focus, modal precedence and a submitted memory finishing after navigation to RA. The SDL scenario exercises both trigger axes, list scrolling, details, empty/loading/error states, editors, shared Y and primary-page return; drawer-bound checks wait for its animation to settle. Rendered checks cover 960x540, 1920x1080 and letterboxing. The Hall list viewport now ends above its attached action panel so the selected row is fully revealed.

The non-testing ARM64 binary was installed on Flip with binary/database backups and SHA-256 readback verification. Schema 7 and all 686 Adventure entries were retained. Production button events through InputPlumber and bounded evdev trigger injection exercised archive/RA return, an actual cached achievement detail, shared-Y cancel/selection without launch, system/account modal priority, L1/R1 return and session restart restoration. Gamescope captures were inspected; the device was left on Home with no running game. The archive was empty, so populated archive-detail proof comes from native/SDL fixtures. Physical human trigger presses remain a separate confirmation; these checks do not prove new achievement earning or current-save mapping.
