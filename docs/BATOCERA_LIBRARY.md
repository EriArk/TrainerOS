# Folder-based library and game media

Owner decision, 2026-09-23: use Batocera's filesystem and `gamelist.xml`
conventions. A future desktop RomM fork is a separate owner project. Do not
introduce a TrainerOS-only manifest as a requirement for adding a game.

```text
Emulation/
  bios/
  roms/
    gba/
      Pokemon - Emerald Version (USA, Europe).gba
      gamelist.xml
      images/
    gamecube/
    psx/
```

Subfolders, including the existing `pokemon/` and `Multiverse/`, are supported.
Physical folders describe systems; Pokémon Worlds still describe regions.
`gamecube` maps to the existing `gc` platform ID; legacy `gc` and `3ds` folders
remain accepted. Existing installation paths and IDs are preserved. Firmware
stays in the sibling `bios/` tree; runtime configuration remains adapter-owned.

## Runtime behavior

`BatoceraLibrary` wraps the real collection repository. Startup and entering
Worlds refresh the library on a worker thread; normal input remains responsive.
The default source is `~/Emulation/roms`, overridable with `--roms-dir`.
New readable game files are persisted through the existing repository and adapter
setup contracts. No ROM bytes are hashed, parsed or rewritten during discovery.
Existing records retain their IDs, names, Worlds, adapter configuration, revisions
and Trainer history. Repeated scans add nothing twice. Disconnected storage never
deletes records. External file moves are not guessed as identity changes; the
future management action will own deliberate moves and rebinding.

Filename matching uses platform plus a normalized catalogue title, ignoring known
dump-region/revision annotations and punctuation. Matching is exact, not fuzzy.
Unrecognized Pokémon names (or files in `pokemon/`) appear in **Other Pokémon**;
they never silently become an official base edition. Other files enter Multiverse.
Folder/name evidence identifies hacks without collapsing their identities.
Catalogue coverage and matching aliases can expand independently of runtime support.

