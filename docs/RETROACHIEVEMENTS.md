# RetroAchievements in Hall of Fame

Hall of Fame keeps account achievements separate from local completion memories and current-save progress. The production provider supports account sign-in, verified content matching, core achievement definitions, Standard/Hardcore unlock flags and an account-scoped offline cache. TrainerOS never awards achievements itself.

## Controller flow

Inside Hall of Fame's RetroAchievements section, **X opens Account** and **Y refreshes recent Adventures**. Account name and masked password use the shared controller keyboard, including case, punctuation and a separate numeric block. A activates the focused field/action; B cancels an input or returns to Hall of Fame. Start overlays the form. L1/R1 remain global section navigation and clear unsubmitted account drafts. Busy operations leave a visible Back action. Signing out requires a second A; B cancels that confirmation.

Sign-in exchanges the password for a client access token. Only the canonical username and token are saved, under `integrations/retroachievements-account.json`, with owner-only permissions on Linux and atomic replacement. Password drafts are cleared on submission, Back and global section changes. A failed sign-in keeps an existing account intact. Sign-out clears the Hall of Fame token and its visible records; it does not delete local memories or another account's private cache. Emulator sign-in is a separate integration setting.

## Verified matching boundary

The initial hash implementation covers uncompressed GB, GBC, GBA and Pokémon Mini cartridge files up to 64 MiB, using the whole-file MD5 defined by [rcheevos's cartridge hash implementation](https://github.com/RetroAchievements/rcheevos/blob/master/src/rhash/hash.c). Matching is exact content identity, not a title search. The worker checks file identity/size/time before and after hashing. Adjacent IPS/UPS/BPS patches defer matching because the patched content needs separate verification. Already-patched ROM hacks can match when RetroAchievements recognizes their exact bytes.

Refresh checks at most eight recent/cached linked Adventures per operation, rather than scanning the whole collection. Startup and return from an Adventure request a refresh; Y is also available. DS, 3DS, disc formats, archives and other hashing schemes remain unsupported by this provider. An absent match never creates invented achievements. Previously confirmed account sets are historical records; their cached presence does not prove a replaced file is the same game.

The provider uses the client API documented by the project's own [user request implementation](https://github.com/RetroAchievements/rcheevos/blob/master/src/rapi/rc_api_user.c) and [runtime request implementation](https://github.com/RetroAchievements/rcheevos/blob/master/src/rapi/rc_api_runtime.c): `login2`, `gameid`, `patch` and `unlocks`. Requests use HTTPS POST bodies to the fixed RetroAchievements endpoint, with normal certificate verification, no redirects, a 15-second per-request deadline, response size limits and cancellation on shutdown. Credentials and response bodies are never logged. File/network work runs on a dedicated worker, independent of controller rendering and the local archive.

Only core definitions (`Flags = 3`) are shown. Standard and Hardcore unlock responses must both validate before a new snapshot becomes authoritative; failures preserve the last complete snapshot. These read endpoints do not provide an unlock timestamp, so dates remain unknown. TrainerOS does not open artificial gameplay sessions or call an award endpoint to obtain dates or fabricate unlocks.

## Cache and earning

Private cache files live in `achievements/<sha256(canonical-account)>/<game-id>.json`. Each bounded versioned file contains account/game/Adventure identity, the matched content hash, definitions, unlock flags and fetch time. Loading rejects mismatched account identities, invalid fields and impossible unlock modes. Cache files use atomic replacement and owner-only permissions. Loading/offline/error states retain clearly labelled, previously confirmed records. Network failure never turns unknown data into zero earned.

Actual earning requires a compatible emulator/core signed into the same account and a recognized game. The current exact Continue integration requires **Standard mode**; Hardcore's state restrictions are incompatible with that path. RetroArch's isolated state preflight explicitly disables achievements. Enabling and validating emulator earning on the device is a separate acceptance check from reading Hall of Fame data.

## Acceptance

- Original content-free fixtures verify whole-file matching and refusal of unsupported/soft-patched layouts.
- Request fixtures verify core-set filtering, both unlock modes, unknown dates, malformed/partial responses and exact form encoding of password punctuation.
- Async provider checks verify refresh, offline restart, account isolation, sign-out and absence of plaintext password persistence.
- SDL/QML checks cover Account, masked entry, Start/Back, global page changes, draft clearing and landscape focus/layout.
- The 2026-09-13 source passed the complete Windows suite (28/28) and the Ubuntu 24.04 / Qt 6.4.2 suite (29/29), including the controller-rendered account scenarios. Handheld deployment was deferred while investigating a separate freeze of the previously installed build.
- Device sign-in succeeded for the owner's account on 2026-09-13. Production provider rendering, refresh/cache and emulator-earned unlock validation must be recorded separately after deployment; a successful login alone does not prove them.
