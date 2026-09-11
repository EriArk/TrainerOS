# Personal library and launch preparation

Normal application mode combines persistent personal metadata with the [read-only collection catalogue](COLLECTION_CATALOGUE.md). The controller management flow connects to the [first real RetroArch adapter](RETROARCH_ADAPTER.md). The owner has confirmed navigation, profile creation and real launch on Flip 2; additional integrations remain open.

## Library ownership

The store starts with nine reference regions and no Adventures, recent trails or invented game progress. A saved Adventure has its own stable ID, one primary World, additional World relationships, title, edition (Original / Remake / ROM hack), notes, a local file reference and opaque adapter configuration. Users can create a named World for a ROM hack. Referenced files stay external and untouched; moving a file requires choosing its new location, not creating a new Adventure.

Save validates title/World/file metadata on the worker and commits the Adventure and all relationships atomically. Revision checks reject stale edits. Missing files, invalid World relationships and SQL failures preserve saved records and the open draft. As with Trainer Save, leaving after submission does not cancel the write; errors remain visible if the user has left the form. Cancelling before Save changes neither the database nor external files. No delete/overwrite/save-management operation is included in this flow.

Worlds browsing includes an Adventure under its primary and additional regions. Stable selection survives edits/reordering/restart, including secondary relationships. The region grid reveals the focused row when custom Worlds exceed the initial nine; only the bounded grid scrolls. Unconfigured records expose Needs setup and Back, not a simulated successful launch. Home/Continue has an honest empty state until a real history/resume provider exists. Hall of Fame and the reference Pokédex still contain labeled demonstration content.

## Controller management

Start → Manage Adventures opens an attached service panel, preserving the primary-page structure. D-pad/left stick browses entries; A edits. The action rail offers Add Adventure and Back. The editor has six fields in a two-column layout, followed by Save/Cancel:

1. Title — shared controller keyboard, up to 64 graphemes.
2. Primary World — a bounded picker, including Create a World (name up to 32 graphemes).
3. Also in Worlds — multiple choices with explicit Apply/Cancel; the primary relationship is excluded.
4. Edition — cycles Original, Remake and ROM hack.
5. Local file — asynchronous controller file picker.
6. Notes — optional single-line description, up to 160 graphemes.

File browsing offers Locations, Parent folder, Previous/Next batch, Refresh/Retry and Cancel. It discovers ready mounted volumes, lists folders first and returns up to 80 entries per batch. The viewport shows only a few focused rows; there is no whole-screen scrolling or touch gesture requirement. Directory enumeration/sorting runs on a worker and is non-recursive. B retraces entered folders, or cancels the chooser at its starting point; while loading, B cancels the chooser and later results are ignored. File selection does not imply format or emulator compatibility. The picker does not display hidden entries in this first implementation.

Down beyond the last file reaches the lower action rail; Up returns to the remembered file. Start overlays a service/form/keyboard without mutating its draft. Selecting another service deliberately closes previous drafts. L1/R1 always changes the primary page and closes transient service edits. B unwinds keyboard → picker → form → service → system menu as applicable.

## Preferences

Start → Settings exposes shell color and reduced motion. Available colors are turquoise, red, green, blue and orange. The same QML pages consume theme tokens; chassis and bevel colors change while recessed light screens and distinct button accents retain their roles. Reduced motion removes drawer, tab and keyboard transition durations. Changes apply after an asynchronous successful save and survive restart; a failed write preserves the previous value. Ephemeral mode applies preferences only for that preview.

## Process lifecycle foundation

`ProcessService` wraps one owned `QProcess`. It accepts a separate absolute executable, argument list and working directory. It never invokes a shell or constructs a command string from a filename. Start and exit/error monitoring are asynchronous. An explicit stop terminates only its child and escalates to killing that child after a bounded grace period. Output is discarded in this foundation; a bounded/redacted adapter logging policy is future work.

`AdventureLaunchController` owns Preparing → Starting → Running → Returned/Failed, with a Stopping path. It requests a persisted navigation checkpoint before starting, suspends presentation only once the child starts, and requests restoration of the captured context exactly once on exit/failure. A failed checkpoint prevents launch. Cancelling preparation invalidates late checkpoint completion. Duplicate launches are rejected. Real adapters must handle file/configuration validation and declare their own lifecycle capabilities; detached or multi-process emulators are not assumed supported.

The test-only probe adapter/child exercise this boundary in the real QML application. SDL A launches the original content-free child from Adventure detail, the window hides, and after success, missing-executable failure or crash it returns to the previous detail/focus. A held shoulder button across handoff is ignored until controls return to neutral. The probe is compiled with `BUILD_TESTING` and is not a user-library adapter or installed session component. No real emulator CLI or ArmadaOS window/session behavior is inferred from it.

## Acceptance

- Version 1 migrates to 2 with profile/favorites and old sample navigation intact; no demo Adventure is seeded.
- Multiple World relationships, a custom World, opaque setup metadata and stable IDs survive database reopening and edits.
- Missing/replaced files, stale revisions and invalid relationships preserve committed records; file bytes remain unchanged.
- Controller-driven application processes create/edit an Adventure, select/relocate its file, cancel edits, create a World, change themes/motion and reopen repeatedly with restored data/focus.
- Empty folders, startup errors, missing-file retry, custom-region scrolling and actual focus outlines fit the bounded landscape viewport.
- Tests cover real child success, start failure, nonzero exit, crash, cancellation, checkpoint failure and literal argument preservation.

The library preparation increment passed all thirteen CTest entries then present; see `DEVELOPMENT.md` for the current suite. A separate Release build with `BUILD_TESTING=OFF` also compiles, excluding the probe adapter/child and feature smoke harnesses. These results do not establish Linux/ARM64 execution, physical input mapping, suspend/wake, emulator integration or dedicated-session recovery. Those remain the device baseline and subsequent adapter work.
