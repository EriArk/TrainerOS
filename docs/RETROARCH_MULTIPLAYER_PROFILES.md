# Shared RetroArch multiplayer profiles

2026-10-04: implemented the owner's platform/core-based expansion. This reuses
the existing invitation, party, relay and lifecycle integration; it is not a new
network adapter per console. Separate-network acceptance is owner-deferred.

## Contract

**Latest delivery, 2026-10-04:** FBNeo compatibility now uses title dependencies
from the pinned public DAT instead of the whole known BIOS collection. European
Battle Circuit prepares a private four-player cabinet when metadata requests
three/four seats. Four clients reached the same level with four active players.
See the final checkpoint below; earlier two-seat findings are historical.

Compatible classic shared-console cores use a versioned local profile, matching
content/runtime/core/firmware identities and private session settings/saves.
The initial generic control profile has two independent pads; the established
exact NES Four Score profile keeps four. More ports require their actual device
configuration, not an inferred capacity from scraped text. Explicit single-player
metadata still excludes invitations. Handheld cable/wireless mechanisms remain
separate. No claim that every game in a supported core works is made.

Cartridge/archive matching requires identical bytes; separately packaged copies
are not silently called identical. Disc track lists must include track contents
in the identity. Personal naming is presentation, not compatibility. Ordinary
save files are never loaded or written by this temporary-progress route.

## Sources

