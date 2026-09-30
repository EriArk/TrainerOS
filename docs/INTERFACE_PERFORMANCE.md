# Interface performance audit — 2026-09-30

The owner reports pauses when switching Home / Worlds / Field Guide. This pass
measures the installed native shell on Flip, fixes the synchronous work behind
those pauses, and preserves the existing composition, artwork and animations.
The first slice measured Flip only. The follow-up below records paired delivery;
Flip measurements still do not establish Odin's GPU stability.

## Measurement

Production ARM64 Release builds, the same Flip library (830 registrations), no
running Adventure and no concurrent build during each measured input sequence.
The instrumented baseline is main `00bb2ce` with measurement scopes only.
Twenty requested shoulder-button transitions use five repetitions of
R1, R1, L1, L1, nominally Home → Worlds → Companions → Worlds → Home.
The first series retained the Shops secondary face. A separate before/after
series explicitly selected Field Guide and verified it on a device screenshot.
Each button is held for 160 ms with 1.1 s between presses. An overloaded UI did
not execute every requested transition, so completed handler counts are reported
separately. The sequence includes two seconds of settling at the end.

Measurements distinguish synchronous `goToPage` duration (including directly
connected slots) from a 100 ms precise GUI heartbeat's lateness. Neither is a
camera-measured button-to-photon latency. Frame-swap intervals include periods
with no rendering, so their average/max must **not** be presented as FPS or GPU
stall evidence. The baseline's 2.35 s GUI heartbeat delay establishes a real
main-thread stall independently of that limitation.

Baseline: 27.67 s elapsed, 25.15 process CPU-seconds, 530,328 KiB RSS after the
sequence. Seventeen page handlers averaged 756.25 ms, maximum 964.62 ms. GUI
heartbeat mean lateness was 198.36 ms, maximum 2355.10 ms. Follow-up phase scopes
identified the closed archive editor at 629.06 ms average / 689.08 ms maximum
per page change. That diagnostic sequence drifted to other pages under load;
it establishes the expensive call, not an additional matched performance run.

## Findings and changes

| Area | Cause | Change |
| --- | --- | --- |
| Hidden Hall editor | Every page transition cancelled even an already closed editor. Its `changed` signal rebuilt hidden rows; each game then fetched Worlds, which fetched the entire collection again. | Closing is idempotent; rows exist only in the Adventure picker; World names are read once per row projection. The hidden QML list has no model. |
| Library projections | Even a cache hit serialized all Adventures and registrations into a comparison key. Home, drawer and editor bindings repeatedly paid this cost. | Committed library revisions let the collection reuse its Adventure/World projections directly. Opening and committed changes invalidate before consumers run. Non-versioned providers retain value-based invalidation. |
| Hidden services | Closed library/account/species/clinic/shop/practice/Link controllers emitted redundant notifications. | No-op closes stay silent. Actual close/cancel and active operation guards remain. |
| Party on navigation | Visiting unrelated primary pages republished the current Party and practice presentation. | Current Adventure, library and save-provider events remain the refresh owners. Reassigning the same Party identity/title is a no-op. |
| Field Guide | Three empty/count checks built the complete entry map in addition to the list itself. Emerald family filtering repeatedly searched the whole catalogue. | A scalar `entryCount` and a species-number lookup remove those repeated walks. Forms, favorites, scope and source progress retain their existing behavior. |
| Choose Adventure | Closed drawer still maintained delegates/images. A recreated delegate can initially lose focus to its ListView. | Create content while visible, retain it through closing motion, then release it. Defer selected-card focus until delegate attachment has completed. |
| Exit pictures | Home/drawer explicitly disabled Qt's image cache, although their saved-session image URLs are immutable. | Reuse decoded images through Qt's cache. Current-owner/registration gates still decide whether a URL is exposed. |

The initial fixed build reduced average transition handler time to 105.12 ms
and maximum GUI heartbeat delay to 276.26 ms; process CPU time was 13.37 s.
That measurement motivated the additional no-op practice/Link/clinic closes and
removal of navigation-only Party refreshes.

## Final measured result

