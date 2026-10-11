# Multiplayer preparation while handhelds are offline

Owner scope, 11 October 2026: continue implementation through Wii U, plus Vita;
PS3 and Switch are excluded. Reuse existing emulator networking and the common
TrainerOS invitation/party/launch/exit flow. Keep checks bounded while the devices
are unavailable; installation, physical controls, actual games and distinct
internet connections remain separate acceptance. Do not repeat completed older
checks or count a list of emulator candidates as implemented multiplayer.

## Scope and implementation state

| Family | Existing mechanism / next integration | Current boundary |
| --- | --- | --- |
| Classic RetroArch | Shared-console netplay and existing relay | Integrated; retained acceptance is in RETROARCH_MULTIPLAYER_PROFILES.md |
| GB/GBC/GBA | Existing linked cores / gpSP netpacket | Integrated bounded routes; remaining MP-02 modes and acceptance stay open |
| PS1 | PCSX ReARMed with the existing RetroArch route | Source implementation and local checks: private BIOS/disc identity, two digital controller ports and private cards; no handheld delivery yet |
| N64 | Review maintained native netplay routes | Installed libretro cores advertise serialized states, which alone does not establish deterministic rollback |
| Dreamcast | Flycast / suitable maintained netplay fork | Ordinary runtime installed; automatic internet session integration remains open |
| PS2 | ARMSX2/PCSX2 game networking; shared-console mode is separate | DEV9 networking does not provide remote second-controller gameplay |
| GameCube/Wii | Extend existing Dolphin native netplay bridge | Generic raw GameCube ISO/GCM admission implemented and locally checked. Exact Melee remains. Wii input route and device delivery remain open |
| DS | Existing deterministic multiplayer implementations | Desktop integration remains open; LAN wireless alone is not internet support |
| PSP | Extend existing PPSSPP AdHoc integration | Exact Lumines route exists; wider AdHoc/infrastructure coverage remains open |
| 3DS | Existing Azahar room networking | Integration remains open |
| Wii U | Cemu and supported game-network services | Launcher/platform feasibility, account setup and supported modes remain open |
| Vita | Vita3K supported AdHoc modes | Launcher/platform feasibility and automatic transport remain open; no general PSN claim |

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

Both handhelds were unreachable before this continuation. When available, deliver
the changed client/runtime files and check actual launch, assigned controls,
gameplay, leave/reinvite and ordinary save preservation. Defer long compatibility
matrices, four physical clients and distinct-network/NAT cases to their actual
prerequisites. Source and fixture evidence cannot close these gates.
MP-03 N64 mechanism selection and the remaining accepted family integrations are
the next independent implementation work. PS2, Wii, 3DS, Wii U, Vita and Hotseat
remain open; no missing route is hidden behind the local PS1/GameCube result.
