# Handheld link over the network (MP-02)

## Red invitation, trade and ordinary return — 2026-10-10

The complete Red journey now passed through the installed TrainerOS interface:
ordinary Gambatte play, Play together / Online friend, accepted invitation,
the host's save/exit confirmation, automatic independent DoubleCherryGB sessions,
an actual Cable Club trade, Home / Exit on each device and ordinary Gambatte
Continue/Party readback. The two handhelds used the public relay from one home
connection; this does not establish distinct-network acceptance.

Exact target: English US/Europe Pokemon Red, SHA-256
`5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b`,
with the pinned DoubleCherryGB bundle documented below. This is runtime/save
interoperability evidence, not a new TrainerOS semantic save-edit capability.
Each console used its own isolated test SRAM and Trainer, prepared at the Cable
Club entrance. Ordinary personal saves were not used for the trade.

### Player-visible fixes included

- The network connection budget pauses while the host is answering the ordinary
  game's save question; connection timing starts afresh when multiplayer launches.
  The existing capture/close controller still bounds its own operations.
- Failed game finalization stops the pending multiplayer launch and preserves its
  actual error. Cancelling the save prompt retains the original process; a queued
  launch from an old invitation cannot attach to a replacement invitation.
- A verified game becomes available for invitations as soon as its own identity
  check finishes, without waiting for large disc images elsewhere in the library.
  The active title is scanned first, then smaller content. Existing compatibility
  and multiplayer eligibility checks remain required.

### Installed and save evidence

- Flip traded Bulbasaur to Odin and received Charmander. Both displayed
  `Trade completed!`. Flip exited first; Odin retained its game and its own
  explicit save/exit confirmation after the host left.
- The previous assistant session stopped at that Odin confirmation. Recovery
  inspected the still-running process and completed the ordinary exit; it did
  not replay the trade or replace SRAM manually.
- Both returned 32 KiB saves have valid Gen I checksums and retain their own
  Trainer. Each `.before-link` backup equals its own pre-trade input. The retained
  Pidgey record, OT and nickname match that player's input byte for byte; the
  received creature's record, OT and nickname match the other player's offered
  creature byte for byte. The game moves the received creature to the end of the
  party, so verification follows the records rather than assuming a fixed slot.
- Both normal-exit finalizers removed their own temporary link directories.
  Ordinary Home launch selected Gambatte on both devices. Continue loaded each
  Trainer; Party showed Pidgey/Charmander on Flip and Pidgey/Bulbasaur on Odin.
  After ordinary exit, checksums, Trainer and party records remained valid.
  Other SRAM bytes changed during ordinary execution; whole-file identity after
  playing is not claimed.
- All 30 Flip and 1,074 Odin configuration files in the pre-test fingerprint
  manifests remained unchanged. The temporary Red save-directory overrides were
  removed after readback, restoring each saved original registration. Test saves
  and evidence remain private, separate from personal progress and Git.
- Both installed and running ARM64 binaries matched SHA-256
  `87f165461afaac3af5b02609a8d6df48101bfe3b0a97bcc9c26a5f339afaaf28`.
  Both TrainerOS sessions are running after cleanup, with no emulator left active.
  Odin volume remains zero; no reboot or boot-preference change was required.

Windows checks passed for `retroarch`, `netplay_client`, `game_party`,
`runtime_multiplayer`, `social`, `process`, `adventure_exit` and
`adventure_exit_presentation`. The ARM build's changed sources were compared
with this checkout, then those eight checks plus `adventure_overlay_transport`
passed. Transition regressions use real subprocesses and cover save-prompt
cancellation, fresh connection timing, finalization failure and replacement
invitation isolation. A pending-worker test covers early game availability.
The refreshed portable runtime copy also built independently, including its
current shared repository contract; the adapter export/knowledge checks passed.

Private originals and compact readback evidence are retained in
`work/research/mp02-recovered-20261010/` and the `mp02-gambatte-returned-party`
captures. They are not distribution assets or replacements for README screenshots.