The installed production binary SHA-256 is
`8893e2ac7ce74026d979bdd38a5ba5a6c497794f5b46597502a2100e09a2d4f2`.
The matched **Field Guide** route uses the instrumented old binary
`7cc1809e343b2b46e2f713ae6031863c28336171542b21dd30aee206243a364d`
as its baseline. The old binary was temporarily restored for this comparison;
the fixed binary was restored afterwards. No database rollback was performed.

| Field Guide route | Before | After |
| --- | ---: | ---: |
| Sequence elapsed | 27.72 s | 27.82 s |
| Completed page handlers / requested presses | 16 / 20 | 19 / 20 |
| Mean synchronous page handler | 758.98 ms | 77.50 ms |
| Maximum page handler | 967.73 ms | 219.51 ms |
| Mean GUI heartbeat lateness | 189.93 ms | 14.61 ms |
| Maximum GUI heartbeat lateness | 1518.38 ms | 230.66 ms |
| Process CPU time | 24.47 s | 12.70 s |
| RSS after sequence | 482,212 KiB | 460,672 KiB |

The handler average is about 9.8 times lower. Input delivery is not proven
lossless: the macro and application counts still differ by one in the improved
run. These are single bounded samples, not a percentile study. RSS is a process
snapshot, not a leak test. Different completed navigation counts also mean CPU
figures describe the same requested interaction workload, not identical work.

The heavier Shops route on the final binary completed 20 handlers, averaging
86.81 ms, with maximum GUI lateness 375.53 ms. This is why the result does not
claim every interaction is below a frame budget. A 12 s settled Home interval
used 2.28 CPU-seconds, with mean/max GUI lateness 1.39/64.89 ms. A 12 s interval
just after entering Guide used 1.37 CPU-seconds and still included save-result
updates, with mean/max lateness 4.60/261.01 ms. It is not a pure idle sample.

## Verification

- Native Windows build and ARM64 Release production build completed.
- Passing affected suites: core, collection, archive, Batocera, library,
  Pokédex, game progress, practice session and Link peer.
- Passing rendered/controller suites: general shell, Worlds, Pokédex, artwork
  and Hall. The first lazy-drawer run exposed focus loss; deferring card focus
  fixed it. The original clipping/focus assertions remain intact.
- New checks cover silent closed-editor cancellation, revision cache hits,
  rename/remove invalidation, non-versioned provider changes and publication
  of LocalStateStore revisions before its consumers.
- Flip screenshots inspected Home, Worlds, Field Guide and the expanded drawer.
  Physical-input injection exercised Y/open, directional movement, B/close and
  shoulder navigation. Three Trainers and 830 registrations were preserved.
- Diagnostic capture is disabled again for ordinary use. Odin remains outside
  this pass because it was off/unavailable.

## Protection and limits

No ROMs, saves, emulator configuration, save transaction rules or controller
mapping change in this pass. In particular, navigation does not skip Link
activity guards or save verification. Cache invalidation follows committed
library snapshots, including content availability and management changes;
providers without revisions are not treated as immutable.

The second slice below addresses the `Checking → Available` save-observation
notification fan-out through explicit observation/presentation invalidation,
retaining mutation and exact-source gates. It does not suppress verification or
leave stale saves writable. Other cost
areas are QML projections using broad `changed` signals, first-use image decode,
and platform graphics/radio behavior. A cached collection is not a claim that all pages,
cold-start workloads or Odin GPU hangs are solved. Save reads still run on their
existing worker and refresh when entering the relevant pages; no freshness or
write-validation gate was weakened to obtain lower timings. Sprite loading is
already asynchronous and hidden animation timers already stop; removing artwork
or flattening the design is not the remedy for this incident.

## Save-presentation follow-up — 2026-09-30

Repeated visits still published an empty Checking observation, clearing Party
actors/rows and rebuilding Field Guide twice despite identical verified save
bytes. This slice separates a retained read-only presentation from readiness to
act. `GameProgressService` carries only the previous source/save identifiers
while Checking, and only when the caller confirms the same active Trainer,
registration revision, ROM path and integration configuration. It carries no
Party/Dex/Journey payload or badge facts. The worker still opens and validates
fresh save bytes, repeats the read to detect replacement and checks the resolved
source again. Generation invalidation and protected write services are unchanged.

