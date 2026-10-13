"""Flatpak audio namespace regression; live ancestry/capture checked on Linux."""
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'packaging/integrations'))
sys.modules['overlay_support'] = SimpleNamespace(X11=None, identity=None)
from game_capture import audio_pid

class AudioIdentityTests(unittest.TestCase):
    def test_flatpak_uses_exact_instance_host_pid(self):
        properties = {'application.process.id': '2', 'pipewire.client.access': 'flatpak',
            'pipewire.access.portal.instance_id': '101', 'pipewire.access.portal.app_id': 'org.libretro.RetroArch'}
        instances = {'101': (4000, 'org.libretro.RetroArch'), '102': (5000, 'org.libretro.RetroArch')}
        self.assertEqual(audio_pid(properties, instances), 4000)
        properties['pipewire.access.portal.instance_id'] = '103'
        self.assertEqual(audio_pid(properties, instances), 0)
        properties['pipewire.access.portal.instance_id'] = '101'
        properties['pipewire.access.portal.app_id'] = 'different.app'
        self.assertEqual(audio_pid(properties, instances), 0)

    def test_missing_flatpak_identity_never_uses_namespace_pid(self):
        self.assertEqual(audio_pid({'application.process.id': '4000', 'pipewire.client.access': 'flatpak'}, {}), 0)
        self.assertEqual(audio_pid({'application.process.id': '4000'}, {}), 4000)
        self.assertEqual(audio_pid({'application.process.id': 'invalid'}, {}), 0)

if __name__ == '__main__': unittest.main()