**MP-02 remains open.** This closes the Red invitation/trade/normal-return gap,
not Gen II/TCG cable transactions, Gen III battle/cross-edition proof, Advance
Wars gameplay or the owner-deferred network/multi-user/audio gates below.

## Gen II clock continuity — 2026-10-07

Target: the pinned DoubleCherryGB revision below and Gambatte libretro
`d9d6cd06382d1ced30de34d56d3609452323dab1`, English Gold/Silver/Crystal
MBC3 cartridges. No additional semantic save-write capability is inferred.
Gambatte exports an eight-byte native-endian epoch base through
`cartridge_libretro.cpp`; DoubleCherryGB exports a four-byte wall clock and
indexes the RTC memory kind as a machine index. Its system-clock mode also
uses host weekday instead of the cartridge day counter. Copying RTC files
between the unpatched cores is unsafe without an explicit core bridge.

The bridge is now implemented in the pinned DoubleCherryGB build, selected only
for the reviewed English Gen II profile. Each player stages their own eight-byte
clock beside their own SRAM. Normal exit returns both through a durable intent
and preimage backups; ordinary TrainerOS launch recovers an interrupted matching
pair. Conflicting originals or an abnormal exit retain recovery files instead.
Unpatched replacement cores are rejected for this route. See the separate
[emulator maintenance record](emulators/doublecherrygb.md) for ABI, patch,
reproduction, update and rollback details. No extra player setup screen is added.

### Bounded installed evidence

- The self-authored MBC3 test cartridge exercised reads/writes in actual Gambatte
  and patched DoubleCherryGB on both handhelds, including a 300-day counter.
- English Gold copies connected through the public Madrid relay with independent
  input, SRAM and clocks. Both loaded Continue, changed position and saved in-game.
  A runtime watcher reported actual normal exits; production preparation and
  finalization returned changed 32 KiB SRAM plus each player's eight-byte RTC.
  Both preimage backups matched their own inputs, and temporary sessions were
  removed only after successful finalization. Ordinary configuration fingerprints
  stayed unchanged.
- Ordinary Gambatte loaded both returned copies into the saved room. The game's
  Sunday clock continued from 11:54/11:57 to 12:02/12:04 respectively instead of
  resetting or becoming the host device's weekday. RTC bytes remained identical
  through Gambatte shutdown; running the game did alter some SRAM bytes, so
  this is readback evidence, not a claim of byte-identical RAM after execution.
- The initial long-running private probe outlived its helper's one-hour timer.
  Its output was preserved but was **not** counted as normal-return evidence.
  The completed cycle used a watcher tied to the real emulator outcome.
- Windows and ARM `retroarch` checks passed (38 Qt test cases); ARM `netplay_client`,
  `game_party` and `process` also passed. The broader `persistence_process` check
  had three pre-existing subprocess-start failures; an old/new binary baseline
  both failed the same seed invocation. That suite is not reported as green.

Both devices remain on one home connection: relay use is not distinct-network
acceptance. This cycle used a private harness around the production adapter,
not the full GB Home invitation UI, and did not perform a Gold cable trade.
Silver/Crystal remain source/profile coverage rather than individual readbacks.
MP-02 stays open for the concrete remaining items below.

Bundle: `handheld-20261007.1`. Installed TrainerOS ARM64 SHA-256:
`474a9720e05be7f4ffe76dcaaf8735c8d3e620b028b95c45c7fe0039108f437f`.
Flip restarted into the new binary; Odin's existing Steam session was retained,
with the new TrainerOS binary installed for its next launch. No device reboot.

## Original research checkpoint — 2026-10-04

This is separate-machine link emulation, not shared-controller rollback.
Ordinary Gambatte/mGBA launch remains unchanged. The existing invitation,
party, relay and Home exit flow must own the network session.

Pinned source targets (not yet runtime acceptance):

