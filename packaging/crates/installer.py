#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-owned package transactions. No commands from catalogue metadata are run."""
import contextlib
try:
    import fcntl
except ImportError:  # Maintainer-side schema/signing tools also run on Windows.
    fcntl = None
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
import tarfile
import urllib.request
from urllib.parse import urlsplit

ID = re.compile(r"[a-z0-9][a-z0-9._-]{0,79}\Z")


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try: os.fsync(fd)
    finally: os.close(fd)


def durable_unlink(path):
    path.unlink(missing_ok=True)
    if path.parent.exists(): sync_directory(path.parent)


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.pending')
    with temporary.open('w') as stream:
        json.dump(value, stream)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)
    sync_directory(path.parent)


def safe_path(name):
    p = PurePosixPath(name)
    if not name or '\\' in name or p.is_absolute() or '..' in p.parts or ':' in name:
        raise ValueError('Unsafe package path')
    return p


def read_registry(path):
    if path.stat().st_size > 12 * 1024 * 1024:
        raise ValueError('Package registry too large')
    result = json.loads(path.read_text())
    if 'signed' in result:
        from catalogue import verified
        result = dict(schema=1, packages=verified(result)['packages'])
    if result.get('schema') != 1: raise ValueError('Unsupported package registry')
    records = result['packages']
    validate_records(records)
    return [r for r in records if not r.get('withdrawn',False)]


def validate_records(records):
    if not isinstance(records,list) or len(records)>4000: raise ValueError('Too many package records')
    seen = set()
    for r in records:
        if set(r)-{'id','version','title','kind','architecture','sha256','size','unpackedSize','entry','icon','url',
                   'repository','revision','license','picture','compatibility','compatibilityNote','channel','notes','withdrawn','releasedOn'}:
            raise ValueError('Unsupported package fields')
        if not ID.fullmatch(r['id']) or not ID.fullmatch(r['version']):
            raise ValueError('Invalid package identity')
        key = (r['id'], r['version'], r['architecture'])
        if key in seen: raise ValueError('Duplicate package version and architecture')
        seen.add(key)
        if r['kind'] not in ('game', 'app') or r['architecture'] not in ('aarch64','x86_64'):
            raise ValueError('Unsupported package type')
        if not isinstance(r.get('title'),str) or not 0<len(r['title'])<=160:
            raise ValueError('Invalid package title')
        if r.get('compatibility','untested') not in ('supported','untested','incompatible'):
            raise ValueError('Invalid compatibility status')
        picture=r.get('picture','')
        if picture and (not re.fullmatch(r'qrc:/(?:crates|series)/[a-zA-Z0-9_./-]+',picture) or '..' in picture):
            raise ValueError('Unapproved package illustration')
        if not re.fullmatch('[0-9a-f]{64}', r['sha256']) or not 0 < r['size'] < 1024**3:
            raise ValueError('Invalid package integrity record')
        u = urlsplit(r['url'])
        if u.scheme != 'https' or u.hostname != 'github.com' or u.username or u.password or u.port or u.query or u.fragment or not re.fullmatch(r'/EriArk/(?:TrainerOS|crates)/releases/download/[A-Za-z0-9._-]+/[A-Za-z0-9._-]+',u.path):
            raise ValueError('Unapproved package source')
        safe_path(r['entry'])
        safe_path(r.get('icon','icon.svg'))
        if not 0 < r['unpackedSize'] < 4*1024**3: raise ValueError('Invalid unpacked size')


def select_release(records, project, version, architecture):
    record = next((r for r in records if r['id'] == project and r['version'] == version
                   and r['architecture'] == architecture and not r.get('withdrawn', False)), None)
    if record is None: raise ValueError('No package for this version and device architecture')
    return record


