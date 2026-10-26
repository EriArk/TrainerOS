# Xbox System Link — 26 October 2026

Source integration is implemented; installation is pending. [Contract](../../emulators/xemu.md).

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
