# Shared RetroArch multiplayer profiles

2026-10-04: implemented the owner's platform/core-based expansion. This reuses
the existing invitation, party, relay and lifecycle integration; it is not a new
network adapter per console. Separate-network acceptance is owner-deferred.

## Contract

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

Configured known core BIOS files are fingerprinted into a compact manifest hash.
This is conservative: extra known BIOS files on one device can prevent a match.
It is not an exhaustive arcade dependency resolver: split/clone parents, arbitrary
MAME BIOS sets and companion CHDs still follow the core's own ROM-set validation.
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
