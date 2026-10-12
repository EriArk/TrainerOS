# Cemu: Wii U online-service boundary

## Device and source audit — 12 October 2026

Wii U remains in scope; no TrainerOS multiplayer profile is enabled by this audit.
The currently installed Odin ARM64 AppImage matches the release digest from
[the package selected by Armada](https://github.com/pkgforge-dev/Cemu-AppImage-Enhanced/releases/tag/nightly):
`Cemu_Nightly-aa20e2f86-anylinux-aarch64.AppImage`, SHA-256
`b2f37abefde3ba8f94556f4ed00909cbc8728959606070571ef4d0d049e6fd84`.
The [packager recipe](https://github.com/pkgforge-dev/Cemu-AppImage-Enhanced/blob/main/get-dependencies.sh)
builds upstream Cemu; this artifact identifies source
`aa20e2f86e93c9f625f048dbd2b6b993f1973180`. It is an ARM64 packaging route,
not an x86_64 executable inferred to work on ARM. The older Flip AppImage is
also ARM64, SHA-256 `8b6920d40b0eee45148833d735b1b40fe40888e11d75a708493c126b8cf853cc`;
its exact source remains unpinned. Historical logs on both mention `cee557d`,
which must not be substituted for the current package identity.

Native game online services, including Pretendo where supported, are distinct
from shared-controller NetPlay. No general Cemu state-synchronization route was
established. A transport tunnel cannot supply game accounts or service support.
The [upstream CLI](https://github.com/cemu-project/Cemu/blob/aa20e2f86e93c9f625f048dbd2b6b993f1973180/src/config/LaunchSettings.cpp)
provides fullscreen/game/title/MLC/account selection; it does not establish a
universal join-room or friend-invitation API.

### Concrete missing prerequisites

On both handhelds the active Cemu user-data directory lacks `otp.bin` and
`seeprom.bin`; the active MLC has no certificate files under
`sys/title/0005001b/10054000/content`. Each has one account file, whose existence
does not prove a usable online identity. Only presence/counts were inspected;
no account or console credential values were printed, copied or generated.
Flip's configured MLC is on its game storage and must remain there.

The [pinned upstream online check](https://github.com/cemu-project/Cemu/blob/aa20e2f86e93c9f625f048dbd2b6b993f1973180/src/Cafe/IOSU/legacy/iosu_crypto.cpp)
requires own OTP/SEEPROM of the expected sizes and dumped certificate files.
[The provider's setup contract](https://github.com/PretendoNetwork/website/blob/master/content/docs/install/cemu.md)
requires a configured own Wii U/Pretendo identity. Its old version wording is
not used to select the current package. Those external prerequisites and a
supported paired title are needed before real service admission can be checked.
Do not share one console/account identity between participants or invent one.

Current content: Flip has Wii U titles; Odin's checked Wii U library has only
catalogue/art. No two-player service login or game session was attempted. The
existing packages, settings, console identities, MLC and game progress were left
unchanged. There is no new image default, source patch or installed adapter to
roll back. Future integration must pin a common maintained package while retaining
per-user accounts, custom storage and native saves; version/package presence is
not performance or multiplayer acceptance.

Continue the independent Vita route while these prerequisites are unavailable.
Retain automatic ordinary invitations, the same-running-game exception for real
native online games, per-game service compatibility, owner physical/network
acceptance and full release licensing. This is an evidence-backed conditional
route, not a delivered Wii U multiplayer feature or a whole-task completion.
