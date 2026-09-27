# Portable Gen III adapter source

This directory is the owner's reusable copy of the **actual TrainerOS adapter**,
including its model headers, factual tables, resource configuration and exact-game
profiles. Copy the whole directory into another project. It builds independently
of the TrainerOS application with C++20, CMake and Qt 6 Core:

```sh
cmake -S . -B build
cmake --build build
./build/inspect_game profiles/emerald-en.json /private/Emerald.gba /private/Emerald.sav
```

There are separate profiles for English Emerald and both allowlisted English
FireRed revisions. Shared source avoids three divergent implementations; profiles
carry the exact full-ROM fingerprint, lengths and **per-capability** evidence.
FireRed does not acquire Emerald shops, movement, release or held-item writes. Original FireRed still lacks
the Rev 1 normal-game healing proof; inspect the evidence before enabling writes.
The profile is a configuration/evidence record, not a trusted runtime permission.
Keep [the knowledge records](../../README.md) alongside it for sources and limits.

`Gen3Progress.h` exposes read, healing, Emerald purchases, movement, release and held-item give/take/swap.
Transformations take immutable bytes and return candidate bytes or a refusal.
`PartyMove` uses zero-based slots, box `-1` for Party, and the SHA-256 of the save
shown to the user. Read the [movement contract](../../../EMERALD_MOVEMENT.md).
`PokemonRelease` similarly binds one source position to the observed save hash;
its [independent release contract](../../../EMERALD_RELEASE.md) requires a deliberate
confirmation and the same host protection pipeline.
`HeldItemChange` binds a Pokemon position and the observed save hash to an item ID;
zero takes the current item, a nonzero ID gives one from the verified Bag.
Replacement returns the old item atomically. `readGen3Progress().party` includes
the read-only Bag choices and independent `canHoldItems` capability. See the
[held-item contract](../../../EMERALD_HELD_ITEMS.md) for exclusions and evidence.
The example verifies the actual ROM against the selected profile and only reads.

**Host responsibility:** this library does not lock emulators, resolve Trainer
ownership, ask for confirmation, create backups or commit save files. The other
project must supply those controls, stale-file checks, a verified backup, atomic
replacement/readback and rollback before enabling writes. TrainerOS supplies them
through `SaveBackupService`/`LocalSaveBackupService`; those platform services are
deliberately not copied into this pure adapter. Do not overwrite a running game's
save merely because a byte transformation succeeded.

From the TrainerOS repository root, refresh with
`python tools/export-game-adapters.py`; validate with the same command plus
`--check`. The tool resolves real source includes, exports tables and exact-game
profiles, and records SHA-256 for every exported input in `manifest.json`.
UTF-8/LF normalization makes drift checks portable across Windows/Linux Git trees.
Edit upstream working sources, then refresh; do not maintain an independent fork
inside this snapshot. The CMake/example/README here are the small reusable wrapper.

2026-09-27 standalone verification: configured and built this directory with
Qt 6 Core/GCC using its own CMake project. The example read the exact private
Emerald ROM/save and reported six Party members. Supplying a FireRed profile
for the same ROM was refused (exit 3); neither input is opened for writing.

No ROMs, BIOSes, private saves, assets, paths or credentials are bundled. Table
source/attribution records travel with the data. This copy does not grant rights
to any third-party game content or relax the recorded validation limits.


TrainerOS now supplies a separate host [signed history foundation](../../../SAVE_LINEAGE.md).
It is deliberately outside this pure transform snapshot, like file locking,
backup/restore and runtime exclusion. Another host must provide those services;
a game parser's successful result is not a signed provenance assertion.
