# Standalone PSP multiplayer research

2026-10-03, block 1; **researching, not accepted**. Both handhelds have PPSSPP
1.20.4. First target: the owner's Lumines - Puzzle Fusion (US) ISO, 209911808 bytes,
SHA-256 `9b21dd44a2b9ceb746ab9ed9cea4b30de69e63880704c0e34177f1c30c38b82b`.
Its multiplayer mode must be verified inside the game. No game content is bundled.

## Online relay and brief loss — 2026-10-03

The same installed executable (`e41f211d…`) passed Home -> Invite friend ->
**Online friend** -> Odin Accept -> organizer Start -> save-aware relaunch.
Both private PPSSPP configurations selected `socom.cc`, relay mode `1`,
`ForceLagSync2=True`, ordinary SAVEDATA and the existing distinct device MACs.
Both actual PPSSPP processes had established connections to `51.91.124.42:27312`
for coordination and `:27313` for gameplay. This proves use of the external relay,
although both handhelds still used the same home internet connection.

The first promptly accepted native challenge reached 2P VS. Independent remote
controller movement/rotation/drop inputs worked on both devices; the short round
ended normally. No address, router or VPN setup was required in the player flow.
Actual handheld captures: [Flip](../screenshots/35-lumines-relay-flip.png),
[Odin](../screenshots/36-lumines-relay-odin.png).

During rematch startup, a temporary Odin nftables table dropped only traffic
to/from that relay on TCP 27312/27313 for 12 seconds. Counters recorded 22 outgoing
and 25 incoming dropped packets. Both games returned to the native opponent
lobby. After rule removal, a fresh native challenge and skin selection reached
another playable round with independent inputs, without restarting either game
or device. [Recovered arena](../screenshots/37-lumines-relay-recovered.png).
This is rematch-start interruption and recovery through a **new match**, not
seamless continuation of the interrupted round or a mid-round recovery proof.
The initial probe command failed syntax validation and applied no rules; only
the corrected, counter-verified probe counts as interruption evidence.

Normal Home -> Exit game -> Leave closed both PPSSPP processes and removed their
private configuration roots. The temporary firewall table was verified absent.
Ordinary executables were restored with the experiment off; database integrity,
counts, boot/helper state and Emerald save hashes passed final checks. Odin stayed
at zero volume and Flip muted. No device reboot or forced emulator kill was needed.
Lumines `DATA.BIN` and `PARAM.SFO` changed on both devices during actual play and
were retained; this is **not** a byte-identical-save claim or a save-format
validation. Flip's ordinary `ppsspp.ini` also changed across the initial ordinary
launch; Odin SYSTEM and both controller mappings were unchanged.

The short online gameplay/retry gate is now evidenced for this exact pair.
Distinct internet networks, mid-round interruption, wider compatibility,
multi-client/parallel parties and party voice remain open. No endurance run was
performed. Do not repeat the passed LAN/relay smoke checks without a new reason.

## Integrated short check — 2026-10-03

The corrected installed build (`e41f211d…`) now passed the actual TrainerOS
route on Flip 2 and Odin 2: physical Home input -> Nearby invitation -> Odin
Accept -> organizer Start -> running-game save confirmation -> paired PPSSPP
launch. Buttons were injected remotely through the handheld input paths; this
does not replace the owner's deferred hands-on controller check.
Both process environments used private configuration roots, ordinary
SAVEDATA, their existing distinct MACs and `ForceLagSync2=True`.

Native Lumines 2P VS reached skin selection and one complete short round, ending
at about 40 seconds with the same result on both devices. Independent movement,
rotation and drop inputs were sent on both controllers. The first native
challenge returned to the lobby; a second promptly accepted challenge succeeded.
Do not claim first-attempt reliability or a sustained-session measurement.
[Flip](../screenshots/33-lumines-invited-flip.png) and
[Odin](../screenshots/34-lumines-invited-odin.png) show the real paired arena.

Physical Home -> Exit game -> Leave returned Flip to its original Home and Odin
to its original Social page. Both PPSSPP processes and private configuration
roots disappeared normally, without kill/reboot. SAVEDATA hashes matched the
pre-run baselines on both. Odin SYSTEM was unchanged; Flip's ordinary
`ppsspp.ini` changed across the run, which included an initial normal launch;
this run does not claim
byte-identical ordinary SYSTEM on Flip. Controller configuration was unchanged.
Both ordinary binaries were restored with the experimental flag off; final checks
confirmed database integrity/counts, boot/helpers, Emerald saves and quiet audio.

This closes the corrected integrated LAN launch/gameplay/return smoke check.
Interruption/reconnection, longer reliability, online/distinct-network and the
remaining multiplayer block acceptance stay open. No endurance run was performed,
per the owner's request. Do not repeat this passed short check without a change.

## Clock synchronization checkpoint — 2026-10-03

