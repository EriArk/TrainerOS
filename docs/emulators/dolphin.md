# Dolphin integration checkpoint

## Ordinary GameCube Hotseat delivery — 12 October 2026

Flip/Odin now offer separate pads or **Take turns with one controller** in ordinary
Play together. The latter admits two people and exposes only P1. Home shows the
confirmed owner and **Pass controller to [friend]**; the spectator retains Home
and touch. Progress is temporary. Existing two-to-four separate-pad profiles remain.
Wii input and persistent multiplayer save return are not added.

The maintained patch reuses Dolphin Golf Mode Prepare/Switch/Release/Acquire and
reliable PadHostData. Internal mappings remain for admission; only the current
native authority polls local pad 0, and only one SI device reaches the game.
There is no live port remapping or new emulation synchronization protocol.
Only the accepted host coordinator requests transfer; native acknowledgement
sender/order checks feed TrainerOS's existing confirmed handoff phases.
Missing confirmation ends the temporary session, without uncertain toggle retries.
Private JSON selects this behavior and the bridge activates Golf Mode before
host/join; the shared base INI is not an observation of the active native mode.
The native Golf overlay is hidden to keep admission nicknames and independent
Take Control buttons out of gameplay. The verified local manifest capability
dolphin-golf-p1-v1 enables the choice; ordinary separate-pad behavior is retained.

**Gameplay:** isolated Mario Golf: Toadstool Tour US reached two-character Stroke
Play with one controller. Six transfers were confirmed (initial owner event 1,
release/acquire events through 13). Odin made Mario's first stroke and Flip made
Peach's; both reached flight with one stroke recorded. Non-owner input did not
dismiss the observed prompt, while owner input did. Guest quit closed both copies.

**Ordinary installed journey:** Online friend consent disclosed turns/temporary
progress; the host retained the solo save/leave barrier. Native traversal launched
both copies. Three Home transfers confirmed guest, host and guest ownership;
input from each advanced the peer's game UI. Host Home -> Exit closed both.
This was one home network through Dolphin traversal, not a relay or distinct-network
proof. Physical controls, sustained/wider-game play, four-person Hotseat and
strict-NAT acceptance remain open. Earlier separate-pad Melee evidence is retained.

Final SHA-256 on both devices:

- TrainerOS: 4d055f96d7408ec7b8673aa28a5fba2bea6979805d1b9262bbadb306d3f57fce
- Native Dolphin: 2bddb337a8ca9ea520bba26165ead594c23e9386bd0501a408a3f473ab6fc5c2
- Source remains c77bbaa0f372c3f72281602a8b087206706542cb; Flathub is unchanged.
- Gameplay probe: ff7f792c68813fdfaa88d392b73faead9ff6dfcd8bd1c884baa0cb389f189670;
  the final bridge additionally hides the native Golf overlay.
- Owner ISO: 1,459,978,240 bytes,
  08a1f0c1f7336418fa814a19907f2094d81062d5e7636ab096a43c41132410f0.

Windows standalone/handoff/party passed (3.06/0.22/0.73 s); final runtime passed
32 checks in 2.423 s. ARM64 standalone/handoff/party passed (4.33/0.22/0.14 s),
then final runtime passed (2.09 s). Both application builds, the native bridge
build and 123-file runtime export check passed. Coverage includes native ordering,
guest inability to assert ownership, stale requests, four-pad-to-two-turn admission,
private configuration and real menu/process bindings. Early failures were fixed.

After the isolated pair all 389 baseline files and Dolphin INIs were unchanged.
After the ordinary journey 388 files, including personal game cards, remain
unchanged. Flip's 68-byte console GC/SRAM.raw, TimePlayed.ini and Qt.ini changed
after its solo launch. SRAM contains console settings/clock/card identifiers, not
a game progress card. Historical and separate post-solo baselines are retained.
No personal cards or controller mappings changed. Both databases pass quick_check,
both native children are gone and volume is zero. Private games/captures stay out of Git.

**Maintenance:** apply the complete patch at the pinned revision and use the
existing recipe; reverse patch validation passed on the actual checkout. Recheck
Golf ordering and SI semantics before rebasing, build identical participants and
repeat bounded native/ordinary transfers before promoting the capability manifest.
Rollback restores previous TrainerOS plus native Dolphin binary/manifest together;
ordinary Flathub, cards, INIs and Sys assets remain independent. PS2, the full
catalogue and external acceptance stay open. No whole multiplayer issue is closed.

