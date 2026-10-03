# Flycast: aligned package and internet transport checkpoint

2026-10-03. Package installation and pinned-source audit only. No TrainerOS
Flycast network adapter, actual match, standalone launch migration or internet
acceptance is claimed. The owner clarified during this investigation that
internet is the main scenario; LAN and direct nearby play remain in scope.

## Installed artifact

- Upstream `flyinghead/flycast`, tag `v2.7`, source commit
  `5aa091fde632fb332c8d8c34e280d62dc951954c`.
- Flathub system application `org.flycast.Flycast`,
  `app/org.flycast.Flycast/aarch64/stable`, commit
  `1d48669e88c0261c5c07d93bc0d48efbdbcf4be8ec5f6865825f47a7100982ef`.
- Both positively identified devices (Retroid Pocket Flip2 and AYN Odin 2)
  now have the same 29,100,344-byte `files/bin/flycast`, SHA-256
  `4b3b69727bf68781ecdba95a3c672da6279009ab87e9045630ba143b5f407c57`.
- Odin already had this package; this pass installed it on Flip. The shared
  [observed baseline](../../packaging/emulators/arm64-baseline.json) records it.
  Package equality is not proof of paired emulation or compiled feature behavior.

No game was running at the pre-install inventory. No TrainerOS executable,
controller configuration, library binding, ROM, BIOS or save was changed.
Flip still has no ordinary Flycast `emu.cfg`; Odin retains its existing settings
under `~/.var/app/org.flycast.Flycast/config/flycast/emu.cfg`. No source patch
or separate modified runtime has been deployed. The existing libretro route
remains available; installing the standalone does not migrate its VMUs.

## Source findings that change the next implementation step

The inspected [GGPO implementation](https://github.com/flyinghead/flycast/blob/v2.7/core/network/ggpo.cpp)
uses two participants, local/remote player assignment and UDP port 19713.
`network:GGPO`, `network:ActAsServer` and `network:server` initiate this path.
It can request UPnP mapping, but that is not a guaranteed route through double
NAT/CGNAT and cannot satisfy the no-router-administration requirement by itself.
The handshake compares game/BIOS and initial state digests. Different ordinary
VMUs/flash contents can therefore prevent synchronization even with equal ROMs.

The separate [ICE implementation](https://github.com/flyinghead/flycast/blob/v2.7/core/network/ice.cpp)
does contain match-code rendezvous, STUN and TURN. However its connected data
path attaches to the emulated serial/Battle Cable interface. The
[network settings](https://github.com/flyinghead/flycast/blob/v2.7/core/ui/settings_network.cpp)
expose that match-code flow in the Battle Cable branch, not GGPO. Consequently,
enabling existing ICE settings does **not** supply automatic GGPO internet play
for Soul Calibur. Do not advertise such a route based on those settings alone.

[Flycast Dojo](https://github.com/blueminder/flycast-dojo) documents internet
match-code P2P as an alternative implementation to evaluate. Its capability is
not present merely because upstream Flycast is installed. No ARM64 artifact,
strict-NAT fallback or TrainerOS integration for Dojo was verified in this pass.

## Next acceptance, in order

1. Resolve the GGPO WAN route: select and prove native match-code traversal with
   a usable relay fallback, or the already-defined managed transport when needed.
   Do not silently use a player's personal VPN or expose manual IP/port setup.
   Any maintained source bridge needs its own patch, recipe and service contract.
2. Prepare isolated configuration, firmware copies and VMUs/flash for the exact
   game. Preserve ordinary progress and map physical pad A to the assigned player.
   Keep network-internal rollback separate from retired user savestate features.
3. Connect existing Home/Social Invite/Accept/Join, status, cancellation and exit
   to real runtime operations. Retain Nearby and Online friend without new setup
   pages. Do not expose an online invitation that only has a LAN implementation.
4. Prove actual two-player inputs, internet route selection, Home return and
   bounded loss recovery. Same-router play and forced relay can provide useful
   separate evidence; neither proves a real distinct-network test by itself.

Soul Calibur from the existing private collection is the first proposed GGPO
target; no title digest/profile or gameplay claim has been registered yet.
Naomi cabinet links and native Dreamcast internet services retain separate modes.

## Update and recovery

Before promoting an update, compare CLI/config keys, GGPO handshake and transport
behavior with this pinned source. Stage the same package on both devices, verify
its artifact, then run the relevant input/save/return and paired transport checks.
Keep device and user settings outside replaceable packages; never overwrite them
with Odin's configuration as a universal default. Do not freeze upstream updates
permanently or infer compatibility from a matching version string alone.

The installation added a package only on Flip. If it must be withdrawn before
integration, uninstall that application without `--delete-data`; preserve its
user directory and shared Flatpak runtimes. Odin's pre-existing package remains.
Future binary rollback must use a recorded compatible commit and must never
restore older personal saves as a side effect.
