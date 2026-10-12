# Emulator maintenance records

The [common emulator set](../EMULATOR_STANDARD.md) selects primaries across all
TrainerOS device variants. The [multiplayer matrix](../EMULATOR_MULTIPLAYER_MATRIX.md)
separates supported upstream mechanisms from missing TrainerOS integrations.
Both are target/audit records, not installed package locks.

Owner decision, 2026-10-03: the Armada-based TrainerOS image may ship maintained
emulator configurations and source patches. Use that freedom where it improves
the accepted experience; an upstream limitation is not automatically a permanent
product limitation. Image delivery and update acceptance remain in
[startup/update planning](../STARTUP_EXPERIENCE_AUDIT.md) and the roadmap.

Every emulator changed from now on needs its own record here, updated in the same
commit as its integration/configuration/patch. This is a maintenance index, not
a second execution plan or a claim that all installed runtimes are validated.

| Emulator | Record | Scope |
|---|---|---|
| melonDS | [DS delivery](melonds.md) | Installed two-player Nearby/Online invitations, native wireless race/soak and own-save return on Flip/Odin; four-player, broader games/saves, frontend polish and physical/distinct-network acceptance remain open |
| RetroArch | [Common ARM64 baseline](retroarch.md) | Exact core bundle, two-device alignment, recoverable update/rollback and remaining image/standalone gates |
| LRPS2 | [PS2 candidate](lrps2.md) | Isolated ARM64 build and native state-restore investigation; ordinary ARMSX2 retained, no shared-console profile admitted |
| PCSX ReARMed | [PS1 integration](pcsx-rearmed.md) | Existing RetroArch network route, private BIOS/cards and paired CTR on Flip/Odin; ordered handoff and retained physical/network acceptance remain open |
| RMG / Mupen64Plus | [N64 Nearby delivery](RMG.md) | Installed isolated bundle, paired Mario Kart 64 race and ordinary Nearby/Home/exit on Flip/Odin; operator Online service, wider groups/games and physical/network acceptance remain open |
| gpSP | [GBA link](gpsp.md) | Additional independent-machine networking core; ordinary mGBA preserved |
| DoubleCherryGB | [GB/GBC link](doublecherrygb.md) | Independent SRAM/netpacket; separate upstream battle and RTC limitations |
| SameBoy | [GB/GBC battery pairs](sameboy.md) | Native two-ROM subsystem/SRAM, maintained rollback frontend and automatic own-save preparation |
| mGBA Splitscreen | [GBA linked pairs](mgba-splitscreen.md) | Existing linked-machine engine, per-viewer screen/audio and protected separate batteries |
| Dolphin | [GameCube and Wii delivery](dolphin.md) | Installed separate GC pads, two-person exclusive-P1 Golf Hotseat and separate Wii Remotes through ordinary invitations/Home; paired gameplay and distinct preservation evidence, wider/physical/network acceptance open |
| Cemu | [Wii U online prerequisites](cemu.md) | Odin ARM64 package provenance checked; both devices lack own online console files/certificates. No enabled TrainerOS network profile or paired service evidence |
| Azahar | [3DS native wireless](azahar.md) | Installed pinned ARM64 client, Nearby/public-relay Minicraft3DS and ordinary Online/Home/own save/Exit; wider retail/update identity and external acceptance remain open |
| PPSSPP | [PPSSPP](ppsspp.md) | Isolated networking, preserved identity, bounded LAN/relay gameplay and remaining recovery/network gates |
| Flycast / Dojo | [Dreamcast delivery](flycast.md) | Ordinary 2.7 retained; separate Dojo bundle installed on Flip/Odin, native Nearby/public-relay fights and ordinary Online/Home/peer Exit; fresh temporary VMUs and retained broader/network/physical gates |

Existing work on other emulators remains documented in
[discovery](../EMULATOR_DISCOVERY.md), [platform routes](../ROM_PLATFORMS.md) and
[multiplayer evidence](../EMULATOR_MULTIPLAYER.md). Create its individual record
when next changing that emulator, carrying forward those facts and unresolved
gates. Do not infer versions or verification from another emulator's record.

Each record must contain:

1. Upstream project, version/commit, package source, architecture and device.
   Distinguish observed runtime fingerprints from image build pins.
2. Exact maintained config keys and source/patch/build-recipe paths; explain
   the reason and scope of each difference from upstream.
3. Ownership: image defaults, per-device identity, user/per-game overrides,
   temporary session settings and save locations. Defaults must not clone an
   emulator's unique identity or personal data onto every installation.
4. Checks and results, including actual gameplay separately from process start,
   discovery, handshake or unit tests. Keep unresolved failures visible.
5. Update procedure: compare upstream keys/launch interfaces, rebase or retire
   patches, build, perform relevant checks, then update recorded evidence.
6. Rollback: retain the last compatible package/config migration and preserve
   user saves. Never silently downgrade or overwrite an incompatible save format.

Store maintained patches and reproducible configuration/build steps in Git, not
only in a modified handheld installation. Link them from the record. Keep ROMs,
BIOS files, private saves, credentials and account tokens out of the repository.
The reusable [adapter source copies](../adapters/README.md) remain synchronized;
these maintenance records complement them rather than replacing them.