**11 October 2026 offline preparation:** the source admission now also recognizes
raw GameCube ISO/GCM discs by disc magic and full content hash. The same pinned
bridge handles two-to-four party-assigned pads; the exact Melee profile is retained.
Unknown player counts default to two, known counts are bounded by four physical
ports, and explicit single-player metadata still excludes invitations. Local
names do not affect matching. [Preparation and remaining acceptance](../MULTIPLAYER_PREPARATION.md#gamecube-extension-research--11-october-2026)
separate this source change from installed wider-game compatibility. No new
Dolphin patch is required; Wii, RVZ/other compressed discs and special peripherals
remain outside this admission. Earlier "no other title" text describes the original
installed delivery. The former global experimental opt-in was removed on 4 October.

2026-10-03. Both handhelds retain Flathub Dolphin 2606a, commit
`50741ae7267181560aa88d3a1ba7177321629c171ad2677504409afa6b4462db`,
using `org.kde.Platform/aarch64/6.10`. The common
[manifest](../../packaging/emulators/arm64-baseline.json) records its executable
hash. Neither installation/configuration nor save directory changed in the
common-core delivery. Existing ordinary launch/controller/Home behavior remains.

## Native bridge delivery — 2026-10-03

A separate native 2606a bridge is installed on Flip and Odin. Source revision:
`c77bbaa0f372c3f72281602a8b087206706542cb`. Executable SHA-256 on both:
`30c959ca95242b9011526b89a0d9f626d4cff9f1bf77482bb9cf603870d6660b`.
It runs against Armada's native libraries, built in the existing Fedora 44
ARM64 container with Qt 6.11.2. Do not put it into the Qt 6.10 Flatpak.
Ordinary Dolphin remains the existing independently updated Flatpak.

The bridge is experimental, under the existing TrainerOS multiplayer opt-in.
This delivery proves exact Melee on two devices over LAN, not every GameCube/Wii
title, four physical clients, or distinct-network internet play.

## Implementation and preservation

The maintained [patch](../../packaging/emulators/dolphin/traineros-netplay.patch),
[bridge](../../packaging/emulators/dolphin/TrainerNetplay.inc) and
[build recipe](../../packaging/emulators/dolphin/build.sh) target the revision
above. The Dolphin modifications are GPL-2.0-or-later. Rebase against upstream
deliberately; do not silently patch a newer Flatpak binary. Rebuild both devices
from the same source/patch, compare executable hashes and repeat the bounded
paired launch/controller/exit check before promoting an update. The ordinary
Flatpak installation, updater and personal configuration remain independent.

The first exact content is US Melee revision 1 (`GALE01`, revision byte 1),
1,459,978,240 bytes, SHA-256
`53a5d0f7c480045a435c889ad47b6d351faff0fc409a7285f3eecc61706b3d9e`.
No other GameCube/Wii title inherits this route yet. The native protocol remains
Dolphin's NetPlay: the patch supplies host/join/start, status and explicit
party-seat mapping, rather than another emulation/network implementation.

TrainerOS hands off a private, temporary session JSON and isolated `-u` directory.
Only the ordinary controller map is copied. Cheats, public room indexing,
save synchronization and persistent save writes are disabled for this versus
profile. Ordinary memory cards, per-game settings and Flatpak files are never
linked into it. Temporary session files are removed after the owned child exits.
This is a fresh versus match, not co-op progression or semantic save support.

The accepted roster closes on Start: Dolphin cannot admit another player during
a running game. The host assigns ports from the accepted party slots, not socket
arrival order; unexpected/duplicate admission names are removed before starting.
The token travels only in the accepted invitation/session file and is suppressed
from native chat/OSD. It is a casual private-room gate, not competitive trust or
a replacement for encrypted transport. Native game/version checks remain active.
The copied player snapshot is taken under Dolphin's players mutex.

Nearby uses the existing LAN route. Online uses Dolphin's traversal rendezvous,
without account, manual room-code entry or user-managed VPN. Traversal is not a
packet relay and cannot guarantee passage through every NAT/firewall. Timeout,
connection and traversal errors must return through TrainerOS; keep distinct
networks, strict-NAT recovery and four-client acceptance separate from two-device
LAN evidence. See the actual bounded evidence below.


## Paired runtime evidence

TrainerOS executable on both devices:
`e0121a5b91bf9976579389bea167010735013bdcc5b8c9617d38f7406ccf96a7`.
The normal route was Worlds A -> ordinary Dolphin -> physical Home -> Invite friend
-> Nearby -> named peer acceptance -> 2/4 party -> Start game -> ordinary-game save
confirmation. TrainerOS then launched the same native bridge on both devices,
created/joined the room, assigned P1 to Flip and P2 to Odin and started Melee.
No Dolphin room widgets, typed address or controller-port setup was used.

Remote input through each device's physical evdev/InputPlumber path selected
Mario and Ness independently, started an Onett match and moved/attacked with
both players. Screenshots show the live match on both devices, not a preview.
Home opened over the native game on both devices. Odin Home -> Exit -> Leave
closed its owned process and returned to TrainerOS; peer departure also ended
Flip's native session and returned to Worlds. No device reboot was required.
Physical comfort, audio and prolonged play were not tested.

Two integration fixes were required: an explicit `-b -C
Dolphin.Interface.ConfirmStop=False` launch contract lets the existing Home
helper recognize the native process; the copied Odin SDL mapping referred to a
Steam virtual controller absent from its native session. The bridge rebinds only
the isolated copy when exactly one SDL controller exists. It retains all original
button mappings and refuses ambiguous selection. Remote single-shot analog
injection was also being overwritten by the hardware's neutral reports; sustained
injection confirmed the real two-controller path without changing device drivers.

The versus session uses fresh temporary memory cards; personal ordinary Dolphin
cards and settings are not connected to it. Session files are removed after exit.
The existing conservative Home save question remains for standalone game exits.

## Online-friend route on the shared network

The same installed binaries also completed public-Fluxer query/offer/invite,
explicit Odin acceptance and the 2/4 roster. Host Start closed ordinary Dolphin
through its save confirmation, launched the native host, exchanged the traversal
room code privately and launched the guest automatically. Both isolated configs
reported `TraversalChoice = traversal`, with UPnP/public indexing/save sync off;
slots were host 1 and guest 2. Native NetPlay started the game on both devices,
and Flip input advanced the guest's synchronized game screen. Odin Home exit
then closed its owned process; Flip also returned after peer departure.

This proves the real Online friend invitation and traversal-assisted startup on
the shared router. It does not prove connectivity between different networks,
strict/symmetric NAT, a relayed game connection or a second complete match.
Earlier three-second screenshots preceded invitation delivery; an eight-second
bounded observation caught it. Do not treat that timing artifact as an outage.

Focused standalone/session-isolation and game-party tests passed on Windows and
ARM64. Four existing process/exit/persistence tests passed on ARM64. The recipe's
patch reverse-check and source include matched the built tree; the staged bundle
hash matched both installed bridges. Actual captures 38-42 are in the [screenshot gallery](../../screenshots/README.md).

## Build, update and rollback

Install build-only dependencies in the ARM64 build environment: CMake, Ninja,
compiler, Qt6 private GUI development headers/SVG, libcurl, libusb, BlueZ,
libevdev, SDL3, Xrandr/Xi/Xcursor/Xinerama, PulseAudio/ALSA and GL development
packages. Keep runtime shared-library versions compatible with the target image.

1. Clone the pinned Dolphin revision with its submodules.
2. Run `sh packaging/emulators/dolphin/build.sh SOURCE BUILD`.
3. Stage with `python3 packaging/emulators/dolphin/bundle.py SOURCE BUILD NEW_OUTPUT`.
   This writes `bin/dolphin-emu`, adjacent `bin/Sys` and the digest manifest.
4. With no running game, back up the existing bridge directory and deploy the
   staged tree to `$XDG_DATA_HOME/TrainerOS/emulators/dolphin-netplay` (normally
   `~/.local/share/TrainerOS/emulators/dolphin-netplay`). Preserve executable mode.
5. Verify matching manifest/binary hashes on every peer and library dependencies;
   repeat one invitation -> independent input -> Home return check after an update.

Keep the recipe, GPL-2.0-or-later patch and exact source with release artifacts.
Upstream changes require an intentional patch rebase, version/digest update and
compatibility check. Restore the prior bridge tree/manifest for rollback; ordinary
Flatpak files, updater, controller map and saves require no rollback because this
route never overwrites them. The reusable runtime adapter export includes this
recipe and patch. No compiled emulator, ROM or private admission data enters Git.

## Updated-handheld recheck — 11 October 2026

The current TrainerOS client and existing exact Melee bridge completed a new
Nearby invitation, Mario/Ness match with independent controls and guest Exit
followed by host peer-departure return. Generic Auto Modellista admission was
checked separately; no paired match of that title is claimed. The ordinary
Dolphin SRAM runtime file changed during solo launch/exit; pre-existing memory
cards did not. [Exact integrity scope and remaining acceptance](../MULTIPLAYER_DEVICE_RECHECK_20261011.md).