class Store:
    def __init__(self, root):
        if fcntl is None: raise RuntimeError('Package transactions require a Linux host')
        self.root = Path(root).absolute()
        self.root.mkdir(parents=True, exist_ok=True)
        if self.root.is_symlink(): raise ValueError('Package root cannot be a link')
        for name in ('objects','active','locks','data','staging','transactions'):
            p = self.root / name
            if p.is_symlink(): raise ValueError('Package storage cannot be a link')
            p.mkdir(exist_ok=True)
        sync_directory(self.root)

    @contextlib.contextmanager
    def lock(self, project, exclusive=True):
        if not ID.fullmatch(project): raise ValueError('Invalid project')
        with (self.root/'locks'/project).open('a') as stream:
            fcntl.flock(stream, (fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH) | fcntl.LOCK_NB)
            yield stream

    def receipt(self, project):
        if not ID.fullmatch(project): raise ValueError('Invalid project')
        p = self.root/'active'/(project+'.json')
        return json.loads(p.read_text()) if p.exists() else None

    def installed(self):
        return {p.stem: json.loads(p.read_text()) for p in (self.root/'active').glob('*.json')}

    def recover(self):
        # Never clean another worker's staging or a launcher's leased image.
        busy = False
        for path in sorted((self.root/'transactions').glob('*.json')):
            try:
                with self.lock(path.stem): self._recover_locked(path.stem)
            except BlockingIOError: busy = True
        return not busy

    def _recover_locked(self, project):
        journal = self.root/'transactions'/(project+'.json')
        if not journal.exists(): return
        transaction = json.loads(journal.read_text())
        if transaction.get('operation') not in ('install','remove'):
            raise ValueError('Invalid package transaction')
        receipt = self.receipt(project)
        # The durable active receipt is the commit point for both operations.
        # A pre-commit interruption keeps the old image; a post-commit one
        # completes the derived desktop/cleanup work, even while offline.
        if receipt:
            if receipt.get('id') != project: raise ValueError('Invalid package receipt')
            self.desktop(receipt)
        else:
            durable_unlink(self.desktop_path(project))
        durable_unlink(self.desktop_path(project).with_suffix('.pending'))
        stage = self.root/'staging'/('unpack-'+project)
        if stage.is_symlink(): raise ValueError('Unsafe package staging')
        if stage.exists(): shutil.rmtree(stage)
        sync_directory(stage.parent)
        if transaction['operation'] == 'remove' and receipt is None:
            for p in (self.root/'objects').glob(project+'-*'):
                if re.fullmatch(re.escape(project)+'-[0-9a-f]{64}',p.name) and p.is_dir() and not p.is_symlink():
                    shutil.rmtree(p)
        elif transaction.get('newObject'):
            name = transaction['newObject']
            if not re.fullmatch(re.escape(project)+'-[0-9a-f]{64}',name):
                raise ValueError('Invalid transaction object')
            if not receipt or name not in (receipt.get('object'),receipt.get('previousObject')):
                p = self.root/'objects'/name
                if p.is_symlink(): raise ValueError('Unsafe installed object')
                if p.exists(): shutil.rmtree(p)
        sync_directory(self.root/'objects')
        durable_unlink((self.root/'active'/(project+'.json')).with_suffix('.pending'))
        durable_unlink(journal)

    def activate(self, archive, record, emit=lambda *a: None, cancelled=lambda: False):
        with self.lock(record['id']):
            self._recover_locked(record['id'])
            return self._activate_locked(archive, record, emit, cancelled)

    def _activate_locked(self, archive, record, emit, cancelled):
        if os.uname().machine != record['architecture']: raise ValueError('Package architecture cannot run here')
        emit('Verifying')
        with Path(archive).open('rb') as stream:
            if hashlib.file_digest(stream,'sha256').hexdigest() != record['sha256']:
                raise ValueError('Package checksum does not match')
        if Path(archive).stat().st_size != record['size']: raise ValueError('Package size does not match')
        project = record['id']
        object_name = project+'-'+record['sha256']
        destination = self.root/'objects'/object_name
        if destination.is_symlink(): raise ValueError('Unsafe installed object')
        journal = self.root/'transactions'/(project+'.json')
        atomic_json(journal,dict(operation='install',newObject=object_name if not destination.exists() else None))
        stage = self.root/'staging'/('unpack-'+project)
        try:
            stage.mkdir()
            with tarfile.open(archive,'r|gz') as tar:
                names = set()
                total = 0
                for count,m in enumerate(tar):
                    total += m.size
                    if count >= 20000 or total > record['unpackedSize']:
                        raise ValueError('Package exceeds declared unpacked size')
                    name = str(safe_path(m.name))
                    if name in names or not (m.isfile() or m.isdir()):
                        raise ValueError('Package contains duplicate paths, links or special files')
                    names.add(name)
                    if cancelled(): raise InterruptedError('Cancelled')
                    target = stage/name
                    if m.isdir(): target.mkdir(parents=True,exist_ok=True)
                    else:
                        target.parent.mkdir(parents=True,exist_ok=True)
                        with tar.extractfile(m) as src, target.open('xb') as dst:
                            shutil.copyfileobj(src,dst)
                            dst.flush()
                            os.fchmod(dst.fileno(),0o755 if m.mode & 0o111 else 0o644)
                            os.fsync(dst.fileno())
            if not (stage/record['entry']).is_file() or not (stage/record['entry']).stat().st_mode & 0o111:
                raise ValueError('Package launch entry missing')
            emit('Installing')
            if cancelled(): raise InterruptedError('Cancelled')
            # Immutable content-addressed directories; the previous receipt
            # remains authoritative until the durable pointer replacement.
            for folder,_,_ in os.walk(stage,topdown=False): sync_directory(folder)
            if not destination.exists(): stage.rename(destination)
            sync_directory(destination.parent)
            sync_directory(stage.parent)
            previous = self.receipt(record['id'])
            receipt = dict(record, object=object_name)
            if previous: receipt['previousObject'] = previous['object']
            atomic_json(self.root/'active'/(record['id']+'.json'),receipt)
            self._recover_locked(project)
            return receipt
        except Exception:
            # Retain the journal if recovery itself fails (e.g. disk full).
            self._recover_locked(project)
            raise

    @staticmethod
    def desktop_path(project):
        return Path.home()/'.local/share/applications'/('org.traineros.crates.'+project+'.desktop')

    def desktop(self, record):
        if record['kind'] != 'app':
            durable_unlink(self.desktop_path(record['id']))
            return
        folder = Path.home()/'.local/share/applications'
        folder.mkdir(parents=True,exist_ok=True)
        launcher = Path(__file__).with_name('launch.py').resolve()
        def quote(s):
            return '"'+str(s).replace('\\','\\\\').replace('"','\\"').replace('`','\\`').replace('$','\\$').replace('%','%%')+'"'
        title = record['title'].replace('\n',' ').replace('\r',' ')
        path = self.desktop_path(record['id'])
        icon = self.root/'objects'/record['object']/record.get('icon','icon.svg')
        text = '[Desktop Entry]\nType=Application\nName='+title+'\nExec=/usr/bin/python3 '+quote(launcher)+' '+quote(self.root)+' '+quote(record['id'])+'\nIcon='+str(icon)+'\nTerminal=false\nCategories=Utility;\n'
        pending=path.with_suffix('.pending')
        with pending.open('w') as stream:
            stream.write(text);stream.flush();os.fsync(stream.fileno())
        pending.replace(path);sync_directory(folder)

    def remove(self, project):
        with self.lock(project):
            self._recover_locked(project)
            atomic_json(self.root/'transactions'/(project+'.json'),dict(operation='remove'))
            durable_unlink(self.root/'active'/(project+'.json'))
            self._recover_locked(project)


class ReleaseRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        u = urlsplit(newurl)
        if u.scheme != 'https' or u.hostname not in ('github.com','release-assets.githubusercontent.com','objects.githubusercontent.com'):
            raise ValueError('Unapproved release redirect')
        return super().redirect_request(req,fp,code,msg,headers,newurl)


class DownloadPaused(Exception):
    pass


def download(record, target, progress, cancelled, paused=lambda: False):
    opener = urllib.request.build_opener(ReleaseRedirect())
    target = Path(target)
    if target.is_symlink(): raise ValueError('Unsafe partial download')
    received = target.stat().st_size if target.exists() else 0
    if received > record['size']: target.unlink(); received = 0
    if received == record['size']:
        progress(1)
        return
    request = urllib.request.Request(record['url'])
    if received: request.add_header('Range', f'bytes={received}-')
    with opener.open(request,timeout=20) as response:
        if received and response.status == 206:
            expected = f"bytes {received}-{record['size']-1}/{record['size']}"
            if response.headers.get('Content-Range') != expected: raise ValueError('Invalid resumed download range')
        elif response.status == 200: received = 0
        else: raise ValueError('Unexpected download response')
        with target.open('ab' if received else 'wb') as out:
            progress(received / record['size'])
            while True:
                if cancelled(): raise InterruptedError('Cancelled')
                if paused():
                    out.flush(); os.fsync(out.fileno())
                    raise DownloadPaused()
                data = response.read(128*1024)
                if not data: break
                received += len(data)
                if received > record['size']: raise ValueError('Download exceeds declared size')
                out.write(data); progress(received / record['size'])
            out.flush(); os.fsync(out.fileno())


