import sys
import unittest
from pathlib import Path
from types import SimpleNamespace as Row

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'packaging/integrations'))
from voice_presence import account, presence


def peer(identity, source=1, muted=False):
    return Row(identity=identity, track_publications={'track': Row(source=source, muted=muted)})


class VoicePresenceTests(unittest.TestCase):
    def test_only_provider_identity_maps_to_an_account(self):
        self.assertEqual(account('user_123_connection-with-dashes'), '123')
        for value in ('Mika', '123', 'user_abc_foo', 'user_123_', 'user_123_x\n', 'user_'+'1'*21+'_x'):
            self.assertEqual(account(value), '')

    def test_sdk_speakers_deduplicate_devices_and_exclude_non_microphone_sources(self):
        peers = [peer('user_1_a'), peer('user_1_b'), peer('user_2_a', 2), peer('user_3_a', muted=True), peer('Mika')]
        state = presence(peers, {p.identity for p in peers}, 1)
        self.assertEqual(state, {'people': ['1', '2', '3'], 'speakers': ['1']})

    def test_local_mute_does_not_hide_the_same_account_on_another_device(self):
        peers = [peer('user_1_a'), peer('user_1_b')]
        self.assertEqual(presence(peers, {'user_1_a'}, 1, 'user_1_a', True)['speakers'], [])
        self.assertEqual(presence(peers, {'user_1_a','user_1_b'}, 1, 'user_1_a', True)['speakers'], ['1'])

    def test_stale_or_departed_speaker_cannot_survive_current_roster(self):
        self.assertEqual(presence([peer('user_2_b')], {'user_2_a'}, 1)['speakers'], [])
        self.assertEqual(presence([peer('user_2_b')], set(), 1)['speakers'], [])


if __name__ == '__main__':
    unittest.main()
