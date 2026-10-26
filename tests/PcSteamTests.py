import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

if sys.platform == "win32":
    sys.modules["fcntl"] = types.SimpleNamespace(flock=lambda *args: None, LOCK_EX=2, LOCK_NB=4)
spec = importlib.util.spec_from_file_location("pc_steam", Path(__file__).resolve().parents[1] / "packaging/pc/steam.py")
steam = importlib.util.module_from_spec(spec); spec.loader.exec_module(steam)


class SteamShortcuts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="Steam shortcuts ")
        self.config = Path(self.temp.name) / "userdata/42/config"
        self.config.mkdir(parents=True)
        self.path = self.config / "shortcuts.vdf"
        self.unrelated = {"shortcuts": (0, {"0": (0, {"appname": (1, "Owner's game"), "exe": (1, "another program"),
            "unknown-float": (3, b"\x01\x02\x03\x04"), "tags": (0, {"0": (1, "Favorites")})})})}
        self.original = steam.encode(self.unrelated)
        self.path.write_bytes(self.original)
        self.request = dict(operation="add", config=str(self.config), id="pc:65aec49c-67dc-4ee0-84cb-c3148c5f59a1",
                            owner="a" * 64, state=str(Path(self.temp.name) / "state"), title="My imported game")

    def tearDown(self):
        self.temp.cleanup()

    def update(self, **kwargs):
        return steam.update(dict(self.request, **kwargs), running=lambda: False)

    def test_add_repeat_rename_remove_preserves_unrelated_and_backup(self):
        self.update()
        first = self.path.read_bytes()
        self.update()
        self.assertEqual(first, self.path.read_bytes())
        self.update(title="Renamed game")
        tree = steam.decode(self.path.read_bytes())
        self.assertEqual(len(tree["shortcuts"][1]), 2)
        self.assertEqual(tree["shortcuts"][1]["0"], self.unrelated["shortcuts"][1]["0"])
        self.assertEqual(tree["shortcuts"][1]["1"][1]["appname"], (1, "Renamed game"))
        self.assertTrue(self.update(operation="status"))
        self.update(operation="remove")
        self.assertEqual(self.path.read_bytes(), self.original)
        self.assertFalse(self.update(operation="status"))
        backups = list(self.config.glob("*.bak"))
        self.assertTrue(any(p.read_bytes() == self.original for p in backups))

    def test_running_steam_refuses_without_writing(self):
        with self.assertRaisesRegex(RuntimeError, "Close Steam"):
            steam.update(self.request, running=lambda: True)
        self.assertEqual(self.path.read_bytes(), self.original)

    def test_concurrent_change_preserved(self):
        calls = 0
        def running():
            nonlocal calls
            calls += 1
            if calls == 2:
                self.path.write_bytes(b"external update")
            return False
        with self.assertRaisesRegex(RuntimeError, "Steam changed"):
            steam.update(self.request, running=running)
        self.assertEqual(self.path.read_bytes(), b"external update")

    def test_changed_owned_entry_not_overwritten(self):
        self.update()
        tree = steam.decode(self.path.read_bytes())
        tree["shortcuts"][1]["1"][1]["LaunchOptions"] = (1, "owner changed this")
        original = steam.encode(tree); self.path.write_bytes(original)
        with self.assertRaisesRegex(ValueError, "changed externally"): self.update()
        self.assertEqual(self.path.read_bytes(), original)

    def test_malformed_database_and_backup_failure_keep_original(self):
        for data in (b"\0shortcuts\0", b"\x09oops\0\x08", self.original + b"extra"):
            self.path.write_bytes(data)
            with self.assertRaises(ValueError): self.update()
            self.assertEqual(self.path.read_bytes(), data)
        self.path.write_bytes(self.original)
        backup = self.config / ("shortcuts.vdf.traineros-" + steam.hashlib.sha256(self.original).hexdigest()[:20] + ".bak")
        backup.write_bytes(b"bad backup")
        with self.assertRaisesRegex(RuntimeError, "backup"): self.update()
        self.assertEqual(self.path.read_bytes(), self.original)

    def test_shortcut_contains_only_stable_identity_and_owner(self):
        self.update()
        entry = steam.decode(self.path.read_bytes())["shortcuts"][1]["1"][1]
        options = entry["LaunchOptions"][1]
        self.assertIn(self.request["id"], options)
        self.assertIn(self.request["owner"], options)
        self.assertNotIn("proton", options.lower())
        self.assertNotIn("game.exe", options)

    def test_launch_argument_quoting_preserves_literal_shell_characters(self):
        value = "folder with ' quotes $(literal) `literal` $HOME"
        self.assertEqual(steam.shlex.split(steam.quote(value)), [value])
        with self.assertRaises(ValueError): steam.quote("newline\nargument")


if __name__ == "__main__":
    unittest.main()
