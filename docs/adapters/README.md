# Game-adapter knowledge base

Start here before researching a game or changing its save adapter (#91).
This is an engineering evidence index, not a runtime plugin ABI or a promise
that another edition of the same game has the same format.

| Record | Exact targets | Current boundary |
|---|---|---|
| [Emerald English](emerald-en.md) | One SHA-256 allowlisted GBA build | Integrated reads/Journey, healing, shops and protected Party/Box moves/occupied swaps, release and held items |
| [FireRed English](firered-en.md) | Original and Rev 1, separately fingerprinted | Integrated reads/healing; normal-game healing proof on Rev 1 only |
| Diamond / Colosseum | No verified fingerprints recorded here | Planned; no write capability inferred |

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
