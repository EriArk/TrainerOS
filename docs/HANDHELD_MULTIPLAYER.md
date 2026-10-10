# Handheld link over the network (MP-02)

## Generic GBA linked pairs - 2026-10-10

The generic GBA route uses [mGBA Splitscreen](emulators/mgba-splitscreen.md),
its existing two-machine cable engine, separate SRAM regions and local screen/
audio selection. RetroArch provides rollback, Nearby and public-relay transport;
TrainerOS supplies the existing invitation/consent/protected-return journey.
ROM header and exact build identity select this route without a game-name list.
Two players are admitted; existing gpSP reviewed protocols remain unchanged.

Kirby & The Amazing Mirror reached actual linked gameplay through ordinary
Online friend invitations on Flip/Odin, with pink/yellow independent controls,
separate cameras and normal Home -> Exit return. The final core also underwent
Nearby paired validation. [The maintenance record](emulators/mgba-splitscreen.md)
separates build-level regressions, diagnostic relay evidence and final-core proof.
Original save sizes remain 32 KiB on Flip and 128 KiB on Odin; only the assigned
machine can return to its owner's verified mGBA target. Windows/ARM regressions
cover all accepted SRAM sizes, save conflicts/crashes, stale on-disk size,
preparation cancellation and replayed invitations.

No ROM is transferred by SRAM preparation. Guest file transfer, four-player GBA
preparation, Single-Pak/RFU/accessories, remaining per-game transactions and
separate-internet/physical/audio acceptance stay open. This delivers an additional
common runtime route, not every game's compatibility or completion of MP-02.

## Generic battery linked pairs — 2026-10-10

