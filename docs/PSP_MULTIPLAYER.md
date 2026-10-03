# Standalone PSP multiplayer research

2026-10-03, block 1; **researching, not accepted**. Both handhelds have PPSSPP
1.20.4. First target: the owner's Lumines - Puzzle Fusion (US) ISO, 209911808 bytes,
SHA-256 `9b21dd44a2b9ceb746ab9ed9cea4b30de69e63880704c0e34177f1c30c38b82b`.
Its multiplayer mode must be verified inside the game. No game content is bundled.

## Implemented route and paired evidence

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
