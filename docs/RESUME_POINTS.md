# Trustworthy Continue cards

This is the prerequisite for filesystem-backed Continue in milestone 6 and issue #6. The domain and controller behavior are implemented with fixtures. Normal mode still shows real launch history; RetroArch still supports ordinary launch only. No state enumeration, screenshot extraction or save parsing is claimed by this increment.

## Identity and ownership

`ResumePoint.id` is a stable, library-wide card ID. Providers reuse it when rediscovering the same source, independent of its current full path. `adventureId` binds the card to one owned Adventure. `ResumeSource` contains adapter ID, adapter-owned source ID, source revision and integration revision. These are opaque, bounded identifiers, not filesystem paths, commands or secrets.

A source revision identifies the exact state represented by the card. Replacing bytes at the same path must change the revision. An integration revision identifies the compatibility context, including core/content changes where relevant. File metadata can be a cheap observation hint but cannot prove exact identity against same-size/timestamp replacement. The real adapter must define and validate its fingerprint policy before advertising exact resume.

`savedAt` is the meaningful source timestamp and may be unknown. `observedAt` is when the provider checked it; rescanning must not make an old save appear newly played. Location and summary belong to that same source revision. Future screenshots must carry the same revision association; a missing screenshot does not invalidate an otherwise usable state. Current cards use original geometric placeholders.

`adapterPayload` is private to the adapter and never projected to QML or stored in browsing preferences. TrainerOS does not delete, recreate or overwrite external states to repair a card.

## Availability

| State | Meaning | Controller behavior |
| --- | --- | --- |
| Exact | Verified source revision and supported direct resume | Offer Resume; revalidate before starting |
| LaunchOnly | Known moment, but direct resume unsupported | Ordinary Start; choose a save inside the Adventure |
| Stale | Unchecked, incomplete provenance or replaced revision | Explain that the moment needs checking |
| Missing | Selected source absent | Keep the Adventure and explain the unavailable moment |
| Incompatible | Ownership or integration context changed | Explain changed resume setup |

Unspecified provenance defaults to Stale. Capability flags alone never prove a particular point resumable. `AdventureAdapter::resumeAvailability()` is a cached, nonblocking assessment; an adapter can refine it using its validated snapshot. `resume()` must validate the exact requested revision at the launch boundary, asynchronously where external I/O is needed. Failed validation returns an error, never an implicit launch or a substituted state.

## Selection and refresh

Y still opens the Home selector and A inside it only selects. Home persists the selected source identity and revision alongside Adventure/card IDs. Refresh and restart cannot silently accept a newer revision. Legacy browsing state without provenance is unverified. Choosing a refreshed card explicitly establishes a new selection.

If the selected moment disappears or becomes invalid, Home shows the reason. Its next A explains the fallback and clears the invalid resume selection without launching. After dismissing the notice, A opens the same Adventure normally. A changed point discovered between drawing the card and pressing A likewise cannot launch. Worlds checks the displayed revision against the current repository snapshot before dispatching Resume.

Continue consumes repository snapshots only. Repeated IDs collapse to the latest observation; focus follows identity through reordering. Save cards and recent-session cards are ordered by meaningful save/start timestamps, not scan time. A recent session is not a save state. An Adventure with state cards does not acquire a duplicate recent-session card.

No filesystem scanner is added here. The next provider increment must publish bounded asynchronous snapshots, retain last-known points with explicit availability on validation failure, discard outdated responses and avoid deep startup scans. It must define scan limits and fingerprints for the first verified emulator/core. Synchronous directory walks in QML getters, capabilities queries or input handlers are forbidden.

## Acceptance and verification

- [x] Exact, launch-only, stale, missing and incompatible fixtures have distinct behavior.
- [x] Foreign/incomplete provenance cannot directly resume.
- [x] Selection survives refresh/restart without accepting a replacement revision.
- [x] Duplicate observations do not duplicate Continue cards or steal focus.
- [x] Save and observation time remain separate; absent screenshots do not block exact mock resume.
- [x] Home and Worlds reject replacement between presentation and activation.
- [x] Adapter-side invalidation after presentation checks cannot implicitly launch.
- [x] SDL/QML smoke exercises unavailable cards, fallback, notice dismissal and fixed Home A at 1920×1080.
- [ ] Bounded asynchronous filesystem provider, exact fingerprint and real direct resume on Flip.

`resume` tests mutable snapshots, queued refresh, revision pinning and adapter invalidation. `qml_smoke` renders the fallback flow through SDL virtual-controller events. Existing history/process/persistence scenarios cover ordinary launch and return independently of these fixtures.
