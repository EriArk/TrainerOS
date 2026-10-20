import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location("power_service",Path(__file__).resolve().parents[1]/"packaging/integrations/power-service.py")
power=importlib.util.module_from_spec(spec);spec.loader.exec_module(power)

class PowerButtonTests(unittest.TestCase):
    def test_short_press_sleeps_only_on_release(self):
        button=power.Button()
        self.assertIsNone(button.event(1,10))
        self.assertIsNone(button.event(2,10.2))
        self.assertEqual(button.event(0,10.3),"sleep")
        self.assertIsNone(button.event(0,10.4))
    def test_long_hold_opens_once_without_release_sleep(self):
        button=power.Button();button.event(1,10)
        self.assertIsNone(button.tick(11.9))
        self.assertEqual(button.tick(12),"menu")
        self.assertIsNone(button.tick(13))
        self.assertIsNone(button.event(0,14))
    def test_delayed_timer_still_recognizes_long_hold(self):
        button=power.Button();button.event(1,10)
        self.assertEqual(button.event(0,13),"menu")
    def test_wake_release_cannot_sleep_again(self):
        button=power.Button();button.event(1,10);button.woke(20)
        self.assertIsNone(button.event(0,20.1))
        self.assertIsNone(button.event(1,20.2))
        self.assertIsNone(button.event(0,20.3))
        self.assertIsNone(button.event(0,22))
        button.event(1,23)
        self.assertEqual(button.event(0,23.2),"sleep")

if __name__=="__main__":unittest.main()
