# Native ScreenScraper — accepted scope and delivery order

Accepted [issue #65](https://github.com/EriArk/TrainerOS/issues/65), 2026-09-24.
Status, 2026-10-08: developer access approved; native jobs, Settings and display
choices delivered on Flip 2 and Odin 2; evidence and remaining boundaries below.
ROADMAP owns the execution queue: resume MP-02 after this owner-requested detour.
The older ScreenScraper/video/Pokedex sequence is historical, not the current queue.

Owner-requested public screenshots: [Downloads, settings and two game pages on
Flip 2](../screenshots/screenscraper-2026-10-08/README.md). This bounded gallery
preserves earlier README images and excludes account fields and raw media files.

## Native integration — 2026-10-08

The owner explicitly resumed #65 after the [developer-account reply](https://www.screenscraper.fr/forumsujet.php?frub=12&fsuj=24501).
The [official WebAPI v2 guide](https://www.screenscraper.fr/webapi2.php) was checked
again for `ssuserInfos`, `jeuInfos`, `jeuRecherche`, system IDs, localized text,
media types, quota fields and HTTP errors. A real account request and a hash/size
lookup for an installed GBA title returned HTTP 200 with matching MD5/SHA1/CRC32.
No credentials or authenticated media URLs are included here.

### Player controls

- Start quick controls → **Scraping** opens a system picker immediately. Select
  any combination of populated ROM systems or Select all, then **Download**.
  Each row has its platform badge and game count; unavailable ScreenScraper
  systems are labelled and cannot be selected. Nothing downloads until Download
  is pressed, and zero selected games disables it. Existing download settings
  apply. Up/down moves through systems, A toggles, right reaches the fixed action
  buttons, left returns to the list, and B returns to Start with Scraping focused.
  Touch uses the same selections. An already active job opens Downloads instead
  of replacing its queue. This owner-requested addition leaves the wider menu
  reorganisation for later.
- Start → Settings → Library → ScreenScraper, or the ScreenScraper category:
  login/password, Save & check connection, sign out, language (English fallback),
  region, game information, 2D/3D/no cover, logos, screenshots, optional backgrounds,
  missing-only/replace mode, and an explicit whole-library job.
- Display choices are independent of downloads: cover/screenshot priority,
  logo/text titles, downloaded/themed background, facts and descriptions. They
  apply to the existing library presentation without re-scraping. Missing primary
  artwork falls back to other locally available media. Player/online indicators
  retain their existing runtime and metadata rules when facts are hidden.
- Hold A / game options → Download information & artwork: this game, its system,
  its series across platforms, or the library. Series reuse the existing Worlds
  classification. World management also offers scraping that world collection.
- A job shows the current title, phase, processed/total, failures and skips.
  Pause finishes the current game; Stop cancels hashing/network/writes at a safe
  boundary and keeps already completed games. Ambiguous or filename-only matches
  require selection, another-title search or Skip. Failed games can be retried.
- Ordinary folder scans still perform no networking. Completed jobs request the
  usual Batocera rescan; game launch, saves, history and Adventure IDs are retained.

### Shared Downloads window

Start quick controls include **Downloads**, with the same entry available while
a scrape job is running. Starting a job opens this shared popup. It shows game
thumbnails with platform-silhouette fallback, platform badges, task state and an
indeterminate activity indicator (not fabricated byte percentages). Closing the
popup leaves the job running; it can be reopened from Start. Up/down selects a
task, left/right moves to its actions, A activates, B closes; X pauses all, Y
resumes all and Select cancels all. Touch exposes the same controls.

Pending games can move up/down, pause independently, resume or cancel. The
currently running game cannot be reordered. Queue pause finishes that game and
then stops; this is labelled **Pause after game**. Cancelling the active game
interrupts its request and advances to the next eligible task after the atomic
write boundary. Completed metadata is retained. Paused tasks do not hold up other
eligible tasks; cancel-all keeps completed games. A missing/ambiguous match offers
**Choose match**; remaining failed games can be retried without repeating success.

`DownloadsController` owns the shared presentation, stable task selection and
provider command routing; providers retain their scheduling and storage rules.
Only ScreenScraper is connected today. Order changes apply within the provider's
pending queue. The current queue/history is session-local; automatic restoration
after app restart and cross-provider scheduling are not delivered. ScreenScraper
retains the existing game/library/session mutation gate while its job is active;
the popup's dismissal does not release that protection. The quick volume/radio
controls remain accessible.

### Matching, limits and storage

101 of the 105 platform entries map to `systemesListe.php` as checked on 8 October.
C128, Enterprise, Sega System SP and Videopac+ have no separate entries in that
response: they are reported individually as unavailable, never assigned another
machine's ID. Generic arcade remains the ScreenScraper MAME catalogue. This is
service catalogue coverage, not a promise that every game will be found.

Single ROMs, ZIP/7z archives, ISO and compressed disc containers are fingerprinted
as the actual file supplied, not as an invented uncompressed/RA identity. A
container match is accepted automatically only if the service returns matching
size/hash/system. CUE/M3U descriptors use title search with deliberate selection;
their small text hashes cannot prove the identity of referenced discs. No archive
extraction or disc rewriting occurs. Multi-disc playlists retain one library entry.

One sequential worker handles hashing, HTTP and XML. Known per-minute/day and
failed-lookup quotas are observed. 423/429 use cancellable backoff and one bounded
retry; daily limits and denied/restricted client access stop the queue. Errors
for individual games do not erase successful results. Account checks are explicit.

Settings live in `screenscraper-settings.json` in the application state directory.
Developer/user credentials are provisioned privately in
`secrets/screenscraper.json`, with owner-only file permissions; neither is a source
constant or checked-in configuration. User changes use the existing masked
controller keyboard. The developer credential provisioning for public releases
belongs to the release pipeline; this delivery does not publish those secrets.
Guest credentials are optional in the protocol, but service availability without a
user account is not guaranteed (the live guest account check was refused).

`cache/screenscraper/` stores selected identity, localized metadata and relative
download paths, keyed by file content/path and download preferences. It contains
no request/media URLs, passwords or ROM contents. Completed cache hits avoid
duplicate game/media requests. Partial successful downloads survive retry.
Media is validated and content-addressed under the system's `images/` folder.
The XML writer preserves names, unrelated games and unknown fields; replacement
of other selected fields is explicitly configured. Existing local media remains
authoritative in missing-only mode. Cancelled writes use atomic replacement.

Static artwork is the delivered acquisition profile. Existing local video preview
controls/playback remain available; video/manual downloading is still the separate
following media increment. This does not bundle scraped artwork into a release.
Selectable library layouts (grid/list and other arrangements) are explicitly
deferred by the owner; the present controls change pictures and game data only.

### Verification record

#### Start system picker follow-up — 2026-10-08

- Native ARM build and the affected `screenscraper` / `interactions` suites passed
  (4.47 s). New coverage exercises multiple system selection, all/clear, empty and
  unsupported systems, existing launch guards, exclusion of removed/curated content,
  selected-system-only XML writes, controller action navigation and return to Start.
- The same binary is installed and observed running on Flip and Odin:
  `0cbe94769394f2a74ff7b1bd70f924adbd809fe476775c0554d1d602b4561a72`.
  Actual captures show the paired Downloads/Scraping quick controls and system
  picker; controller selection, scrolling and Back plus direct pointer selection
  were exercised. No changed-panel QML errors appeared in session logs.
- Flip completed a one-game C64 job through the new entry into Downloads. This
  happened before the owner's subsequent instruction to leave scraping for their
  own test. No further scrape was started after that instruction; Odin's check
  only opened the picker, toggled a selection and returned to Start.
- Odin's pre-update compositor was already unable to capture frames and then
  stalled in GPU waits during session restart. The remote reboot request did not
  restore SSH; the owner rebooted it, after which installation and visual checks
  succeeded. Its prior boot preference was restored after entering TrainerOS for
  verification. This is recovery evidence, not a new GPU/driver fix. Both devices
  retain quiet output at 0%.
- At the owner's request, 13 additional ROMs for seven systems were copied from
  their server ROM disk to Odin only, without media, gamelists or saves. Source
  and destination SHA-256 matched; all 160 pre-existing destination files retained
  size/mtime, and all 13 new records appeared in the library. No scraping or game
  launch was performed for this addition. Private paths, manifests and ROMs stay
  outside Git; this is not a redistribution or emulator compatibility claim.
- Ignored evidence: `work/research/ux01-flip-quick-scrape-*.png`,
  `work/research/ux01-odin-quick-scrape-*.png`, `quick-scrape-tests.log`,
  `quick-scrape-rom-transfer.json` and `quick-scrape-odin-indexed.txt` in that same
  research directory. Existing public screenshots remain unchanged.

#### Original integration and shared Downloads

- Native ARM Qt build passed. The `screenscraper`, `library` and `interactions`
  CTest suites passed together (13.88 s). The scraper suite passed again after
  adding cross-platform series scope, left/right preference controls and the
  explicit MSX-family mapping checks. Tests cover live-job orchestration through
  injected transport, ambiguous selection, cancellation, pause, partial download
  reuse, retry-only-failed behavior, cache privacy and XML/ROM preservation.
- Both installed handhelds completed the real account check and an explicit
  one-game job for Kirby & The Amazing Mirror (USA), GBA: 1/1 processed, zero
  failures/skips. The ordinary rescan displayed its cover, logo, screenshot and
  localized game data. Flip used controller events; Odin also used touch.
- Flip rendered independent cover-first/text-title/hidden-facts/hidden-description
  choices without another scrape. Player count remained visible. Test preferences
  were restored afterward. Downloaded backgrounds are implemented but have no
  separate live-background visual acceptance in this pass.
- Before/after hashes matched for 21 ROM/save files on Flip and the one inspected
  ROM on Odin. Six identity/history table snapshots per device matched, including
  stable Adventure identities. This is bounded preservation evidence, not an audit
  of every save on either device.
- Installed screenshots in ignored `work/research/ux01-*-ss-*.png` record Settings,
  account checks, job/result, artwork, alternate display, and corrected compact
  menus. Private account screenshots and downloaded game artwork are not committed
  or substituted for README screenshots. Both devices remain at 0% system volume.
- The shared Downloads addition passed `screenscraper` and `interactions` again
  (4.50 s): provider routing, stable selection after reorder, Start/controller
  entry, individual cancellation during an active request, queued pause/cancel,
  reordered execution and closing without cancellation. The installed Flip
  completed a four-game Kirby batch (GBA/N64/DS); its next paused queue was reordered
  through controller events, one queued game was cancelled, and the other three
  completed after closing/reopening the popup through Start and resuming. Odin
  completed a one-game cache job and reopened its
  result through Start. Thumbnails/platform badges and the shorter one-game popup
  were inspected on-device. Current captures are
  `work/research/ux01-flip-downloads-final-reordered.png` and
  `work/research/ux01-odin-downloads-final-compact.png` (private, ignored).
- Final ARM executable installed and separately observed running on both devices:
  SHA-256 `f4cad089aa87ebdce691a99b58e8160c9070032d41a10586b5a411aaccea89a1`.
- Physical owner acceptance, large-library service soak, every-title matching,
  public-release credential provisioning and video/manual acquisition remain
  separate. The task does not close artwork/distribution issues #115/#116.

## Historical prepared backend — 2026-09-24

`src/integrations/scraper` provides a worker-confined WebAPI v2 client, injectable
transport/delay, explicit credentials, English metadata/media parsing and a
33-platform map checked against Batocera discovery. No production UI calls this
client yet; ordinary scans remain offline. Developer credentials are pending.
The protocol reference is [ScreenScraper WebAPI v2](https://www.screenscraper.fr/webapi2.php).

Call `account()` before a job, then use one Client on one worker. Requests are
serial, paced to the returned per-minute limit, count toward known daily limits,
and stop at quota exhaustion. Busy responses impose at least 30 seconds of
cancellable backoff on the next call. A caller decides retries; the client never
silently retries a game. HTTPS is mandatory; responses/downloads have size and
time limits. Redirects cannot forward credential queries to another origin.
Credentials are atomically stored with owner-only Unix permissions; request and
media URLs can contain secrets and must never enter logs, cache or XML.

MD5/SHA1/CRC32/size describe the exact supplied file bytes. Archive contents,
multi-disc identities and RetroAchievements normalization are not implemented
by this helper. Only returned matching ROM hashes/size/system mark an exact
result; title search always needs a user choice. Existing catalogue IDs stay
independent. Live response shapes and service coverage still need verification.

Validated image/MP4 downloads can be stored under `images/` or `videos/` using
content-addressed names. The separate XML writer keeps relative media paths,
owner names, other games and unknown fields. Missing-only preserves populated
fields; explicit refresh replaces supported fields except names. Invalid XML,
duplicate paths/fields, changed ROMs and escaping media paths fail without
replacing the original. Updates use QSaveFile and a per-system lock. The future
job controller must serialize commits/rescans with existing library edits;
the lock alone does not serialize external file managers or TrainerOS Move.

Still required for #65: authenticated service proof, persistent identity/download
cache, archive/disc matching policy, cancellable job queue and progress, pause/
retry, ambiguous-result selection, game/system/library controller actions and
rescan handoff. These remain acceptance below, not completed work. At the owner's
request, independent [local video playback](VIDEO_PREVIEWS.md) proceeds while
developer access is being requested.

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
   permissions. Access is now approved and verified; see the dated native-integration record above.
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
