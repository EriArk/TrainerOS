#!/usr/bin/env python3
"""One-shot, offline illustration seed audit (#58), not a runtime art pack.

Requires Pillow. Import a folder or ZIP; extract 7z seeds with 7-Zip first.
Original bytes are preserved. No image or source manifest belongs in Git.
"""
import argparse
import hashlib
import html
import io
import json
import os
from pathlib import Path, PureWindowsPath
import re
import subprocess
import unicodedata
import zipfile

from PIL import Image

MAX_FILE = 64 * 1024 * 1024
MAX_TOTAL = 8 * 1024 * 1024 * 1024
MAX_FILES = 20000
EXTENSIONS = {'.png', '.jpg', '.jpeg', '.webp'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def atomic_json(path, value):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    os.replace(temp, path)


def norm(value):
    value = value.replace('♀', ' female ').replace('♂', ' male ')
    # Unown ! / ? and Arceus ??? are identities, not disposable punctuation.
    return re.sub(r'[^a-z0-9!?]', '', unicodedata.normalize('NFKD', value).encode('ascii', 'ignore').decode().lower())


def short_form_label(value, species_id):
    """Naming equivalents, never a default-form or shared-art fallback."""
    # These shorten explicit source labels; none turns a bare species name into
    # a presumed default form. Scope them so unrelated form names stay intact.
    if species_id in ('shellos', 'gastrodon'):
        value = value.replace(' Sea', '')
    if species_id == 'basculin':
        value = value.replace('-Striped', '')
    if species_id == 'eiscue':
        value = value.replace(' Face', '')
    if species_id == 'morpeko':
        value = {'Full Belly Mode': 'Full', 'Hangry Mode': 'Hangry'}.get(value, value)
    if species_id in ('zacian', 'zamazenta') and value == 'Hero of Many Battles':
        value = 'Hero'
    value = re.sub(r'\b(Alolan|Galarian|Hisuian|Paldean)\b',
                   lambda m: {'Alolan': 'Alola', 'Galarian': 'Galar',
                              'Hisuian': 'Hisui', 'Paldean': 'Paldea'}[m[0]], value)
    value = re.sub(r'\b(Forme?|Pattern|Trim|Cloak|Plumage|Style|Breed)\b', '', value)
    if species_id == 'pikachu':
        value = re.sub(r'\bCap\b', '', value)
    return norm(value)


def inventory(reference):
    targets = {}
    species = {}
    for entry in reference['entries']:
        species[entry['number']] = entry
        for form in entry['forms']:
            key = entry['id'] + '/' + form['id']
            if key in targets:
                raise ValueError('Duplicate reference target: ' + key)
            targets[key] = {'speciesId': entry['id'], 'formId': form['id'],
                            'number': entry['number'], 'name': entry['name'], 'form': form['name']}
    return species, targets


def candidates(filename, species):
    """Strict label matches only; no first-form or number-only inference."""
    stem = Path(filename.replace('\\', '/')).stem
    match = re.match(r'^(\d{1,4})[ _-]*(.*)$', stem)
    if not match or int(match[1]) not in species:
        return [], None
    entry = species[int(match[1])]
    label = norm(match[2])
    name = norm(entry['name'])
    names = {name}
    if entry['id'] in ('nidoran-f', 'nidoran-m'):
        names.add('nidoran')  # The explicit dex number disambiguates sex.
    matched = []
    for form in entry['forms']:
        form_name = norm(form['name'])
        aliases = set()
        if form['name'] == 'Standard':
            aliases.update(names)
        else:
            short = short_form_label(form['name'], entry['id'])
            for n in names:
                aliases.update((n + short, short + n))
                if n in short:
                    aliases.add(short)
                    aliases.add(n + short.replace(n, ''))
        if label in aliases:
            matched.append(entry['id'] + '/' + form['id'])
    return matched, entry['id']


def guard_output(output, seed):
    output = output.resolve()
    seed = seed.resolve()
    if output == seed or output.is_relative_to(seed) or seed.is_relative_to(output):
        raise ValueError('Seed and corpus must be separate, non-nested paths')
    # Refuse every non-ignored location in any enclosing Git worktree, including
    # tracked files that match an ignore pattern. No override to bypass the gate.
    for parent in (output, *output.parents):
        if (parent / '.git').exists():
            relative = output.relative_to(parent).as_posix()
            tracked = subprocess.check_output(['git', '-C', str(parent), 'ls-files', '--', relative])
            ignored = subprocess.run(['git', '-C', str(parent), 'check-ignore', '-q', '--', relative + '/']).returncode == 0
            if tracked or not ignored:
                raise ValueError('Corpus output must be outside Git or in an ignored private directory')
            break
    if output.exists():
        for path in output.rglob('*'):
            if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
                raise ValueError('Links are not allowed inside corpus output')
    return output


def safe_name(name):
    p = PureWindowsPath(name)
    if p.drive or p.root or '..' in p.parts or ':' in name:
        raise ValueError('Unsafe archive member: ' + name)


def write_review(output, targets, records, covered):
    """Private, read-only browser contact sheet; no network or Studio contract."""
    groups = {}
    for key, target in targets.items():
        groups.setdefault(target['speciesId'], []).append((key, target))
    images = {}
    for record in records:
        if record['speciesId']:
            images.setdefault(record['speciesId'], []).append(record)
    esc = html.escape
    parts = ['<!doctype html><meta charset="utf-8"><title>Classic art review</title>',
             '<style>body{font:16px system-ui;background:#eee;color:#222;margin:24px}'
             'section{border-bottom:2px solid #999;padding:16px 0}.grid{display:flex;flex-wrap:wrap;gap:12px}'
             'figure{width:230px;margin:0;padding:10px;background:white;overflow-wrap:anywhere}'
             'img{width:230px;height:190px;object-fit:contain}code{font-size:11px}'
             '.missing{color:#8a2900}</style><h1>Private illustration review</h1>'
             '<p>Provisional mappings only. Unresolved does not mean absent. '
             'No uploads or network requests. Copy a full hash for explicit review/selection.</p>']
    for species_id, forms in groups.items():
        if all(key in covered for key, _ in forms):
            continue
        parts.append('<section><h2>' + esc(forms[0][1]['name']) + '</h2><ul>')
        for key, target in forms:
            parts.append('<li class="' + ('ok' if key in covered else 'missing') + '">' +
                         esc(key + ': ' + target['form']) + (' — associated' if key in covered else ' — unresolved') + '</li>')
        parts.append('</ul><div class="grid">')
        for record in images.get(species_id, []):
            parts.append('<figure><img loading="lazy" src="' + esc(record['path'], quote=True) +
                         '" alt=""><figcaption>' + esc(record['sourceName']) + '<br><code>' +
                         record['sha256'] + '</code><br>' + esc(', '.join(record['candidates']) or 'No exact form') +
                         '</figcaption></figure>')
        parts.append('</div></section>')
    temp = output / 'review.tmp'
    temp.write_text('\n'.join(parts), encoding='utf8')
    os.replace(temp, output / 'review.html')


def seed_files(seed):
    """Stream bounded bytes; ZIP members never get extracted by their names."""
    total = count = 0
    if seed.is_dir():
        paths = sorted(seed.rglob('*'))
        for path in paths:
            if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
                raise ValueError('Seed contains a link: ' + str(path))
        items = [(p.relative_to(seed).as_posix(), p.stat().st_size, p) for p in paths
                 if p.is_file() and p.suffix.lower() in EXTENSIONS]
        archive = None
    else:
        archive = zipfile.ZipFile(seed)
        items = []
        for info in archive.infolist():
            safe_name(info.filename)
            if (info.external_attr >> 16) & 0o170000 == 0o120000:
                archive.close()
                raise ValueError('ZIP symlink is not supported')
            if not info.is_dir() and Path(info.filename).suffix.lower() in EXTENSIONS:
                items.append((info.filename, info.file_size, info))
    try:
        for name, size, source in items:
            count += 1
            total += size
            if size > MAX_FILE or total > MAX_TOTAL or count > MAX_FILES:
                raise ValueError('Seed exceeds import limits')
            with (archive.open(source) if archive else source.open('rb')) as stream:
                data = stream.read(MAX_FILE + 1)
            if len(data) > MAX_FILE:
                raise ValueError('Image exceeds byte limit')
            yield name, data
    finally:
        if archive:
            archive.close()


def run(seed, output, reference_path, source, review_path=None, selection_path=None, supplement=None):
    seed = Path(seed).resolve()
    output = guard_output(Path(output), seed)
    supplement_records = {}
    if supplement:
        supplement = Path(supplement).resolve()
        guard_output(output, supplement)
        manifest = json.loads((supplement / 'acquisition.json').read_text(encoding='utf8'))
        for record in manifest.values():
            safe_name(record['file'])
            if not (supplement / record['file']).is_file():
                raise ValueError('Supplement manifest references a missing file')
            supplement_records.setdefault(record['file'], []).append(record)
    reference_bytes = Path(reference_path).read_bytes()
    species, targets = inventory(json.loads(reference_bytes))
    review = json.loads(Path(review_path).read_text(encoding='utf8')) if review_path else {}
    selections = json.loads(Path(selection_path).read_text(encoding='utf8')) if selection_path else {}
    for sha, target in review.items():
        if not re.fullmatch('[a-f0-9]{64}', sha) or target not in targets:
            raise ValueError('Review must map image SHA-256 to an existing species/form target')
    for target, sha in selections.items():
        if target not in targets or not isinstance(sha, str) or not re.fullmatch('[a-f0-9]{64}', sha):
            raise ValueError('Selection must map an existing target to an image SHA-256')
    output.mkdir(parents=True, exist_ok=True)
    originals = output / 'originals'
    originals.mkdir(exist_ok=True)
    records, errors = [], []
    def sources():
        for name, data in seed_files(seed):
            yield name, data, source, None
        if supplement:
            for filename, data in seed_files(supplement):
                if filename not in supplement_records:
                    raise ValueError('Supplement image lacks provenance: ' + filename)
                for record in supplement_records[filename]:
                    if digest(data) != record['sha256']:
                        raise ValueError('Supplement hash mismatch: ' + filename)
                    yield record['sourceName'], data, record['source'], record

    for name, data, image_source, supplement_record in sources():
        sha = digest(data)
        try:
            with Image.open(io.BytesIO(data)) as im:
                width, height = im.size
                fmt = im.format
                alpha = 'A' in im.getbands() or 'transparency' in im.info
                animated = getattr(im, 'n_frames', 1) != 1
                im.verify()
            if animated:
                raise ValueError('Animated image excluded from classic illustration corpus')
            if fmt not in ('PNG', 'JPEG', 'WEBP'):
                raise ValueError('Unsupported decoded image format')
        except (OSError, ValueError, Image.DecompressionBombError) as exc:
            errors.append({'sourceName': name, 'sha256': sha, 'error': str(exc)})
            continue
        suffix = {'PNG': '.png', 'JPEG': '.jpg', 'WEBP': '.webp'}[fmt]
        path = originals / (sha + suffix)
        if path.exists():
            if digest(path.read_bytes()) != sha:
                raise ValueError('Existing corpus original failed hash verification: ' + str(path))
        else:
            temp = path.with_suffix('.tmp')
            temp.write_bytes(data)
            os.replace(temp, path)
        matches, species_id = candidates(name, species)
        confidence = 'exact-label-provisional' if len(matches) == 1 else 'review-required'
        if sha in review:
            matches = [review[sha]]
            species_id = targets[matches[0]]['speciesId']
            confidence = 'reviewed'
        records.append({'sourceName': name, 'path': path.relative_to(output).as_posix(),
                        'sha256': sha, 'source': image_source, 'creator': None,
                        'proposedTarget': supplement_record['target'] if supplement_record else None,
                        'artworkClass': supplement_record.get('artworkClass', 'classic-illustration') if supplement_record else 'seed-illustration',
                        'notes': ('Source category identifies an artwork family, not verified authorship or a redistribution license.'
                                  if supplement_record else 'Seed attribution is not per-image authorship or a redistribution license.'),
                        'width': width, 'height': height, 'alpha': alpha,
                        'speciesId': species_id, 'candidates': matches, 'confidence': confidence})
    if not records:
        raise ValueError('No valid static images in seed; previous reports retained')
    image_hashes = {record['sha256'] for record in records}
    if set(review) - image_hashes:
        raise ValueError('Review references images absent from this import; previous reports retained')
    species_images = {}
    for record in records:
        if record['speciesId']:
            species_images.setdefault(record['speciesId'], set()).add(record['sha256'])
    by_target = {key: {} for key in targets}
    for record in records:
        if len(record['candidates']) == 1:
            by_target[record['candidates'][0]][record['sha256']] = record
    covered = {key: next(iter(values)) for key, values in by_target.items() if len(values) == 1}
    for target, sha in selections.items():
        if sha not in by_target[target]:
            raise ValueError('Selected hash is not an unambiguous candidate for ' + target)
        covered[target] = sha
    conflicts = [{'target': key, 'hashes': sorted(values)} for key, values in by_target.items()
                 if len(values) > 1 and key not in selections]
    missing = [dict(target=key, **value, reason='multiple-images' if by_target[key] else 'no-exact-match',
                    speciesCandidateCount=len(species_images.get(value['speciesId'], set())),
                    exactCandidateHashes=sorted(by_target[key]))
               for key, value in targets.items() if key not in covered]
    summary = {'images': len(records), 'uniqueImages': len({r['sha256'] for r in records}),
               'species': len(species), 'forms': len(targets), 'coveredForms': len(covered),
               'unresolvedForms': len(missing), 'conflictingTargets': len(conflicts), 'invalidImages': len(errors)}
    summary['coveredSpecies'] = len({targets[key]['speciesId'] for key in covered})
    summary['speciesWithCandidates'] = len(species_images)
    summary['reviewedSelections'] = len(selections)
    # Index is written last and contains the entire authoritative report snapshot.
    # Separate convenience reports can be regenerated after interruption.
    ambiguous = {'conflicts': conflicts, 'unmapped': [r for r in records if len(r['candidates']) != 1]}
    covered_species = {targets[key]['speciesId'] for key in covered}
    species_review = [{'speciesId': entry['id'], 'name': entry['name'], 'number': number,
                       'candidateHashes': sorted(species_images.get(entry['id'], set())),
                       'status': 'candidate-identity-unresolved' if entry['id'] in species_images else 'no-candidate-in-corpus'}
                      for number, entry in species.items() if entry['id'] not in covered_species]
    provenance = {'source': source, 'referenceSha256': digest(reference_bytes),
                  'reviewSha256': digest(Path(review_path).read_bytes()) if review_path else None,
                  'selectionSha256': digest(Path(selection_path).read_bytes()) if selection_path else None,
                  'supplementManifestSha256': digest((supplement / 'acquisition.json').read_bytes()) if supplement else None,
                  'purpose': 'private classic illustrations; not sprites, not a final pack',
                  'networkAcquisition': 'not performed by this offline import stage'}
    for name, value in [('missing', missing), ('ambiguous', ambiguous), ('provenance', provenance),
                        ('species-review', species_review)]:
        atomic_json(output / (name + '.json'), value)
    write_review(output, targets, records, covered)
    atomic_json(output / 'corpus-index.json', {'version': 1, 'provisional': True, 'summary': summary,
                'provenance': provenance, 'images': records, 'associations': covered,
                'missing': missing, 'ambiguous': ambiguous, 'errors': errors,
                'speciesReview': species_review,
                'reviewedSelections': selections})
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--reference', type=Path, default=Path(__file__).resolve().parents[1] / 'data/pokedex.json')
    parser.add_argument('--source', required=True, help='Original source URL/reference, retained with every image')
    parser.add_argument('--review', type=Path, help='Private JSON object mapping SHA-256 to speciesId/formId')
    parser.add_argument('--selection', type=Path, help='Private JSON object selecting target -> candidate SHA-256')
    parser.add_argument('--supplement', type=Path, help='Private output of fetch-classic-art.py')
    args = parser.parse_args()
    print(json.dumps(run(args.seed, args.output, args.reference, args.source, args.review, args.selection, args.supplement), indent=2))


if __name__ == '__main__':
    main()