Discovery currently covers the cartridge/disc/arcade extensions in
`BatoceraLibrary.cpp`; existing registered files on other platforms still receive
metadata. Recognized extensions are not a promise of an installed working emulator.
Playlist member discs are not separately added; existing bindings remain intact.
Common arcade BIOS archives, NAOMI/Atomiswave companion CHDs and Neo Geo CD
companion descriptors are excluded from new discovery. Explicitly labelled DLC
and update files are not standalone Adventures; existing registrations remain
intact. These dependency rules follow Batocera's [NAOMI layout](https://wiki.batocera.org/systems:naomi)
and [FBNeo BIOS placement](https://wiki.batocera.org/systems:fbneo).
Known catalogue matches retain dump annotations as their edition/variant label.

## Metadata and artwork

Each system's `gamelist.xml` maps `<game><path>` to its file, including subfolders.
Local absolute, home-relative and system-relative paths are supported. Remote
URLs are ignored. `<name>` supplies a new entry's display title; existing owner
names are never overwritten. Descriptions and basic metadata remain available
without importing external play counts, favorites or achievements as Trainer data.

The loader recognizes `image`, `screenshot`, `thumbnail`, `marquee`, `fanart`, `titleshot`,
`video`, `manual`, `magazine`, `map`, `bezel`, `cartridge`, `boxart`, `boxback`,
`wheel` and `mix`. These names follow the
[Batocera metadata declarations](https://github.com/batocera-linux/batocera-emulationstation/blob/master/es-app/src/MetaData.cpp)
and its [gamelist contract](https://github.com/batocera-linux/batocera-emulationstation/blob/master/GAMELISTS.md).
There is no required image filename convention: XML is the association.

Worlds and Multiverse details display cover art and marquee logos. Home and its
selector can use imported art when no personal exit capture exists. Exit captures
keep priority and their truthful labels; generic covers are not presented as
gameplay captures. Pokédex illustration/sprite providers are unchanged.
Video/document paths are recognized for later consumers, not played/opened now.

### Shared game wheel and navigation cost

Multiverse browsing keeps a scrolling logo wheel on the left and selected-game
details on the right. It uses `marquee` (or `wheel`), readable typography when no
logo exists, and `screenshot` / `image` / `titleshot` / thumbnail / cover fallback.
The right side reads the full XML description, release year, platform, genre,
developer/publisher and player count. Missing fields stay absent; another game's
art or invented metadata must never fill them. A chooses for Home; B returns to
systems. Search, filters and per-system position survive page/face changes.

The same `LibraryWheel` component is used inside Pokemon's region Worlds.
Edition platform badges remain visible and missing catalogue editions are grey.
Metadata can fall back to the bundled catalogue's known release year; unknown
years are omitted. Regions, hacks and owner identities remain distinct. Existing
Pokemon detail actions, launch and missing-file linking remain available through
A; the visual change does not replace those working routes with Multiverse's
independent Home selection. Left/Right retains the eight-edition jump.

Presentation maps and system counts are cached until library refresh. Focus
movement does not reset the QML game model or re-read each game's media. Wheel
images decode asynchronously at bounded sizes. Repeated discovery requests are
coalesced within ten seconds; read-only scanning does not activate the shell's
service input gate. Actual registration writes still activate that gate. Unchanged
media/availability scans do not announce a library change. Explicit maintenance
rescans remain immediate. This is bounded refresh, not a filesystem watcher.

Metadata/media snapshots are read-only and separate from registration revisions,
so refreshing graphics cannot invalidate exit pictures or save routing. Missing
images use the existing visual fallback. Malformed/oversized XML is discarded as
a whole; scanning the actual games still works. XML is bounded to 32 MiB and
20,000 entries, traversal to 50,000 entries per system. Unknown XML elements,
emulator/core commands and external statistics are never executed/imported.
TrainerOS does not rewrite external gamelists during scans.

## Management acceptance

- Settings switch, off by default, unlocks World editing and its wrench popup.
- Long A on an Adventure in Worlds opens Rename / Move / Delete / Properties;
  short A and Home behavior remain unchanged.
- New games arrive through folders. Preserve the existing missing-edition linking
  route as an optional correction, not the main registration workflow.
- Editing must preserve external XML fields/media, IDs and saves. Deletion and
  moves need concrete, reversible behavior, not extra explanatory UI.

Further emulator groups, difficult title compatibility and old/obscure platforms
are deferred by the owner. Pack Studio and generic artwork packs remain last.

## Verification

The `batocera` tests exercise nested folders, catalogue matching versus hacks,
metadata paths, malformed XML, missing files, hidden new entries, playlists and
real SQLite rescans preserving owner edits/revisions. `trainer_batocera_import`
uses the same service for isolated maintenance rehearsals. Device delivery evidence
is recorded after the production installation has been checked.

### Delivery status — 2026-09-23

Windows: all 40 checks passed across the suite and isolated reruns of the Worlds
screen test. Its fixed animation waits run close to the existing 30-second limit:
timeouts occurred under parallel load and once after relinking (with a PASSED
scenario report); isolated reruns completed in 28.6 seconds. The final matching/validation
changes also passed the dedicated discovery tests. Existing controller smoke and
persistence scenarios passed; no assertions/time limits were weakened.

On Flip, prepared standard XML for the 819 existing records across 25 populated
system folders, plus 300 private images from the owner's server matching 88
Multiverse games. These private files are not in Git. Preparation does not change
TrainerOS database rows or ROM/save bytes.

**Installed and checked on Flip.** After the owner's reboot, Btrfs device error
counters were zero and the single-job ARM build completed; all 44 ARM checks
passed. This does not establish the cause of the earlier disk-I/O stall.
Database-copy rehearsal exposed arcade dependencies/DLC being mistaken for games;
the correction passed dedicated Windows and ARM regression tests before delivery.
The second rehearsal scan added nothing. Production installation retained all
819 original Adventure rows byte-for-byte and added six recognized Smash editions
in Crossovers, preserving their distinct identities. `gamecube/` is now canonical,
with `gc/` as a compatibility symlink for existing paths. The prior binary and
database were backed up; schema remains 11, SQLite and foreign-key checks pass.

A new F-Zero: Maximum Velocity ROM copied from the owner's server was discovered
by the running shell without SQL/manual registration, selected for Home, launched
through the existing mGBA route and returned through guarded exit. Final library:
826 records (692 Pokémon, 134 Multiverse). Its SHA-256 matched the source.
A first test source, Mario Kart: Super Circuit, contained only zero bytes; its
new test copy was quarantined outside the library and its sole failed test record
removed after backup. Server originals and pre-existing records were untouched.

Injected controller input on the actual installed build exercised L1/R1, lists,
A selection versus Home launch, Back and the Y drawer. Actual Gamescope captures
were shown in chat: Sonic CD details with imported logo/image, the new game's
launch and the drawer mixing real exit captures with imported-art fallback.
Private media and captures remain outside Git. Installed binary SHA-256:
`010fca3066753ab9644c436168ac8e8e4f390773c6f9b6f50bfc353602e97d15`.

Device reliability follow-up: the raw-pad read ACL was absent after reboot and
maintenance/session transitions, although the boot hook logged success. Running
the existing grant helper restored guarded exit; its persistence across those
transitions remains an explicit U1 investigation, not a claimed permanent fix.

### Shared game wheel and navigation follow-up — 2026-09-23

Multiverse systems and Pokemon regions now share a logo wheel and a same-screen
metadata preview. Region grouping, missing editions, file linking and existing
Pokemon detail actions remain. Multiverse selection goes straight to Home without
launching. Marquee/wheel assets retain their aspect ratios; absent logos fall back
to styled titles. Only nearby delegates are instantiated, and images decode
asynchronously at bounded display sizes. A regression check verifies that the
selected edition actually occupies the highlighted slot after route/model changes.

The owner's server supplied another 150 private images for 50 Pokemon entries
across GameCube, N64, DS, Wii and Wii U. Exact platform/filename matches were used;
source originals were preserved. Broken Pokemon accent encoding was corrected in
the copied XML after backup. The source lacks matching artwork/metadata for some
GBA titles, including the displayed Castlevania edition; those gaps remain.

Navigation no longer gates controller input on a read-only folder scan. Repeated
scan requests are coalesced, unchanged availability/media no longer rebuilds all
views, and Multiverse caches its presentation records and system counts. Page
transitions publish one completed shell update instead of each intermediate
controller cleanup. Actual file writes retain the service gate.

Windows 40/40 and ARM 44/44 passed for the wheel/scanner integration. The subsequent
shell-notification batching passed the Windows 40/40 suite; final layout/alignment
changes received focused persistence and QML controller checks plus actual Flip
validation. ARM's full suite was not repeated for those final small adjustments.
The initial tab probe observed stalls up to 1.56 seconds. After batching, a
16-transition controller-to-tab-pixel probe measured 195–375 ms. Repeating it on
the final installed binary measured 189–449 ms (15 of 16 at or below 302 ms), with
Home reset/captures at its start/end. These are bounded UI observations, not long-run frame-time or
whole-device freeze guarantees. Shorter synthetic button holds were unreliable and
were replaced with the same 240 ms hold used by device interaction checks.

Production delivery preserves all 826 Adventure rows and schema 11; database and
foreign-key checks pass. The previous binary/database are backed up. Private
captures and metadata remain outside Git. Final installed binary SHA-256:
`f8d4d241075c69cc97310e04466a064a5d079c10f2cd20b4914042f217afb4c3`.

### Wheel selection refinement — 2026-09-23

Both wheels use an animated gold crystal pointer from the left rail edge instead
of a rectangular selection plate. Controller hints now live in the shared bottom
footer. The initial Down clamp is superseded by the circular-wheel correction below; B returns to regions/systems without
an extra Back focus stop in populated lists. Empty lists keep a recovery action.

### Logo hierarchy refinement — 2026-09-24

The shared wheel now uses a 27×30 logical-pixel crystal, a 104-pixel selected
logo slot and platform badges in the right-hand detail header. Missing logos
use bundled Bungee with an outlined, offset-shadow treatment and a stable
single color derived from the Adventure identity. Long names fit two lines;
file/media availability and IDs are unchanged.

Windows Worlds/Pokédex/artwork controller-render scenarios passed 3/3. Worlds'
existing 61-stage scenario received a 60-second deadline: its deliberate waits
already take about 28 seconds, and the first run exceeded the old 30-second
limit. Assertions remain intact; the rerun completed in 28.59 seconds.
The ARM64 production build was installed with binary/SQLite rollback copies,
schema 11 and all 826 Adventure rows preserved. Real-session injected gamepad
navigation and Gamescope captures cover F-Zero X, Pokémon Platinum, a Diamond
entry without a logo, paired browsing and return from the system menu.
Installed binary SHA-256:
`be106dd6e8839c51196fead016d8df3d9caf174ce1217ca41d01e4afb6fa7c5c`.

### Permanent deletion — owner correction, 2026-09-24

This supersedes the new-game trash behavior described in the earlier checkpoint
below. The game menu and affirmative confirmation now say **Удалить**. The
confirmation starts on Cancel and states that the ROM is permanently removed;
saves, history and artwork stay. Successful deletion unlinks the actual single
ROM file, with no trash copy. Its existing schema-12 removal row has an empty
trash path and retains the Adventure identity/history references.

Unavailable, symlinked, shared or playlist-referenced files are rejected. Multi-file
entry points (CUE/M3U/GDI/CCD/RPX) require Desktop Mode for now: deleting only their
launcher is not reported as deleting the full installation. No directory, sibling
save, artwork or BIOS cleanup is implied. File removal failure rolls back metadata;
if the final database commit fails after unlink, the error says the ROM was removed
but library bookkeeping failed. A process/power loss at that point may leave a
missing installation record; it cannot create a false recovery copy.

Copying a ROM back into its original library path lets the next folder scan reuse
the original Adventure ID and name. There is no restore operation without a real
file. Legacy nonempty trash records retain explicit recovery in Settings, and that
entry disappears when the old trash is empty. Existing trash files are never
silently purged by this update.

### Contextual library editing — initial 2026-09-24 checkpoint

- Settings → Library → Edit Worlds is off by default. When enabled, the selected
  Pokémon World has a wrench and a bottom Select shortcut. Its popup renames the
  World without changing its ID or the catalogue's original region identities.
- Hold physical A for 600 ms on an installed game in either wheel to open
  Rename / Move / Delete / Properties. A short press retains the existing action;
  releasing a long press never confirms a menu row. Navigation, input disable and
  controller disconnect cancel a pending hold. Home A remains immediate.
- Rename changes the displayed title, retaining filename, media and save routes.
  Move changes a Pokémon game's World membership and clears additional memberships.
  Multiverse remains system-based: Move is unavailable there, since changing the
  console is not a valid move. Physical folder moves remain a separate follow-up.
- Delete requires confirmation and moves the launch ROM to a hidden
  `.traineros-trash/<unique-id>/` directory beside its original folder. Settings →
  Library → Game trash restores it. Saves, artwork, XML and history are retained.
  The full Pokémon reference catalogue keeps the now-missing edition; owned-game
  lists and Choose Adventure omit the removed installation until it is restored.
  Playlist disc dependencies stay in place and are not rediscovered as new games.
  A ROM referenced by another registration is not moved. Restore never overwrites
  a replacement file at the original path.
- Both game wheels wrap first/last in either vertical direction, including filtered
  lists. B remains the return action; empty lists retain their recovery action.

Schema 12 adds a default-off preference and `library_removals`, retaining Adventure
rows and references. A durable removal intent precedes the same-filesystem rename;
Restore handles interruption on either side of that rename. Scanning includes
removed registrations for identity matching. Display edits advance the registration
revision and matching exit-media revisions together without relinking the ROM.
These changes do not rewrite `gamelist.xml` or erase game history.

### Management verification — 2026-09-24

Windows native build and the full 40-test suite passed, including real SDL short/
long-press and cancellation, filtered wheel wrapping, asynchronous edits,
interrupted trash intents, restore collisions and scanner playlist suppression.
A copied live schema-11 database migrated to schema 12 with all original values
in 16 tables unchanged. The ARM64 production build was installed with binary and
database rollback copies; all 826 Adventure rows survived installation.

On the installed Flip session, injected gamepad events exercised F-Zero X rename
and restoration of its original title, ROM trash, and Settings restoration. The
ROM was absent from its original path during trash and restored byte-for-byte
with the same SHA-256 and Adventure ID. Sinnoh was renamed and restored through
the controller keyboard; its wrench/Select popup and both wheels' first/last
wrapping were captured. Test names were restored and World editing returned to
its original off setting. Captures shown to the owner came from the handheld
compositor, not host renders. Physical thumb testing remains the owner's check.

Private evidence is under `work/research/library-edit-*` and the corresponding
device task directory; ROMs, database copies and captures are not committed.
Installed binary SHA-256:
`188cb8c9d290113e3f78bb4bf53213cde7fa6174f37f6983960471474d61991a`.

### World cards and permanent deletion verification — 2026-09-24

The full Windows build and 40 tests passed. After correcting semantic pairing
for reversed repository order, both the World unit tests and SDL/QML World
scenario passed again. Coverage includes permanent unlink, preserved saves,
unavailable/shared/playlist file rejection, legacy recovery collisions and
rediscovery under the original identity after copying a ROM back. Sprite tests
verify consistent all-frame alpha cropping without changing source assets.

The ARM64 production build was installed with binary/database backups. Actual
Flip controller events exercised grid scrolling, separate Fiore/Almia selection,
opening each half and Back, L1/R1 peers, the Multiverse pair, enlarged Dex sprite
walking, and deletion through long A. A synthetic ROM and neighboring synthetic
save verified Cancel keeps both, then Delete unlinks only the ROM with no trash
copy. The surviving synthetic save was checked byte-for-byte and cleaned up
afterward. No existing game was deleted in this probe.

All 826 original Adventure rows remain unchanged; one additional removed test
record preserves the deletion evidence. SQLite quick/foreign-key checks passed,
and one installed shell owns the database. Device compositor captures, including
the cards, both pair routes, Dex walking and deletion confirmation, were reviewed
and shown in chat. Physical thumb acceptance remains the owner's check. Private
evidence is under `work/research/world-cards-*` and the corresponding device task.

Installed binary SHA-256:
`1bc6b642dcf3d504c0e9111eca67fb20d977666847c6f6766b05f485b559aa0b`.
