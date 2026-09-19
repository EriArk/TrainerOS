"""Content-free fixtures for reversible legacy retirement, including drift."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
import uuid

spec = importlib.util.spec_from_file_location('retire', Path(__file__).resolve().parents[1] / 'packaging/maintenance/retire-legacy-states.py')
retire = importlib.util.module_from_spec(spec)
spec.loader.exec_module(retire)


class RetirementTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve()
        self.data = self.base / 'data'; (self.data / 'integrations').mkdir(parents=True)
        self.root = self.base / 'legacy'; self.root.mkdir()
        (self.data / 'integrations/retroarch.json').write_text(json.dumps({'resumeProtocol': 'mgba-entry-v1', 'resumeDirectory': str(self.root)}))
        with sqlite3.connect(self.data / 'traineros.sqlite3') as db:
            db.execute('PRAGMA user_version=7')
            db.execute('CREATE TABLE adventures(id,content_path,adapter_id)')
            db.execute('INSERT INTO adventures VALUES(?,?,?)', ('fixture', str(self.base / 'original.gba'), 'retroarch'))
        db.close()
        self.owner = self.root / hashlib.sha256(b'fixture').hexdigest(); self.owner.mkdir()

    def session(self):
        path = self.owner / str(uuid.uuid4()); path.mkdir()
        settings = {'savestate_directory': str(path), 'sort_savestates_enable': 'false',
                    'sort_savestates_by_content_enable': 'false', 'savestates_in_content_dir': 'false',
                    'savestate_auto_save': 'true', 'savestate_auto_load': 'false',
                    'savestate_thumbnail_enable': 'true', 'config_save_on_exit': 'false', 'auto_overrides_enable': 'false'}
        (path / 'launch.cfg').write_bytes(''.join(f'{key} = "{value}"\n' for key, value in settings.items()).encode())
        (path / 'original.state.auto').write_bytes(b'ORIGINAL TEST DATA')
        return path

    def test_inventory_preserves_unknown_files_and_saves(self):
        accepted = self.session()
        save = self.session(); (save / 'original.srm').write_bytes(b'ORDINARY SAVE')
        child = self.session(); (child / 'validation').mkdir()
        changed = self.session(); (changed / 'launch.cfg').write_bytes(b'USER CONFIG')
        report = retire.inventory(self.data)
        self.assertEqual([item['relative'] for item in report['sessions']], [str(accepted.relative_to(self.root))])
        self.assertEqual(len(report['kept']), 3)
        self.assertTrue(accepted.exists())
        manifest = retire.retire(report)
        self.assertFalse(accepted.exists())
        self.assertEqual((save / 'original.srm').read_bytes(), b'ORDINARY SAVE')
        retire.move(report, manifest.parent, restore=True)
        self.assertEqual((accepted / 'original.state.auto').read_bytes(), b'ORIGINAL TEST DATA')
        retire.move(report, manifest.parent, restore=True) # Safe repeated recovery.

    def test_drift_is_rejected_before_any_session_is_moved(self):
        first, second = self.session(), self.session()
        report = retire.inventory(self.data)
        (second / 'original.state.auto').write_bytes(b'CHANGED')
        with self.assertRaises(ValueError):
            retire.retire(report)
        self.assertTrue(first.exists()); self.assertTrue(second.exists())

    def test_restore_conflict_and_interrupted_apply(self):
        first, second = self.session(), self.session()
        report = retire.inventory(self.data)
        archive = self.base / ('traineros-retired-' + str(uuid.uuid4())); archive.mkdir()
        destination = archive / first.relative_to(self.root); destination.parent.mkdir()
        first.rename(destination) # Simulate termination between two moves.
        retire.move(report, archive)
        self.assertFalse(second.exists())
        first.mkdir(); (first / 'personal.srm').write_bytes(b'NEW SAVE')
        with self.assertRaises(ValueError):
            retire.move(report, archive, restore=True)
        self.assertFalse(second.exists())
        self.assertEqual((first / 'personal.srm').read_bytes(), b'NEW SAVE')

    def test_manifest_cannot_escape_archive_or_traverse_symlinks(self):
        session = self.session(); report = retire.inventory(self.data)
        archive = self.base / ('traineros-retired-' + str(uuid.uuid4())); archive.mkdir()
        report['sessions'][0]['relative'] = '../outside'
        with self.assertRaises(ValueError):
            retire.move(report, archive)
        self.assertTrue(session.exists())
        if hasattr(__import__('os'), 'geteuid'):
            (session / 'original.state.auto').unlink()
            (session / 'original.state.auto').symlink_to(self.data / 'traineros.sqlite3')
            self.assertEqual(retire.inventory(self.data)['sessions'], [])


if __name__ == '__main__':
    unittest.main()
