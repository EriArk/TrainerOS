# Trainer and account screens - 2026-09-20

This is the P1 presentation increment for #19/#20, not P2 ownership or security.

## Normal app

Start → Settings → Trainer & accounts provides the existing persistent profile
editor and the existing RetroAchievements account controller. It opens over the
current page; Back from account management returns to this service, and Back from
the service restores the Settings row. The Hall account shortcut remains a route
to the same controller/provider, not a second credential store. Account errors,
busy state and explicit sign-out confirmation retain their existing behavior.

Separate Trainers & PIN remains unavailable with an explanation. The installed
profile, journal, saves and RA identity are neither migrated nor replaced.
Switch Player in Power remains unavailable. No automatic startup gate is added.

## Isolated UI rehearsal

Run `traineros --ephemeral`, then Settings → Trainer & accounts → Preview
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

Bind these screens only after lossless Trainer ownership migration and async
owner isolation. Replace sample cards/favorites with real identities/reference
selection; bind atomic creation, loading/write-failure/retry states, real startup
gating, memory-hard salted PIN verification and bounded retry/recovery. Preserve
the existing one-profile editor until that path is proven. Shared saves do not
become separate playthroughs. No #19/#20 completion claim follows from UI tests.

## Verification

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
