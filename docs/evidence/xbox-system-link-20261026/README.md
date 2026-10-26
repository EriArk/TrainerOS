# Xbox System Link — 26 October 2026

Implemented and installed on Flip/Odin from `7220e65b`. [Contract](../../emulators/xemu.md).

- ARM64 standalone, Lobby, GameParty and RuntimeMultiplayer suites: 4/4 passed.
- Real existing ICE/DTLS helpers plus the Xbox UDP endpoint: 2/3/4/repeated-4,
  14/60/1518-byte Ethernet broadcast, every sender, no self echo, forged source
  rejection and loss while the game child remains alive passed.
- Native ARM64 xemu on Flip/Odin shows the UDP backend attached to `nvnet.0`;
  normal running status and QMP quit exit 0. Own HDD/EEPROM hashes and native
  settings remained unchanged. These no-game runs used native `-snapshot` only
  for verification; the product launch uses each player's ordinary persistent HDD.
- The first paused/headless bootstrap did not quit within its deadline and required
  termination. It is not counted as a successful lifecycle check. A separate
  normal native run exited cleanly. Upstream logs still emit a GLib teardown warning
  with exit 0; no claim is made that this warning is repaired.
- Flip's configured EEPROM was absent. Its own native xemu generated it during
  initialization; the newly created 256-byte file was retained. It was not copied
  from Odin. Ordinary Applications binaries and configs were left unchanged.
- The initial socket/hub launch left nvnet detached; corrected to native `-nic`.

No game match, new title-update compatibility claim or different-ISP acceptance
is inferred from these tests. There is no title-specific workaround, VPN or new
relay service. DS 3–4 expansion was stopped at the owner's request in favor of
platforms with no connected multiplayer route; the existing two-player DS is unchanged.

## Installed delivery

[Odin](odin.json) and [Flip](flip.json) run production shell
`207bc5112d544a823d31389d56bdb36a87b24d7a9e6e7eb8cad211bcacbef4ae`.
Both use package manifest `c4fdb1285cbe4da9669b9b973a83a4d3578b42a240b66c8a8c2af0ccb5b306c1`.
The executing processes and all package files were rechecked after installation.
No QML errors; 19 other package manifest records remained unchanged.

Own Xbox config, ordinary Applications runtime, HDD, EEPROM and firmware hashes
were identical before/after delivery on each device. The broader protected file
inventory was unchanged: Odin 2169 files, Flip 1539. Complete database rows for
`play_sessions`, `worlds`, `adventures` match each device's own SQLite backup;
history contains 245 and 412 sessions respectively. Audio stayed Odin 0%, Flip 20%.
No game was launched by installation; no reboot was needed.

Rollback shell/settings/SQLite are retained under `~/.cache/traineros-xbox26/before`.
Restore the previous shell and remove only the new `xemu-lan` bundle with games
closed; do not overwrite own HDD/EEPROM or later progress from the backup.
To retain disk headroom on Odin, the previous wireless delivery's older rollback
binary was copied to SD, checked by SHA-256, and its original path replaced by a
symlink. It remains available; the current Xbox rollback is retained internally.
After delivery Odin has 187 MiB free and Flip 3513 MiB.

The production build includes the existing core navigation `38cb8947`; the other
core checkout was rechecked at `85934bc7` (documentation-only changes since that
code). No parallel core WIP was overwritten. `BUILD_TESTING=OFF`, 1199 source
files matched the build snapshot, runtime adapter exports and knowledge check pass.

At the owner's request work pauses after Xbox delivery. Model 3 or other missing
routes have not been started; this does not reinstate DS group expansion.
