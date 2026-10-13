"""Flatpak audio namespace regression; live ancestry/capture checked on Linux."""
import sys
import unittest
import tempfile
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'packaging/integrations'))
sys.modules['overlay_support'] = SimpleNamespace(X11=None, identity=None)
from game_capture import audio_pid, namespace_pid

class AudioIdentityTests(unittest.TestCase):
    def test_flatpak_uses_exact_instance_host_pid(self):
        properties = {'application.process.id': '2', 'pipewire.client.access': 'flatpak',
            'pipewire.access.portal.instance_id': '101', 'pipewire.access.portal.app_id': 'org.libretro.RetroArch'}
        instances = {'101': (4000, 'org.libretro.RetroArch'), '102': (5000, 'org.libretro.RetroArch')}
        resolve = lambda anchor, local: 4001 if (anchor, local) == (4000, 2) else 0
        self.assertEqual(audio_pid(properties, instances, resolve), 4001)
        properties['pipewire.access.portal.instance_id'] = '103'
        self.assertEqual(audio_pid(properties, instances), 0)
        properties['pipewire.access.portal.instance_id'] = '101'
        properties['pipewire.access.portal.app_id'] = 'different.app'
        self.assertEqual(audio_pid(properties, instances, resolve), 0)

    def test_namespace_resolution_excludes_siblings_and_other_instances(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(Path, 'readlink', lambda p: Path(p.read_text())):
            proc = Path(directory)
            def process(pid, parent, local, namespace):
                entry = proc / str(pid)
                (entry / 'ns').mkdir(parents=True)
                (entry / 'ns/pid').write_text(namespace)
                fields = ['S', str(parent)] + ['0'] * 17 + ['12345']
                (entry / 'stat').write_text(str(pid) + ' (fixture) ' + ' '.join(fields))
                (entry / 'status').write_text(f'NSpid:\t{pid}\t{local}\n')
            process(4000, 1, 1, 'pid:[100]')
            process(4001, 4000, 2, 'pid:[100]')
            process(5000, 1, 1, 'pid:[200]')
            process(5001, 5000, 2, 'pid:[200]')
            self.assertEqual(namespace_pid(4000, 2, proc), 4001)
            self.assertEqual(namespace_pid(5000, 2, proc), 5001)
            self.assertEqual(namespace_pid(4000, 99, proc), 0)
            process(6001, 1, 2, 'pid:[100]')
            self.assertEqual(namespace_pid(4000, 2, proc), 4001)
            process(4002, 4000, 2, 'pid:[100]')
            self.assertEqual(namespace_pid(4000, 2, proc), 0)

    def test_missing_flatpak_identity_never_uses_namespace_pid(self):
        self.assertEqual(audio_pid({'application.process.id': '4000', 'pipewire.client.access': 'flatpak'}, {}), 0)
        self.assertEqual(audio_pid({'application.process.id': '4000'}, {}), 4000)
        self.assertEqual(audio_pid({'application.process.id': 'invalid'}, {}), 0)

if __name__ == '__main__': unittest.main()
