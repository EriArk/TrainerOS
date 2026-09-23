# Remove a Trainer and ownership audit

## Controller flow

Open Settings → Trainer → Trainers, select the currently open Trainer and press
X. The shortcut is available only for that unlocked profile, outside startup.
To remove another profile, first switch to it through its normal PIN flow.

If a family code is configured, enter it before the confirmation. Keep Trainer
is selected by default. B, Home or L1/R1 cancels an unsubmitted confirmation;
holding A cannot move focus onto Remove Trainer. Submission blocks navigation
until the operation finishes. Active games, storage services and account work
must finish first.

Removal deletes that Trainer's profile, PIN, journal/favorites, history, Hall
memories, navigation and exit-picture records together. Shared library rows,
ROMs, ordinary saves, backups, artwork and other Trainers are untouched. The
shell returns to the chooser without restarting the OS. Removing the last
profile returns to registration; the family code remains a device setting.

## Storage and failure behavior

The existing schema 10 is sufficient. The SQLite worker checks family-code
verification again and deletes only the active owner's personal rows in one
transaction, then selects a remaining profile or a fresh unnamed owner. A failed
write rolls back the whole deletion. The old store refuses further writes while
the application reconstructs its controllers and providers.

Reserved owner IDs remain after profile removal, including the original
`legacy_account_owner` association. They are not selectable accounts. New
registration cannot reuse them or inherit the original root RA directory.
After the last deletion, first-profile adoption uses a newly generated owner.

The active RA provider removes its saved sign-in before database deletion.
Failure to remove that file stops deletion. If the subsequent database write
fails, the profile/records remain, but the user may need to sign in again.
Existing offline RA cache files and external maintenance backups are retained;
they are never rebound to a newly created Trainer. This feature is profile
removal, not secure erasure of filesystem history or backups.

## P2 consumer audit

| Consumer | Current ownership boundary | Remaining work |
| --- | --- | --- |
| Profile, manual Dex and favorites | Owner-filtered SQLite reads/writes and revisions | No save-owned individuals implied |
| Play history, personal totals, Hall and exit pictures | Owner-scoped rows; exit media tied to its owner's session/build | New Journey/Champion observations stay P8 |
| Home choice and page/filter navigation | One owner-scoped JSON record per library scope; explicit Pokémon choice survives switching | Real independent Multiverse domain choices need P3 identities and P4 binding |
| Shared Y and Dex/Center, Hall/RA consumers | One shell context, reconstructed when changing Trainer | Exact ordinary-save lineage remains to be bound |
| RetroAchievements | Provider created per session, directory bound only after entry; switch waits for work to finish | Actual emulator earning remains P6 |
| Save observations and backups | Existing external source, refreshed by new provider after switching | Per-Trainer ordinary-save routing is the next bounded P2 integration |
| Library, runtimes, artwork, hardware and appearance | Device-wide; profile deletion never duplicates or removes them | P3/P4 library/domain work follows |

Owner clarification, 2026-09-23: do not add explanatory ownership labels to Home
as a substitute for correct save routing. Keep its existing presentation. Bring
the first proven per-Trainer ordinary-save namespace forward: one supported
RetroArch route, preserving legacy saves and backups, then verify normal
launch/save/return with two Trainers. This is path/lineage integration, not new
save parsing or editing. Other runtimes require separate proof; heavy semantic
save research stays late P8.

This is also a general delivery rule: ship working behavior, not additional
explanatory copy as a substitute. The repository contract records it in AGENTS.md.

## Verification - 2026-09-23

- Windows native build and all 39 tests passed; ARM64 container build on Flip
  and all 43 tests passed. Focused ownership tests cover rollback, wrong family
  code, final-profile onboarding, reserved IDs, late writes, account-file
  failure and preservation of the other Trainer and shared library.
- Installed the non-testing ARM64 build after a settled Plasma maintenance
  transition, with a paired database/previous-binary backup. Production SHA-256:
  `368f1e2d01dc8121ddd4a517d861e882e6c3fe5c00fd60b57a29aff06ff56df4`.
- On the actual installed Flip session, InputPlumber controller events opened
  the current-profile X action. Default Keep, B and L1/R1 cancellation worked.
  Confirmed removal of the existing disposable ASH profile, returned through
  the chooser and reopened the original Trainer without another OS/session
  restart. No personal family/PIN code was changed for this check.
- Compared the resulting database with the paired backup: all 686 shared
  Adventure rows unchanged, original profile/account file unchanged, all 36
  original history rows and two exit pictures unchanged. Removed-owner personal
  rows are absent; integrity and foreign-key checks pass.
- Captured and inspected actual handheld screens (chooser, confirmation and
  return). Private captures, backups and verification manifests remain outside
  Git. Human button comfort and destructive power-loss testing were not claimed;
  existing session-transition recovery follow-up stays in P10/P12.
