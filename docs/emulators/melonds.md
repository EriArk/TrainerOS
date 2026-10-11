# melonDS deterministic wireless preparation

## Research registration — 11 October 2026

Registered as **researching**, now **partial** after the bounded core port below.
MP-06 / #162–166; native DS Local Wireless, initially
two independently controlled consoles. No game fingerprint or compatible-game
claim is registered. No semantic save capability, installed runtime or invitation
profile is supplied by this record.

Target: upstream melonDS `906e9ebb27da8c6a715cd7abab4abfe8a8d29427`.
Reference: WatermelonDS app `9b603e53d4d098a0fcee3401ffa5e9c9992f01a3`,
core `85fdb7596011a3d0e679a6f1f88c312e77ff4cc5`; the actual deterministic core
change is `88a65f9445bc2537ff6f5a78f27694df5a254dcf`. Research and bounded
native-port checks follow here before any runtime enablement.

The accepted [DS requirements](../EXPANSION_161_173.md) retain separate native
port, private state, party integration and real gameplay gates. LAN-only wireless,
game WFC and shared-controller netplay do not meet those requirements.

## Existing implementations compared

- [Desktop upstream at the selected pin](https://github.com/melonDS-emu/melonDS/tree/906e9ebb27da8c6a715cd7abab4abfe8a8d29427):
  `src/net/CMakeLists.txt` compiles `Netplay.cpp`, but its input/mirror paths still
  contain disabled `#if 0` code and cleanup/desync TODOs. In `MPInterface::Set`,
  the existing `MPInterface_Netplay` enum falls through to `DummyMP`; only local
  and LAN implementations are selectable. Presence of a dialog or source file
  does not supply the accepted deterministic Internet route.
- [Reference core change](https://github.com/jojodogm-ctrl/melonDS-android-lib/commit/88a65f9445bc2537ff6f5a78f27694df5a254dcf)
  adds actual `DetMP`, instance clocks, wireless hooks, a shared-WRAM state-load
  repair and a JIT fault-handler guard. Its unrelated generated Vulkan shader
  changes are excluded. The subsequent HgEngine JIT setting is Android-fork
  specific and is not copied into desktop upstream. The final reference pin
  merely restores the shader blobs. The fork's current default branch at
  `246b302c7e68cf4d1315e6914da578a6da23c21c` does not contain DetMP: updating to
  an arbitrary fork HEAD would lose the required implementation.
- [Android frontend reference](https://github.com/jojodogm-ctrl/WatermelonDS/blob/9b603e53d4d098a0fcee3401ffa5e9c9992f01a3/app/src/main/cpp/NetplayAndroid.cpp)
  supplies input delay, mirror workers and ENet orchestration. It also has Android
  properties/logging and a package of firmware, both BIOS images, complete SRAM,
  savestate and native JIT settings. `BuildMirror` allocates the advertised raw
  length before any overall bound. None of that frontend/package parser is
  imported or enabled here. ENet alone does not provide authenticated encryption.

This comparison selects reuse of DetMP on current desktop upstream, not a new
wireless protocol, LAN-over-VPN or porting the entire Android renderer fork.

## Maintained port and ownership

[`packaging/emulators/melonds/`](../../packaging/emulators/melonds) contains:

- `traineros-detmp.patch`: DetMP and factory registration; emulated clock hooks
  in WiFi, frame completion and the desktop Platform callbacks; shared WRAM
  restoration and the reference JIT signal-handler fix. Default Local/LAN/WFC
  selection is unchanged; the core-only checkpoint did not yet supply a player
  frontend. The later isolated local frontend below uses DetMP directly.
- Native lifecycle hardening: permanent thread-safe `Abort()`, wait release on
  participant departure, instance/buffer validation, 1024-byte reply-slot limits,
  requested reply masks, deterministic first-answer handling and a 256-frame
  bound per inbox/reply queue. Overflow aborts; it never silently drops data and
  continues a supposedly synchronized session. This cap needs game-workload
  validation before shipping. Use of C++17 node extraction avoids moving from a
  still-ordered multiset element.
- `CMakeLists.txt`, `detmp_checks.cpp`: build the real desktop core with JIT and
  a focused executable using the actual patched DetMP implementation. No Android,
  network, ROM, BIOS or save is required by those lifecycle checks.
- `build.sh`: a new, isolated native build directory, exact Git checkout, checked
  patch, core build and CTest. `source_manifest.py` verifies the upstream pin and
  reverse patch applicability, then records recipe/patched-source hashes. This
  provenance file is deliberately **not** a runtime launch/admission manifest.

Each instance has one emulation worker. All instances must be registered and
parked before `ResetSession`; it is not a concurrent live state-import API.
The coordinator must call `Abort`, then stop/join all workers before
destroying DetMP or replacing the global MP interface. An aborted object cannot
be reused for another session. `IsAborted` must be handled by that coordinator;
the existing desktop menu is not a completed mirror-worker lifecycle.

The core patch never opens personal files or sends data over a socket. It grants
no save read/write capability. Ordinary melonDS installation, identity, firmware
and storage remain under existing ownership; this recipe installs nothing.

License: upstream/reference GPL-3.0-or-later headers and
[`LICENSE.upstream`](../../packaging/emulators/melonds/LICENSE.upstream) are
retained. The pinned source tree retains its bundled dependency notices. ENet
(MIT) and zstd (BSD) belong to a future transport/package integration if actually
used; they are not copied with the Android frontend. Binary/image redistribution
and artwork acceptance remain #115/#116.

## Bounded local verification

### Native workers and updated handhelds — 11 October 2026

`NativeSession.{h,cpp}` adds two to four independent native consoles with one
worker each, all-seat frame admission and separate key/touch input. All machines
register before the shared clock starts. Input queues are limited to eight frames
per machine, results to 64 checkpoints; malformed/incomplete/out-of-order input
is rejected. Failure aborts shared waits. Stop joins workers and releases volatile
consoles/results; a stopped session cannot accept more work or be reused.

`ProbePlatform.cpp` is a headless verification host: actual DetMP callbacks,
threads and clocks, but no persistent file/save writes, network transport or
audio output. `native_pair_probe.cpp` reads an explicitly supplied local ROM,
uses upstream FreeBIOS/generated firmware and fresh volatile cartridge progress,
then compares two full input replays with different submission scheduling.
The fixed verification clock is 2026-01-01 12:00:00. This cold-boot baseline is
not an owned-save startup policy or a completed player frontend.

Both handhelds first received the owner-requested
[Armada update](../ARMADA_UPDATE_20261011.md), booted `20261010.94f52f8` and retained
their ordinary TrainerOS installation. Flip's existing Fedora 44 build container
built the full ARM64 core/JIT and linked probe with GCC 16.2.1/CMake 4.3.0;
CTest `detmp_lifecycle` passed (1/1, 0.19 s). The same probe was copied into an
isolated development directory on Odin; its native lifecycle check also passed.
The Windows x86_64 linked probe build passed; no Windows gameplay is claimed.

Each final probe run used two replays of **120 startup frames per machine**
(roughly two emulated seconds), comparing each frame's emulated cycles and
XXH64 state digest. Final results, all exit 0 with empty stderr:

| Device | Local DS machines | Compared checkpoints | Probe elapsed | Process peak RSS | Replay aggregate |
|---|---:|---:|---:|---:|---|
| Flip 2 | 2 | 240 | 4.050 s | 444164 KiB | `28b2be93a28b29af` |
| Flip 2 | 4 | 480 | 5.650 s | 849276 KiB | `fe39c02110f885ec` |
| Odin 2 | 2 | 240 | 3.067 s | 443520 KiB | `28b2be93a28b29af` |
| Odin 2 | 4 | 480 | 3.874 s | 849340 KiB | `fe39c02110f885ec` |

Local test case: Mario Kart DS, ROM SHA-256
`0ea8a91a0ae3e9db26337c25a6a8568f6737fa36c1e0761988c0fa0ecbf210ed`.
Each device read its existing accessible owner library; no ROM or personal save
was copied between handhelds. This is a case-specific startup check, not a game
compatibility registration. The check also rejects invalid/duplicate input and
verifies idempotent teardown, empty results and rejected work after Stop.

Final ARM64 probe SHA-256:
`4a293a1df94bed16abb6a838700c031f198f4bf8e157dc1047da0cb13bc33682`.
Windows linked probe:
`a093059889f8631a0842f685709865c571f65068446793ea41c03a7cba142ccd`.
These are development artifacts, not approved runtime fingerprints.

The table measures short software-rendered startup, construction, per-frame state
hashing and teardown; ROM reading precedes the timer. It is **not an FPS benchmark**,
wireless game, four-player acceptance or a network match between handhelds.
Within-device per-frame replay equality and matching cross-device aggregates are
bounded evidence, not universal determinism proof. XXH64 is diagnostic, not
authentication. No DS invitation profile is enabled and #162–166 remain open.

### Earlier Windows core checkpoint — 11 October 2026

11 October 2026, Windows x86_64 / GCC 16.1.0 / CMake 4.3.3:

- All 95 build steps passed: full upstream static core with x86_64 JIT, bundled
  Teakra, and the DetMP lifecycle executable. This is not a linked desktop app.
- CTest `detmp_lifecycle`: **1/1 passed**, 0.32 seconds. The actual test log records
  packet ordering under opposite sender scheduling (four synthetic participants),
  emulated-time barriers, cancellation/peer or self departure during ordinary and
  command-reply waits, invalid inputs, bounded replies/duplicate answers, queue
  exhaustion, power cycling and stale-packet reset. No assertion is disabled in
  Release builds; the fixture uses explicit failure returns rather than `assert`.
- Patch application to a fresh pinned upstream archive, reverse applicability
  and provenance generation passed. Shell recipe syntax passed. The factory
  translation unit also passes C++17 syntax checking with ENet 1.3.18 headers;
  no ENet library or Internet transport is linked by the core-check executable.
- The reusable source snapshot and research registry are maintained with this
  increment. Ordinary TrainerOS application source is unchanged; its previous
  build/tests are not relabeled as a fresh DS client build.

Local artifacts: `libcore.a` SHA-256
`8f440affc269204bbc71c6178b4973fc8b90b5a644cb6c462327e0958510bf41`;
`traineros_detmp_checks.exe`
`15419af91c9297ce809ed250a7dd8d5883c9c31df73d8632c8d1008c2d83b753`.
These identify check artifacts, not installable or approved runtime fingerprints.

This earlier Windows checkpoint alone did not establish Linux/ARM64 or game
execution. The later native-worker evidence above supersedes those build/startup
gaps, while retaining full frontend and meaningful gameplay acceptance.

## Local player frontend — 11 October 2026

The maintained workers now feed a Qt6/SDL2 local player frontend. It presents
both DS screens, direct touch and explicit DS buttons, controller hotplug and
2–4 independently controlled consoles. A single controller follows the selected
console; separate controllers occupy separate consoles. Switching seats, focus
or controller membership requires neutral buttons before accepting held input.
The labels here name DS actions; this does not implement the queued #77 automatic
physical-controller identification and glyph selection.

Rendering copies the latest bounded frame/audio output out of each worker.
Every console drains its own audio; only the selected console is played. Local
checkpoints no longer require serialization on every displayed frame; the player
uses one checkpoint per 60 frames, while the original short probe retains its
full per-frame trace. Exit aborts wireless waits, joins workers and releases the
volatile machines. SIGTERM closes through the same path.

This frontend currently uses fresh volatile cartridges and generated firmware.
It does not open personal saves, write firmware, load incoming states, transfer
ROMs or enable a DS GameParty profile. The visible temporary-session warning is
intentional; owned-save preparation/return remains the next implementation gate.

Optional `--diagnostics` emits bounded packet counts without packet contents.
`--trace-inputs NEW_FILE` records at most 36,008 frames of buttons/touch for local
replay evidence, with no memory, save or cartridge bytes. The probe's optional
sixth argument reads that input-only trace with strict size/order/value bounds,
then compares two runs under different admission timing. This is a developer
verification format, not a new network/state protocol.

Windows x86_64 and Linux ARM64 frontend/probe builds pass. Focused CTest checks
cover DetMP lifecycle, malformed/oversized input traces, firmware SPI reset
and direct/packed GPU command initialization (4/4 on each platform).
The 120-frame ARM64 `jit-present` replay retained the earlier digest
`28b2be93a28b29af`, with copied image/audio bounds and stopped-output rejection.

On Flip, Mario Kart DS reached native Normal Local Wireless group discovery,
2/8 participants and an actual Figure-8 Circuit race. Mario and Luigi responded
to separate selected-console controls; both displayed the other kart on track.
A first empty search was caused by remote input during menu transitions, not a
confirmed packet-delivery defect. This is two virtual DS consoles on one handheld,
not gameplay across two physical handhelds or Internet acceptance. Odin first ran a bounded 600-frame startup/exit, then reached its own two-console
Normal Wireless race with separate Mario/Luigi controls. The Flip race remained
connected through **10:32.618** of the in-game clock. Its 17:03 development process
peaked at **469.6 MiB**, with about **396 MiB** current memory before exit and a
frontend lifetime average near **59.7 fps**. This includes idle time on track;
it is not a human-played complete race, four-console benchmark or Internet test.
Both player processes exited with `DS_LOCAL_EXIT result=0`; ordinary TrainerOS
remained running and both device volumes stayed at zero.

Save inventory after these DS sessions matches the preceding recheck: Odin
28/28 unchanged; Flip retains the same earlier Dolphin `GC/SRAM.raw` change
(SHA-256 `1df76f996f370a684ecc0829f896daf8aded28523e6f3fbbc2cd0c8dab45d144`),
with no additional changed/missing/new files. Both TrainerOS databases pass
read-only `quick_check`. DS personal-save reads/writes remain absent from this
host, rather than inferred safe from only an inventory.

Final local-frontend ARM64 SHA-256:
`14b825e6acd9d3fb8a4bc59b0e0934b8b5fbb855661b0419517e62163ddd0070`;
probe: `5e88d2f1b57dbc71fe0ece16b5431f7be29804480294a86d63df78dcfc63f8dc`.
Both devices have the same final frontend in an isolated development directory;
a final 600-frame startup/exit passed on each after the replay fixes. The earlier
interactive races used the preceding frontend build; the recorded-input replay
below validates the corrected core through the same game sequence.

the ordinary installed emulator/client and invitation availability are unchanged.

The Odin input-only recording contains 23,566 admitted frames / 648,908 bytes,
SHA-256 `414b181fc595bd409971f31f01773655db1865210fa2492a3f7e26d17f531d42`.
It remains private test evidence. The first 23,500-frame two-run replay failed with `REPLAY_MISMATCH seat=0
frame=0` after 297 seconds. A 120-frame prefix using the same sparse checkpoints
reproduced the failure on both devices. Startup section hashes isolated `SPFW`:
`FirmwareMem::Reset` did not initialize the address latch or reset its data
position, even though both are serialized. The maintained patch now clears both.
A ROM-free regression dirties those latches through a normal SPI read and checks
reset state: **fails before the fix, passes after**. Full hashes stay enabled;
no differing field is excluded. The same 120-frame trace prefix then passed
(digest `d9b98c438fffcb9d`). Further full replay isolated undefined bytes in wireless default-reply/ACK
headers and in `GP3D`. The maintained patch zero-initializes packet buffers,
transient vertex arrays/zero-W high-resolution coordinates and both direct/packed
GPU FIFO command entries. The latter is a 64-bit union with three unused bytes;
writing only opcode/parameter left old stack data in serialized FIFO entries.
A ROM-free poisoned-stack regression fails before command initialization and
passes afterwards for both command paths. This fixes the stored bytes rather
than excluding any section from the fingerprint. The complete fixed replay **passes on both devices**: two runs of 23,500
frames, 47,000 compared frame results and **784 full-state checkpoints** per
device, with identical final digest `4c320bcaef435dd`. Flip completed in
300,760 ms and Odin in 285,180 ms; peak memory was 447.4/447.6 MiB. Each reported
12,946 sent and 26,921 received emulated wireless packets. This covers group
discovery, joining and race start from the recorded input, retaining all state
sections. It is replay on each handheld, not a connection between them. Network
admission remains disabled until the other accepted gates are implemented.


## Prepared cartridge ownership — 12 October 2026

The native coordinator now accepts one explicit fresh cartridge or exact-sized
opaque SRAM image per seat, before any worker starts. The pinned core determines
save size; empty/short/long supplied images fail instead of falling back to fresh
progress or silently truncating. The bound is 64 MiB per cartridge; full local
checkpoint storage is bounded at 128 MiB to include the largest supported memory.
No incoming BIOS/firmware/savestate parser was added.

The local owner seat is immutable and independent of the displayed/controlled
seat. A requested return captures only that owner's cartridge after a complete
frame, forces full checkpoints at that frame and waits until every mirror has
reached it. Taking the candidate stops and joins the workers first. Cancellation,
worker failure, duplicate requests and stopped sessions cannot export progress.
The caller still owns peer parity and protected original-save commit.

`--prepared-session ABSOLUTE_DIRECTORY` is a private **local launcher** contract,
not a network message or an ordinary-user setup flow. A bounded `session.json`
binds the local ROM SHA-256, roster and owner. Each seat explicitly selects fresh
progress or a size/hash-checked `seat-N.sav` copy. Paths are fixed; symlinks,
changed copies and reused output files are rejected. Normal Exit retains only
`owner-return.sav` and a final identity/frame/hash receipt, using new files rather
than overwriting any original. SIGTERM/INT aborts without a return. The shell must
validate normal process completion/receipt and use its existing protected-save
lifecycle before committing to the original. Automatic staging, consent and
cleanup through GameParty remain to be connected; the standalone frontend does
not grant those permissions or write personal original saves.

Windows/ARM64 builds and six focused CTests pass. On **both Flip and Odin**, an
existing local Mario Kart DS cartridge with synthetic SRAM passes native two/four
machine owner-isolation and checkpoint checks, including cancelled and failed
return rejection. Two-seat digest: `b2b366475d54141c` (16 checkpoints). These are
storage/lifecycle checks, not four-player gameplay or real personal-save migration.
The actual Qt player on both handhelds returned only owner seat 1's 262,144-byte
candidate at frame 120 on normal exit (`result=0`); forced termination returned
`result=3` with no candidate/receipt. All staged inputs stayed unchanged. The
fixtures contained generated bytes only; personal saves were not read or written.

Updated isolated ARM64 frontend SHA-256:
`9fd154a656aba66c010708fc46fc7aef4baa8c6ddbe2b0dd8b5f3824a5f0da80`;
owned-state probe:
`1b12552c3b78727c3d5cf5d85156507e87b1d288425fb6319c1ea84eaaf14d43`.
Both devices have those exact files. The ordinary TrainerOS binary/profile is
unchanged. The 23,500-frame wireless replay above remains evidence for the prior
core checkpoint; this addition changes owned startup/return and is covered by
its focused tests, without claiming that the long gameplay run was repeated.

## Remaining work and update procedure

MP-06 and #162–166 remain open. This is an incomplete DS implementation, with a
maintained native core preparation rather than a player-visible network route.
Next: connect prepared owner state and protected-save return to ordinary
GameParty admission/consent and the secure automatic transport. Native synthetic
startup/return proof above does not establish real personal-save migration under #164. The cold-boot verification host does not deliver these gates.
Local compatible BIOS/firmware must stay local. Any unavoidable save-derived
state needs bounded parsing, explicit session/recipient/purpose consent,
authenticated encryption and ephemeral lifetime; raw Android packages are not
an acceptable shortcut. ROM transfers remain separate guest-content policy.

Further gates: broader wireless replay/checkpoint compatibility, deterministic
JIT/render/input behavior across supported workloads, adaptive host-selected
delay, sustained two/four-machine resource measurements, full player-runtime
Linux x86_64/ARM64 builds, protected local-save return, actual wireless
gameplay/recovery and separate Internet connections. The reference mirror
renderer has game-dependent GPU capture fixes; no rendering shortcut is adopted
before those dependencies are checked. Synthetic four-instance packet ordering
is neither four DS emulators nor four-player gameplay evidence.

For a native build, run `packaging/emulators/melonds/build.sh NEW_WORK_DIRECTORY`
with Git, Ninja, CMake >=3.16, Python 3, a C/C++17 toolchain, Qt6 Widgets and
SDL2 development files. It builds isolated checks and the local frontend, not an
admitted Internet runtime bundle. For updates, compare upstream/core reference ancestry and all clock,
WiFi, state/JIT and frontend hooks, rebase or retire each patch hunk, rebuild and
rerun scoped checks before changing the pin. Retain old source/build manifests.
Rollback is selecting the previous isolated build or ordinary emulator; there
is no installed config/save migration to undo in this increment.