The copied-save diagnostic now has verified input: PPSSPP's debugger observed
the normal injected controller Start press/release on both handhelds. Allow the
title animation to finish; repeated blind presses can enter 1P mode. Lumines
also cancels an unanswered challenge after roughly nine seconds. This resolves
the harness gate below, not owner-operated physical input acceptance.

With unchanged device MACs and default clock settings, a promptly accepted
challenge still failed: Flip logged ESTABLISHED at `00:50:892`, Odin ACCEPT at
`00:50:903`, then Odin stopped matching at `00:50:942` and sent BYE. Only Odin
joined the generated group. These times are within the respective logs.

A controlled relaunch with `[General] ForceLagSync2=True` on both devices
passed that transition: both joined `HGFOEPD` (`06:17:867` and `06:18:253` in
the host log), opened skin selection and reached the actual two-player arena.
A subsequent round accepted separate movement/rotation/drop inputs from each
device; [Flip](../screenshots/31-lumines-lan-flip.png) and
[Odin](../screenshots/32-lumines-lan-odin.png) are real Gamescope captures with
the same placed blocks and independent active players. These are sequential
captures, not a frame-synchronization or sustained-performance measurement.
This is evidence for the configuration change, not proof of a specific upstream
callback bug. No emulator source/binary patch was needed.

The setting is now part of the exact experimental Lumines session configuration;
the settings fingerprint changes to `ppsspp-adhoc-isolated-config-clock-v2` so
older and corrected clients do not advertise identical settings. It does not
change ordinary PPSSPP preferences or broaden the title/runtime allowlist.
Regression coverage checks the upstream General section and preservation of
an ordinary per-game False preference after private-session cleanup.

Private diagnostics copied SAVEDATA as well as SYSTEM; original hashes matched
after both processes exited. Debugger/logging were diagnostic-only. The initial
`LocalHost IP will be 127.0.0.1` message is not a failure by itself: upstream
`InitLocalhostIP()` always initializes a loopback address, while actual friend
records and traffic used each device's LAN address. Do not repeat that false lead.

**Superseded by the integrated short check above:** the corrected invitation,
short gameplay and Home-return route now passed. Longer reliability,
interruption/reconnection and online/distinct-network evidence remain open.
Both ordinary deliveries retain the
experimental/off gate. Block 1 is not complete. Do not repeat the old input or
discovery investigation; start from the corrected session profile.

Private raw evidence: `~/traineros-social/lumines-transition/trace-v3/probe.log`
and `clock/probe.log` on each handheld. The diagnostics use upstream PPSSPP
1.20.4; no ROM or save is added to the repository.

ARM build and the scoped standalone suite passed; adapter export/verification
passed. Both ordinary shells were updated without reboot to SHA-256
`e41f211dd92d5736f6d4473f0d4a62a589d9eff5f0c5f657f8e4fc52a95a9e7c`.
Final checks confirmed the running binaries, unchanged Trainer/library counts,
boot/helpers, Emerald saves and volume policy (Flip muted, Odin zero).

Sources: [clock setting](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/Config.cpp),
[localhost initialization](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/HLE/sceNet.cpp).

## Implemented route and paired evidence

The following records the earlier delivery; the checkpoint above supersedes its
input blocker and failing default-clock transition, not the remaining acceptance.

Maintenance and update instructions now have a separate
[PPSSPP record](emulators/ppsspp.md). The 2026-10-03 continuation removed the
per-invitation random MAC override: copied global/per-game console identity is
retained, because upstream warns that saves may be tied to it. The ARM standalone
suite verifies both copies, absence of an appended identity override, ordinary
save preservation and private-root cleanup. Adapter exports/checks passed.

The old paired logs were compared with upstream v1.20.4 matching code. Their
ACCEPT/ESTABLISHED followed by BYE/group transition still does not identify a
proven upstream defect. No PPSSPP binary patch or timing workaround was applied.
An isolated copied-save diagnostic with stable MACs did **not** reach a new match:
remote injected input did not reliably advance beyond the title/attract screen.
Its first root also lacked Flatpak write access; that harness error was corrected
with a command-local filesystem permission, not a persistent sandbox override.
Both keyboard/controller injections and a longer Start press remained inconclusive.
Do not repeat that input sequence as a networking test or count DEMO as gameplay.
Next repair/verify diagnostic input with the real launch environment before
comparing matching behavior or testing clock-sync changes.

All owned diagnostic processes were stopped; original SYSTEM/SAVEDATA hashes
matched their baselines. Odin's exit exceeded the first two-second check but
subsequently completed without a kill or reboot. Both ordinary shells received
build `c8c773b80dc71fd0fe507158cd7ffe532be52b106f5bb1f2aa8e48312be667f2`;
the experimental route stays disabled. This correction does **not** close block 1.

