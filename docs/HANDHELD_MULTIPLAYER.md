# Handheld link over the network (MP-02)

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
uses the ordinary owner resolver. Gambatte RTC bytes are never overwritten by
DoubleCherryGB's incompatible clock representation; Gen II clock continuity
remains unresolved.

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

- Actual Gen I/II/TCG cable transactions and GB ordinary invitation/exit coverage;
  Gen II RTC interoperability. Red boot does not establish whole-family support.
- Upstream warns that DoubleCherryGB battles can be unstable; general real-time
  GB cable games are not established by trading support.
- Gen III battles, cross-edition runtime proof, Advance Wars gameplay, and four
  actual clients for a four-player activity. Header mappings are not that proof.
- Distinct networks and actual multi-user/company acceptance are owner-deferred.
  Implemented invitations stay available for those checks. Human group-voice
  listening during play remains a separate deferred gate.