def main():
    # Separate short-lived worker: GUI never blocks on HTTP, extraction or disk.
    import signal
    cancelled = False
    paused = False
    def stop(*_):
        nonlocal cancelled
        cancelled = True
    signal.signal(signal.SIGTERM,stop)
    def pause(*_):
        nonlocal paused
        paused = True
    signal.signal(signal.SIGUSR1,pause)
    registry, root, operation, project, version = sys.argv[1:6]
    store = Store(root)
    def emit(stage, progress=None):
        event={'stage':stage}
        if progress is not None:
            event['progress']=progress
            event['received']=round(progress*record['size'])
            event['total']=record['size']
        print(json.dumps(event),flush=True)
    try:
        if operation == 'recover':
            if not store.recover():
                emit('Waiting for the application to close')
                return 75
        elif operation == 'remove':
            while True:
                try:
                    store.remove(project); break
                except BlockingIOError:
                    emit('Waiting for the application to close')
                    import time
                    time.sleep(.5)
                    if cancelled: raise InterruptedError('Cancelled')
        elif operation == 'install':
            records = read_registry(Path(registry))
            record = select_release(records, project, version, os.uname().machine)
            emit('Reserving space')
            if shutil.disk_usage(store.root).free < record['size']+record['unpackedSize']+16*1024**2:
                raise ValueError('Not enough storage space')
            # A second launcher/updater must not write the retained partial.
            with store.lock('download-'+record['sha256']):
                target = store.root/'staging'/(project+'-'+record['sha256']+'.part')
                try:
                    emit('Downloading')
                    download(record,target,lambda p:emit('Downloading',p),lambda:cancelled,lambda:paused)
                    if cancelled: raise InterruptedError('Cancelled')
                    if paused: raise DownloadPaused()
                    while True:
                        try:
                            store.activate(target,record,emit,lambda:cancelled); break
                        except BlockingIOError:
                            emit('Waiting for the application to close')
                            import time
                            time.sleep(.5)
                            if cancelled: raise InterruptedError('Cancelled')
                except DownloadPaused:
                    return 75
                except Exception:
                    target.unlink(missing_ok=True)
                    raise
                else:
                    target.unlink(missing_ok=True)
        else: raise ValueError('Unknown operation')
        emit('Done')
    except Exception as e:
        print(json.dumps({'error':str(e) or type(e).__name__}),flush=True)
        return 1
    return 0


if __name__ == '__main__': sys.exit(main())
