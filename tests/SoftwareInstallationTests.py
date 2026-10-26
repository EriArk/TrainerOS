# SPDX-License-Identifier: GPL-3.0-or-later
"""Disposable filesystem/keys only. No host session install, device or publication."""
import copy
from contextlib import closing
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest
from unittest.mock import patch
import SoftwareChannelTests as channel_tests
from SoftwareChannelTests import Response, SOURCE
import releases as sw
import installation as install
import installer
import host
import publisher

spec = importlib.util.spec_from_file_location('session_ownership', SOURCE / 'packaging/session/ownership.py')
ownership = importlib.util.module_from_spec(spec); spec.loader.exec_module(ownership)


class InstallationTests(unittest.TestCase):
    setUpClass = classmethod(channel_tests.SoftwareChannelTests.setUpClass.__func__)
    tearDownClass = classmethod(channel_tests.SoftwareChannelTests.tearDownClass.__func__)

    def setUp(self):
        channel_tests.SoftwareChannelTests.setUp(self)
        self.home = self.root / 'home'; self.home.mkdir()
        self.personal = self.home / 'personal'; self.personal.mkdir()
        self.database = self.personal / 'traineros.sqlite3'
        with closing(sqlite3.connect(self.database)) as db, db:
            db.execute('PRAGMA user_version=16'); db.execute('CREATE TABLE saves(value TEXT)')
            db.execute("INSERT INTO saves VALUES ('private progress')")
        (self.personal / 'first-run.json').write_text('{"version":2,"stage":"accounts"}')
        self.installation = install.initialize(self.root / 'owned', {'beta': self.feed_url, 'preview': self.feed_url}, self.public, self.personal)
        self.prepared = sw.prepare(self.manifest, self.public, self.cache, progress=lambda *args: None,
                                  opener=lambda req, timeout: Response(self.archive.read_bytes(), url=req.full_url))

    def install(self): return install.install_prepared(self.installation, self.prepared)

    def new_version(self, sequence=8, maximum=16):
        payload = self.root / ('payload-' + str(sequence)); (payload / 'bin').mkdir(parents=True)
        (payload / 'bin/traineros').write_bytes(b'fixture-new-' + str(sequence).encode())
        out = self.root / ('out-' + str(sequence))
        manifest = sw.build(payload, out, self.key, 'fixture-' + str(sequence), sequence, 'b' * 40, self.archive_url,
                            {'sqlite': {'min': 0, 'max': maximum}, 'firstRun': {'min': 0, 'max': 2}})
        return sw.prepare(manifest, self.public, self.cache, progress=lambda *args: None,
                          opener=lambda req, timeout: Response((out / 'traineros.tar.gz').read_bytes(), url=req.full_url))

    def test_install_idempotence_and_personal_data_preservation(self):
        before = self.database.read_bytes(); result = self.install(); again = self.install()
        self.assertEqual(result, again); self.assertEqual(install.audit(self.installation)['status'], 'ready')
        self.assertEqual(self.database.read_bytes(), before)
        self.assertEqual(install.context(self.installation, False)['installedSequence'], 7)

    def test_rollback_checks_current_schemas_and_never_reverts_data(self):
        self.install(); install.install_prepared(self.installation, self.new_version(maximum=17))
        with closing(sqlite3.connect(self.database)) as db, db: db.execute('PRAGMA user_version=17')
        with self.assertRaisesRegex(ValueError, 'incompatible'): install.rollback(self.installation)
        self.assertEqual(install.context(self.installation, False)['installedSequence'], 8)
        with closing(sqlite3.connect(self.database)) as db, db: db.execute('PRAGMA user_version=16')
        self.assertEqual(install.rollback(self.installation)['sequence'], 7)
        with closing(sqlite3.connect(self.database)) as db, db: self.assertEqual(db.execute('SELECT value FROM saves').fetchone()[0], 'private progress')

    def test_interruption_before_receipt_keeps_previous_version_and_retry_finishes(self):
        old = self.install(); prepared = self.new_version()
        def crash(phase):
            if phase == 'version-published': raise RuntimeError('simulated power loss')
        with self.assertRaisesRegex(RuntimeError, 'power loss'): install.install_prepared(self.installation, prepared, checkpoint=crash)
        self.assertEqual(install.receipt(self.installation)['active'], old['identity'])
        self.assertEqual(install.install_prepared(self.installation, prepared)['sequence'], 8)
        self.assertEqual(install.rollback(self.installation)['sequence'], 7)

    def test_interruption_after_receipt_is_an_installed_unconfirmed_version(self):
        self.install(); prepared = self.new_version()
        def crash(phase):
            if phase == 'receipt-published': raise RuntimeError('interrupted')
        with self.assertRaises(RuntimeError): install.install_prepared(self.installation, prepared, checkpoint=crash)
        result = install.context(self.installation, False)
        self.assertEqual(result['installedSequence'], 8); self.assertEqual(result['health'], 'awaiting-first-frame')
        self.assertTrue(result['rollbackAvailable'])

    def test_changed_payload_cannot_be_installed(self):
        (self.prepared / 'payload/bin/traineros').write_bytes(b'damaged')
        with self.assertRaises(ValueError): self.install()
        self.assertFalse(install.receipt(self.installation)['active'])

    def test_repair_restores_only_signed_files_and_survives_retry(self):
        installed = self.install(); active = self.installation / 'versions' / installed['identity']
        (active / 'payload/bin/traineros').write_bytes(b'damaged')
        self.assertEqual(install.audit(self.installation)['status'], 'repairable-drift')
        original = sw.download
        def fetch(archive, part, progress=sw.event, opener=None):
            return original(archive, part, lambda *args: None, lambda req, timeout: Response(self.archive.read_bytes(), url=req.full_url))
        with patch.object(sw, 'download', fetch):
            install.repair(self.installation)
            cached = self.installation / 'repair-cache' / installed['identity'] / 'payload/bin/traineros'
            cached.write_bytes(b'broken cache')
            (active / 'payload/bin/traineros').write_bytes(b'broken again')
            install.repair(self.installation)
        self.assertEqual(install.audit(self.installation)['status'], 'ready')

    def test_uninstall_preserves_data_and_rejects_unowned_paths(self):
        self.install(); installer.shortcuts(self.installation, self.home)
        unrelated = self.home / 'keep.txt'; unrelated.write_bytes(b'keep')
        records = install.read(self.installation / 'shortcuts.json')
        sw.atomic(self.installation / 'shortcuts.json', records + [{'path': str(unrelated), 'sha256': sw.sha256(unrelated)}])
        with self.assertRaisesRegex(ValueError, 'Unexpected launcher'): install.uninstall(self.installation, self.home)
        self.assertTrue(unrelated.exists()); self.assertTrue(install.receipt(self.installation)['active'])
        sw.atomic(self.installation / 'shortcuts.json', records)
        self.assertEqual(install.uninstall(self.installation, self.home)['status'], 'uninstalled')
        self.assertTrue(self.database.exists()); self.assertEqual(unrelated.read_bytes(), b'keep')
        self.assertFalse((self.home / '.local/bin/traineros').exists())

    def test_modified_launcher_and_system_integration_block_uninstall(self):
        self.install(); installer.shortcuts(self.installation, self.home)
        path = self.home / '.local/bin/traineros'; path.write_text('user customization')
        with self.assertRaisesRegex(ValueError, 'changed'): install.uninstall(self.installation, self.home)
        sw.atomic(self.installation / 'integration.json', {'systemInstalled': True})
        with self.assertRaisesRegex(ValueError, 'changed'): install.uninstall(self.installation, self.home, preflight=True)
        self.assertTrue(install.read(self.installation / 'integration.json')['systemInstalled'])
        with self.assertRaisesRegex(ValueError, 'system session'): install.uninstall(self.installation, self.home)

    def test_runtime_lock_rejects_real_maintenance_subprocess(self):
        self.install()
        with sw.cache_lock(self.installation / 'runtime'):
            result = subprocess.run([sys.executable, str(SOURCE / 'packaging/software/installation.py'), '--root', str(self.installation), 'uninstall'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1); self.assertIn('Another release preparation', result.stdout)
        self.assertTrue(install.receipt(self.installation)['active'])

    def test_orphan_marker_is_fail_closed_until_a_new_boot(self):
        sw.atomic(self.installation / 'runtime-session.json', {'boot': 'a', 'pid': 123})
        with patch.object(install, 'boot_id', return_value='a'):
            with self.assertRaisesRegex(RuntimeError, 'games'): install.ensure_no_orphans(self.installation)
        with patch.object(install, 'boot_id', return_value='b'): install.ensure_no_orphans(self.installation)
        self.assertFalse((self.installation / 'runtime-session.json').exists())

    def test_stale_request_never_replays_on_later_launch(self):
        self.install()
        sw.atomic(self.installation / 'request.json', {'operation': 'rollback', 'active': install.receipt(self.installation)['active'], 'created': 0})
        with self.assertRaisesRegex(ValueError, 'stale'): install.apply_request(self.installation)
        self.assertFalse((self.installation / 'request.json').exists())

    def test_cleanup_does_not_remove_installed_versions_or_unknown_files(self):
        installed = self.install(); cache = self.installation / 'cache'; cache.mkdir()
        (cache / ('a' * 64 + '.part')).write_bytes(b'partial')
        (cache / 'personal.txt').write_bytes(b'keep')
        abandoned = cache / '.extract-abandoned'; abandoned.mkdir(); (abandoned / 'x').write_bytes(b'x')
        result = install.clean_cache(self.installation)
        self.assertEqual(result['bytes'], 8); self.assertTrue((cache / 'personal.txt').exists())
        self.assertTrue((self.installation / 'versions' / installed['identity']).exists())

    def test_bootstrap_and_shortcuts_are_repeatable_but_do_not_adopt_other_files(self):
        installer.bootstrap(self.installation, SOURCE / 'packaging')
        installer.bootstrap(self.installation, SOURCE / 'packaging')
        installer.shortcuts(self.installation, self.home); installer.shortcuts(self.installation, self.home)
        helper = self.installation / 'bootstrap/packaging/software/launcher.py'; helper.write_text('changed')
        with self.assertRaisesRegex(ValueError, 'modified'): installer.bootstrap(self.installation, SOURCE / 'packaging')

    def executable_release(self, sequence, script):
        payload = self.root / ('executable-' + str(sequence)); (payload / 'bin').mkdir(parents=True)
        executable = payload / 'bin/traineros'; executable.write_bytes(b'#!/usr/bin/python3\n' + script.encode()); executable.chmod(0o755)
        out = self.root / ('executable-out-' + str(sequence))
        manifest = sw.build(payload, out, self.key, 'fixture-' + str(sequence), sequence, 'c' * 40, self.archive_url,
                            {'sqlite': {'min': 0, 'max': 16}, 'firstRun': {'min': 0, 'max': 2}})
        return sw.prepare(manifest, self.public, self.cache, progress=lambda *args: None,
                          opener=lambda req, timeout: Response((out / 'traineros.tar.gz').read_bytes(), url=req.full_url))

    @unittest.skipUnless(sys.platform == 'linux', 'Requires real Linux subreaper and flock; never substitute Windows proof')
    def test_launcher_retains_runtime_lease_until_orphaned_game_exits(self):
        child_ready, release_child = self.root / 'child-ready', self.root / 'release-child'
        child_script = f"from pathlib import Path; import time; Path({str(child_ready)!r}).touch()\nwhile not Path({str(release_child)!r}).exists(): time.sleep(.02)\n"
        script = f"import os,subprocess\nos.write(int(os.environ['TRAINEROS_READY_FD']),b'R')\nsubprocess.Popen(['/usr/bin/python3','-c',{child_script!r}],close_fds=True)\n"
        install.install_prepared(self.installation, self.executable_release(7, script))
        launcher = subprocess.Popen([sys.executable, str(SOURCE / 'packaging/software/launcher.py'), '--root', str(self.installation)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            limit = time.monotonic() + 15
            while not child_ready.exists() and time.monotonic() < limit and launcher.poll() is None: time.sleep(.02)
            self.assertTrue(child_ready.exists()); self.assertIsNone(launcher.poll())
            blocked = subprocess.run([sys.executable, str(SOURCE / 'packaging/software/installation.py'), '--root', str(self.installation), 'uninstall'], capture_output=True)
            self.assertEqual(blocked.returncode, 1); self.assertTrue(install.receipt(self.installation)['active'])
        finally:
            release_child.touch(); stdout, stderr = launcher.communicate(timeout=15)
        self.assertEqual(launcher.returncode, 0, stderr.decode()); self.assertFalse((self.installation / 'runtime-session.json').exists())
        self.assertEqual(install.receipt(self.installation)['health'], 'first-frame-confirmed')

    @unittest.skipUnless(sys.platform == 'linux', 'Requires real Linux launcher; no device is contacted')
    def test_launcher_applies_only_explicit_maintenance_exit(self):
        ready = "import os\nos.write(int(os.environ['TRAINEROS_READY_FD']),b'R')\n"
        install.install_prepared(self.installation, self.executable_release(7, ready))
        install.install_prepared(self.installation, self.executable_release(8, ready + "raise SystemExit(75)\n"))
        install.request(self.installation, 'rollback')
        launched = subprocess.run([sys.executable, str(SOURCE / 'packaging/software/launcher.py'), '--root', str(self.installation)], capture_output=True, timeout=20)
        self.assertEqual(launched.returncode, 0, launched.stderr.decode())
        self.assertEqual(install.context(self.installation, False)['installedSequence'], 7)
        install.request(self.installation, 'rollback')  # An ordinary exit must discard this request.
        launched = subprocess.run([sys.executable, str(SOURCE / 'packaging/software/launcher.py'), '--root', str(self.installation)], capture_output=True, timeout=20)
        self.assertEqual(launched.returncode, 0, launched.stderr.decode())
        self.assertEqual(install.context(self.installation, False)['installedSequence'], 7)
        self.assertFalse((self.installation / 'request.json').exists())

    def test_initialization_validates_before_persisting_identity(self):
        root = self.root / 'invalid'
        with self.assertRaisesRegex(ValueError, 'outside'):
            install.initialize(root, {'beta': self.feed_url, 'preview': self.feed_url}, self.public, root / 'personal')
        self.assertFalse((root / 'config.json').exists()); self.assertFalse((root / 'trusted-key.pem').exists())
        with self.assertRaisesRegex(ValueError, 'Both'):
            install.initialize(root, {'beta': self.feed_url}, self.public, self.personal)
        self.assertFalse((root / 'config.json').exists())

    def test_same_sequence_cannot_replace_different_signed_content(self):
        self.install()
        with self.assertRaisesRegex(ValueError, 'different installed content'):
            install.install_prepared(self.installation, self.new_version(sequence=7))

    def test_explicit_request_revalidates_release_before_switching_receipt(self):
        self.install(); newer = self.new_version()
        available = {'status': 'available', 'version': 'fixture-8', 'sequence': 8}
        with patch.object(install, 'resolve', return_value=available):
            install.request(self.installation, 'apply', sequence=8)
        with patch.object(install, 'resolve', return_value={'status': 'blocked'}):
            with self.assertRaisesRegex(ValueError, 'no longer'): install.apply_request(self.installation)
        self.assertEqual(install.context(self.installation, False)['installedSequence'], 7)
        self.assertFalse((self.installation / 'request.json').exists())
        with patch.object(install, 'resolve', return_value=available):
            install.request(self.installation, 'apply', sequence=8)
        with patch.object(install, 'resolve', return_value={'status': 'prepared', 'prepared': str(newer)}), patch.object(installer, 'payload_dependencies'):
            self.assertEqual(install.apply_request(self.installation)['sequence'], 8)
        self.assertFalse((self.installation / 'request.json').exists())

    def test_publisher_packages_only_public_trust_and_requires_real_provenance(self):
        payload = self.root / 'arm64'; (payload / 'bin').mkdir(parents=True)
        header = bytearray(64); header[:6] = b'\x7fELF\x02\x01'; struct.pack_into('<H', header, 18, 183)
        (payload / 'bin/traineros').write_bytes(header); (payload / 'bin/traineros').chmod(0o755)
        source = self.root / 'source.tar.gz'; source.write_bytes(b'fixture source, not a distributable release')
        notices = self.root / 'notices.txt'; notices.write_text('Fixture licence record')
        provenance = self.root / 'provenance.json'
        record = {'source': 'a' * 40, 'target': 'linux-aarch64', 'buildCommand': 'fixture only',
                  'dependencies': {'fixture': '1'}, 'sourceArchive': str(source), 'notices': str(notices), 'redistributionReviewed': False}
        sw.atomic(provenance, record)
        output = self.root / 'publication'
        def build():
            return publisher.build(payload, output, self.key, self.public, 'fixture', 7, 'a' * 40, 'fixture/repo',
                'fixture-7', 'beta', 7, '2026-10-24T00:00:00Z', '2026-10-31T00:00:00Z',
                {'sqlite': {'min': 0, 'max': 16}, 'firstRun': {'min': 0, 'max': 2}}, None, provenance)
        with self.assertRaisesRegex(ValueError, 'review'): build()
        self.assertFalse(output.exists())
        record['redistributionReviewed'] = True; sw.atomic(provenance, record)
        result = build(); self.assertFalse(result['published'])
        plan = sw.decode((output / 'publication.json').read_bytes()); self.assertFalse(plan['published'])
        for name, digest in plan['files'].items(): self.assertEqual(sw.sha256(output / name), digest)
        with tarfile.open(output / 'TrainerOS-Setup.tar.gz') as archive:
            launch = archive.getmember('TrainerOS-Setup/Install TrainerOS.sh')
            self.assertEqual(launch.mode, 0o755); self.assertNotIn(b'\r', archive.extractfile(launch).read())
            for member in archive.getmembers():
                if member.isfile(): self.assertNotIn(self.key.read_bytes(), archive.extractfile(member).read())
            self.assertEqual(archive.extractfile('TrainerOS-Setup/trusted-key.pem').read(), self.public.read_bytes())
        self.assertEqual(sw.verified(output / 'release.json', self.public)['signed']['sequence'], 7)

    def test_progress_closing_or_cancelling_aborts_download(self):
        with patch.object(installer.shutil, 'which', return_value='fixture-qdbus'), patch.object(installer, 'dialog', return_value=subprocess.CompletedProcess([], 0, 'service /path')):
            with patch.object(installer.Progress, 'call', return_value=subprocess.CompletedProcess([], 0, 'false')):
                with installer.Progress() as progress:
                    with patch.object(progress, 'call', return_value=subprocess.CompletedProcess([], 1, '')):
                        with self.assertRaises(installer.Cancelled): progress.update('download', 1, 10)



class HostAndOwnershipTests(unittest.TestCase):
    def test_os_storage_is_derived_from_system_mount_not_game_disk(self):
        mount = {'filesystems': [{'source': '/dev/sda3[/ostree/deploy]', 'fstype': 'btrfs'}]}
        for removable, expected in ((False, 'internal'), (True, 'removable')):
            blocks = {'blockdevices': [{'path': '/dev/sda3', 'type': 'part', 'children': [{'path': '/dev/sda', 'type': 'disk', 'rm': removable}]}]}
            self.assertEqual(host.storage(mount, blocks)['location'], expected)
        with self.assertRaisesRegex(ValueError, 'ambiguous'): host.storage(mount, {'blockdevices': []})

    def test_armada_probe_distinguishes_sd_from_emmc_even_when_rm_is_false(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in ('usr/libexec/armada/device-env', 'usr/libexec/armada/session-control', 'usr/bin/gamescope'):
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.touch(); path.chmod(0o755)
            card = root / 'sys/class/block/mmcblk0/device/type'; card.parent.mkdir(parents=True)
            def run(args):
                if args[0] == 'findmnt': return json.dumps({'filesystems': [{'source': '/dev/mmcblk0p2', 'fstype': 'btrfs'}]})
                return json.dumps({'blockdevices': [{'path': '/dev/mmcblk0p2', 'type': 'part', 'children': [{'path': '/dev/mmcblk0', 'type': 'disk', 'rm': False}]}]})
            for kind, location in (('SD', 'removable'), ('MMC', 'internal')):
                card.write_text(kind)
                report = host.probe(root, run=run, system='Linux', machine='aarch64', filesystem=root)
                self.assertTrue(report['supported'], report['errors']); self.assertEqual(report['osStorage']['location'], location)

    def test_schema_probe_reads_wal_without_migration(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            db = sqlite3.connect(root / 'traineros.sqlite3')
            try:
                db.execute('PRAGMA journal_mode=WAL'); db.execute('PRAGMA user_version=17'); db.commit()
                self.assertEqual(host.schemas(root), {'sqlite': 17, 'firstRun': 0})
            finally: db.close()

    def test_session_journal_handles_interrupted_upgrade_and_preserves_drift(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); journal = root / 'ledger.json'; name = '/etc/sddm.conf.d/90-traineros.conf'
            owned = ownership.Ownership(journal, 'fixture', root)
            target = owned.target(name); target.parent.mkdir(parents=True)
            owned.preparing(name, b'first'); target.write_bytes(b'first')
            owned.preparing(name, b'second')  # interrupted before replace
            ownership.Ownership(journal, 'fixture', root).check(name)
            target.write_bytes(b'second'); owned.check(name)
            target.write_bytes(b'user change')
            with self.assertRaisesRegex(ValueError, 'changed'): owned.uninstall()
            self.assertEqual(target.read_bytes(), b'user change')
            target.write_bytes(b'second'); owned.uninstall(); self.assertFalse(target.exists())
            with self.assertRaises(ValueError): owned.preparing('/etc/passwd', b'bad')


if __name__ == '__main__': unittest.main()
