# Dolphin integration checkpoint

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
