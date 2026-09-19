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
`review.html` is a private, read-only local contact sheet for species with
unresolved forms; it shows names, IDs, original filenames and full hashes.
Images load locally and lazily. This is an audit aid, not Pack Studio.
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
license or final pack path. Use `--selection private-selection.json` to explicitly
choose a target's image from its unambiguous candidates (object: `speciesId/formId`
to SHA-256). Missing/mismatched choices fail; competing originals remain intact. Creator
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

At the initial checkpoint, the Bulbagarden category was readable through web search,
but direct Archives robots/API probes returned HTTP 403 during this increment.
No missing-art batch fetch was attempted. Access/policy/rate-limit verification,
missing-only acquisition with pagination/backoff/cache tests, ambiguous image
review and maximum-coverage corpus completion remain required. Do not label 470
as proven absent illustrations, bypass access restrictions, or start #60 with a
claim of completed corpus coverage. No Flip installation or native UI changes
were made by this seed-audit increment. The following checkpoint supersedes the
network blocker, not the remaining completion gates.

## Missing-only acquisition and review checkpoint — 2026-09-19

`tools/fetch-classic-art.py` now accepts an explicitly reviewed list of missing
targets and specific Bulbagarden File-page URLs. It requires an imported seed
index, skips already associated targets before making network requests, rejects
unknown IDs and writes a separate private supplement. It does not crawl the site
or assume that all illustrations have a predictable filename.

```sh
python tools/fetch-classic-art.py \
  --index /private/classic-corpus/corpus-index.json \
  --plan /private/missing-candidates.json --output /private/classic-supplement
```

Plan entries contain `target` (`speciesId/formId`) and `page` (a specific
`https://archives.bulbagarden.net/wiki/File:...` URL). Batches are bounded to 200
candidates. HTTP requests identify TrainerOS. The source now responds, but its
[robots policy](https://archives.bulbagarden.net/robots.txt) disallows `/w/`,
including the API. This implementation uses allowed File pages, respects the
five-second crawl delay and checks policy for image URLs too. It rejects redirects
for manual review, uses bounded responses/timeouts and limited 429/503 backoff;
long cooldowns stop the job. A changed or unavailable policy stops new downloads.

Only pages tagged in the classic illustration category with one original-image
link are accepted. Static PNG validation is a second gate. That category can
include 3D renders, so visual review still chooses the intended illustration
style. Per-image source pages, original names/URLs, hashes and page bytes are
retained; attribution stays unknown until actually verified. Successful items
are checkpointed separately. Reruns verify cached images and source-page hashes,
reuse completed work and resume subsequent candidates without redownloading.

Import the supplement with `--supplement /private/classic-supplement` alongside
the original seed. Each image retains its own source; a fetch plan's proposed
target does not force a form association. Explicit `--review` and `--selection`
record reviewed identity and preferred candidate separately. Unknown suffixes,
duplicate form labels and punctuation identities (Unown !/? and Arceus ???) stay
distinct. Regional adjectives and descriptive label suffixes now have bounded
naming equivalents; there is still no implicit first/default-form fallback.

Actual run: 99 additional seed associations; ten new files for six missing Mega
forms fetched and individually inspected. Six drawn versions were selected;
four 3D versions remain as alternatives. Combined coverage is **1,214 / 1,579
forms**, with **365 unresolved**, across **995 / 1,025 species**. The corpus has
**1,868 entries / 1,867 unique originals**, zero invalid images and zero unresolved
exact-candidate conflicts after the six explicit selections.

Seventeen synthetic tests pass on Windows and Linux (12 importer + 5 downloader),
including missing-only selection, seed prerequisite, source/category rejection,
policy/backoff, cache corruption, interrupted-job resume, supplement provenance,
explicit candidate selection and escaped local review HTML. Actual download
reruns reused the first six completed items while obtaining four alternatives.

**#58 remains open:** inspect remaining seed identities/default appearances and
style/attribution, discover additional missing illustration candidates, complete
the raw corpus and classify genuine unavailable art honestly. Automated category
pagination/discovery is not implemented by this explicit-plan downloader. #60
has not started; no runtime UI, sprite provider or Flip installation changed.

## Species identity review checkpoint — 2026-09-19

The private corpus contains numbered illustration candidates for **all 1,025
reference species**. This is not exact-form coverage: ten species still have no
confirmed form association. Their candidate artwork remains available for
review rather than being assigned to the first form. In particular, the seed's
bare `Unown.png` depicts F, not A. Thirteen originals were individually inspected
and assigned explicit form IDs in private review records, including visible
flower/plumage colors, Maushold's four-member family and Unown F.

Scoped naming equivalents for Shellos/Gastrodon Sea, Basculin stripes, Eiscue
faces, Morpeko modes and Zacian/Zamazenta Hero labels add thirteen more exact
label associations. Bare species names still do not establish defaults. Eight
additional missing Mega illustrations were downloaded, individually inspected
and selected using the existing missing-only workflow. No sprite substitution,
recoloring or invented art was used.

Current private corpus: **1,876 entries / 1,875 unique originals**, **1,248 / 1,579
forms associated**, **331 unresolved**, **1,015 species with at least one exact
association**, and **1,025 species with candidate images**. Fourteen downloaded
illustrations now have explicit preferred-image selections. Invalid images and
unresolved exact-candidate conflicts remain zero.

`species-review.json` and the index's `speciesReview` distinguish species with
unresolved candidate identity from those with no candidate in the corpus. Missing
form records expose species candidate counts and exact candidate hashes; neither
is proof that a form has no available artwork elsewhere. A review hash absent
from the current inputs now fails instead of silently dropping a prior review;
the previous valid index is retained.

Twenty synthetic tests pass on Windows and Linux (15 importer + 5 downloader),
including scoped labels, candidate-versus-form coverage, stale review rejection
and retention of the previous index. All acquisition/mapping/review manifests
and artwork remain private. Remaining #58 identity, discovery, style/attribution
and maximum-coverage work is still open; #60 device bootstrap has not started.