- [RetroArch protocol](https://docs.libretro.com/development/retroarch/netplay/):
  common synchronization and input transport; deterministic cores and matching
  content are required. State serialization affects rollback behavior.
- [Core metadata](https://github.com/libretro/libretro-core-info): review
  deterministic-state declarations per selected core; installation alone does
  not establish compatibility.
- [FBNeo](https://docs.libretro.com/library/fbneo/): arcade ROM-set and BIOS
  requirements remain those of the selected core.
- [Controller slots](https://docs.libretro.com/guides/netplay-multiple-controllers/):
  request the party-assigned pad instead of depending on connection order.
- [MAME core metadata](https://github.com/libretro/libretro-core-info/blob/master/mame_libretro.info):
  generated CFG/NVRAM/memory-card files belong under the frontend save directory;
  this route supplies a private directory. This is a source contract, not a
  paired MAME game test.

## Reviewed profiles

| Core | Platform folders | Generic controls |
| --- | --- | --- |
| FCEUmm / Nestopia | nes, fds | Two pads; established exact FCEUmm Four Score profile remains four |
| Snes9x | snes | Two pads |
| Genesis Plus GX | sg1000, mastersystem, megadrive, segacd | Two pads |
| PicoDrive | mastersystem, megadrive, segacd, sega32x | Two pads |
| Mednafen PCE Fast | pcengine, pcenginecd | Two pads |
| Mednafen SuperGrafx | supergrafx, pcengine, pcenginecd | Two pads |
| Stella | atari2600 | Two pads |
| FBNeo | fbneo, neogeo | Two pads |
| MAME | mame | Two pads |

These are ten core names and fifteen platform IDs including FDS. ProSystem was
excluded: its inspected core-info advertises serialized, not deterministic,
states. Other cores remain unsupported by this table until their mechanism is
established. It does not enable arbitrary multiplayer on single-player games.

ROM SHA-256 replaces a per-title allowlist for these profiles. Established exact
identities/ZIP aliases remain for compatibility. Runtime/core binaries must
match. CUE identity includes every referenced track, with the existing safe disc
validator; M3U/CCD/TOC/CMD descriptors are excluded rather than hashing just the
descriptor. Raw files/archives/CHD have a 2 GiB content bound. This does not turn
two differently packaged archives into the same identity.

Configured firmware is fingerprinted into a compact manifest hash. FBNeo uses
its reviewed own/parent/BIOS archive dependencies; NES and PCE cartridges and
non-CD PicoDrive modes skip unused disc firmware. Other core profiles retain
conservative known-file matching, which can still reject extra optional firmware.
This is not exhaustive ROM-member validation: arbitrary MAME BIOS sets and
companion CHDs still follow the core's own ROM-set validation.
The generic route does not promise every arcade set, exotic peripheral, or saved
co-op campaign. Normal save files stay separate; a multiplayer session starts
without ordinary SRAM and discards its temporary progress. Restarting an ordinary
generic game retains the normal save/exit question.

Discovery uses one worker-local digest cache, so runtime/core/BIOS files are not
rehashed for each title in the same scan. Launch revalidates without that cache.
Ordinary experimental-off sessions skip multiplayer hashing altogether.
Renaming a local title does not break compatibility; content/settings identities
still must match. No new user-facing binding, setup or lobby page was added.

## Evidence and remaining acceptance

- Windows and ARM builds passed; `retroarch`, `game_party`, `netplay_client`
  passed on both. Checks cover arbitrary content profiles, invalid combinations,
  two-pad assignment, exact Four Score preservation, changed BIOS/track bytes,
  renamed-title invitation/accept/start, and private configuration cleanup.
  The exported runtime's standalone CMake library and example also build.
- A bounded new-core probe used production preparation on Flip/Odin with their
  installed FBNeo and Bubble Bobble through the public Madrid relay. Both joined
  assigned P1/P2, rendered through Vulkan, and accepted fixture coin/start input.
  Flip captured the introduction; Odin captured the first level. Independent
  simultaneous gameplay was **not established** by these non-synchronous captures.
  This was a fixture launch, not fresh end-to-end Social invitation acceptance.
- Both were on the same home network. Do not call this distinct-network proof.
  Previous NES/SNES/Mega Drive gameplay evidence remains in
  [runtime multiplayer](EMULATOR_MULTIPLAYER.md); unchanged scenarios were not
  repeated as new work.
- Both probe processes and private configuration directories were removed.
  All 25 Flip and 1066 Odin ordinary config/option/remap files remained byte-identical.
  Temporary credentials were removed. No ordinary saves or library files were moved.
- Both handhelds received ARM binary SHA-256
  `aaad7af15b93b1a906894f3defc97a9df90aa1c869939c57a3e39829a0b229f5`.
  The new running executable was verified on both after a shell-only restart;
  database quick-check passed. Odin volume remains zero; Flip remains muted.
  Ordinary builds retain the existing experimental-off gate. No device reboot.

Still open: full shared-party acceptance/release gate, owner's deferred different
networks, wider peripheral capacities, arcade dependency completeness and any
concrete core/game failures. A reviewed profile is not a claim that every title
has been physically played. Portable source and its standalone CMake target are
kept under `docs/adapters/implementations/runtime`.

## Three/four-player profiles — 2026-10-04

The shared invitation path now selects three/four-seat profiles from game player
metadata for the following reviewed controller layouts. No extra lobby, binding
or controller setup page was added. The ordinary experimental gate stays off.

| Core / platform | Session preparation | Automatic capacity |
| --- | --- | --- |
| Snes9x / SNES | Normal P1; Multitap device 257 on port 2, with five logical pads | Metadata maximum, capped at four |
| PCE Fast / PC Engine and CD | Five core pad ports | Metadata maximum, capped at four |
| SuperGrafx / its supported PCE-family platforms | Five ports, private `sgx_multitap = enabled` | Metadata maximum, capped at four |
| FBNeo / arcade | Four independent driver ports; private European Battle Circuit cabinet | Metadata maximum capped at four for `batcir`; other sets remain two |

Missing counts and alternating-player metadata retain two-seat preparation;
known single-player metadata still suppresses invitations. Exact reviewed profiles
take priority over scraped counts. Genesis Team Player versus EA 4-Way Play and
NES Four Score versus Famicom expansion cannot be inferred from count alone:
their generic routes stay two; the existing exact NES Four Score remains four.
Neo Geo, MAME and the other baseline cores are not newly promoted to four seats.

Capacity and controller layout are included in the peer identity. Each participant
requests only its assigned port; the host owns port 1, and the connection limit
is capacity minus one. Metadata capacity changes invalidate discovery. Launch
revalidates the accepted layout/content/runtime rather than silently reverting
to two pads. Core options remain private and are removed on exit; ordinary
saves/settings are unchanged. Different metadata capacities currently require
matching local profiles; there is no silent capacity negotiation.

### Bounded evidence and unresolved differences

The following is the earlier capacity checkpoint. Its whole-BIOS-set mismatch
and European Battle Circuit cabinet limitation are resolved by the later
checkpoint below; its real-user and distinct-network limits remain open.

- Windows and ARM builds and the `retroarch`, `game_party`, `netplay_client`
  suites pass. Added coverage exercises three/four-seat mappings, invalid ports,
  unsupported automatic layouts, private SuperGrafx options, forged layout
  rejection, and a three-seat party that rejects a fourth member and reuses a
  vacated seat. This is configuration/coordinator evidence, not new physical
  SNES/PCE four-player gameplay evidence.
- A production-preparation fixture launched four FBNeo Battle Circuit clients
  across Flip/Odin through the public Madrid relay, with guests joining in order
  P2, P4, P3. Logs confirm each assigned slot. Four emulator processes are not
  four actual TrainerOS users, and no new Social UI acceptance is claimed.
- The devices' ordinary known BIOS manifests differ. The normal compatibility
  check rejects that mismatch; the fixture used identical BIOS only in isolated
  folders. Installed BIOS were not replaced. Resolving unnecessary whole-set
  mismatches remains a concrete classic-family gap.
- Battle Circuit's default cabinet showed only P1/P2 in its in-game input test.
  Consequently this probe does **not** prove four-player gameplay. Automatic
  FBNeo capacity remains two until a game-specific cabinet mode can be prepared
  and checked. Merely assigning four RetroArch ports would overstate support.
- Both fixture attempts were stopped and private session options/credentials
  removed. Ordinary config/option/remap hashes stayed unchanged: 25 files on
  Flip and 1066 on Odin. Vulkan was used; audio was muted. No save/library
  migration or device reboot. Distinct networks remain owner-deferred.

MP-01 remains open: real-user/company and distinct-network acceptance, effective
arcade cabinet/BIOS compatibility, and the shared release decision are not closed
by this delivery. Reuse the established seat/relay evidence instead of repeating
the same four-process experiment as another stage.

Both handhelds now run ARM executable SHA-256
`e78249f7cef44881802fa194b825cd35fef06bc7ca023c07090c4b6e22418b7d`,
verified from each live process after shell-only restart. Both databases passed
read-only quick-check; no probe games/private session directories remain.
Odin volume is zero; Flip is muted. The portable runtime snapshot was refreshed
and its standalone library/example build passed. Ordinary experimental-off
launchers were preserved; this installation does not enable the unfinished lane.

Sources inspected:
- https://github.com/libretro/snes9x/blob/master/libretro/libretro.cpp
- https://github.com/libretro/beetle-pce-fast-libretro/blob/master/libretro.c
- https://github.com/libretro/beetle-supergrafx-libretro/blob/master/libretro.cpp
- https://github.com/libretro/Genesis-Plus-GX/blob/master/libretro/libretro.c
- https://docs.libretro.com/library/fbneo/

## Title dependencies and arcade cabinet checkpoint — 2026-10-04

The FBNeo identity includes its own archive and transitive parent/BIOS archives,
searched beside the content, under `system/fbneo`, then under `system`. Both ZIP
and 7z candidates are included because the core can load members from multiple
archives. Identical duplicates and moving an identical dependency between these
locations do not change the identity; differing alternate bytes do. Unrelated
BIOS no longer prevent a match. This does not equate differently packed archives
or prove every required member is present; merged sets may already contain BIOS.
An unknown driver name is unavailable for this multiplayer profile, without
changing ordinary launch. Display-title renaming remains supported.

`FBNeoRomSets.h` contains 8,384 public set/dependency records generated by
`tools/import-fbneo-dependencies.py`, not ROM/BIOS content. Source:
[FBNeo arcade DAT at 63c4190](https://github.com/libretro/FBNeo/tree/63c4190785cadd5ff84483399375871ed6e98754/dats),
`FinalBurn Neo (ClrMame Pro XML, Arcade only).dat`, SHA-256
`e3221ec11959d8ddd352de6dd1c9ac93ee641fef2be233c68a7045752300889e`.
The [upstream search contract](https://docs.libretro.com/library/fbneo/) and
[driver](https://github.com/libretro/FBNeo/blob/63c4190785cadd5ff84483399375871ed6e98754/src/burn/drv/capcom/d_cps2.cpp)
were inspected. Review this table alongside core updates; a newer core is not
automatically proven against the pinned dependency map.

Only European `batcir` receives automatic three/four-seat preparation. Its private
128-byte EEPROM is generated from configuration fields, with mirrored chute mode
6 at offsets `0x26`/`0x56`: four players, four independent coin inputs, SINGLE.
No ordinary NVRAM or progress is imported. Both `.fs` and `.nv` names cover the
observed core persistence paths. Private save sorting is disabled so the core
finds these files; private diagnostic input is disabled so holding Start does not
open service mode. Cleanup removes these files with the session. Japanese/Asian
clones and other cabinets are not inferred from the European configuration.

Evidence:

- Windows and ARM application builds and `retroarch`, `netplay_client`,
  `game_party` suites pass. Added cases cover unrelated/required/alternate
  archives, clone parents, dependency relocation, unknown drivers, exact cabinet
  identity, private EEPROM/options and cleanup. Portable adapter export/check and
  its standalone library/example build pass.
- Four production-adapter-prepared FBNeo clients on Flip/Odin joined the public
  Madrid relay in P2/P4/P3 guest order. All four independently supplied coin/start
  input and reached the same stage with four characters and four health bars.
  The host log confirms loading the generated `.fs` EEPROM. Both devices used
  their ordinary BIOS paths: no identical substitute BIOS folder was required.
  This is emulator preparation/seat/gameplay evidence, not four actual TrainerOS
  users, new Social invitation acceptance or a dedicated directional-input test.
- The owner's server disk was inspected read-only. No BIOS needed copying for
  this probe: Puzz Loop's installed merged archive already contains SKNS BIOS.
  No firmware or source originals were overwritten; no private dumps enter Git.
- Vulkan was used and audio muted. Probe processes and temporary session files
  were removed; all 25 Flip and 1,066 Odin ordinary settings/option/remap files
  stayed byte-identical. Both devices remain on the same home network.
- Both live TrainerOS processes now match ARM executable SHA-256
  `75d8af8d61ea961e1e62b91a68f2de82bfebee757ab847d3ad33d66bfa4ffa23`.
  Shell-only restart, read-only database quick-check and process cleanup passed.
  Odin volume remains zero and Flip muted; ordinary experimental-off launchers
  remain unchanged. No device reboot.

This closes the observed unused-FBNeo-BIOS mismatch and exact European cabinet
defect. It does not close MP-01's real-user/company, deferred distinct-network
or release-enablement gates. Do not repeat this four-process probe as another
stage or expand into tuning every arcade ROM.
