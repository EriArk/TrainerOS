"""Subscription/privacy regressions; live codec and provider checks are separate."""
import sys
import struct
import unittest
from pathlib import Path
from types import SimpleNamespace as NS

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'packaging/integrations'))
source = NS(SOURCE_MICROPHONE=1, SOURCE_SCREENSHARE=2, SOURCE_SCREENSHARE_AUDIO=3, SOURCE_CAMERA=4)
sys.modules['livekit'] = NS(rtc=NS(TrackSource=source))
from fluxer_streams import ScreenStreams, frame_header


class Publication:
    def __init__(self, source):
        self.source, self.muted, self.track, self.selected = source, False, None, False
    def set_subscribed(self, value): self.selected = value


class ScreensTests(unittest.TestCase):
    def setUp(self):
        self.people = {str(i): NS(identity=f'user_{i}_connection', track_publications={
            str(k): Publication(k) for k in (1, 2, 3, 4)}) for i in (1, 2)}
        self.events = []
        self.screens = ScreenStreams(NS(remote_participants=self.people),
            lambda kind, **fields: self.events.append((kind, fields)), None, None)

    def test_only_selected_screen_audio_and_video_are_subscribed(self):
        self.screens.watch('user_1_connection', 'local-socket')
        for id, p in self.people.items():
            self.assertTrue(p.track_publications['1'].selected)
            self.assertEqual(p.track_publications['2'].selected, id == '1')
            self.assertEqual(p.track_publications['3'].selected, id == '1')
            self.assertFalse(p.track_publications['4'].selected)
        self.screens.watch('user_2_connection', 'another-socket')
        self.assertFalse(self.people['1'].track_publications['3'].selected)
        self.assertTrue(self.people['2'].track_publications['3'].selected)

    def test_lost_or_blocked_publisher_stops_screen_without_ending_other_voice(self):
        self.screens.watch('user_1_connection', 'local-socket')
        self.screens.blocked.add('1'); self.screens.catalogue()
        self.assertEqual(self.screens.watching, '')
        self.assertTrue(self.people['2'].track_publications['1'].selected)
        self.assertTrue(all(not p.selected for p in self.people['1'].track_publications.values()))
        self.assertFalse(self.people['2'].track_publications['3'].selected)

    def test_frame_bounds_reject_overflow_and_wrong_size(self):
        self.assertEqual(frame_header(struct.pack('<IIII', 0x31565254, 1280, 720, 1280*720*4)), (1280,720,1280*720*4))
        for dims in [(0,720,0),(1281,1,5124),(1,721,2884),(10,10,399),(0xffffffff,1,4)]:
            with self.assertRaises(ValueError):frame_header(struct.pack('<IIII',0x31565254,*dims))


if __name__ == '__main__':unittest.main()
