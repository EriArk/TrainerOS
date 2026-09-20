# RetroAchievements in Hall of Fame

**Accepted target, not delivered — #48/#9/#20:** RA becomes the L2/R2 Hall companion, following the shared Adventure via verified exact content/set mapping. Settings owns the active Trainer's account; caches remain isolated and independent from ordinary-save/Journey truth. The Hall/RA L2/R2 route and shared Y selector are delivered; exact selected-Adventure projection and per-Trainer account ownership remain pending. Select refreshes the existing collection; X opens Account. Each face keeps its own list/detail/focus. #49 eliminates normal state resume in every achievement mode; old resume restrictions are historical compatibility evidence. [RA projection acceptance](EXPANSION_42_62.md#pokédex-journey-and-achievements).

Hall of Fame keeps account achievements separate from local completion memories and current-save progress. The production provider supports account sign-in, verified content matching, core achievement definitions, Standard/Hardcore unlock flags and an account-scoped offline cache. TrainerOS never awards achievements itself.

## Accepted account and presentation extension — planned

#12 centralizes account management in Settings while Hall remains the achievement consumer. #20 scopes that global service's identity/token/cache and in-flight responses to the **active Trainer**; it does not share one account across people. Migrate the existing valid account into its legacy Trainer without needless sign-in. #24 adds clear locked/Standard/Hardcore/unknown presentation based on complete confirmed/cached account snapshots. #25 presents deduplicated verified new unlocks after return first; initial sign-in/cache rebuild is not a new-unlock event. Live in-game presentation remains conditional, and original jingle playback shares #36's audio priorities. None of these planned extensions establishes current emulator earning; see [roadmap](ROADMAP.md) and [acceptance](EXPANSION_PLAN.md#achievement-state-and-notification).

## Controller flow

Inside Hall of Fame's RetroAchievements section, **X opens Account** and **Select refreshes recent Adventures**. Account name and masked password use the shared controller keyboard, including case, punctuation and a separate numeric block. A activates the focused field/action; B cancels an input or returns to Hall of Fame. Start overlays the form. L1/R1 remain global section navigation and clear unsubmitted account drafts. Busy operations leave a visible Back action. Signing out requires a second A; B cancels that confirmation.

Sign-in exchanges the password for a client access token. Only the canonical username and token are saved, under `integrations/retroachievements-account.json`, with owner-only permissions on Linux and atomic replacement. Password drafts are cleared on submission, Back and global section changes. A failed sign-in keeps an existing account intact. Sign-out clears the Hall of Fame token and its visible records; it does not delete local memories or another account's private cache. Emulator sign-in is a separate integration setting.

## Verified matching boundary

The initial hash implementation covers uncompressed GB, GBC, GBA and Pokémon Mini cartridge files up to 64 MiB, using the whole-file MD5 defined by [rcheevos's cartridge hash implementation](https://github.com/RetroAchievements/rcheevos/blob/master/src/rhash/hash.c). Matching is exact content identity, not a title search. The worker checks file identity/size/time before and after hashing. Adjacent IPS/UPS/BPS patches defer matching because the patched content needs separate verification. Already-patched ROM hacks can match when RetroAchievements recognizes their exact bytes.

Refresh checks at most eight recent/cached linked Adventures per operation, rather than scanning the whole collection. Startup and return from an Adventure request a refresh; Select is also available. DS, 3DS, disc formats, archives and other hashing schemes remain unsupported by this provider. An absent match never creates invented achievements. Previously confirmed account sets are historical records; their cached presence does not prove a replaced file is the same game.

The provider uses the client API documented by the project's own [user request implementation](https://github.com/RetroAchievements/rcheevos/blob/master/src/rapi/rc_api_user.c) and [runtime request implementation](https://github.com/RetroAchievements/rcheevos/blob/master/src/rapi/rc_api_runtime.c): `login2`, `gameid`, `patch` and `unlocks`. Requests use HTTPS POST bodies to the fixed RetroAchievements endpoint, with normal certificate verification, no redirects, a 15-second per-request deadline, response size limits and cancellation on shutdown. Credentials and response bodies are never logged. File/network work runs on a dedicated worker, independent of controller rendering and the local archive.

Only core definitions (`Flags = 3`) are shown. Standard and Hardcore unlock responses must both validate before a new snapshot becomes authoritative; failures preserve the last complete snapshot. These read endpoints do not provide an unlock timestamp, so dates remain unknown. TrainerOS does not open artificial gameplay sessions or call an award endpoint to obtain dates or fabricate unlocks.

## Cache and earning

Private cache files live in `achievements/<sha256(canonical-account)>/<game-id>.json`. Each bounded versioned file contains account/game/Adventure identity, the matched content hash, definitions, unlock flags and fetch time. Loading rejects mismatched account identities, invalid fields and impossible unlock modes. Cache files use atomic replacement and owner-only permissions. Loading/offline/error states retain clearly labelled, previously confirmed records. Network failure never turns unknown data into zero earned.

Actual earning requires a compatible emulator/core signed into the same account and a recognized game. **Legacy behavior pending #49:** exact Continue requires Standard mode and its isolated state preflight disables achievements; Hardcore is incompatible with that old path. Target #49 removes normal state resume in both modes. Enabling and validating emulator earning on the device remains a separate acceptance check from reading Hall of Fame data.

## Acceptance

- Original content-free fixtures verify whole-file matching and refusal of unsupported/soft-patched layouts.
- Request fixtures verify core-set filtering, both unlock modes, unknown dates, malformed/partial responses and exact form encoding of password punctuation.
- Async provider checks verify refresh, offline restart, account isolation, sign-out and absence of plaintext password persistence.
- SDL/QML checks cover Account, masked entry, Start/Back, global page changes, draft clearing and landscape focus/layout.
- The 2026-09-13 source passed the complete Windows suite (28/28) and the Ubuntu 24.04 / Qt 6.4.2 suite (29/29), including the controller-rendered account scenarios. Handheld deployment was deferred while investigating a separate freeze of the previously installed build.
- Device sign-in succeeded for the owner's account on 2026-09-13. Production provider rendering, refresh/cache and emulator-earned unlock validation must be recorded separately after deployment; a successful login alone does not prove them.

## Earned presentation - 2026-09-20

Achievement rows now distinguish earned (gold), locked (muted grey) and unrecorded (lilac) using the existing checked account/set snapshot. Text still states unlock mode or unknown status; detail adds a matching symbol. Offline/error snapshots retain their confirmed cached states. The heading explicitly describes the account collection across matched Adventures: shared Y does not claim to filter it by the current save. No provider, matching, cache, earning or notification protocol changed. [Journey/RA boundary](JOURNEY_UI.md).
