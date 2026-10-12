# Vita3K: native LAN integration

## Installed delivery — 12 October 2026

Flip and Odin run the maintained Vita bundle and TrainerOS
`c75a93f389af6e34cf5541e26207e662e27a79f5b30c70832d16f10ccc9b9954`.
Vita (`psvita`) is an ordinary library platform. A `.psvita` link contains one
installed nine-character title ID; it supplies no emulator arguments. The
launcher validates that ID against the installed SFO. Unmanaged native executables
are not treated as compatible `.psvita` launchers.

The enabled `runtime.vita3k.lan.v1` profile currently covers one exact rebuilt
vitaQuake EBOOT. This is an installed native IP LAN route, not commercial AdHoc,
PSN, universal Vita compatibility or release/distribution acceptance.

### Ordinary player journey

- A normal solo launch reached a level using that device's own native data.
- Online friend invitation arrived while another Vita copy was running. The guest
  could keep it for later, return to the same process, save in-game and explicitly
  exit. Its separate invitation window retained the compositor overlay role and
  nonzero opacity across the solo exit. Compositor scene captures exclude this
  external layer, so those images alone are not badge-visibility evidence.
- The kept invitation subsequently expired during the longer inspection. A fresh
  invitation was delivered and accepted; stale notices correctly rejected joining.
  This does not claim the original expired invitation was accepted.
- Starting the accepted party asked the live host to save/leave before restarting.
  Both native copies then launched with their own ordinary Vita data paths.
- In the game, the host chose New Game and the guest chose Search for local games.
  The server appeared as `1/2`; selecting it entered one shared level. No address
  was entered and no autoexec/direct-connect file was installed. A read-only native
  query confirmed two players/capacity two; independent movement and shots changed
  ammunition to 23 on Flip and 24 on Odin from 25 each.
- Home, minimize and return kept Odin's original PID. Guest Game Options -> Exit
  -> Leave closed both native copies and removed both ordinary temporary sessions.
- The final production helpers separately admitted both native copies through a
  forced public relay. Killing the guest's owned transport in an idle native menu
  ended both copies; the host observation took about 14 seconds. This proves
  bounded idle-loss handling, not different-ISP or long-match acceptance.

The earlier ordinary-exit trial exposed a host left waiting after a guest left.
The final helper explicitly closes the admitted channel; a small periodic receipt
also exercises the existing channel deadline when the game sends nothing. Native
traffic still uses the emulator/game protocol. Diagnostic logs are bounded by a
separate pipe reader, without a process file-size limit on native saves.

### Identity, saves and packaging

Each participant independently verifies the maintained runtime, EBOOT/SFO, entire
installed app, update and DLC trees, and the profile's reviewed external game-data
extensions. Personal saves, account settings and controls are not peer identity
or transferred progress. Ordinary preparation rechecks effective content before
launch. No semantic Vita save adapter or save-write capability is granted.

The guest created its own `s0.sav` before joining; its SHA-256 remained
`b184e56c83501b67a5df02a9aa049eede1418fbbdb946b48c575585755b00f8a`
after the paired game and guest Exit. Of the original 220 Odin files inventoried
before Vita work, 219 remained byte-identical. Only native `ux0/user/time.xml`
changed through normal game-time accounting. Flip had no pre-existing Vita data
in that inventory. These are bounded checks, not an inventory of every user file.
The new test title/data were added separately; no existing title was replaced.

The installed runtime identity is
`45cbbd15422d7d7369930ac8522d97f1a6196b4066b0c05e2dd0611d271e8a6b`;
the reviewed game-content identity is
`90d98e6834185d6380fa576c4e585cbcf969918c4fa1cfa2bfcc826b8a6eb611`.
The same values were checked on both devices.

[Maintained packaging](../../packaging/emulators/vita3k/) verifies the original
AppImage digest, extracts it into a new version directory, installs the reviewed
shared-channel Python dependency freeze plus PyYAML 6.0.3, and creates a hashed
manifest. It does not select the version or change personal data by itself.
The AppRun wrapper changes only to `exec` and quoted arguments: one owned native
PID, correct paths with spaces and a parent-death guard. Native Vita3K code is
unchanged. Session network-setup capabilities are dropped before game launch.

The homebrew probe needed a source correction: its UDP sockets originally bound
only the local unicast address, preventing native broadcast discovery. Binding
`INADDR_ANY` fixed the actual game browser. The final EBOOT, VPK, SDK/package pins,
patch and rebuild limits are recorded in the
[probe recipe](../../packaging/emulators/vita3k/probes/README.md).
The ordinary graphics preference stays intact; only the reviewed EBOOT uses the
verified OpenGL fallback. Managed sessions disable the emulator PS/Home binding
so TrainerOS Home remains available. Own user, account and audio preferences stay
in native settings; both devices ended quiet at volume zero.

