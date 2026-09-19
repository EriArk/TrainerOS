# Classic illustration seed audit (#58)

This is PC development tooling, not an installed TrainerOS feature, artwork pack
contract, or sprite/animation provider. #51 sprites remain a separate workstream.
The execution order remains #58 → #60 physical Flip layout proof → #57 contract
→ #59 Studio → Settings polish.

## Import and inspect

Use Python 3.10+ with Pillow on Windows/Linux. The importer never accesses the
network. Supply the owner's downloaded folder or ZIP; for a 7z seed, inspect its
member paths and extract it with 7-Zip into a private folder first. Preserve the
archive, original source name, hash and acquisition reference alongside the seed.

```sh
python tools/import-classic-art.py \
  --seed /private/classic-seed \
  --output /private/classic-corpus \
  --source 'original-source-URL-or-reference'
python tools/test-classic-art.py
```

Within a checkout, output must be Git-ignored and contain no tracked files.
The tool refuses overlapping seed/output locations and linked files. ZIP input
is read without extracting source-controlled paths, rejects traversal/absolute
Windows and POSIX paths and links, and has image/count/byte limits. Keep input
and output unchanged by other processes while importing; run one importer per
output directory.

The authoritative `corpus-index.json` contains the complete report snapshot:
original source names, provenance, original SHA-256 paths, dimensions/alpha,
provisional candidates/associations, errors, missing targets and ambiguity.
`missing.json`, `ambiguous.json` and `provenance.json` are convenience exports.
Original bytes are retained under content hashes. Existing originals are verified
and reused; interrupted reports regenerate on rerun. Removed seed entries cease
to be indexed but old originals are never deleted automatically.

Mapping uses the current `data/pokedex.json` species/form IDs and strict filename
labels. A dex number alone never assigns every form, nor is the first form
implicitly considered the default. A uniquely identified form with several
different images remains unresolved rather than choosing the first file. Equal
bytes are deduplicated without losing original filenames. Unknown labels, art-era
suffixes and regional naming differences remain available for review. These
conservative rules intentionally undercount coverage. Source metadata and actual
visual review must resolve such cases before the #60 handoff.

`--review private-review.json` accepts an explicit object mapping image SHA-256
to an existing `speciesId/formId`. This records a reviewed association, not a
license or final pack path. It does not silently resolve competing images for the
same target; candidate selection/review remains a subsequent #58 task. Creator
is null unless actually established elsewhere; a collection named “Sugimori” is
not per-file attribution. Static PNG/JPEG/WebP only; animated inputs are rejected.

## Verified seed checkpoint — 2026-09-19

Imported the organized archive from the [owner's Drive seed](https://drive.google.com/drive/folders/1T2hF3ieas4mNBKQN6v94mlY8lbwT4KLx).
Private acquisition notes retain the exact archive identity/hash. No imagery or
private file manifest is committed.

| Observation | Result |
| --- | ---: |
| Seed PNG entries | 1,858 |
| Unique original images | 1,857 |
| Reference species / forms | 1,025 / 1,579 |
| Strict provisional form associations | 1,109 |
| Unresolved forms, including naming differences | 470 |
| Invalid images / conflicting exact targets | 0 / 0 |

Eight synthetic-fixture tests pass on Windows Python 3.12 and Linux Python 3.12:
folder/ZIP import, missing forms, duplicate hashes, conservative mapping/review,
conflicts, rerun/interruption recovery, invalid bytes/cache corruption, bounded
imports, cross-platform paths and Git guard. No copyrighted test fixtures are used.

**#58 remains open.** The Bulbagarden category is readable through web search,
but direct Archives robots/API probes returned HTTP 403 during this increment.
No missing-art batch fetch was attempted. Access/policy/rate-limit verification,
missing-only acquisition with pagination/backoff/cache tests, ambiguous image
review and maximum-coverage corpus completion remain required. Do not label 470
as proven absent illustrations, bypass access restrictions, or start #60 with a
claim of completed corpus coverage. No Flip installation or native UI changes
were made by this seed-audit increment.
