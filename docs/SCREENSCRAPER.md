# Native ScreenScraper — accepted scope and delivery order

Accepted [issue #65](https://github.com/EriArk/TrainerOS/issues/65), 2026-09-24.
Status: planned, not implemented. This document records acceptance; ROADMAP owns
the execution queue. The owner places this before video previews and completed
game metadata/description presentation, followed by Pokedex/Party/Center work.

## Boundaries

- Support every platform recognized by TrainerOS/BatoceraLibrary through an
  explicit TrainerOS/Batocera-to-ScreenScraper system-ID table, including
  cartridge, disc and arcade families. Do not restrict the initial design to GBA.
  An unmapped/unsupported system fails individually without stopping a batch;
  mapping coverage is tested, and unavailable service coverage remains explicit.
- Keep ROM folders authoritative and BatoceraLibrary an offline read/scan layer.
  Only an explicit scrape/update action performs networking. A separate service
  writes compatible media and `gamelist.xml`, then requests the ordinary rescan.
  No TrainerOS-only manifest or resident background scraper.
- Preserve ROM bytes, saves, stable Adventure IDs, Trainer history, owner-edited
  names, unrelated games and unknown XML fields. Refresh/replace is explicit.
- ScreenScraper game media is separate from Pokedex illustrations/sprites and
  the final generic artwork-pack/Pack Studio project.

## Coherent implementation chain

1. Verify WebAPI v2 access, developer credential provisioning, optional user
   authentication, quotas and the complete platform map. Keep credentials out of
   Git, logs and UI-visible URLs; store local user secrets with restrictive
   permissions. Actual credentials/access have not yet been verified.
2. Add an injectable HTTP client and separate service, with bounded responses,
   validated TLS, timeouts, cancellation and per-game errors. Queue work off the
   UI thread, observe service concurrency/request/daily limits, busy/closed and
   rate-limit responses, and support pause/cancel and appropriate retries.
3. Stream cancellable MD5/SHA1/CRC32/size computation and verify that files did
   not change. Match exact hash/size/system first, additional hashes next, then
   filename/title fallback. Ambiguous results require controller selection.
   Preserve hacks, translations and revisions; handle real archive/disc formats.
   Share hashing primitives with RetroAchievements only where their semantics
   match; do not change achievement identity as a side effect.
4. Cache identity/metadata/download state outside the ROM tree. Download bounded
   media with deterministic collision-safe names under the system media tree.
   Use a separate atomic XML writer that updates only the chosen game's fields,
   preserves unrelated/unknown elements and emits relative media paths. Cancel
   or interruption must not truncate XML or overwrite unrelated user files.
5. Deliver controller actions for one game, current system or the whole library,
   missing-only and explicit refresh modes. Show current game, completed/total,
   phase, failed/skipped counts and pause/cancel. Keep the shell responsive.
   Rescan and verify the existing Worlds/Home consumers pick up the results.

The first usable profile includes name, description, genre, players, release
date, developer and publisher; cover → `image`, logo → `marquee`/`wheel`, gameplay
capture → `screenshot`, optional fanart. Video/manual remain recognized optional
media. Video acquisition/playback and final metadata/description layout belong
to the immediately following increment; they do not block the static profile.

## Verification and completion

Use fake/injectable transport for deterministic tests: complete platform mapping,
hash and fallback matching, ambiguous selection, quotas/rate limits, offline and
individual failures, cancellation in each phase, malformed/oversized responses,
atomic XML preservation, media paths, cache hits without duplicate requests,
batch continuation and rescan visibility. Test archive/disc cases explicitly.

Live service access, real controller flow, no ordinary-scan networking, bounded
device performance and installed Flip screenshots remain delivery gates. A fake
transport pass alone does not prove live scraping. Verify preservation of game
files, saves, IDs/history and pre-existing media/XML before reporting delivery.

## Following work, without dropping earlier acceptance

After scraping, finish video previews and related descriptions/metadata/media
in the game views, including lifecycle pause and launch/return performance.
Then finish Pokedex/Party/Center in dependency order: remaining UI and reference
presentation; verified individual/ordinary-save readers; current-save Dex and
related Journey/Champion projections; Party/Storage and guarded Heal/Backup/
Restore; living Party/Playroom, practice and Link Counter with their actual data.
Exact-title support, protected writes and a second-device requirement for Link
remain real gates. This is not a promise of universal save parsing. Other runtime,
RA, system/recovery, Help and final pack-tool commitments remain in ROADMAP.
