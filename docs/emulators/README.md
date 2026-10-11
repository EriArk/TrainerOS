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
| melonDS | [DS native core preparation](melonds.md) | Pinned desktop DetMP port and bounded lifecycle checks; mirror frontend, safe state, invitations and Linux/device/gameplay delivery remain open |
| RetroArch | [Common ARM64 baseline](retroarch.md) | Exact core bundle, two-device alignment, recoverable update/rollback and remaining image/standalone gates |
| PCSX ReARMed | [PS1 preparation](pcsx-rearmed.md) | Existing RetroArch network route, private BIOS/cards and two pads; device acceptance pending |
| RMG / Mupen64Plus | [N64 preparation](RMG.md) | Existing native server/protocol, automatic private rooms and isolated launch bridge; full runtime build, online service and device/gameplay acceptance pending |
| gpSP | [GBA link](gpsp.md) | Additional independent-machine networking core; ordinary mGBA preserved |
| DoubleCherryGB | [GB/GBC link](doublecherrygb.md) | Independent SRAM/netpacket; separate upstream battle and RTC limitations |
| SameBoy | [GB/GBC battery pairs](sameboy.md) | Native two-ROM subsystem/SRAM, maintained rollback frontend and automatic own-save preparation |
| mGBA Splitscreen | [GBA linked pairs](mgba-splitscreen.md) | Existing linked-machine engine, per-viewer screen/audio and protected separate batteries |
| Dolphin | [Bridge checkpoint](dolphin.md) | Preserved ordinary Flatpak; separate pinned native bridge with paired Melee LAN gameplay/Home return; internet and four-client gates open |
| PPSSPP | [PPSSPP](ppsspp.md) | Isolated networking, preserved identity, bounded LAN/relay gameplay and remaining recovery/network gates |
| Flycast / Dojo | [Dreamcast preparation](flycast.md) | Ordinary 2.7 retained; separate Dojo native relay/GGPO launch integration in source; full runtime build, operator service and device/gameplay checks pending |

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