The generic GB/GBC route now includes supported battery cartridges using
[SameBoy's existing two-ROM subsystem](emulators/sameboy.md), separate SRAM APIs
and the established RetroArch rollback/relay. It is selected by cartridge hardware,
with exact ROM/core/runtime/helper matching. Existing DoubleCherryGB volatile
pairs and the older independent netpacket routes remain available. TrainerOS does
not implement a new serial-cable protocol or game-specific network layer.

The player journey remains ordinary play → Game Options / Play together / Online
friend → Accept → Start game → the ordinary save/exit question. Guest SRAM
preparation runs automatically after consent; connection codes and file transfers
are private implementation details. Both clients emulate the pair but show and
control only their own machine. Only that machine's SRAM can return to the local
original, with the existing backup/conflict/crash protection. The peer's temporary
SRAM is not installed as personal progress. Late joining this route is disabled.

### Installed gameplay and return

Validation used the author's publicly committed From Below Pocket release
candidate, [source/build](https://github.com/mhughson/mbh-firstnes/tree/a567ec4973fafbe40bdcff1585177e9182853483/gb),
`beta_builds/fbp_2022_09_12_rc_1_0.gbc`: 131,072 bytes, MBC1 battery, 8 KiB SRAM,
SHA-256 `02a22677d7a6f86a222446ae0903a0b8aea62ebecd3341098af1a6e1f09cbb7c`.
This private verification copy is not included in Git or the product image;
public source availability is not distribution/licence clearance for game assets.

- A normal UI invitation on Flip was accepted on Odin. Automatic preparation and
  the public RetroArch relay launched both machines without player-entered IPs,
  codes, save files or emulator configuration.
- Selecting two-player mode on Flip advanced Odin to the linked waiting screen.
  Starting the match produced separate boards and independently controlled pieces.
  A complete match ended with **YOU LOSE** on Flip and **YOU WIN!!** on Odin.
- After leaving the in-game versus mode, different per-machine settings were
  selected through the game itself: Flip Timed / level 9 / music on; Odin Classic /
  level 0 / music off. Normal Game Options / Exit returned each assigned SRAM.
- Flip's original 8 KiB SRAM hash `920504c3a553d8355cc06d91e9c36b3e2531a8267dc5795db5bb6fc1326f807f`
  was retained exactly in `.before-link`; its returned SRAM hash is
  `85daebd5560b75a9f6714d2abcb0afa6183e6578b1fd3d6d0aa8f898ac81b0ee`.
  Odin started without a personal save and received a new 8 KiB save, hash
  `6c693a72b42b42aa006851117173f527782e42b1ed0a4e7dec7d2fb9a1a096f9`.
  Ordinary Gambatte launches on **both** devices displayed their own settings
  again. This is actual emulator save/readback evidence, not a semantic parser.
- Host departure left the guest process available for its own normal exit.
  Both linked temporary directories were removed after successful return.
  Private reduced captures and hash evidence use `frombelow-*` under
  `work/research/`; no game/saves/screenshots were added to the repository.

Both devices used shell SHA-256
`16b678387fa89fac7cba56ee1dbcd4502ac106c44fdbe4d7ad9fbe17d1306b42`
for this journey and the final patched SameBoy artifact documented in its
maintenance record. A subsequent shell update adds only the exact failed-ROM
compatibility exception described below; its installed fingerprint is recorded
at final delivery: `43cb4c311791efe099ae79d1c1f66696ff3f4e7607b9ece49bf7d629fbe6b412`
matches both installed and running shells. Final verification found no emulator
processes, both linked temporary directories removed and system volume zero on
both devices. The devices share one home internet connection, so use of a
public relay is **not** separate-network acceptance. Owner physical/audio checks
remain deferred; quiet system output is retained.

### Checks and limits

The maintained SameBoy patch restores frontend serial/input state omitted from
upstream rollback, polls input once per frame, supports initial serialization and
selects the assigned screen. The original synthetic two-SRAM/serial program
passes 200 frames with seven predicted frames before each rollback; the previous
view-only build failed at frame 2. Real 128 KiB Online/Nearby helper transfers and
four helper validation/cancellation tests passed. Windows and ARM application
checks cover RetroArch, parties, preparation, process/exit and save ownership;
the separately exported runtime library builds with the new helper controller.

Hnefatafl's exact build `f76a1a8f9292bd68c9330dc9f2721d9b516e03b5ecba1f19cf81d540f528d3bb`
stalled its cable handshake in the pinned local pair too. Diagnostic snapshots
from both network clients agreed on the game position; that remaining failure
was not evidence of network divergence. The exact failed combination is excluded
from invitations. A speculative emulator clock change did not fix it and was
discarded. This evidence must not be advertised as working Hnefatafl multiplayer.

**MP-02 remains open:** generic battery preparation/own-slot return is now
implemented and verified on this title, not certified for every mapper/game.
Generic GBA link, special GB hardware/RTC routes, earlier Gen II/TCG transactions,
GBA battles/cross-edition/Advance Wars and the retained multi-client/external
acceptance remain. Reuse existing runtime solutions for all subsequent families;
the universal product is not defined by any one franchise or cable mechanism.

## Generic linked-pair integration — 2026-10-10

The owner's reuse rule applies to every emulator family. MP-02 now also uses
DoubleCherryGB's existing two-machine cable emulation and RetroArch rollback,
through the existing party/password/relay route. Each client receives its own
machine's screen/audio and controls exactly its assigned machine. TrainerOS adds
configuration and lifecycle integration, not another cable or rollback protocol.

This route selects GB/GBC cartridges by supported hardware without persistent
battery/RTC memory, a valid header checksum and matching declared ROM size.
It does not inspect a franchise title. Exact ROM/core/runtime identities still
match; explicit single-player metadata still excludes invitations. Cartridge
compatibility is not a claim that every game implements two-player cable play.
The upstream single-content memory API exports only machine one's SRAM, so
battery-backed games are not silently admitted to this route. Existing independent
save/netpacket profiles remain intact, and general independent battery-save
exchange remains unfinished. Four-player adapters and infrared are not enabled.

Comparison: Gambatte's existing network serial API performs blocking byte
round-trips and needs a separate network route; it is not a WAN latency solution.
Coffee GB `554ea56b465bb4147e2ed93c06135671c5e8db11` already has generic rollback
and a join CLI, but automatic hosting/admission, managed reachability and ARM
delivery need their own integration. Neither requires inventing serial emulation.
For this increment the installed DoubleCherryGB/RetroArch pair reuses the already
integrated relay. See [upstream mode and maintenance](emulators/doublecherrygb.md).

Verification target: the author's free [Into the Blue](https://jonas-fischbach.itch.io/into-the-blue),
131,072 bytes, SHA-256
`71c4c7cdaee320385407c7c68211b2b388459e9da2090f2888601802b328ba8b`.
It uses an MBC1 cartridge without battery memory and has ordinary competitive
link play. The downloaded ROM stays outside Git and is not a distribution asset.
Windows `retroarch` and ARM `retroarch`, `netplay_client`, `game_party`,
`runtime_multiplayer` checks passed. Regression coverage exercises both player
slots, actual preparation/cleanup, untouched personal SRAM/RTC, all 256 cartridge
type values, damaged/truncated headers, single-player metadata and content mismatch.

### Installed gameplay evidence

Both handhelds run shell SHA-256
`528e367f424d87470b724f8677f1135dc4aa6452bea7c05d3017ebf6e49c8fa3`.
The existing `handheld-20261007.1` core bundle is reused without another core patch.
Ordinary Gambatte launch → Game Options / Play together / Online friend →
incoming notification / Accept → host Start game → ordinary save/exit question
launched the linked pair through the public RetroArch relay. Both still use one
home internet connection; this is not distinct-network acceptance.

- Actual runtime configurations select two machines, P1 on Flip and P2 on Odin,
  corresponding separate screens/audio and `noload-nosave`. No IP or emulator
  menu setup was entered by the players.
- Flip selected Versus while Odin remained at its own title menu. After Odin
  selected Versus, the game recognized both cable participants: Odin displayed
  that player one selects music and starts the game.
- Both exchanged their difficulty/speed settings and entered the puzzle match.
  Distinct boards and directional cursor/tile changes were observed on both
  devices, followed by a return to the linked settings screen. Winner/loser
  presentation was not captured, so a specific match result is not claimed.
- The first startup observation captured the old menu before the asynchronous
  save question appeared; Back cancelled that attempt. A new invitation and
  delayed capture verified the ordinary question and successful continuation.
  No emulator or save-confirmation workaround was added for this timing.
- Both exited through Game Options and have `returned` history entries. Flip's
  linked session lasted 337 seconds; Odin's lasted 398 seconds including the
  interval after host departure. Odin's process remained available for its own
  normal exit after Flip left. Both temporary session directories were removed.
- Final installed/running hashes matched on each device, neither retained an
  emulator process, and all 30 Flip / 1,074 Odin original configuration files
  matched their retained fingerprints. Both device volumes remained zero.
  The ARM build's four changed source/test files matched this checkout; the
  independently exported runtime source also built successfully on Windows.

Private reduced captures use `linked-*` under `work/research/`; the actual
configuration snapshot is `linked-live-config.json`, with final verification in
`linked-final-verification.json`. The free author-provided
game is in each handheld's GB/homebrew library, separate from commercial content.
**MP-02 remains open:** generic independent battery-save integration, other
handheld mechanisms and all retained acceptance below are not replaced by this
volatile-cartridge route. The next implementation decision is reusable own-save
link support, not another sequence of Pokemon-specific network implementations.

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
  Game Boys instead uses rollback; the new generic volatile-cartridge route
  above uses that existing mechanism. By itself it does not deliver independent
  persistent saves.
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
