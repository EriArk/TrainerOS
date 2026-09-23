# Trainer and account screens - 2026-09-20

The original P1 screens now have real P2 registration/selection binding and
[optional PIN/startup/family-code recovery](TRAINER_ACCESS.md). The explicitly
labelled sample mode below remains an isolated rehearsal.

## Normal app

Start -> Settings -> Trainer provides profile editing directly in the right
pane and the shared RetroAchievements account action. Back from account
management restores that row; Save/Cancel in the inline editor restores the
profile row. B then returns to the category rail. The Hall account shortcut
uses the same controller/provider, not a second credential store. Existing
account errors, busy state and explicit sign-out confirmation are preserved.

Trainers opens the real chooser; Power → Switch Player uses the same route.
Add Trainer opens name/emblem/favorite registration and a final review, then
creates and selects a separate personal context. The chooser supports up to eight
profiles with controller scrolling. B cancels; L1/R1 leaves the flow. Submitted
creation/switching is guarded until completion. Existing profiles and external
saves survive the schema-9 migration. The active Trainer owns the account shown
in Settings and Hall. Startup now chooses/unlocks the Trainer as described in
[Trainer access](TRAINER_ACCESS.md). [Ownership and failure/recovery boundary](TRAINER_OWNERSHIP.md).

## Isolated UI rehearsal

Run `traineros --ephemeral`, then Settings → Trainer → Preview
registration & PIN. The preview is available only with the sample library; normal
personal-library mode cannot enter it. Every screen labels itself a development
preview. It has no repository/provider dependency, persistence or authorization
output; finishing cannot change the actual Trainer or launch an Adventure.

The controller flow includes:

- Welcome, controller name keyboard, original emblem and optional sample favorite.
- Name validation, optional 4–6 digit numeric keypad, repeat/mismatch correction,
  skip and review with an edit route.
- Sample chooser: River (no PIN), Sky (sample PIN 1234) and Add Trainer.
- Masked entry, digit limit, erase/clear, incorrect PIN and Back restoration.
- A completion screen that explicitly says no profile was created/switched.

The PIN keypad uses D-pad navigation and A entry; Up from Continue returns to the
numeric block. B clears PIN entry and goes back one step. Start temporarily covers
the draft; L1/R1 or Home cancels the rehearsal and clears its draft. Opening a
different service also clears it. No digits are exposed to QML beyond a bullet
count; no input is logged or serialized. This in-memory rehearsal is not a secure
PIN verifier, throttling mechanism or authenticated recovery implementation.

## Remaining P2 acceptance

PIN verification, startup gating and family recovery are now bound; see
[entry and recovery](TRAINER_ACCESS.md). Expand favorite selection beyond the optional starter choices during
registration (the existing profile editor already has its full picker). Preserve
lossless owner switching and shared-save disclosure. No #19/#20 completion claim
follows from the current profile chooser alone.

## Original P1 verification

Interaction tests cover validation, repeat mismatch, optional PIN, wrong unlock,
masking, cancellation and absence of profile mutation. The SDL diagnostic render
scenario traverses Settings, keyboard, registration, keypad, review, chooser and
unlock, checks focused geometry, Start/Back restoration and global R1 cancellation.
Windows passed all 35 checks; ARM passed 38 checks plus the separate persistence
process check. Affected interaction/SDL checks were repeated after final changes.
The account panel was also checked from both Hall and Settings after correcting
the Settings service's top inset. Native Gamescope SDL rendering passed on Flip
at 1920×1080; keypad, chooser, review and account layouts were visually inspected.
The installed service routes were exercised through InputPlumber controller
events, including cancelling the real profile editor without saving. The profile
row and the count of 686 Adventures matched the pre-install backup; schema stayed 7.
These checks do not certify physical PIN comfort or future multi-owner security.

## Real profile binding — 2026-09-23

The installed non-testing Flip build was exercised through InputPlumber: create
ASH with the controller keyboard and review, open its empty Home/account, switch
back to the original owner, and retain ASH across a separate shell restart. Power
Back restores Switch Player; L1/R1 navigation remains available. Real device
captures were inspected and delivered in chat. Full checks passed on Windows
(38) and ARM64 (42); SDL scenarios include a full eight-profile chooser with
unclipped focused rows. This does not certify PIN security or physical comfort.
[Migration and recovery evidence](TRAINER_OWNERSHIP.md).