- [gpSP](https://github.com/libretro/gpsp/tree/5819380c2ffb0900219d700a382ee68c464ebb99):
  netpacket RFU plus explicit Pokemon Gen III and Advance Wars serial protocols.
  The source has more serial support than its README's older TODO suggests.
  `gpsp_serial` selects the actual protocol. SRAM is 128 KiB; no libretro RTC
  memory is exported. Review save interoperability before enabling persistence.
- [DoubleCherryGB](https://github.com/TimOelrichs/doublecherryGB-libretro/tree/03f58ca3dfb4b716f7e66a0e0467f9e85ef82abb):
  `dcgb_emulated_gameboys = 1` uses netpacket for independent SRAM/ROMs.
  The author supports Gen I/II and TCG trading; battles can be unstable and
  real-time link games are not generally supported by this mode. Two emulated
  Game Boys instead uses rollback and is not a substitute for this requirement.
  Its RTC representation differs from Gambatte; never overwrite ordinary RTC.
- [RetroArch netpacket introduction](https://www.libretro.com/index.php/retroarch-1-17-new-netplay-features/):
  uses ordinary netplay/lobbies, while exchanging emulated adapter packets.
- [mGBA](https://github.com/mgba-emu/mgba#planned-features) still lists network
  link as planned. Do not infer WAN support from its local linked windows.

Required evidence: ARM64 core/runtime compatibility; protocol-specific game
eligibility; both local controls remain player one; own save load/write and
normal-emulator readback; accepted invitation and relay join; bounded disconnect
and normal return. Distinct home networks remain owner-deferred. Registration of
this research does not claim those checks have passed or close MP-02.

## Initial development checkpoint — 4 October

Two unpatched ARM64 cores were built from the pinned sources above and installed
alongside the ordinary cores on both handhelds. Runtime dependency resolution
inside the actual RetroArch Flatpak passed. gpSP artifact SHA-256:
`bf0aee3cbb0bbb6d5dcee715a7b3a4da24631bf925a620ab02b937b43a626b4c`;
DoubleCherryGB:
`526e62b1f961086dbcc04ee6f286630753644d202d907bae3628fb9110bfefc4`.
Ordinary mGBA/Gambatte selections remain unchanged.

The draft preparation code was exercised with copied Emerald cartridges and
separate copied saves on Flip/Odin through the public Madrid relay. Both clients
joined the same gpSP netpacket session, reporting about 134–150 ms ping. Their
screens showed independent progression (one early game, one completed save).
This proves neither an in-game cable transaction nor the integrated invitation
journey. Both consoles were on the same home internet connection.

The helper's UDP `QUIT` attempt did not stop the emulator. That observation must
not be reported as a confirmed emulator freeze: the normal SIGTERM exit then
saved SRAM and unloaded the core. A bounded OpenGL comparison also connected
and exited; it does not justify changing the product's Vulkan preference.
The test processes are stopped. Fingerprinted ordinary settings and original
save files were unchanged. The production TrainerOS binary has not been replaced
with this draft.

Windows and ARM `retroarch`, `game_party`, `netplay_client` and `process` tests
pass, including the finalization error/order test. An accidental mutation while
reading absent JSON keys in profile comparison was found and corrected; classic
profiles retain their comparison behavior.

Still required before the implementation delivery: DoubleCherryGB runtime and
save-format proof, actual cable operation/readback, complete invitation/exit
checks, conservative cross-edition matching for modified ROMs, the reusable
adapter export and per-core maintenance recipe, then both-device deployment.
MP-02 remains open. The website screenshots are a separate completed delivery in
`screenshots/site-2026-10-04`, commit `55483ff`.


## Installed delivery and actual evidence - 2026-10-04

This section supersedes the incomplete checkpoint above. Implemented and installed
on both devices; **MP-02 remains open** for the explicit acceptance below.

Ordinary GBA still uses mGBA; link sessions automatically use gpSP. English Gen III
headers select `mul_poke`; Advance Wars 1/2 have their own serial profiles. Ordinary
GB/GBC remains Gambatte; Gen I/II/TCG link profiles use DoubleCherryGB with one
emulated machine per player. Automatic distribution partners and Mew are disabled.
A scraped one-local-player count does not disable a reviewed handheld link mode.
Different Gen III editions match only reviewed retail digests, not header text
alone. Other content must match exactly. Existing invitation consent, relay,
parties, local player-one controls and physical Home flow are reused.

### Save ownership

Each machine stages its own ordinary SRAM, never the host's save. The GBA path
uses the ordinary owner resolver. The 4 October implementation left Gambatte
RTC untouched; the 7 October bridge above supersedes that limitation for the
reviewed Gen II profile. Unrelated clocks remain untouched.

RetroArch can select guest `.netplay` SRAM before DoubleCherryGB registers
netpacket. Both private paths are now seeded; finalization selects the changed
output and detects conflicts. Normal exit atomically returns SRAM with a preimage
backup. Crash, changed originals, invalid sizes and conflicting outputs retain the
original and recovery data. Finalization runs off the UI thread before completion
and cleanup; failures are reported, not silently discarded.

### Paired handheld results

- Unmodified pinned ARM64 cores loaded in both actual RetroArch Flatpaks. Build
  recipes/digests are in `packaging/emulators/handheld`; separate update records
  are `docs/emulators/gpsp.md` and `docs/emulators/doublecherrygb.md`.
- An actual Emerald cable-club trade completed: Flip Swellow / Odin Alakazam,
  including the game's save sequence. Isolated copies of existing saves were
  relocated to the cable-club entrance for this fixture; originals stayed intact.
  Ordinary mGBA then loaded the results: Alakazam on Flip, Swellow on Odin.
  This independently proves the protocol/readback, not production UI acceptance.
- The installed TrainerOS journey passed separately: ordinary Emerald A launch;
  physical Home / Invite friend / Online friend / Odin; Accept; Start; normal save
  reminder before closing mGBA; automatic gpSP launch on both. Actual journals
  confirmed relay connection and both players, about 205-317 ms in that sample.
  Both opened their own Continue. Home / Exit retained the save reminder. Host
  departure left the guest game available for normal exit; both returned without
  rebooting. No new setup screen or manual IP entry was needed.
- Red booted on both through DoubleCherryGB with independent input and saves.
  The initial guest test exposed the `.netplay` defect above. After the fix,
  both loaded existing saves; Odin saved again in-game. The production finalizer
  returned its changed 32 KiB guest SRAM with no manual relocation. Original
  configuration fingerprints stayed unchanged. Earlier Red output also loaded
  in ordinary Gambatte. This is not yet a Gen I cable trade or battle.
- Windows build and `retroarch`, `netplay_client`, `game_party`, `process` tests
  passed, as did ARM checks. The reusable exported runtime built independently.
  Save tests cover guest output, conflicting files, crashes and missing output.

Both consoles were on one home network, reaching the public Madrid relay. This
is **not distinct-network acceptance**. TrainerOS deployed SHA-256:
`343a9a5c8ab39f6aa26cffb0cf27135a5d5654442d9156be46b44f446c2dca5b`.
Representative actual captures: [evidence](../screenshots/handheld-link-2026-10-04/).

### Remaining acceptance - MP-02 stays open

- Red Gen I trade and ordinary invitation/exit/readback passed on 10 October.
  Gen II/TCG cable transactions remain; Gold clock continuity above does not
  establish whole-family support or a trade.
- Upstream warns that DoubleCherryGB battles can be unstable; general real-time
  GB cable games are not established by trading support.
- Gen III battles, cross-edition runtime proof, Advance Wars gameplay, and four
  actual clients for a four-player activity. Header mappings are not that proof.
- Distinct networks and actual multi-user/company acceptance are owner-deferred.
  Implemented invitations stay available for those checks. Human group-voice
  listening during play remains a separate deferred gate.
