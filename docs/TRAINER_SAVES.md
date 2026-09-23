# Per-Trainer ordinary saves: first RetroArch route

## Delivered boundary

The authenticated session binds one immutable save owner shared by launch,
Home/Journey observations and Center backups. GBA registrations configured for
RetroArch/mGBA with the verified `mgba-sram-v1` setup use it. New Trainers start
with a missing save, never another Trainer's progress. No new save parser,
editor or emulator state is introduced.

The original migrated Trainer continues using the exact existing RetroArch save
layout and backup shelf. Existing saves are not moved or copied into new profiles.
The reserved `legacy_account_owner` identity from schema 9 identifies that
original owner and survives profile deletion. This preserves legacy lineage;
it does not isolate games opened outside TrainerOS.

Other Trainers use:

```
<TrainerOS data>/trainers/<SHA256 Trainer ID>/saves/mgba/
    <SHA256 Adventure ID>/<SHA256 ROM>/<content basename>.srm
```

This separates owners, distinct Adventure registrations and changed ROM builds.
Renaming a content basename changes its SRAM filename and requires an explicit
future relink/migration. Removing a Trainer retains external saves and backups
for maintenance recovery; new profiles cannot reuse the deleted identity.

## Launch, observations and recovery

- Successful entry binds the owner. Before entry, save operations fail closed.
  Switching drains work and reconstructs providers; workers never consult
  mutable UI-owned profile state.
- Launch creates only the verified owner directory and a bounded appended
  configuration. It sets the save directory, disables both sorting flags and
  content-directory saves, and retains ordinary-launch state/override guards.
  Symlinked owned folders/save files and conflicting settings fail without
  falling back to shared saves.
- Home/Journey and Center resolve the identical target. A fresh owner's absent
  directory is an empty shelf, not a read error. Existing exact FireRed/Emerald
  progress coverage remains unchanged.
- Backup shelf keys and bundle metadata include the non-legacy Trainer ID.
  Foreign copies cannot be listed or restored. Restore still verifies source
  and content, protects current bytes and atomically replaces SRAM. Legacy
  bundles without an owner retain their original compatibility.
- Global RetroArch configuration and ROMs remain unchanged. The adapter uses
  documented [appended configuration](https://docs.libretro.com/guides/cli-intro/#using-a-config-file)
  and [directory settings](https://docs.libretro.com/guides/change-directories/).

Only GBA/mGBA is private per Trainer in this increment. Other RetroArch cores,
melonDS and standalone emulators retain shared saves until individually verified.
Real emulator achievement earning is separate; semantic save research stays P8.

## Acceptance

Tests cover pre-entry refusal, immutable binding, unchanged legacy paths and
backups, fresh-owner empty state, launch/resolver agreement, different owners,
Adventures and ROM builds, foreign backup refusal, protected restore, conflicting
configurations and symlink refusal. Installed-device evidence follows below.

## Verification - 2026-09-23

- Full native suites passed: Windows 39/39 and ARM64 43/43. After tightening
  conflicting CLI flags, affected save/adapter checks passed 3/3 on both hosts.
- Installed production ARM64 binary SHA256:
  `a30ba62a483d4038dc9371310b26109537592d2fea4bd97e5c1509a3aa015ede`.
- On the actual Flip, a disposable Trainer opened Emerald with New Game only.
  Ordinary in-game Save produced its own 128 KiB SRAM. A second launch loaded
  that same new playthrough. Home observed 0/8 badges and 0 caught; switching
  back to the original Trainer restored its existing 8/8 and 386 observations.
  Launching the original Trainer's Emerald displayed its existing Continue
  entry with 8 badges and 386 Pokemon; loading and leaving used that legacy save.
- Center began with an empty owner shelf, created a byte-verified copy, cancelled
  one restore question and completed a protected restore. The manual and
  protection bundles both matched the owned SRAM and carried its Trainer ID.
- Removed the disposable Trainer through the installed controller UI. Retained
  its save/copies as documented. The original profile, pre-existing journal and
  history rows, entire Adventure library, account file and all legacy save bytes passed
  comparison with the paired pre-install backup; SQLite integrity/foreign-key
  checks passed. The original Emerald exit image refreshed through its normal
  return; other existing media stayed unchanged. Private saves, screenshots and
  manifests stay outside Git.
- InputPlumber controller events exercised game and shell navigation. For the
  guarded exit, the test drove the existing interception transition and clicked
  the verified overlay action; this is not a new physical Home/A/B acceptance.
  After the maintenance transition, the raw pad's named read ACL was absent.
  Running the existing identity-checked root grant hook restored the helper;
  repeated source/session-transition recovery remains P10/P12 in ROADMAP.