The exact profile now uses the existing physical Home invitation flow. After
explicit acceptance, the host keeps its ordinary save/exit question, and both
owned standalone PPSSPP processes launch with matched content/runtime identities.
No additional per-title setup screen or RetroArch PSP core is introduced.
Fingerprint work stays off the UI thread; file size/mtime changes invalidate the
availability scan and launch preparation rechecks the actual hashes.

On Flip and Odin, LAN invitation, named consent, host save confirmation, paired
launch and ordinary Home exit were exercised through remote controller events.
Actual `/proc` environments confirmed private XDG config roots on both processes.
Their SAVEDATA symlinks resolve to the ordinary memory sticks. System, per-game
and controller INIs are copied, not linked. Original INI hashes stayed unchanged
after exit, and temporary roots were removed. These are remote runtime checks,
not owner-operated physical-button or sustained multiplayer acceptance.

Two integration faults were found and corrected:

- Flatpak resets reserved `XDG_CONFIG_HOME` when passed with `--env`; invoke its
  in-sandbox `env` command before PPSSPP instead. Actual process environments,
  not just constructed arguments, confirmed isolation.
- The local host must use the live invitation socket's LAN address. Loopback
  reached the coordination server but prevented opponent discovery. With the
  real LAN address both games displayed the other player's in-game profile.

**Gameplay gate remains failed.** The native Lumines challenge reaches its peer,
and PPSSPP logs report matching ACCEPT/ESTABLISHED. During the transition one
instance leaves the unnamed lobby and joins a generated game group; the other
stays in the lobby and sees DISCONNECT. The joining game then returns to the
lobby. Reversing the in-game challenge reproduced the problem. Both had distinct
MACs, identical port offsets, live TCP coordination and UDP matching; device
firewall rules did not explain the failure. This is evidence of the failing
transition, not a proven diagnosis of a particular upstream bug. Next inspect
PPSSPP's matching/group transition with this evidence, or establish a supported
second PSP title before broadening the route. Do not repeat discovery-only tests
or add arbitrary timing overrides and call that a working match.

Online friend negotiation was attempted but did not produce an incoming prompt
in that attempt. Public relay TCP reachability alone is **not** online gameplay
proof. Online consent/launch, gameplay, independent controls, interruption and
distinct-network acceptance remain open. LAN is not router-free nearby play.
The experimental capability remains disabled in ordinary device delivery.

The ordinary Flip PPSSPP OpenGL launch also crashed before networking. Its
original INI was backed up privately and only GraphicsBackend changed to Vulkan;
normal game launch then succeeded. Odin already used Vulkan. No emulator binary,
ordinary controller profile, firmware, sleep setting or save was replaced.
Final Flip comparison with that backup differs only in the deliberate graphics
backend and normal PPSSPP RunCount/Recent/PlayTime bookkeeping. Network and
SystemParam sections still match the backup; controller INIs are unchanged.
Odin's original INI hashes are unchanged. Both have no remaining private session
roots, PPSSPP processes or runtime listeners. Final database, Emerald save hashes,
input service, boot preferences and nearby/voice helper checks passed; Flip is
muted and Odin remains at zero volume. No reboot was required.

ARM build and six scoped suites passed (standalone, RetroArch, netplay client,
OnlineLink, Social and Adventure exit presentation). Both devices received the
ordinary build SHA-256
`9b157a0ec83ad7843e6c79e1a04c04be33e8142faf32bb15a7ccd5f69649455d`.
Temporary verbose diagnostics were removed from the delivered code. Private
logs `traineros-social/psp-lumines-lan-probe.log` retain the failed transition.
Actual consent is [screenshot 24](../screenshots/24-experimental-psp-invitation.png).
Block 1 stays open; this is an evidence checkpoint, not completion.

Unlike RetroArch controller sharing, PPSSPP connects two emulated PSPs through
ad hoc networking. Its native AemuPostoffice relay is supported from 1.20.1.
The accepted invitation should configure both instances, then retain the game's
own create/join flow. Shell consent must not be presented as a private or encrypted
room on a public ad hoc server.

Upstream `Config::LoadAppendedConfig()` saves the merged values. Consequently a
session must use a private configuration root containing copied system and
per-game settings/controller mappings, with the ordinary SAVEDATA directory
retained. Do not merge temporary network settings into the owner's live INIs.
Restarting an existing PSP game must retain the ordinary save/exit confirmation;
the no-persistent-save auto-confirm used for Contra is not applicable.

Sources: [official quickstart](https://www.ppsspp.org/docs/multiplayer/quickstart/),
[v1.20.4 configuration](https://github.com/hrydgard/ppsspp/blob/v1.20.4/Core/Config.cpp),
[command-line handling](https://github.com/hrydgard/ppsspp/blob/v1.20.4/UI/NativeApp.cpp),
[upstream relay list](https://github.com/hrydgard/ppsspp/blob/v1.20.4/assets/adhoc-servers.json).
