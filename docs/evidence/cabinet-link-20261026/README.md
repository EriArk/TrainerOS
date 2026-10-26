# Native cabinet link — 26 October 2026

[Feature contract](../../emulators/flycast-cabinet.md) ·
[Build, patches and rollback](../../../packaging/emulators/flycast-lan/README.md)

## Source and build

- Separate Flycast Dojo native cabinet executable, pinned to
  `d0e47e572b1e7b355e88bda8308c89d0c5156cbf`, with reproducible private inspector,
  session/input/startup and existing local-data/compiler patches.
- Existing ICE/DTLS helper, libdatachannel
  `6b1e2e620f1e37f0eafeee702eaea0043cb305fd`, explicit `cabinet` admission mode.
- Initial ARM64 production candidate SHA-256
  `a224723bc191e7f22d5ecce596901fd841b6448a37048cefeb37abc038986ee5`.
  `BUILD_TESTING=OFF`; 991 normalized source inputs matched before collection.
  This candidate was **not installed**: the pre-install guard detected the
  concurrent Crates delivery `8056f761…` on both handhelds. Its source commit
  `b92e48a8` was merged, retaining Update/automatic-update UI and lifecycle.
  The final combined artifact is recorded with the delivery receipts below.
- Native package and endpoint built on ARM64. No game/BIOS data is published.

## Checks performed

- Four focused ARM64 suites passed: `flycast_netplay`, `local_lobby`, `lobby`,
  `runtime_multiplayer` (129.71 seconds). Existing GGPO and new cabinet profiles
  remain separate; stale selection, readiness and loss handling are covered.
- Native namespace identity/staging test ran separately on the Odin host:
  `cabinetCapabilityAndPrivateLaunch`, **no skip**, passed. The build container
  rejects nested namespaces; its explicit skip is covered by this host run.
  This uses an explicit capability fixture, not a synthetic commercial ROM.
- Nine real Lobby/helper admission cases passed, including cabinet guest removal
  and malformed candidate batches. Temporary connection material is revoked;
  the room's playing phase is retained.
- Both transport suites passed, including real ICE/DTLS, mode mismatch, packet
  sizes 1–16388 bytes, split framing, a 32-packet maximum-size burst in both
  directions, duplicate local attachment, bounds and cleanup. Coalesced valid
  records no longer trip the former 64 KiB buffer boundary.
- Two endpoint tests passed on Flip: actual separate loopback namespaces,
  UDP → local IPC → ICE/DTLS → local IPC → UDP, fixed peer ports, foreign-source
  and oversized-packet rejection, and connection loss with the child still alive.
  The child is a protocol fixture; these are not gameplay results.
- Actual patched native loader inspected the owner's existing Dolphin Blue ROM
  without a GUI/input/CPU session and returned `link:none`, exit 0. Thus a game
  supported by GGPO is not incorrectly advertised as cabinet-compatible.
- Adapter/runtime export and knowledge checks passed.
- After the concurrent Crates merge, all **23 Crates tests** and the
  `runtime_multiplayer` suite passed again. The previous two unrelated CoreTests
  failures remain recorded in the [Crates evidence](../crates-updates-20261026/README.md);
  this increment does not claim a passing whole-project test suite.

These checks do not establish a played cabinet match, native compatibility of all
listed upstream titles, a full account-to-game journey, or different-network play.
No such acceptance is claimed. Native Dreamcast game-server Online is not part
of this implementation.

## Delivery

**Installed and running on Flip 2 and Odin 2.** Implementation `537bfd8a`,
combined source `776bbb25` (includes Crates `b92e48a8`).
[Sanitized receipts and package hashes](delivery.json).

- Final production shell SHA-256:
  `c16454045073749590a451f20e49224a1a22fdbf85eb19510d0a66213efff698`.
- Separate `flycast-lan` manifest SHA-256 on both devices:
  `b072c27555b4fb0e259818a26de62084c3fb84923676c85a6888bb3374ef5913`.
- Each live process and installed executable matched the collected artifact.
  No QML startup errors; SQLite checks passed and play history was unchanged.
- The existing **17 other emulator manifests**, including GGPO Flycast, matched
  their pre-install values. Protected personal/configuration files were unchanged
  (Flip 75, Odin 1284); system volumes were retained.
- Crates `active/data` was additionally compared with the preceding Crates
  delivery's retained SHA snapshot: **1463 files on Flip / 882 on Odin** unchanged,
  including the file inventory. This is a separate retained baseline, not part of
  the personal-file counts above.
- Previous shell, settings and a consistent database backup remain private on
  each device. Rollback restores the preceding shell and removes only the newly
  added cabinet package; it does not restore old personal progress.

No active game was replaced. There was no reboot, sleep-cycle test, ROM transfer
or personal-save migration in this delivery. The installation does not change
the gameplay/different-network evidence boundaries above.
