# Game-adapter knowledge base

Start here before researching a game or changing its save adapter (#91).
This is an engineering evidence index, not a runtime plugin ABI or a promise
that another edition of the same game has the same format.

| Record | Exact targets | Current boundary |
|---|---|---|
| [Handheld network link](../HANDHELD_MULTIPLAYER.md) | Pinned gpSP / DoubleCherryGB; exact game eligibility under research | Partial: Emerald trade/invitation/return; Red invitation, actual trade, independent save return and Gambatte Party readback; Gen II clock bridge and Gold return/readback on both. Gen II/TCG transactions, GBA modes and external acceptance stay open |
| [Emerald English](emerald-en.md) | One SHA-256 allowlisted GBA build | Integrated reads/Journey, healing, shops and protected Party/Box moves/occupied swaps, release and held items |
| [FireRed English](firered-en.md) | Original and Rev 1, separately fingerprinted | Integrated reads/healing; normal-game healing proof on Rev 1 only |
| Diamond / Colosseum | No verified fingerprints recorded here | Planned; no write capability inferred |
| [NES Pong runtime](../NES_MULTIPLAYER.md) | Exact author-provided homebrew; matching FCEUmm/RetroArch | Public-relay independent gameplay, invitations, loss/exit/rejoin, reverse join and permitted-group gameplay verified; distinct networks and wider compatibility remain open |
| [Contra III runtime](../EMULATOR_MULTIPLAYER.md) | Exact US SNES content, matching Snes9x/RetroArch binaries | Researching netplay: LAN controls and public-relay password handshake verified separately; online gameplay/recovery open; no semantic save adapter or persistent writes |
| [Mega Drive runtime profiles](../EMULATOR_MULTIPLAYER.md#multiple-games-and-a-second-platform---2026-10-03) | Exact US Streets of Rage 2 and Gunstar Heroes; matching Genesis Plus GX/RetroArch | Streets named online consent and P2 character/movement verified; Gunstar prepared only. Full gameplay/recovery gates open; no persistent save writes |
| [Standalone PSP](../PSP_MULTIPLAYER.md) | Exact US Lumines; PPSSPP ARM64 1.20.4 fingerprint | LAN and external-relay Lumines gameplay verified with isolated settings; bounded relay interruption/rechallenge passed. Distinct networks and mid-round recovery remain open. Ordinary SAVEDATA retained. |

[Shared RetroArch runtime profiles](../RETROARCH_MULTIPLAYER_PROFILES.md) now
complement these exact records. Generic core/platform compatibility does not
create a semantic save adapter or prove every title. The portable runtime copy
includes the profile implementation and its disc-validation dependency.

The [10 October linked-pair route](../HANDHELD_MULTIPLAYER.md) also reuses existing
DoubleCherryGB/RetroArch rollback for GB/GBC cartridges without battery/RTC
storage. Into the Blue reached real paired gameplay and normal return on both
handhelds. This hardware-based route adds no semantic save capability and does
not replace the remaining general independent-save link work.

[registry.json](registry.json) indexes exact identities, source records and
per-capability evidence. It deliberately does not duplicate the factual tables,
runtime allowlist or service contracts. Human records explain unknowns and failed
assumptions. `tools/check-adapter-knowledge.py` checks identities/links against the
compiled allowlist; it does not confer write permission.

States: **planned** (no research), **researching** (active unproven work),
**partial** (bounded evidence with explicit gaps), **read-verified**,
**write-verified** (protected transformation and stated validation),
**integrated** (consumer delivered), **blocked** (named external gate).
Unknown differs from unsupported; validation levels remain separate from state.
An integrated capability may still lack independent physical evidence for one
revision. Never silently promote that revision.

## Required research loop

1. Consult this index, the exact-build record and its dated findings first.
2. Register the exact target and mark new capabilities researching **before**
   implementation. Record missing identity instead of guessing a hash.
3. Append meaningful findings during work: date, source/revision, external claim,
   hypothesis, local reproduction, failed assumption and remaining uncertainty.
4. Validate separately: synthetic; private read; copied before/after;
   allowed-delta/checksums; actual-device interaction; normal-game readback;
   protection/rollback. A screenshot alone is not byte-level validation.
5. Update record, registry, tests and consumer scope in the same code commit.
   A stale knowledge record means the adapter increment is not done.

Keep ROMs, saves, personal paths, credentials and private manifests outside Git.
Link dated historical delivery documents rather than copying their narratives.
The [roadmap](../ROADMAP.md) remains the execution queue and the
[adapter audit](../ADAPTER_AUDIT.md) remains the interface map.

## Reusable implementation copies — owner clarification, 2026-09-27

Keep a [portable source copy](implementations/gen3/README.md) beside the evidence,
configured with a separate profile for each exact game/revision. It includes the
actual adapter code, dependencies, factual tables and a standalone CMake example.
The current Gen III copy builds using Qt Core without the TrainerOS application.
Refresh it with `python tools/export-game-adapters.py` after code/data/registry
changes. `python tools/check-adapter-knowledge.py` also checks exported source
and profile drift. This is a reusable source snapshot, not a new plugin ABI.
The receiving project must implement its own protected file transaction and
runtime ownership; pure byte transforms alone do not make file writes safe.

[Runtime configuration modules](implementations/runtime/README.md) have their own
source/dependency snapshot, exact profiles and a read-only fingerprint example.
The same export/check commands refresh both copies. This does not turn networking
configuration into semantic save support or expand the proven-game allowlist.