Party keeps its verified rows, actors and selection through that qualified read;
its `available`/management gates remain false until completion. QML uses a
separate display-availability property, so an unavailable-data panel does not
cover retained rows. Entries have their own notification and cached projection;
focus and status changes no longer republish the entire grid. Actual source,
save, box/section or artwork changes invalidate the projection. Field Guide
rebuilds only when its displayed records, scope or stale status change. Missing
or unqualified observations still clear the display; a new owner's records are
never filled from the previous owner.

The matched comparison below starts with the already improved `04d004d` binary,
not the original pre-audit baseline above. Both runs use the same Flip, Armada
`20260929.5915c28`, library and Field Guide face, with no Adventure or build
running. The production build uses Release with `BUILD_TESTING=OFF`.

| Same 20 requested transitions | Before this slice | After this slice |
| --- | ---: | ---: |
| Sequence elapsed | 27.72 s | 27.70 s |
| Completed page handlers | 20 | 20 |
| Mean synchronous page handler | 102.71 ms | 51.89 ms |
| Maximum page handler | 197.27 ms | 134.33 ms |
| Mean GUI heartbeat lateness | 15.24 ms | 8.73 ms |
| Maximum GUI heartbeat lateness | 234.40 ms | 203.81 ms |
| Process CPU time | 13.91 s | 10.47 s |
| Field Guide rebuild calls | 26 | 0 |
| Party actor synchronization calls | 26 | 0 |

This bounded sample approximately halves mean handler time and reduces process
CPU time by 25%. The macro deliberately waits between presses, so its elapsed
time is not an improvement metric. Heartbeat peaks remain above a frame budget;
this is not a claim of smoothness on every page or a button-to-photon measurement.

Verification and delivery:

- Windows: eight affected suites passed (core, game progress, Guide,
  interactions, practice, Link, protected backups and ownership); shell,
  Guide and artwork SDL/QML scenarios passed too.
- ARM64: core, game progress, Guide and interactions passed, followed by a
  separate production configuration/build. Portable adapter snapshot and
  knowledge checks passed; no parser or exact-game profile changed.
- Regression cases cover same-save retention, changed bytes during a worker
  read, rollback/new-save refresh, owner invalidation, disabled actions while
  Checking and focus-only changes without rebuilding rows/actors.
- Both devices received SHA-256
  `eaa851b3a5ebd104172ae29fbd097ed56c6ff8a0782df10289360eed1e34ea01`.
  Each retained its own database (Flip: 3 Trainers / 830 registrations;
  Odin: 1 / 25). A binary/database backup preceded atomic replacement.
- Flip's live process hash matched. Actual Home, Guide and Party captures were
  inspected. Shoulder/trigger navigation and Home worked; Party selection
  remained on Swampert after Home and a fresh return/read. Diagnostic capture
  was disabled, the process restarted, trace growth stopped and InputPlumber
  remained active. The shell was left on Home.
- Odin's installed hash matched, but it remained in its existing Steam session.
  A bounded application launch in that session did not produce the first-frame
  signal within six seconds and was terminated. SSH and InputPlumber remained
  available afterwards. This does not verify its normal TrainerOS session or
  solve the previously recorded graphics/radio incident. No boot preference,
  emulator configuration, ROM or save was replaced.

Remaining cost includes broad Home/other presentation notifications, cold image
decoding and device graphics/radio behavior. Nearby reliability and RA remain in
the roadmap; sleep work is deferred by the owner and suspend stays disabled.

## Reproduce a bounded diagnostic capture

Create `~/.local/state/traineros/performance.enabled` before starting TrainerOS.
The optional recorder writes `performance.jsonl` beside it, with page numbers and
aggregated named durations only. It does not record game titles, Trainer names,
paths, credentials or save contents. Output stops at approximately 8 MiB.
Capture a fresh byte range for each fixed input sequence, after startup settles
and the build process exits. Count actual navigation calls, not just button
presses. Remove the enable file and restart after diagnosis; ordinary production
has no timing timers or writes. The file is checked once at process start.

The approach follows Qt's guidance on expensive bindings, unnecessary
notifications, lazy content and image caching:
[Qt Quick performance](https://doc.qt.io/qt-6/qtquick-performance.html).
Image-provider work must respect provider/thread ownership;
[QQuickImageProvider](https://doc.qt.io/qt-6/qquickimageprovider.html) does not make
access to a GUI-owned store safe merely by enabling asynchronous loading.
