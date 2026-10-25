#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native Azahar package preparation. Never edits game saves or the owner's INI.

The native loader/AM service interprets CIA/ZCIA/NCCH/TMD. This wrapper owns
bounded IO, private staging, content-only promotion and interrupted-install recovery.
"""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import uuid

LIMIT = 8 * 1024**3

class ContentError(ValueError):
    def __init__(self,message,observed):
        super().__init__(message);self.observed=observed


def digest(path, allow_link=False):
    if (path.is_symlink() and not allow_link) or not path.is_file() or not 0 < path.stat().st_size <= LIMIT:
        raise ValueError('Unreadable game content')
    before = path.stat()
    with path.open('rb') as stream:
        result = hashlib.file_digest(stream, 'sha256').hexdigest()
    after = path.stat()
    if (before.st_size, before.st_mtime_ns, before.st_ino) != (after.st_size, after.st_mtime_ns, after.st_ino):
        raise ValueError('Game content changed')
    return result


def atomic(path, value):
    temporary = path.with_name(path.name + '.pending-' + uuid.uuid4().hex)
    try:
        with temporary.open('x', encoding='utf-8') as stream:
            json.dump(value, stream, separators=(',', ':'));stream.flush();os.fsync(stream.fileno())
        temporary.replace(path)
        descriptor = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:os.fsync(descriptor)
        finally:os.close(descriptor)
    finally:
        temporary.unlink(missing_ok=True)


def directory(path):
    path = Path(path)
    if not path.is_absolute() or any(c in str(path) for c in '\r\n\0'):
        raise ValueError('Invalid storage directory')
    # Resolve the owner's configured mount/symlink once; descendants are checked separately.
    return path.resolve()


def tree(root,observed=None):
    if observed is None:observed=[]
    observed.append(str(root));entries = [];total = 0
    if root.is_symlink():raise ValueError('Linked installed content is ambiguous')
    if not root.exists():return None, observed
    if not root.is_dir():raise ValueError('Invalid installed content directory')
    stack = [root];count = 0
    while stack:
        folder = stack.pop()
        for path in sorted(folder.iterdir()):
            count += 1
            if count > 8192 or len(path.relative_to(root).parts) > 8 or path.is_symlink():
                raise ValueError('Unsupported installed content tree')
            observed.append(str(path))
            if path.is_dir():stack.append(path)
            elif path.is_file():
                total += path.stat().st_size
                if total > LIMIT:raise ValueError('Installed content is too large')
                entries.append([path.relative_to(root).as_posix(), digest(path)])
            else:raise ValueError('Invalid installed content file')
    entries.sort()
    return hashlib.sha256(json.dumps(entries,separators=(',', ':')).encode()).hexdigest(), observed


def child_dies_with_parent():
    # If the cancellable shell/helper is killed, native installation cannot keep
    # running after its private stage has been abandoned.
    if sys.platform.startswith('linux'):
        import resource
        resource.setrlimit(resource.RLIMIT_AS,(2*1024**3,2*1024**3))
        if ctypes.CDLL(None).prctl(1, signal.SIGTERM, 0, 0, 0):os._exit(125)
        if os.getppid() == 1:os._exit(125)


def native(runtime, source, user, nand, sdmc, install=False):
    config = user / 'config';config.mkdir(parents=True)
    # Azahar ignores custom storage paths until they exist. Metadata probes must
    # not create personal directories merely by opening the Lobby/library.
    mappings = []
    storage = []
    for name, requested in (('nand', nand), ('sdmc', sdmc)):
        actual = requested
        if not actual.is_dir():
            if actual.exists():raise ValueError('Invalid storage directory')
            if install:raise ValueError('Native installation requires existing private storage')
            actual = user / ('metadata-' + name);actual.mkdir()
        mappings.append((actual, requested));storage.append(actual)
    nand, sdmc = storage
    text = '[Data Storage]\nuse_custom_storage=true\nuse_custom_storage\\default=false\n'
    for name, path in (('nand_directory', nand), ('sdmc_directory', sdmc)):
        text += name + '=' + json.dumps(str(path) + '/', ensure_ascii=False) + '\n' + name + '\\default=false\n'
    (config/'qt-config.ini').write_text(text, encoding='utf-8')
    env = dict(os.environ, TRAINEROS_AZAHAR_CONTENT_USER=str(user), QT_QPA_PLATFORM='offscreen')
    env.pop('TRAINEROS_AZAHAR_SESSION', None)
    with tempfile.TemporaryFile() as errors, tempfile.TemporaryFile() as output:
        process = subprocess.Popen([str(runtime), '--traineros-install' if install else '--traineros-content', str(source)],
                                   env=env, stdout=output, stderr=errors, preexec_fn=child_dies_with_parent)
        try:
            code = process.wait(timeout=900 if install else 30)
        except BaseException:
            process.terminate()
            try:process.wait(timeout=3)
            except subprocess.TimeoutExpired:process.kill();process.wait()
            raise
        if code:raise ValueError('Azahar could not read/install this package (encrypted, incomplete or unsupported content)')
        if output.tell() > 65536:raise ValueError('Invalid native content response')
        output.seek(0);value=json.load(output)
    if value.get('protocol') != 1 or not re.fullmatch('[a-f0-9]{16}', value.get('title','')):
        raise ValueError('Invalid native title identity')
    def remap(value):
        path = Path(value)
        for actual, requested in mappings:
            if path.is_relative_to(actual):return str(requested / path.relative_to(actual))
        raise ValueError('Native metadata escaped its storage roots')
    value['extras'] = [remap(item) for item in value['extras']]
    for name in ('contentRoot', 'entry', 'tickets'):
        if name in value:value[name] = remap(value[name])
    return value


def inspect(request):
    source=Path(request['source']);runtime=Path(request['runtime'])
    if not source.is_absolute() or not runtime.is_absolute():raise ValueError('Absolute paths required')
    content=digest(source,allow_link=True)
    nand=directory(request['nand']);sdmc=directory(request['sdmc'])
    with tempfile.TemporaryDirectory(prefix='traineros-3ds-probe-') as temporary:
        value=native(runtime,source,Path(temporary),nand,sdmc)
    dependencies=[];observed=[str(source)]
    observed.extend(value['extras'])
    try:
        for item in value['extras']:
            path=Path(item)
            if not path.is_relative_to(sdmc) or path.name!='content':raise ValueError('Invalid native dependency')
            fingerprint,_=tree(path,observed);dependencies.append(fingerprint)
    except (ValueError,OSError) as error:raise ContentError(str(error),observed) from error
    value['sourceHash']=content
    value['fingerprint']=hashlib.sha256(json.dumps({'base':content,'extras':dependencies},separators=(',',':')).encode()).hexdigest()
    value['observed']=observed
    return value


def no_game():
    if not Path('/proc').is_dir():raise ValueError('Native package installation requires Linux')
    for process in Path('/proc').iterdir():
        if not process.name.isdigit() or int(process.name)==os.getpid():continue
        try:name=(process/'comm').read_text().strip().lower()
        except (OSError,ProcessLookupError):continue
        if name.startswith(('azahar','citra')):raise ValueError('Close the running 3DS game before installing content')


def recover(journal, allowed):
    """Rollback only paths recorded by this title transaction, never whole storage."""
    if not journal.exists():return
    if journal.is_symlink() or journal.stat().st_size>65536:raise ValueError('Invalid installation journal')
    data=json.loads(journal.read_text())
    if data.get('committed'):
        journal.unlink();return
    for move in reversed(data['moves']):
        target=Path(move['target']);backup=Path(move['backup']);pending=Path(move['pending'])
        content_target=target.name=='content' and re.fullmatch('[a-f0-9]{8}',target.parent.name) \
            and target.parent.parent.name in ('00040000','00040002','0004000e','0004008c')
        ticket_target=target.parent.name=='ticket.db' and re.fullmatch(r'[A-F0-9]{16}\.[A-F0-9]{16}\.tik',target.name)
        receipt_target=target.parent==allowed[2] and re.fullmatch(r'[a-f0-9]{16}\.json',target.name)
        if not any(target.is_relative_to(base) for base in allowed) or backup.parent!=target.parent \
           or pending.parent!=target.parent or not backup.name.startswith('.traineros-before-') \
           or not pending.name.startswith('.traineros-next-') or not (content_target or ticket_target or receipt_target) \
           or target.parent.resolve()!=target.parent:
            raise ValueError('Invalid installation recovery path')
        if target.is_symlink() or backup.is_symlink() or pending.is_symlink():raise ValueError('Installation path changed')
        if target.exists() and (backup.exists() or (not move['existed'] and not pending.exists())):
            current=tree(target)[0] if target.is_dir() else digest(target)
            if current!=move.get('publishedHash'):raise ValueError('Installed content changed after interruption; previous content was retained')
        if backup.exists():
            if target.exists():
                if target.is_dir():shutil.rmtree(target)
                else:target.unlink()
            backup.rename(target)
        elif not move['existed'] and not pending.exists() and target.exists():
            if target.is_dir():shutil.rmtree(target)
            else:target.unlink()
        if pending.exists():
            if pending.is_dir():shutil.rmtree(pending)
            else:pending.unlink()
    journal.unlink()


def install(request):
    import fcntl
    no_game()
    state=directory(request['state']);state.mkdir(parents=True,exist_ok=True,mode=0o700)
    nand=directory(request['nand']);sdmc=directory(request['sdmc'])
    with (state/'install.lock').open('a') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError('Another 3DS installation is in progress')
        journal=state/'install-journal.json';recover(journal,(nand,sdmc,state))
        before=inspect(request)
        if not before['package']:return before
        if request.get('expected') and before['fingerprint']!=request['expected']:
            raise ValueError('Game or installed updates changed; choose the game again')
        title=before['title'];target=Path(before['contentRoot'])
        if not target.is_relative_to(sdmc) or target.name!='content':raise ValueError('Invalid native install target')
        receipt=state/(title+'.json')
        if receipt.exists() and not receipt.is_symlink():
            value=json.loads(receipt.read_text())
            if value.get('sourceHash')==before['sourceHash'] and value.get('installedHash')==tree(target)[0]:
                return before
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.parent.resolve()!=target.parent:raise ValueError('Linked install target')
        with tempfile.TemporaryDirectory(prefix='.traineros-install-',dir=target.parent) as temporary:
            stage=Path(temporary);(stage/'nand').mkdir();(stage/'sdmc').mkdir()
            after=native(Path(request['runtime']),Path(request['source']),stage/'user',stage/'nand',stage/'sdmc',True)
            if after['title']!=title or digest(Path(request['source']),allow_link=True)!=before['sourceHash']:
                raise ValueError('Package changed during installation')
            installed=Path(after['contentRoot'])
            if not installed.is_relative_to(stage/'sdmc') or not installed.is_dir():raise ValueError('Incomplete native installation')
            entry=Path(after['entry'])
            if not entry.is_relative_to(installed) or not entry.is_file():raise ValueError('Missing installed title content')
            if target/entry.relative_to(installed)!=Path(before['entry']):raise ValueError('Installed entry changed')
            installed_hash,_=tree(installed)
            transfers=[(installed,target)]
            tickets=Path(after['tickets'])
            if not tickets.is_relative_to(stage/'nand'):raise ValueError('Invalid native ticket path')
            ticket_target=Path(before['tickets'])
            if not ticket_target.is_relative_to(nand):raise ValueError('Invalid ticket destination')
            if tickets.is_dir():
                for ticket in tickets.iterdir():
                    if ticket.is_symlink() or not re.fullmatch(title.upper()+r'\.[A-F0-9]{16}\.tik',ticket.name):
                        raise ValueError('Unexpected native ticket')
                    transfers.append((ticket,ticket_target/ticket.name))
            staged_receipt=stage/'receipt.json'
            staged_receipt.write_text(json.dumps({'title':title,'sourceHash':before['sourceHash'],
                'installedHash':installed_hash,'entry':before['entry']}))
            transfers.append((staged_receipt,receipt))
            moves=[];token=uuid.uuid4().hex
            try:
                for number,(origin,destination) in enumerate(transfers):
                    destination.parent.mkdir(parents=True,exist_ok=True)
                    if destination.parent.resolve()!=destination.parent or destination.is_symlink():raise ValueError('Linked destination')
                    pending=destination.with_name('.traineros-next-'+token+'-'+str(number))
                    backup=destination.with_name('.traineros-before-'+token+'-'+str(number))
                    # Journal each planned move before copying, including cancellation during copy.
                    moves.append({'target':str(destination),'pending':str(pending),'backup':str(backup),'existed':destination.exists()})
                    atomic(journal,{'moves':moves})
                    if origin.is_dir():origin.rename(pending)
                    else:shutil.copyfile(origin,pending)
                    moves[-1]['publishedHash']=tree(pending)[0] if pending.is_dir() else digest(pending)
                    atomic(journal,{'moves':moves})
                no_game()
                if inspect(request)['fingerprint']!=before['fingerprint']:raise ValueError('Game dependencies changed during installation')
                for move in moves:
                    destination=Path(move['target'])
                    if move['existed']:destination.rename(move['backup'])
                    Path(move['pending']).rename(destination)
                atomic(journal,{'moves':moves,'committed':True})
                journal.unlink()
                # Old content/tickets/receipt are intentionally retained beside each target.
            except BaseException:
                recover(journal,(nand,sdmc,state));raise
        return before


def main():
    os.umask(0o077)
    if len(sys.argv)!=3 or sys.argv[1] not in ('inspect','install'):return 2
    signal.signal(signal.SIGTERM,lambda *_: (_ for _ in ()).throw(InterruptedError('Installation cancelled')))
    path=Path(sys.argv[2])
    if not path.is_absolute() or path.is_symlink() or path.stat().st_size>65536:return 2
    request=json.loads(path.read_text())
    try:
        value=inspect(request) if sys.argv[1]=='inspect' else install(request)
        print(json.dumps(value,separators=(',',':')));return 0
    except (OSError,ValueError,KeyError,InterruptedError,subprocess.SubprocessError) as error:
        print(json.dumps({'error':str(error),'observed':getattr(error,'observed',[])}));return 3


if __name__=='__main__':sys.exit(main())
