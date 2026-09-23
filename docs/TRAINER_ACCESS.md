# Trainer entry, PIN and family code

P2 extends the existing real Trainer creation/switching flow. The family-code
recovery follows the owner's 2026-09-23 clarification: this is a shared children's
handheld, not an enterprise account system.

## Normal use

- No Trainer: welcome, name/emblem/favorite, optional PIN, review, atomic creation.
- One Trainer without a PIN: open normally.
- Multiple Trainers or a protected single Trainer: choose a Trainer before entry.
- Trainer PIN: 4–6 digits, entered and repeated with the controller keypad.
- Settings → Trainer → Trainer PIN: set, change or remove; existing PIN required
  for a change/removal. Games, game saves and journals are unaffected.
- Settings → Trainer → Family code: a parent chooses six digits and repeats them.
  Changing an existing family code requires that code. There is no built-in code.
- At PIN entry, X → Forgot PIN → enter the family code → explicitly choose
  Remove PIN. The default is Keep PIN. Reset retains the entire Trainer and opens
  their journal; they can choose a new PIN in Settings.
- B cancels input/reset without changing protection. The family code is separate
  from every child's PIN and should be kept somewhere safe by the parent.

A parent sets the family code before any Trainer can enable a PIN. Registration
without a family code creates an unprotected Trainer; optional PIN entry appears
once family recovery is configured. Settings explains this prerequisite.
This prevents a new unprotected profile from assigning its own reset code after
other Trainers have already been protected. With no configured family
code, the unlock screen explains where a parent can set one on an open Trainer.
If all profiles are locked and no usable family code remains, local device-owner
maintenance is required. There is deliberately no universal reset password.

Before entry, Home, shoulders, Start and other feature shortcuts have no primary
page destination. After entry their usual behavior returns. PIN dialogs capture
input; submitting work cannot be cancelled halfway through a database operation.
The shell PIN is local privacy, **not encryption or protection against the device
owner's maintenance/root access**. No emulator gameplay mapping changes.

## Storage and lifecycle

Schema 10 adds `trainer_access`, one mandatory row per Trainer owner, and one
device-wide `family_access` row. Schema 9 records migrate without enabling PINs.
The ownership trigger and update cascade also cover first-profile adoption.
Profile creation and optional verifier insertion share one SQLite transaction.
Failed migrations/writes preserve the previous committed state.

Production opens only roster and access metadata until entry is authorized.
Personal repositories, navigation, history, exit images and the RA provider are
not populated/bound before entry. A successful switch drains the old session,
destroys its controllers/providers and carries a transient in-process grant into
the new composition. A cold restart has no such grant and runs entry again.

PIN work runs on the existing SQLite worker, outside the QML thread. Version 1
uses OpenSSL scrypt (N=32768, r=8, p=1, 32-byte verifier, random 16-byte salt,
64 MiB allocation ceiling). Parameters are fixed by the version rather than read
from untrusted database work factors. Verification uses a constant-time compare.
Input uses bounded, non-copyable, cleansed storage; QML receives only a mask.
Neither plaintext PIN nor input buffers are serialized or logged.

Failed checks persist the counter and next allowed attempt. Delay starts at two
seconds and caps at five minutes; restarting does not clear it. A backward clock
change is capped to the full maximum wait rather than locking a child out forever.
Unknown/malformed verifiers and failed attempt writes fail closed. Family-code
attempts share their own device-wide counter, not a separate allowance per child.

Crypto references: [OpenSSL scrypt](https://docs.openssl.org/3.4/man7/EVP_KDF-SCRYPT/),
[constant-time comparison](https://docs.openssl.org/3.4/man3/CRYPTO_memcmp/).
Builds require OpenSSL 3 Crypto development files; deployments require libcrypto 3.

## Remaining P2 acceptance

[Profile removal and the current consumer audit](TRAINER_REMOVAL.md) are delivered.
Per-Trainer ordinary-save routing is next; independent Multiverse consumers retain
their P3/P4 slots. PINs do not create private emulator save namespaces or
prove per-Trainer RetroAchievements earning. Shared external saves remain shared.

## Verification — 2026-09-23

- Windows native build and complete CTest suite: **39/39 passed**. ARM64 build
  in the existing Flip container and complete suite: **43/43 passed**.
  After the final keypad/Settings layout adjustment, affected interaction,
  Trainer-access and profile tests passed again on both platforms (**3/3 each**).
- Tests cover migration rollback/retry, first-profile entry, protected startup,
  wrong-code retry across reopen, malformed records, failed verifier writes,
  family-code prerequisites, reset cancellation and isolated Trainer switching.
- Migrated a stopped copy of the actual schema-9 device database first. Every
  pre-existing table/column value survived; integrity and foreign-key checks
  passed. Kept paired database/executable backups before installation.
- On the installed Flip build, InputPlumber controller events exercised the
  startup chooser, blocked page shortcuts before entry, family-code creation,
  child-PIN creation, app restart, wrong/correct PIN entry, X recovery,
  default Keep PIN and explicit Remove PIN. Reviewed actual handheld captures
  of the keypad, reset question and Trainer settings. Corrected a Settings
  footer overlap found during this review.
- Removed disposable test codes, reopened the original Trainer and verified
  its profile, account file, 36 history rows, two exit pictures and all 686 shared
  Adventures against the baseline. No games were launched or ordinary saves
  modified. The second Trainer still has no inherited history/account.
- Installed production executable SHA-256:
  `9234e93f93f4a5bdaf95b8fc185e32391f8754ade62448f38f278c1cc6695558`.

After repeated session changes, SDDM refused both TrainerOS and Plasma Mobile
sessions with exit code 5 before the TrainerOS service started. A normal device
reboot restored the default TrainerOS chooser. The captured SDDM journal is kept
privately; the cause and repeated-switch reliability remain platform follow-up,
not a claimed PIN fix. One successful full-device boot does not close that gate.

Private captures, migration comparisons, test logs and paired backups stay outside
Git. These checks use actual handheld rendering/controller events; human button
comfort and power-loss acceptance remain separate checks.
