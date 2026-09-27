# Emerald battle-engine feasibility probe

Development-only, offline, no TrainerOS runtime dependency. It neither reads
ROM/save files nor writes battle results. It does not start the Showdown server
or a browser. Original synthetic teams exercise the pinned simulator API.

```sh
npm ci --prefix tools/research/emerald-practice --ignore-scripts --omit=optional --no-audit --no-fund
node tools/research/emerald-practice/probe.cjs
```

Alternatively install the same package/lock files in an ignored work directory:

```sh
node tools/research/emerald-practice/probe.cjs work/research/practice-engine
```

Tested on Node 24.18.0, Windows x64 and Armada Linux ARM64. The npm dependency
is locked to `pokemon-showdown@0.11.11` and transitive integrity hashes. Optional
dependencies are represented in npm's lock but omitted from installation; package
lifecycle scripts are disabled. Precompiled JS is sufficient for this probe.
Keep `node_modules`, private semantic data and result files outside Git. The
dependency retains its upstream MIT LICENSE; no dependency source is vendored.

The probe checks Gen III move categories/types, numeric stats/nature rounding,
Hidden Power, friendship-dependent Return, max-PP defaults, invalid choices,
deterministic complete battles and cancellation of a separate process waiting
for input over stdio. Protocol timestamps are excluded from replay hashes.
It measures module load, first battle, 100 short battles, memory high-water mark
and child readiness. These are a feasibility sample, not sustained UI frame or
cartridge-equivalence proof.

An optional third argument is a **private semantic JSON**, never a raw save:
`{ "mode": "fresh-copies", "members": [...] }`. Each member has a Showdown
`set` with explicit species, level, ability, nature, gender, friendship
(`happiness`), item, six IVs, six EVs and 1-4 moves, plus `stats` ordered
HP/Attack/Defense/Sp. Attack/Sp. Defense/Speed and `maxPp` in move order.
This input belongs outside Git. The probe compares all members' six stats,
counts PP mismatches, runs the first pair twice and verifies the input was not
mutated. It starts fresh simulated copies; existing HP/status/current PP are
not restored by this probe. A full production input gate is still required.

The direct `Battle` API is intentionally pinned research, not a stable host
contract. Production must use a bounded semantic bridge, input validation,
timeout/process cleanup and the exact-game gates described in
[the findings](../../../docs/EMERALD_PRACTICE.md).
