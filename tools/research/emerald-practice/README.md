# Emerald battle-engine feasibility probe

The original development probe is offline. It neither reads
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

## Read-only bridge (next increment delivered)

The original probe above is retained as feasibility evidence. The native service
and the bounded worker now live in `src/integrations/practice`; its reproducible
dependency remains this pinned lock. No server/runtime is auto-installed.

```sh
node tests/EmeraldPracticeEngineTests.cjs
# Or pass the actual package directory (not its parent):
node tests/EmeraldPracticeEngineTests.cjs /path/to/node_modules/pokemon-showdown
```

This exercises correct PP Ups before start, sparse and locked moves, Transform,
exhausted-PP Struggle, friendship, rejected malformed pairs, deterministic finish
and bounded stdio/cancel. `practice_session` adds the native child lifecycle and
source-change checks when Node and this dependency are available. The optional
`TRAINER_BUILD_PRACTICE_PROBE=ON` target verifies actual ROM/save inputs and runs
read-only private Party copies on the handheld. Its detailed output is private.
The native UI now consumes the bounded bridge. Full cartridge rule comparisons
remain separate gates; this is a Gen III practice simulation.

## Explicit offline runtime installation

Use the pinned npm installation above and a Node 24.18.0 binary from the official
archive, verified against its published SHASUMS before installation. Preserve
its LICENSE file. Close practice before replacing a bundle, then run:

```sh
python3 tools/install-practice-runtime.py \
  --node /prepared/node-v24.18.0-linux-arm64/bin/node \
  --node-license /prepared/node-v24.18.0-linux-arm64/LICENSE \
  --dependencies tools/research/emerald-practice/node_modules \
  --prefix ~/.local/share/TrainerOS/TrainerOS/practice/emerald-v1
```

The installer checks versions, copies the worker/reference/locked dependency and
all dependency licenses into a staged bundle, records file hashes and activates
it by rename. A previous bundle is retained; an existing backup requires review
before another install. The hash manifest records installed files, not a claim
of third-party signature verification. No resident downloader, server, global
Node installation or personal game content is required. Host UI/source checking
stays outside the portable game adapter; its actual semantic/process bridge and
dependency lock remain in the permanent configured implementation copy.
