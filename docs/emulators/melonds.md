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
  selection is unchanged; no UI or command line selects the new interface yet.
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
The future coordinator must call `Abort`, then stop/join all workers before
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

No Linux/ARM64, desktop frontend, actual DS game or handheld acceptance is inferred
from a Windows static-core build or synthetic packet exchange.

## Remaining work and update procedure

MP-06 and #162–166 remain open. This is an incomplete DS implementation, with a
maintained native core preparation rather than a player-visible network route.
Next: implement/join mirror workers, establish deterministic startup/state parity
under #164, then reuse GameParty admission and the secure automatic transport.
Local compatible BIOS/firmware must stay local. Any unavoidable save-derived
state needs bounded parsing, explicit session/recipient/purpose consent,
authenticated encryption and ephemeral lifetime; raw Android packages are not
an acceptable shortcut. ROM transfers remain separate guest-content policy.

Further gates: per-frame replay/checkpoint hashes, deterministic JIT/render/input
behavior, adaptive host-selected delay, two/four-machine resource measurements,
full Linux x86_64/ARM64 builds, protected local-save return, actual wireless
gameplay/recovery and separate Internet connections. The reference mirror
renderer has game-dependent GPU capture fixes; no rendering shortcut is adopted
before those dependencies are checked. Synthetic four-instance packet ordering
is neither four DS emulators nor four-player gameplay evidence.

For a native build, run `packaging/emulators/melonds/build.sh NEW_WORK_DIRECTORY`
with Git, Ninja, CMake >=3.16, Python 3 and a C/C++17 toolchain. No runtime bundle
is produced. For updates, compare upstream/core reference ancestry and all clock,
WiFi, state/JIT and frontend hooks, rebase or retire each patch hunk, rebuild and
rerun scoped checks before changing the pin. Retain old source/build manifests.
Rollback is selecting the previous isolated build or ordinary emulator; there
is no installed config/save migration to undo in this increment.
