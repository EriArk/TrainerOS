# Local save lineage foundation - 2026-09-27

This is the bounded first #93 delivery, not completed verified sessions (#94),
peer exchange (#45) or portable Trainer export (#73). Ordinary play stays
independent of provenance. No new everyday UI or save-format changes.

## Delivered contract

The existing protected save service records healing, purchases, Party/Box moves,
release, held-item edits and restore after durable save replacement and byte
readback. One lazy-created Ed25519 identity belongs to each proven save owner,
independently of PIN/profile name. A known original Trainer may retain the legacy
save/backup paths: `lineageOwner` is separate from the compatibility `backupOwner`,
and the proof labels that namespace legacy. Routes without a proven active Trainer
remain unassigned. Merely reading a save does not grant it a history.

Before the first managed edit, record an **imported** observation of the source.
Its past is unknown. Successful edits are **managed** descendants of that point;
every record retains `unknownOrigin=true`. There is no `Verified` state here.
An unexpected save/config change adds an **external-observation** with no state
parent. Its audit link preserves earlier records but does not certify continuity.
Further local edits can be managed from this new unknown point. No imported or
externally edited save is labelled cheating or blocked from ordinary play.

Each signed payload binds version/sequence, Trainer, Adventure, ROM hash, opaque
save namespace, runtime adapter, configuration/source/registration revision,
operation/version, protection-copy reference, before/after save hashes, audit
parent, state parent, observation time and software-only assurance. Time is the
host's observation time, not a trusted clock or native capture date. Namespace
hashes hide paths from proof payloads; hashes are still private metadata.

`SaveLineageEdit` is a host-storage component around the existing transaction,
not a new semantic adapter or plugin framework. The pure game-adapter snapshot
and native OT/origin fields remain unchanged. The receiving project's host must
provide its own file protection, provenance and runtime ownership.

## Persistence and interruption

Under the existing private backup root, `lineage/<hashed-owner>/` contains an
owner-only `identity.key` and schema-1 `history.sqlite3`. The main library stays
schema 14. Keys are generated with OpenSSL, atomically written and synchronized;
missing/replaced keys never silently generate a replacement identity. SQLite
uses DELETE journaling and synchronous EXTRA; record plus head advance together.
The implementation only inserts records. Both the existing service lock and an
owner-history lock prevent competing cooperating writers.

Preparation may append a source observation; it cannot append an edit success.
Failed/no-op/read-only/stale edits create no successful successor. A crash between
save replacement and history commit leaves the new file unaccounted for; the next
observation breaks continuity instead of inferring success. A history failure
after replacement reports that the save **was changed**, retains its backup and
does not misreport the original as untouched. Ordinary Center restore appends a
new restore event rather than rewinding history.

Damaged signatures/sequence/head or identity problems stop managed service writes
with an actionable storage error while preserving saves, history and backups.
Local gameplay is unaffected. Records are bounded to 4 KiB each and 16,384 per
stream; capacity is checked before save replacement, never silently pruned.
Future archival/paged proof work must preserve previous heads and signatures.

## Trust and portability limits

The public verifier requires an independently expected public key, owner and
stream; it never needs save bytes or a private key. It checks signatures,
sequence, context and parent links. A valid prefix is not proof of the latest
head: future peers must retain/compare checkpoints to detect replay/forks.
The local head table detects incomplete tail loss, but is not an external anchor.

Flip inspection found an exposed `/dev/tee0` and no exposed TPM; an Ed25519
hardware-keystore path has **not** been established. Current keys are software
keys protected by OS file permissions. A process with account/root control can
replace code, copy keys or roll back the entire directory. Signatures do not
prove fair gameplay, prevent full-host tampering or establish hardware identity.
Odin's facilities still need independent investigation.

Keep keys and history together in private recovery copies. Binary replacement
preserves both. Actual #73 export/import, clone/fork reconciliation and image/OTA
rollback remain separate acceptance gates. Do not include private keys in public
proofs, diagnostics bundles, Git, distributable images or artwork packs. Existing
profile removal leaves these recovery files inert, like prior save backups.

Normal launch/return recording, authenticated sessions, entity transfer lineage,
peer transport/checkpoints, hardware-backed storage and full issue closure remain
planned. Until launch/return tracking arrives, changed saves from normal gameplay
are also unaccounted-for observations; the system does not falsely certify them.

## Sources and verification

- Final native build and full CTest suite: 43/43 passed. The reusable adapter
  snapshot checker passes for 20 source files and three exact profiles.
- [OpenSSL Ed25519](https://docs.openssl.org/3.0/man7/EVP_SIGNATURE-ED25519/):
  one-shot PureEdDSA signing/verification, no external digest parameter.
- [SQLite durability settings](https://www.sqlite.org/pragma.html#pragma_synchronous):
  EXTRA also covers journal unlink directory synchronization in DELETE mode.
- `SaveLineageTests` covers signed edit/restore, independent verification,
  external/config changes, interrupted preparation, no-op/read-only/stale writes,
  key loss/substitution, tail deletion, post-save history failure, namespace/owner
  separation, known legacy-owner compatibility and concurrent writers.
- Flip on ArmadaOS `20260924.1de6489`: production ARM64 build, SDL-controlled
  Emerald held-item change and Center restore on isolated save copies. Three
  records (`imported`, `held-item`, `restore`) independently verified with the
  OpenSSL CLI. Restored bytes match the initial copies exactly; private key and
  directory permissions are 0600/0700. History and identity survive restart.
- Installed binary SHA-256:
  `38504647f5813615785f1aa92d3dfed7931000b944503101b99cdf705210a1bb`.
  Installed/running hashes match; personal save hashes are unchanged. Library
  integrity passes with schema 14, 829 Adventures and three Trainers retained.
  No UI changes in this increment.
