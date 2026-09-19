#!/usr/bin/env python3
"""Fetch explicitly reviewed missing illustration candidates; no API/site crawl.

Plan JSON: [{"target": "species/form", "page": "https://.../wiki/File:..."}].
Input corpus must have been imported first. Output is a private supplement.
"""
import argparse
import hashlib
from html.parser import HTMLParser
import importlib.util
import io
import json
from pathlib import Path
import time
import urllib.error
import urllib.parse
import urllib.request
import urllib.robotparser

from PIL import Image

spec = importlib.util.spec_from_file_location('classic', Path(__file__).with_name('import-classic-art.py'))
art = importlib.util.module_from_spec(spec)
spec.loader.exec_module(art)
ORIGIN = 'https://archives.bulbagarden.net'
UA = 'TrainerOS-art-bootstrap/0.1 (https://github.com/EriArk/TrainerOS)'


class FilePage(HTMLParser):
    def __init__(self):
        super().__init__()
        self.originals = set()
        self.illustration = False

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        href = urllib.parse.urljoin(ORIGIN, a.get('href', ''))
        if tag == 'a' and 'internal' in a.get('class', '').split() and '/media/upload/' in href:
            self.originals.add(href)
        if tag == 'a' and 'Category:Ken_Sugimori_Pokémon_artwork' in urllib.parse.unquote(href):
            self.illustration = True


def allowed_url(url):
    p = urllib.parse.urlsplit(url)
    return p.scheme == 'https' and p.netloc == 'archives.bulbagarden.net' and not p.fragment


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValueError('Review redirect destination before fetching: ' + newurl)


class Client:
    def __init__(self):
        self.opener = urllib.request.build_opener(NoRedirect())
        self.last = 0
        self.delay = 5
        self.robot = urllib.robotparser.RobotFileParser()
        self.policy = self.get(ORIGIN + '/robots.txt', 128 * 1024, policy=False)
        self.robot.parse(self.policy.decode('utf8').splitlines())
        self.delay = max(5, self.robot.crawl_delay(UA) or self.robot.crawl_delay('*') or 0)

    def get(self, url, limit, policy=True):
        if not allowed_url(url) or (policy and not self.robot.can_fetch(UA, url)):
            raise ValueError('Source policy or host rejects URL: ' + url)
        for attempt in range(3):
            time.sleep(max(0, self.last + self.delay - time.monotonic()))
            self.last = time.monotonic()
            try:
                with self.opener.open(urllib.request.Request(url, headers={'User-Agent': UA}), timeout=30) as response:
                    data = response.read(limit + 1)
                    if len(data) > limit:
                        raise ValueError('Source response exceeds limit')
                    return data
            except urllib.error.HTTPError as exc:
                if exc.code not in (429, 503) or attempt == 2:
                    raise
                retry = exc.headers.get('Retry-After', '')
                if retry and (not retry.isdigit() or int(retry) > 30):
                    raise ValueError('Source requests longer cooldown; rerun later') from exc
                self.delay = max(self.delay, int(retry or 0), 5 * 2 ** (attempt + 1))


def fetch(index_path, plan_path, output, client_factory=Client):
    index_path, output = Path(index_path), Path(output)
    index = json.loads(index_path.read_text(encoding='utf8'))
    if not index.get('images') or not index.get('provenance', {}).get('referenceSha256'):
        raise ValueError('Import the seed before fetching missing artwork')
    missing = {r['target'] for r in index['missing']}
    known = missing | set(index['associations'])
    plan = json.loads(Path(plan_path).read_text(encoding='utf8'))
    if len(plan) > 200:
        raise ValueError('Review batches are limited to 200 candidates')
    output = art.guard_output(output, index_path.parent)
    tasks = []
    skipped = 0
    for row in plan:
        if row['target'] not in known:
            raise ValueError('Plan target is not in the imported reference inventory')
        if row['target'] not in missing:
            skipped += 1
            continue
        url = row['page']
        if not allowed_url(url) or not urllib.parse.unquote(urllib.parse.urlsplit(url).path).startswith('/wiki/File:'):
            raise ValueError('Plan requires a same-origin File page')
        tasks.append(row)
    output.mkdir(parents=True, exist_ok=True)
    manifest_path = output / 'acquisition.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf8')) if manifest_path.exists() else {}
    client = None
    acquired = reused = 0
    for row in tasks:
        url = row['page']
        key = hashlib.sha256(url.encode()).hexdigest()
        if key in manifest:
            old = manifest[key]
            art.safe_name(old['file'])
            path = output / old['file']
            if not path.is_file() or art.digest(path.read_bytes()) != old['sha256']:
                raise ValueError('Cached supplement image failed verification')
            art.safe_name(old['pageFile'])
            page_path = output / old['pageFile']
            if not page_path.is_file() or art.digest(page_path.read_bytes()) != old['pageSha256']:
                raise ValueError('Cached source metadata failed verification')
            if old['target'] != row['target']:
                raise ValueError('Cached page has a different proposed target; review manually')
            reused += 1
            continue
        if client is None:
            client = client_factory()
            (output / 'robots.txt').write_bytes(client.policy)
        page = client.get(url, 4 * 1024 * 1024)
        parser = FilePage()
        parser.feed(page.decode('utf8'))
        if not parser.illustration or len(parser.originals) != 1:
            raise ValueError('Not an unambiguous classic illustration page: ' + url)
        image_url = next(iter(parser.originals))
        data = client.get(image_url, art.MAX_FILE)
        with Image.open(io.BytesIO(data)) as image:
            if image.format != 'PNG' or getattr(image, 'n_frames', 1) != 1:
                raise ValueError('Expected static PNG illustration')
            width, height = image.size
            image.verify()
        name = urllib.parse.unquote(urllib.parse.urlsplit(image_url).path.rsplit('/', 1)[-1])
        sha = art.digest(data)
        filename = sha + '.png'
        temp = output / (sha + '.tmp')
        temp.write_bytes(data)
        temp.replace(output / filename)
        (output / (key + '.html')).write_bytes(page)
        manifest[key] = {'file': filename, 'sha256': sha, 'sourceName': name,
                         'source': url, 'originalUrl': image_url, 'target': row['target'],
                         'creator': None, 'width': width, 'height': height,
                         'pageSha256': art.digest(page), 'pageFile': key + '.html',
                         'notes': 'Candidate only; page metadata retained for authorship and visual review.'}
        art.atomic_json(manifest_path, manifest)
        acquired += 1
    return {'downloaded': acquired, 'reused': reused, 'coveredTargetsSkipped': skipped}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--index', required=True, type=Path)
    p.add_argument('--plan', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    print(json.dumps(fetch(args.index, args.plan, args.output), indent=2))


if __name__ == '__main__':
    main()