Rollback: close the owned session first, restore the previous TrainerOS binary
and overlay helper, then restore the prior launcher link (the Armada dispatcher
on Odin; no previous Vita launcher on Flip). Select/remove only the owned managed
bundle, retaining all ordinary Vita data and saves. Updating the emulator requires
new upstream pins, manifest/dependency review, compatibility tests and the real
paired journey; never silently reuse old game/runtime acceptance.

Validation: focused Windows/ARM library, collection, standalone and runtime-party
checks; 18 Linux overlay checks; 15 installed-helper checks on each handheld.
No public artwork, firmware or game redistribution clearance follows from this.
Physical controls/audio, different networks, sustained/wider-title play, larger
groups and commercial native services remain open. Wii U retains its own missing
prerequisites; PS3/Switch remain excluded.

## Earlier isolated research — 12 October 2026

The following private development-launcher checks preceded ordinary delivery.

Odin's AppImage matches the published official continuous ARM64 asset:
Vita3K `v0.2.1 4168-46602447`, source
[`46602447c7f84d53790b44e1869ab5105178bf16`](https://github.com/Vita3K/Vita3K/tree/46602447c7f84d53790b44e1869ab5105178bf16),
SHA-256 `8a6b8ec4b65cc6769d69e4a48449d286ecd564ebe3a945a39218d0fa5ae3bf58`.
An independently extracted copy of the same runtime was checked on Flip.
No OS or ordinary emulator replacement was required for this probe.

The [upstream FAQ](https://vita3k.org/faq.html) describes partial multiplayer
support. Native SceNet/P2P sockets are distinct from the unimplemented
ScePspnetAdhoc module and general PSN services. A game's ordinary IP LAN mode
does not prove commercial AdHoc matchmaking.

The available ordinary Vita titles were single-player. The paired probe used
the author's [vitaQuake 2.1 source](https://github.com/Rinnegatamante/vitaQuake/tree/045f0e3d000f8b8eba8a763c4343445811ac39d1),
revision `045f0e3d000f8b8eba8a763c4343445811ac39d1`, with original Quake 1.06
shareware data. This is a privately rebuilt probe, not an unmodified author
release. VitaSDK ARM64 snapshot `20260926.790.1` required header/RTC API, C
language, audio enum and static-library link compatibility adjustments.
VPK SHA-256: `944e1b50f81947af78f6ecb509c6c65204a8de630825c3e5ba9ffb9231de6d75`.
No game assets, firmware or user data enter Git or gain redistribution clearance.

The newer vitaQuakePlus probe failed before gameplay with Vulkan. A later
OpenGL retry reached an Odin level, but the paired guest encountered invalid
guest-memory writes and both processes closed; it is not an accepted pair.
Rebuilt 2.1 reached its
console with Vulkan, but repeatedly faulted in the native `sceClibMemcpy` bridge.
Disabling NGS/GPU memory mapping did not fix it. OpenGL reached an actual level
and accepted input. This bounded fallback does not change ordinary Vita games
or other emulators' Vulkan defaults. Audio is not accepted by a quiet remote check.

### Paired gameplay and relay

- Native UDP LAN connected both devices to `e1m1`. A read-only native server
  query confirmed two participants; virtual-controller shots independently
  changed each player's ammunition. Separate viewpoints were captured.
- A forced public-relay check reused TrainerOS's admitted encrypted transport.
  Each emulator ran in a per-session Linux network namespace with one TAP and
  no default route. Native Ethernet/ARP/IPv4 crossed the channel; no Vita wireless,
  game-state or save-synchronization protocol was written.
- Both relay clients entered the same map, moved and fired independently. A
  server query inside the isolated network confirmed two players after over
  100 seconds. Its only route was the private two-peer subnet, preventing direct
  home-LAN use by the game. Both devices still shared home internet; this is
  not different-ISP acceptance.
- Queues are bounded and may drop excess native datagrams. The trials recorded
  such drops. Short working gameplay is not latency or sustained-play acceptance.
- Closing the guest session ended both copies. Closing the guest native game
  also ended both. Network-setup capabilities are dropped before game launch,
  with privilege regain disabled; the original owner can inspect its executable
  and PID. Installed Home/minimize still requires the actual TrainerOS journey.

### Data ownership and remaining delivery

All trial installations, configuration, firmware copies, game data and logs used
separate private directories. No ordinary accounts, licences or saves were copied
between players. The final bounded Odin inventory retained all 220 pre-existing
Vita configuration/user-save and Cemu save files byte-for-byte; Flip had no prior
Vita data in the checked locations. Both ended with no Vita process and volume
zero. This does not claim to cover every personal file.

The installed section above supersedes the earlier packaging/invitation/discovery
work list. Preserve personal console identity and explicit game support; retain
commercial AdHoc, native services, broader games, physical/different-network and
release acceptance separately. Wii U's [own prerequisites](cemu.md) and older-family
gates remain open; PS3/Switch stay excluded.
