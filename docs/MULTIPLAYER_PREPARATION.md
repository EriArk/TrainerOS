# Multiplayer preparation and device continuation

**Latest PS2 scope correction, 12 October:** native game online using ordinary
ARMSX2 networking, plus ordinary Hotseat where supported. Stop the LRPS2
shared-console experiment. This supersedes older PS2 both-input-mode and
candidate-promotion requirements below. Other platforms retain their modes.

**12 October installed continuation:** DS now has ordinary two-player Nearby
and Online friend delivery, N64 has ordinary Nearby/private Online delivery, and Dreamcast
has installed native Nearby/public-relay gameplay plus ordinary Online friend
invitation/Home/exit. Evidence and limits are recorded in [melonDS](emulators/melonds.md),
[RMG](emulators/RMG.md) and [Flycast](emulators/flycast.md). N64 private Online
reuses native networking over admitted channels; wider-group, distinct-network
and production service acceptance remain open.
These records supersede older source/build-only statements below. Keep all other
accepted routes, both Hotseat modes, Wii U and Vita in scope; PS3/Switch excluded.

**Devices returned, 11 October:** Armada was updated on both handhelds before
tests. Native DS core/JIT and two/four-machine workers now pass short local
startup replay checks on Flip/Odin; [evidence](emulators/melonds.md) includes
resource measurements and limits. This supersedes the offline-only and ARM64
deferral below. Player frontend, owned-save parity, secure party integration,
actual wireless gameplay and sustained/physical/network acceptance remain open.

Owner scope, 11 October 2026: continue implementation through Wii U, plus Vita;
PS3 and Switch are excluded. Reuse existing emulator networking and the common
TrainerOS invitation/party/launch/exit flow. Keep checks bounded while the devices
are unavailable; installation, physical controls, actual games and distinct
internet connections remain separate acceptance. Do not repeat completed older
checks or count a list of emulator candidates as implemented multiplayer.

## Current scope summary - 12 October owner correction

The table below is the current integration summary; dated sections afterwards
retain their original preparation evidence. Private PSP, the comprehensive
Hotseat catalogue and fine per-title compatibility work are deferred. Existing
ordinary PSP is sufficient. PS2 follows the latest native-online/Hotseat scope above.

## Scope and implementation state

**DS native core preparation, 11 October:** WatermelonDS DetMP is ported onto
pinned desktop melonDS with bounded queues/replies and cancellable waits. The
full Windows static core with JIT builds; synthetic lifecycle checks pass.
The [maintenance record](emulators/melonds.md) distinguishes this from the still
missing mirror frontend, safe state parity, authenticated transport, invitation
integration and Linux/device/gameplay evidence. No DS runtime profile is enabled.

**PS2/Hotseat review and PSP continuation, 11 October:** the bounded
[runtime review](HOTSEAT_RUNTIME_RESEARCH.md) records gaps instead of calling
DEV9 or Golf Mode exclusive shared-pad play. New ARM LRPS2 is a candidate, not
admitted netplay. Independent PSP preparation now extends the existing Ad Hoc
route to raw UMD game ISOs; see [profile and limits](emulators/ppsspp.md).
PS2, Hotseat and the complete catalogue remain unfinished.

**Dreamcast local verification, 11 October:** full Windows client build and
`flycast_netplay`, `game_party`, `runtime_multiplayer` pass (3/3). The patched
native relay client also compiles/runs in a bounded UDP fixture harness; that
does not compile or execute the full emulator. Source exports/registry, recipe
syntax and patch preflight pass. No handheld, ROM or public relay was used.
See [maintenance and remaining delivery](emulators/flycast.md).

