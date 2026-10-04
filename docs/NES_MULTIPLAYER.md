# NES internet multiplayer

2026-10-03. Oldest-first continuation: integrated experimental NES profile, with
bounded actual-device evidence below. Overall block 1 remains open.
The existing RuntimeMultiplayer/GameParty and public-relay path are reused.

## Reproducible first game

[NES-Pong by Tom Brannan](https://github.com/TomBrannan/nes-pong), pinned commit
`a906d33373becb501add88371cb1bd5cdaa09c6a`, upstream `pong.nes` (24,592 bytes),
SHA-256 `b9116433d8f5d3293adfe871b47af68198e1596d40eccc2c3a99b14e2ca2afe0`.
The author provides the ROM in the repository for emulator use. Its accompanying
`pong.asm` reads both NES controller ports ($4016/$4017); this is a two-player
homebrew candidate, not a commercial ROM download. No ROM is redistributed here.
The private server NES folder had metadata/images but no ordinary NES game files
at this inventory, so the public author-provided test game supplies this check.

Use FCEUmm from the aligned ARM64 bundle. Exact content, runtime and core hashes
must match on both peers. Networking starts a fresh two-player game in isolated
session settings with no persistent save writes. This first profile does not
claim that every NES mapper/game/peripheral has multiplayer support.

## Acceptance retained

Real Online friend consent and automatic relay launch; independent P1/P2 controls;
reverse join and permitted group entry; normal Home exit and rejoin; bounded
mid-game transport loss and recoverable return; ordinary settings/save isolation.
Distinct internet networks remain separate from both devices reaching an external
relay through one home router. LAN/direct routes are retained, not the primary
completion target. No new social or emulator setup screens are introduced.

## Flip/Odin runtime evidence - 2026-10-03

Both handhelds ran TrainerOS executable SHA-256
`3bc9023ee3c2c98c902e5e435af40356b8bc7b80f8eb66c38cee9d16999b7bbc`,
with identical FCEUmm SHA-256
`1b13b00d4680394dad8000d5175f97be727107e0945bc9b412da91d70c07b267`.
The ROM was copied into each existing `nes/Multiverse` folder and discovered
without per-game binding. Existing runtime settings and public Fluxer test
accounts were reused. The experiment was enabled only during these checks.

Passed through the installed controller interface:

- One-A normal launch, Home -> Invite friend -> Online friend, named incoming
  Accept/Decline, accepted 2/2 roster, organizer Start, automatic relay host/join.
- Actual Pong gameplay with separate P1/P2 paddles: Flip moved the left paddle up,
  Odin moved the right down; both screens reflected them. Serving the ball
  advanced the timer/score, so this was gameplay rather than a title screen.
- Real TCP payload connections to the public Madrid relay, with observed guest
  join ping 133-149 ms. No player-entered IP or router changes were needed.
- A scoped 12-second relay-only packet drop on Odin interrupted the match.
  The helper's counters confirmed dropped traffic and its finally block removed
  the temporary rules. This was a disconnect, not seamless automatic recovery.
  Home remained usable. Odin exited normally and accepted a fresh invitation;
  it rejoined the still-running Flip host and its paddle control resumed.
- After another normal guest departure, the existing chat showed the game and
  free place. Y sent Ask to join; Flip accepted inside Home and Odin rejoined
  the same host without another invitation or host restart.
- A fresh party attached to the existing private group appeared in Groups.
  Its previously permitted member entered through the game card without an
  extra host approval. Explicit organizer Start reached another real relay
  match and both paddles responded. This proves one party, not two concurrent
  parties or four physical clients.
- Normal Home Exit returned both devices to TrainerOS. No emulator/device kill
  or reboot was required. Database quick checks passed, discovery added exactly
  one Adventure per device, and boot settings/nearby helper hashes matched the
  deployment baseline. Ordinary save files were not byte-audited in this pass;
  the existing isolated no-SRAM session policy remains enforced and tested.

Windows and ARM builds passed, with `retroarch`, `netplay_client` and `game_party`
passing on both. The final ordinary installation contains the new executable on
both devices; the test-only enabling wrapper was removed, preserving the existing
experimental-off release policy. Actual captures are in screenshots 43-48.

## Remaining acceptance and current next work

**Status clarification, 2026-10-04:** the NES transport and exact tested profiles
are implemented. Paired invitation/gameplay/rejoin, Four Score controls and the
subsequent option-isolation/concurrent-runtime evidence below are completed
checkpoints. The active [MP-01 task](CURRENT_TASKS.md) tracks shared real-user,
distinct-network and release acceptance, not another NES implementation pass.
Ordinary builds still keep netplay experimental-off; no arbitrary-title support
or complete internet release is implied. Old "next" statements below retain
historical context and do not override that task register.

The 4 October [paired game/call check](GAME_PARTIES.md#shared-call-during-relay-gameplay-and-rejoin---2026-10-04)
now passes group-call retention during actual relay gameplay and explicit guest
loss/exit/rejoin. Its synthetic-audio evidence supersedes the open ongoing-call
item below for this route; human speech, distinct networks and other gates remain.

- Distinct internet networks/NAT conditions: both devices here shared one home
  router while using the external payload relay. Do not label this a two-network
  proof. Seamless reconnection is not delivered; explicit exit/rejoin is proven.
- More NES games/mappers, multi-controller peripherals and wider compatibility
  policy are not enabled by this single exact homebrew profile.
- Parallel live parties and router-free runtime proof
  remain their separate shared acceptance. No new nearby proof is claimed here.
- The temporary-session save question is fixed by the 4 October shared lifecycle
  refinement below; ordinary games retain their own save policy.
- Continue the oldest-family lane (remaining NES gates, then Mega Drive/SNES),
  before resuming the retained newer-emulator investigations. The common social,
  invitation and party architecture is reused; do not rebuild it.


## Temporary-session exit refinement - 2026-10-04

The adapter now marks its enforced no-SRAM/private-save netplay session as
`temporaryProgress` after preparation. The launch controller reads this only
when that prepared process starts. Explicit Home Exit still captures a fresh
frame and closes the owned process, but skips a meaningless saved-progress
question. Capture failure remains cancellable; the next ordinary game retains
its own manual/unknown/autosave policy. PSP and Dolphin do not set this marker.
The lifecycle regression passed on Windows and ARM, including failed capture,
cancellation and a subsequent ordinary launch. Both handhelds passed actual
Home Exit without the save question in the exact Contra III temporary session;
an ordinary Contra launch afterwards retained its confirmation. See the
[SNES runtime checkpoint](EMULATOR_MULTIPLAYER.md#snes-paired-gameplay-and-temporary-session-exit---2026-10-04)
for delivery and remaining acceptance. This is a shared lifecycle change;
NES gameplay was not repeated just to recheck the same marker.

## Four-player relay checkpoint - 2026-10-04

Super Homebrew War 2025 is an author-distributed free NES game for 1-4 players.
The exact downloaded SHA-256 is
`063eec9f883b44a0a11aa63238316d4e676034aab72eae0b9a092ed50f2bceed`.
The ROM stays outside Git. The profile remains experimental/partial; the test
below is four emulator clients on two handhelds, not four independent Trainers.

The session enables FCEUmm Four Score through explicit Gamepad devices on ports
1-4 and disables the mutually exclusive Famicom expansion on port 5. Party seats
select exactly one RetroArch requested controller, independent of connection
arrival order. Existing two-player profiles retain their previous configuration.
Ordinary launches retain their previous arguments. The no-SRAM/private-save
policy remains; auxiliary per-core option-file preservation is not claimed.

Sources: [author download and controls](https://www.bitethechili.com/superhomebrewwar/),
[FCEUmm input contract](https://docs.libretro.com/library/fceumm/#multitap-support),
[RetroArch requested controllers](https://docs.libretro.com/guides/netplay-multiple-controllers/).

Implementation detail verified against installed FCEUmm `236ccdf`: controller
IDs must be passed through RetroArch `--device PORT:513` for ports 1-4 and
`--nodevice 5`. `input_libretro_device_pN` belongs to remap files; putting it
in session.cfg left players 3/4 disconnected even though the netplay handshake
reported all four players. The rejected config-only attempt is not acceptance.
The corrected CLI is part of the portable adapter and focused regression.

Actual checkpoint, 4 October:

- Flip/Odin ordinary launch -> Home -> Online friend -> named acceptance ->
  2/4 party -> organizer Start reached the public Madrid payload relay.
- Two additional isolated stock RetroArch instances on Flip joined as players
  **4 then 3**, using the production `NetplayClient` password bridge compiled
  into a temporary harness. Observed join pings were 133-152 ms across trials.
  Both received their requested seat, despite the reversed arrival order.
- After the CLI correction, FCEUmm logs report Gamepad for both ports 3/4.
  All four game slots were set to ON. Separate loopback Remote RetroPad input
  changed only P4's character, then only P3's character, as captured on Odin.
  On the arena, P3 changed direction and P4 moved across/down the stage;
  Odin's ordinary P2 controller moved its own character. The match reached real
  four-character play. This is bounded gameplay, not endurance/latency approval.
- The extra clients used isolated test settings, muted audio and unique remote
  input ports. Those ports are a harness detail, never a TrainerOS user setup.
  They were stopped after capture. Ordinary Home Exit returned both user-owned
  games; no handheld reboot was needed.
- Rapid development restarts exhausted the shell supervisor's retry budget and
  correctly returned to Plasma. A raw user-service start was insufficient
  because Plasma still owned DRM. The existing explicit TrainerOS session
  switch restored the proper session on both. Do not use a bare service start
  as recovery while Plasma owns the display.

Windows and ARM builds/targeted tests passed. Both handhelds received executable
SHA-256 `2028de8d7c613e9568424c2ca1fdbd214c8ae92f5704bf9778c7f1a8f911b482`;
ordinary ELF launchers replace the test enabling wrappers at handoff.
Screenshots 62-66 show the real device roster, independent character selection
and arena. No ROM or private endpoint/password is committed.

Remaining: four actual TrainerOS clients/accounts and controllers through the
whole invitation lifecycle; distinct internet networks; concurrent independent
parties; new-route loss/rejoin; inherited auxiliary core-option isolation before
release; broader game compatibility. Existing shared group-call proof is retained
without another repeat. Block 1 stays open and the oldest-first queue is unchanged.

## Session isolation follow-up - 2026-10-04

The remaining auxiliary core-options isolation issue above is fixed and verified
on both handhelds: ordinary settings stayed byte-identical through four temporary
emulator lifetimes. Two simultaneous public-relay rooms also retained independent
input and cleanup; ending room A left room B playable. This is fixture-launched
payload evidence, not four actual TrainerOS users in the company UI.
[Exact checks and remaining acceptance](emulators/retroarch.md#session-option-isolation-and-concurrent-rooms---2026-10-04).
The Four Score settings identity is now `fceumm-four-score-no-sram-v2`;
prior evidence remains historical, with this preservation check as its successor.
