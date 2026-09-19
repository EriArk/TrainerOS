#!/usr/bin/env python3
"""Audit source category metadata only for species with unresolved illustrations.

Does not fetch images or assert that an unfound illustration does not exist.
Use the resulting candidate pages in an explicit fetch-classic-art.py plan.
"""
import argparse
from html.parser import HTMLParser
import importlib.util
import json
from pathlib import Path
import re
import urllib.parse

spec = importlib.util.spec_from_file_location('fetcher', Path(__file__).with_name('fetch-classic-art.py'))
f = importlib.util.module_from_spec(spec)
spec.loader.exec_module(f)


class Category(HTMLParser):
    def __init__(self, url):
        super().__init__()
        self.url = url
        self.files = {}
        self.has_more = False
        self.next_cursor = None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag != 'a':
            return
        url = urllib.parse.urldefrag(urllib.parse.urljoin(self.url, a.get('href', '')))[0]
        decoded = urllib.parse.unquote(urllib.parse.urlsplit(url).path)
        if f.allowed_url(url) and decoded.startswith('/wiki/File:'):
            name = decoded.split('/wiki/File:', 1)[1].replace('_', ' ')
            self.files[url] = name
        if f.allowed_url(url) and 'filefrom=' in url:
            self.has_more = True
            self.next_cursor = urllib.parse.parse_qs(urllib.parse.urlsplit(url).query).get('filefrom', [None])[0]


def discover(index_path, output, client_factory=f.Client):
    index_path = Path(index_path)
    index = json.loads(index_path.read_text(encoding='utf8'))
    if not index.get('images') or not index.get('provenance', {}).get('referenceSha256'):
        raise ValueError('Import the seed before source discovery')
    output = f.art.guard_output(Path(output), index_path.parent)
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / 'discovery.json'
    report = json.loads(report_path.read_text(encoding='utf8')) if report_path.exists() else {}
    groups = {}
    for row in index['missing']:
        groups.setdefault(row['speciesId'], []).append(row)
    client = None
    for species, targets in groups.items():
        cached = None
        if species in report and report[species].get('state') == 'checked':
            old = report[species]
            f.art.safe_name(old['cacheFile'])
            if f.art.digest((output / old['cacheFile']).read_bytes()) != old['pageSha256']:
                raise ValueError('Discovery metadata cache failed verification')
            cached = (output / old['cacheFile']).read_bytes()
            if old.get('numberedRangeChecked'):
                for page in old.get('pages', []):
                    f.art.safe_name(page['file'])
                    if f.art.digest((output / page['file']).read_bytes()) != page['sha256']:
                        raise ValueError('Discovery page cache failed verification')
                continue
        name = targets[0]['name']
        url = f.ORIGIN + '/wiki/Category:' + urllib.parse.quote(name.replace(' ', '_'), safe='')
        try:
            if cached is None and client is None:
                client = client_factory()
                (output / 'robots.txt').write_bytes(client.policy)
            data = cached if cached is not None else client.get(url, 4 * 1024 * 1024)
            parser = Category(url)
            parser.feed(data.decode('utf8'))
            first_has_more = parser.has_more
            pages = []
            cursors = set()
            while parser.next_cursor and re.match(r'^\d', parser.next_cursor):
                cursor = parser.next_cursor
                if cursor in cursors or len(cursors) >= 10:
                    raise ValueError('Repeated or excessive numbered category pagination')
                cursors.add(cursor)
                next_url = url + '?' + urllib.parse.urlencode({'filefrom': cursor})
                if client is None:
                    client = client_factory()
                    (output / 'robots.txt').write_bytes(client.policy)
                more = client.get(next_url, 4 * 1024 * 1024)
                page_file = species + '-page-' + str(len(cursors)) + '.html'
                f.art.safe_name(page_file)
                (output / page_file).write_bytes(more)
                pages.append({'url': next_url, 'file': page_file, 'sha256': f.art.digest(more)})
                next_parser = Category(next_url)
                next_parser.feed(more.decode('utf8'))
                parser.files.update(next_parser.files)
                parser.next_cursor = next_parser.next_cursor
            candidates = []
            for page, filename in parser.files.items():
                match = re.match(r'^(\d{1,4})(?=\D)', filename)
                if match and int(match[1]) == targets[0]['number']:
                    candidates.append({'page': page, 'sourceName': filename})
            cache = species + '.html'
            # Species is a stable reference slug, still validate before writes.
            f.art.safe_name(cache)
            (output / cache).write_bytes(data)
            report[species] = {'state': 'checked', 'source': url, 'pageSha256': f.art.digest(data),
                               'cacheFile': cache, 'firstCategoryPageOnly': not pages,
                               'hasMoreCategoryFiles': first_has_more, 'candidates': candidates,
                               'numberedRangeChecked': True, 'pages': pages,
                               'targets': [r['target'] for r in targets]}
        except Exception as exc:
            report[species] = {'state': 'blocked', 'source': url, 'error': str(exc),
                               'targets': [r['target'] for r in targets]}
            f.art.atomic_json(report_path, report)
            # Policy/authentication/rate failures must not cause a request storm.
            raise
        f.art.atomic_json(report_path, report)
        print(species + ': ' + str(len(candidates)) + ' numbered candidate pages', flush=True)
    return {'speciesChecked': len(groups), 'cachedRecords': len(report)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--index', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    print(json.dumps(discover(args.index, args.output), indent=2))


if __name__ == '__main__':
    main()
