# Adapter boundary audit (#89)

2026-09-27; code-backed review of the delivered Emerald chain before additional
title support. This is a composition decision, not a new plugin implementation.

| Responsibility | Existing boundary | Decision |
| --- | --- | --- |
| Launch and lifecycle | `AdventureAdapter`, RetroArch/standalone adapters | Keep runtime capability separate from semantic save support. An emulator launch does not certify a save writer. |
| Save location and ownership | RetroArch/melonDS save resolvers, `SaveTarget`, per-Trainer runtime namespace | Host owns paths, active-owner binding, source revision and running-game exclusion. A content module receives bytes, not authority over files. |
| Read progress | `GameProgressProvider`, `GameProgressService`, Gen3 reader | Preserve asynchronous context/revision invalidation. Return optional facts and explicit availability; no invented values. |
| Semantic changes | `healEmeraldParty`, `buyEmeraldItems`, `EmeraldShops` | Pure candidate-byte transforms with exact content identification, checksums and allowed field changes. Keep game offsets and rules here. |
| Protected commit | `SaveBackupService`, `LocalSaveBackupService`, `applySaveEdit` | Single host transaction for re-resolution, token verification, backup, atomic write and readback. Basket composes candidate transforms before this one commit. |
| UI | Shell, Party/Dex/Center presentations, QML | Consume small projections/capabilities. Never infer editable support from readable Party/money or from a console name. |
| Art and franchise presentation | ClassicArt/SpriteArt, library domain and media projections | #90 franchise experience is distinct from #57 artwork packs. Presentation does not own saves, launch commands or host services. |

## Actual coupling and bounded follow-ups

`Main.cpp` currently composes save resolvers and wires Emerald healing/shop
functions. This is a useful composition root, but each additional verified
writer would add a title-specific branch. Keep the same service and introduce
a small exact-build selector at this point when a second writer is proven.
Do not create a second backup/transaction service.

`Gen3Progress.cpp` shares container/checksum decoding while Emerald shop rules
are isolated in `EmeraldShops`. Share verified format mechanisms, not presumed
game layouts. English FireRed requires its own field map and regression proof.

Shell currently combines page/face routing with controller priority. Replace
the old pairs with explicit stable face identities; retain the existing feature
controllers and modal guards. This is navigation composition, not a feature
rewrite. Properties should distinguish runtime launch, save observation and
verified write support rather than compress them into one “supported” badge.

## Portability and versioning decision

* **Now:** compiled C++ modules with typed contracts and tests. Lowest operational
  cost, reproducible builds, and no untrusted in-process extensions.
* **Declarative data:** suitable for identifiers, exact-build allowlists,
  presentation metadata and bounded rule tables. Version and validate each
  format; data cannot grant host powers or widen a write capability.
* **Native dynamic plugins:** deferred. A stable ABI, crash isolation and update
  compatibility are not established; loading a library is not sandboxing.
* **Process helpers:** appropriate only for a concrete external engine/provider.
  Require bounded IPC, size/time limits, cancellation and context revision. The
  host still verifies candidates and owns commit. No helper receives blanket
  filesystem or network authority merely by declaring support.
* **Sandboxed portable formats (for example WASM):** possible later, not selected
  without a working use case and measurements on Armada. They do not replace
  exact-build safety or the host transaction.

A future descriptor needs its own contract version, stable module ID,
exact-build matches and separately declared reads/transforms. Save schema,
franchise presentation schema and asset-pack schema must not share one version
number. Unknown versions fail closed for writes. No universal plugin loader,
new dependency or speculative execution framework is required by this audit.

## Acceptance retained

#42/#50 source proof, #75 device-wide read-only enforcement, #76 runtime owner
isolation, #82 second exact vertical and #45 two-device durable exchange remain
independent gates. This document does not claim any of those future gates pass.
Distribution/update compatibility belongs to #70/#71, not to an adapter flag.