| Family | Existing mechanism / next integration | Current boundary |
| --- | --- | --- |
| Classic RetroArch | Shared-console netplay and existing relay | Integrated; retained acceptance is in RETROARCH_MULTIPLAYER_PROFILES.md |
| GB/GBC/GBA | Existing linked cores / gpSP netpacket | Integrated bounded routes; remaining MP-02 modes and acceptance stay open |
| PS1 | PCSX ReARMed / existing RetroArch transport | Installed separate pads and ordinary exclusive P1 handoff, including relay evidence; physical/distinct-network acceptance remains ([record](emulators/pcsx-rearmed.md)) |
| N64 | RMG / Mupen64Plus native server | Installed Nearby/private Online invitations/Home/minimize/peer exit and paired native/forced-relay Mario Kart 64 on Flip/Odin; larger groups, wider games, physical/distinct-network and production service acceptance remain open ([record](emulators/RMG.md)) |
| Dreamcast | Flycast Dojo native relay / GGPO | Installed CHD route; native Soul Calibur Nearby/public-relay fights and ordinary Online invitation/Home/peer exit passed. Fresh temporary VMUs; wider games, distinct networks, physical input/audio and relay release review remain open ([record](emulators/flycast.md)) |
| PS2 | Ordinary ARMSX2 native game networking / DEV9; ordinary Hotseat where supported | Installed network capability and TrainerOS native-online bindings remain unverified. LRPS2 shared-console experiment stopped after scope correction ([record](emulators/lrps2.md)) |
| GameCube/Wii | Dolphin native NetPlay | Installed separate GameCube pads, ordinary GameCube P1 handoff and Wii two-player route; preserve wider-input and physical/distinct-network limits ([record](emulators/dolphin.md)) |
| DS | WatermelonDS DetMP reused on pinned desktop melonDS | Installed two-player Nearby/Online flow, native race/soak and protected own-save return on Flip/Odin; four players, broader compatibility, frontend polish and physical/distinct-network acceptance remain open ([record](emulators/melonds.md)) |
| PSP | PPSSPP native Ad Hoc / aemu relay | Ordinary Nearby/Online integration and Lumines evidence retained; sufficient for the current milestone. Private party admission and fine title work deferred ([record](emulators/ppsspp.md)) |
| 3DS | Azahar native room networking | Installed ordinary Nearby/Online route and Minicraft shared-world evidence; commercial-title effective-update/DLC and physical/distinct-network limits remain ([record](emulators/azahar.md)) |
| Wii U | Cemu native game online services | ARM64 package provenance established; own console/account/certificate data and a supported paired title are missing. No generic Cemu NetPlay claim ([record](emulators/cemu.md)) |
| Vita | Vita3K native game LAN | Installed ordinary Nearby/Online integration with vitaQuake gameplay; commercial AdHoc/PSN is not established ([record](emulators/vita3k.md)) |

