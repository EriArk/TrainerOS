#!/usr/bin/env python3
"""Report bounded source discovery without fabricating exact-form artwork.

Every missing form must have a verified source-discovery record. Missing remains
missing in the corpus; this report records what was checked, not nonexistence.
"""
import argparse
import importlib.util
import json
from pathlib import Path

spec = importlib.util.spec_from_file_location('classic', Path(__file__).with_name('import-classic-art.py'))
art = importlib.util.module_from_spec(spec)
spec.loader.exec_module(art)


def audit(index_path, discovery_dir, output):
    index_path, discovery_dir = Path(index_path), Path(discovery_dir)
    index = json.loads(index_path.read_text(encoding='utf8'))
    discovery = json.loads((discovery_dir / 'discovery.json').read_text(encoding='utf8'))
    output = art.guard_output(Path(output), index_path.parent)
    art.guard_output(output, discovery_dir)
    unresolved = []
    for row in index['missing']:
        source = discovery.get(row['speciesId'])
        if not source or source.get('state') != 'checked' or not source.get('numberedRangeChecked'):
            raise ValueError('Source review incomplete for ' + row['speciesId'])
        art.safe_name(source['cacheFile'])
        if art.digest((discovery_dir / source['cacheFile']).read_bytes()) != source['pageSha256']:
            raise ValueError('Source metadata failed verification')
        for page in source.get('pages', []):
            art.safe_name(page['file'])
            if art.digest((discovery_dir / page['file']).read_bytes()) != page['sha256']:
                raise ValueError('Source pagination metadata failed verification')
        unresolved.append({**row, 'disposition': 'no-confirmed-exact-classic-illustration',
                           'source': source['source'], 'pageSha256': source['pageSha256'],
                           'scope': 'numbered candidates in species category, numeric continuation pages included',
                           'additionalPages': source.get('pages', []),
                           'categoryHasMoreFiles': source['hasMoreCategoryFiles'],
                           'candidatePages': source['candidates'],
                           'runtimeInstruction': 'Keep exact form unavailable; do not silently assign species art.'})
    report = {'provisional': True, 'categoryDiscoveryCompleteForCurrentMissingTargets': True,
              'corpusIndexSha256': art.digest(index_path.read_bytes()),
              'exactAssociations': index['associations'], 'unresolved': unresolved,
              'summary': index['summary'],
              'limitations': ['Bounded configured-source review, not proof that artwork does not exist.',
                              'Category discovery does not certify candidate acquisition or visual review.',
                              'No shared-appearance fallback or final runtime contract is defined here.',
                              'Unresolved exact forms remain explicit; no recoloring or sprite substitution.']}
    output.mkdir(parents=True, exist_ok=True)
    art.atomic_json(output / 'source-review.json', report)
    return {'associatedForms': len(index['associations']), 'reviewedUnresolvedForms': len(unresolved)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--index', required=True, type=Path)
    p.add_argument('--discovery', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    print(json.dumps(audit(a.index, a.discovery, a.output), indent=2))


if __name__ == '__main__':
    main()
