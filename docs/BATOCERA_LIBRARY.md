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

The loader recognizes `image`, `thumbnail`, `marquee`, `fanart`, `titleshot`,
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

Metadata/media snapshots are read-only and separate from registration revisions,
so refreshing graphics cannot invalidate exit pictures or save routing. Missing
images use the existing visual fallback. Malformed/oversized XML is discarded as
a whole; scanning the actual games still works. XML is bounded to 32 MiB and
20,000 entries, traversal to 50,000 entries per system. Unknown XML elements,
emulator/core commands and external statistics are never executed/imported.
TrainerOS does not rewrite external gamelists during scans.

## Following management increment

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