The owner's follow-up explicitly requests BOTH simultaneous independent pads and
ordered exclusive control of one live pad on PS1, PS2 and GameCube. Existing #126
owns the latter. Native game networking is another mode again. Keep these
distinctions in capabilities and acceptance. A comprehensive
[game/mode catalogue](HOTSEAT_GAME_CATALOG.md) must identify where handoff is needed
or optional; it is an unfinished catalogue task, not a handful of example games.
Do not silently present PS2 DEV9 as couch multiplayer.
The current [issue #126](https://github.com/EriArk/TrainerOS/issues/126) was reread:
its accepted baseline is live ordered pass-controller, not asynchronous turns.
It remains open. No token/handoff implementation is claimed by the shared-console
PS1/GameCube profiles in this pass.

Wii U and Vita stay explicit future integrations in this same scope. Cemu's
[online setup](https://cemu.cfw.guide/online-play) and
[Pretendo prerequisites](https://pretendo.network/docs/install/cemu) remain
per-player prerequisites, not a generic emulator room. Vita3K's
[current FAQ](https://vita3k.org/faq.html) describes partial AdHoc through overlays;
automatic provisioning is still required for TrainerOS's ordinary invitation UX.
Neither family has a new installed launch/network adapter from this pass.

## Hotseat catalogue evidence — 11 October 2026

The [catalogue](HOTSEAT_GAME_CATALOG.md) now contains 66 mode records for 43
game/platform pairs, backed by 43 publisher manuals or publisher statements.
48 records confirm a shared input device, 15 document another scheme or a solo
restriction, and three remain unresolved. The
[structured data](../data/hotseat-catalog.json) retains source URLs, document
hashes where downloaded, page/section references, player/controller limits,
edition boundaries and special input requirements. Its research inventory has
411 manual/edition records; these are not 411 individually classified games.

This source pass distinguishes optional controller sharing, required handoff,
simultaneous split-controller input, role-specific controller swapping and
private-screen/touch requirements. It does not generalize across ports, sequels
or all modes in a compilation. Unknowns remain unknown. A complete release
baseline and the remaining manual/game checks are still needed; #126 stays open.
The data is not used for runtime admission or title-based matching. Every entry
has `runtime_verified: false`; PS1/GameCube's prepared independent-pad profiles
do not implement handoff, and PS2 shared-console integration remains pending.

Validation for this data-only increment: JSON structure, unique mode IDs, source
references and document hashes, bounded count/status invariants, local links and
Markdown diff checks. No runtime build, handheld installation or gameplay check
was performed. Continue HS-01's common live handoff mechanism and catalogue
coverage within the accepted platform order; device acceptance waits for the
handhelds to become available.

## PS1 runtime decision — 11 October 2026

Researched targets; these source findings do not establish device acceptance:

- Stock [DuckStation](https://github.com/stenzek/duckstation) at
  `cd024df237d46faa4c65574b101261b31267e2fd` has no netplay implementation in its
  current source tree. Its ordinary controller/multitap support is not netplay.
- [HeatXD's netplay fork](https://github.com/HeatXD/duckstation-netplay) at
  `2cce4a404b7a80a9cba56388654ccb93a9ec70af` has GGPO and host-code traversal.
  The repository is archived. Its latest published release, 0.4 (30 July 2023),
  supplies a Windows binary only. ARM availability/maintenance is unverified;
  this is an alternative, not the selected handheld delivery.
- [PCSX ReARMed](https://docs.libretro.com/library/pcsx_rearmed/) documents netplay;
  its [core metadata](https://github.com/libretro/libretro-core-info/blob/master/pcsx_rearmed_libretro.info)
  declares deterministic states. Reuse the existing RetroArch relay, party seats
  and protected session lifecycle. No new emulator networking protocol.

The initial PS1 route requires matching game, core, runtime and owned BIOS,
two digital pads, a single CUE/CHD disc and isolated temporary memory cards.
Multitap, analog-only controls, multidisc changes and PS1 system-link are separate
capabilities. Ordinary emulator selections and personal cards remain untouched.

The inspected PCSX source is
[`c8816799b50388e61cfe237fe2cdbb7d8175f20a`](https://github.com/libretro/pcsx_rearmed/tree/c8816799b50388e61cfe237fe2cdbb7d8175f20a);
core-info is `5a74858ab2f7a50cebb5a6330895bc38899531c0`. The integration does not
patch or replace the installed core: peers must match its actual binary hash.
`frontend/libretro.c` owns BIOS selection; `libpcsxcore/cdriso.c` loads sidecars.
TrainerOS selects one known-name 512 KiB owned BIOS and copies it into the private
session system directory, matching the copied bytes before launch. CUE identities
include every track; SBI/SUB and same-basename BIN/IMG fallback bytes also match.
Competing TOC/CCD/MDS descriptors, a CHD with companion CUE, raw BIN and M3U are
excluded from this initial route. No filenames or catalog titles establish game
compatibility. These are format boundaries, not a per-game whitelist.

The existing `noload-nosave` route and private save directory keep normal memory
cards untouched. Multitap is disabled; party members request their own distinct
port. Missing BIOS or a changed disc/core/runtime/BIOS prevents network launch.

## GameCube extension research — 11 October 2026

The existing Dolphin bridge at upstream `c77bbaa0f372c3f72281602a8b087206706542cb`
constructs a native `UICommon::GameFile`, retains Dolphin game/version checks and
maps the accepted roster to GameCube controller ports. Its implementation is
generic; the prior TrainerOS admission profile singled out Melee. Source admission
now includes fingerprinted raw GameCube ISO/GCM discs, retaining isolated memory
cards, SkipIPL, native NetPlay and the existing two-to-four port mapping. No Wii
controller mapping, compressed-disc parsing, GBA connectivity or shared progression
is implied. This target was registered before extending its source admission.
The old exact Melee profile remains wire-compatible. Other titles use a generic
profile, two ports by default or up to four according to catalogue player counts
bounded by actual controller support. Explicit solo metadata excludes invitations.
Each accepted seat maps to a distinct native pad; this is simultaneous shared-console
play, not #126's exclusive pass-controller token. Source preparation marks its
progress temporary; no memory-card transfer or shared persistent campaign is added.

## Local verification — 11 October 2026

- Windows native build passed for `traineros`, `trainer_retroarch_tests`,
  `trainer_standalone_tests` and `trainer_runtime_multiplayer_tests`.
- `ctest --test-dir build/native --output-on-failure -R '^(retroarch|standalone|runtime_multiplayer)$'`:
  3/3 suites passed, 68.32 seconds. Platform-specific Linux cases are not Windows
  evidence. No emulator/gameplay benchmark or hardware test was added.
- New fixtures cover PS1 BIOS/track/sidecar mismatches, private card/config
  preservation and cleanup, unsupported descriptors, and generic GameCube disc,
  runtime manifest, capacity and local-label matching. Existing invitation,
  lifecycle and controller-port regressions passed with them.
- Portable adapter exports and `tools/check-adapter-knowledge.py` passed;
  `git diff --check` passed. No emulator binary, source patch or device data was
  modified. Windows debug executable SHA-256:
  `c94622418ea3d9ca94eed0944095b9df4bb167164f4adf9d8a57d182de13eeee`.

The first overlapping build saw an old Dolphin object after its function signature
changed. The final build and checks above contain the final sources. Ninja also
recovered a truncated dependency log and rebuilt the client; do not repeat a full
build solely to collect the same evidence again.

## Deferred acceptance

Both handhelds are now available with updated Armada and the current ARM64 client.
The [11 October device recheck](MULTIPLAYER_DEVICE_RECHECK_20261011.md) records
paired PS1 CTR racing, a paired Melee regression and real generic Auto Modellista
admission. Generic GameCube gameplay, wider PSP gameplay, loss/reinvite and
internet acceptance remain distinct gates. Defer long compatibility
matrices, four physical clients and distinct-network/NAT cases to their actual
prerequisites. Source and fixture evidence cannot close these gates.
MP-03 N64 now has the [native integration preparation](emulators/RMG.md). Its
host-compatible dependency bundle/operator service/device gates remain open; Dreamcast is the next
independent family in the retained queue. PS2, Wii, 3DS, Wii U, Vita and Hotseat
remain open; no missing route is hidden behind the local PS1/GameCube result.
