# Native cabinet link — 26 October 2026

[Feature contract](../../emulators/flycast-cabinet.md) ·
[Build, patches and rollback](../../../packaging/emulators/flycast-lan/README.md)

## Source and build

- Separate Flycast Dojo native cabinet executable, pinned to
  `d0e47e572b1e7b355e88bda8308c89d0c5156cbf`, with reproducible private inspector,
  session/input/startup and existing local-data/compiler patches.
- Existing ICE/DTLS helper, libdatachannel
  `6b1e2e620f1e37f0eafeee702eaea0043cb305fd`, explicit `cabinet` admission mode.
- ARM64 production TrainerOS SHA-256
  `a224723bc191e7f22d5ecce596901fd841b6448a37048cefeb37abc038986ee5`.
  `BUILD_TESTING=OFF`; 991 normalized source inputs matched before collection.
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

These checks do not establish a played cabinet match, native compatibility of all
listed upstream titles, a full account-to-game journey, or different-network play.
No such acceptance is claimed. Native Dreamcast game-server Online is not part
of this implementation.

## Delivery

Production and the separate `flycast-lan` package are prepared for both handhelds;
installation receipts will be added after verifying the live processes and state.
Existing `flycast-netplay` and all other emulator packages are retained.
