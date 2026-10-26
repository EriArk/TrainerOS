import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
import types

if sys.platform == "win32":
    sys.modules.setdefault("fcntl", types.SimpleNamespace(flock=lambda *args: None, LOCK_EX=2, LOCK_NB=4))

HELPER = Path(__file__).resolve().parents[1] / "packaging/pc/launch.py"
spec = importlib.util.spec_from_file_location("pc_launch", HELPER)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class InstallerCommands(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="PC install ")
        self.root = Path(self.temp.name)
        self.source = self.root / "source with spaces.msi"
        self.source.write_bytes(b"fixture")
        self.install = self.root / "new game"
        self.install.mkdir()
        self.proton = self.root / "proton"
        self.proton.mkdir()
        (self.proton / "proton").touch()
        self.umu = self.root / "umu-run"
        self.umu.touch()

    def tearDown(self):
        self.temp.cleanup()

    def config(self):
        return dict(mode="installer", executable=str(self.source), installRoot=str(self.install),
                    workingDirectory=str(self.install), runner="proton", installerFormat="msi",
                    proton=str(self.proton), umu=str(self.umu), prefix=str(self.install / "prefix"))

    def test_msi_uses_msiexec_in_owned_prefix(self):
        config = self.config()
        argv, cwd, env = module.command(config)
        self.assertEqual(argv, [sys.executable, str(self.umu), "msiexec", "/i", str(self.source)])
        self.assertEqual(env["WINEPREFIX"], config["prefix"])
        self.assertEqual(env["PROTON_VERB"], "waitforexitandrun")
        self.assertEqual(cwd, str(self.install))

    def test_external_installer_does_not_relax_game_path_boundary(self):
        config = self.config()
        del config["mode"]
        with self.assertRaises(ValueError): module.command(config)

    def test_patch_keeps_existing_prefix_and_msi_invocation(self):
        config = self.config()
        config.update(mode="maintenance", operation="patch")
        argv, cwd, env = module.command(config)
        self.assertEqual(argv[-3:], ["msiexec", "/i", str(self.source)])
        self.assertEqual(env["WINEPREFIX"], config["prefix"])
        config["operation"] = "tool"
        with self.assertRaises(ValueError): module.command(config)

    def test_components_use_umu_not_system_wine_and_refuse_unknown_verbs(self):
        game = self.install / "game.exe"
        game.touch()
        config = self.config()
        config.update(mode="maintenance", operation="component", operationValue="vcrun2022", executable=str(game))
        with self.assertRaisesRegex(ValueError, "Winetricks"): module.command(config)
        (self.proton / "protonfixes").mkdir()
        (self.proton / "protonfixes" / "winetricks").touch()
        argv, _, env = module.command(config)
        self.assertEqual(argv, [sys.executable, str(self.umu), "winetricks", "vcrun2022"])
        self.assertEqual(env["WINEPREFIX"], config["prefix"])
        config["operationValue"] = "--force; touch /tmp/bad"
        with self.assertRaises(ValueError): module.command(config)

    def test_two_games_keep_distinct_prefixes_with_one_runtime(self):
        first = self.config()
        second = dict(first, prefix=str(self.install / "second-prefix"))
        a = module.command(first); b = module.command(second)
        self.assertEqual(a[0], b[0])
        self.assertNotEqual(a[2]["WINEPREFIX"], b[2]["WINEPREFIX"])

    def test_runtime_migration_keeps_original_and_does_not_repeat(self):
        original = self.install / "prefix"
        original.mkdir()
        (original / "save.dat").write_bytes(b"precious save")
        config = self.config()
        config.update(prefix=str(self.install / "new-prefix"), prefixSource=str(original))
        module.prepare_prefix(config)
        new = Path(config["prefix"])
        self.assertEqual((new / "save.dat").read_bytes(), b"precious save")
        (new / "save.dat").write_bytes(b"new session")
        module.prepare_prefix(config)
        self.assertEqual((new / "save.dat").read_bytes(), b"new session")
        self.assertEqual((original / "save.dat").read_bytes(), b"precious save")

    def test_failed_migration_never_publishes_partial_prefix(self):
        from unittest.mock import patch
        original = self.install / "prefix"
        original.mkdir()
        (original / "save.dat").write_bytes(b"precious save")
        config = self.config()
        config.update(prefix=str(self.install / "new-prefix"), prefixSource=str(original))
        with patch.object(module.shutil, "copytree", side_effect=OSError("disk full")):
            with self.assertRaises(OSError): module.prepare_prefix(config)
        self.assertFalse(Path(config["prefix"]).exists())
        self.assertEqual((original / "save.dat").read_bytes(), b"precious save")
        self.assertEqual(list(self.install.glob(".prefix-copy-*")), [])
        config["prefixSource"] = str(self.install / "disconnected")
        with self.assertRaisesRegex(ValueError, "Reconnect"): module.prepare_prefix(config)

    def test_drive_c_install_follows_selected_environment(self):
        original = self.install / "prefix"
        game_dir = original / "drive_c/Game"
        game_dir.mkdir(parents=True)
        (game_dir / "play.exe").write_bytes(b"game")
        config = self.config()
        config.update(mode="game", executable=str(game_dir / "play.exe"), installRoot=str(original / "drive_c"),
                      workingDirectory=str(game_dir), prefixSource=str(original), contentPrefix=str(original),
                      prefix=str(self.install / "selected-prefix"))
        module.prepare_prefix(config)
        argv, cwd, env = module.command(config)
        self.assertEqual(argv[-1], str(Path(config["prefix"]) / "drive_c/Game/play.exe"))
        self.assertEqual(cwd, str(Path(config["prefix"]) / "drive_c/Game"))
        self.assertTrue((game_dir / "play.exe").exists())

    def test_runtime_pin_before_first_launch_does_not_require_an_old_prefix(self):
        game = self.install / "play.exe"
        game.touch()
        config = self.config()
        config.update(mode="game", executable=str(game), prefixSource=str(self.install / "unused-prefix"),
                      prefixSourceExisted=False, contentPrefix=str(self.install / "unused-prefix"),
                      prefix=str(self.install / "new-prefix"))
        module.prepare_prefix(config)
        self.assertEqual(module.command(config)[0][-1], str(game))

    def test_steam_context_is_bounded_and_does_not_leak_to_direct_launch(self):
        from unittest.mock import patch
        config = self.config()
        with patch.dict(os.environ, {"SteamAppId": "999"}):
            self.assertNotIn("SteamAppId", module.command(config)[2])
            config["steamEnvironment"] = {"SteamAppId": "123", "SteamGameId": "bad", "UNTRUSTED_TEST_KEY": "bad"}
            env = module.command(config)[2]
            self.assertEqual(env["SteamAppId"], "123")
            self.assertNotIn("SteamGameId", env)
            self.assertNotIn("UNTRUSTED_TEST_KEY", env)

    def test_native_script_interpreter_is_explicit_and_no_shell_interpolation(self):
        self.source.write_bytes(b"#!/bin/sh\nexit 0\n")
        config = self.config()
        config.update(runner="native", installerFormat="script", arguments=["$(touch stolen)"])
        argv, cwd, env = module.command(config)
        self.assertEqual(argv, ["/bin/sh", str(self.source), "$(touch stolen)"])
        self.assertEqual(env["TRAINEROS_INSTALL_DIRECTORY"], str(self.install))
        self.source.write_bytes(b"#!/usr/bin/python3\n")
        with self.assertRaises(ValueError): module.command(config)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux subreaper")
class PcSessions(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pc session ")
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def start(self, body):
        script = self.root / "game with spaces"
        script.write_text("#!/usr/bin/python3\n" + body)
        script.chmod(0o700)
        config = dict(executable=str(script), installRoot=str(self.root), runner="native")
        return subprocess.Popen([sys.executable, str(HELPER), json.dumps(config)])

    def test_detached_child_keeps_session_alive(self):
        p = self.start("import os,time\nfrom pathlib import Path\n"
                       "if os.fork(): os._exit(0)\nos.setsid()\n"
                       "if os.fork(): os._exit(0)\n"
                       "Path('ready').touch()\ntime.sleep(.4)\nPath('done').touch()\n")
        self.assertEqual(p.wait(timeout=5), 0)
        self.assertTrue((self.root / "done").exists())

    def test_cancellation_kills_only_owned_detached_children(self):
        unrelated = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(20)"])
        p = self.start("import os,time,signal\nfrom pathlib import Path\n"
                       "if os.fork(): os._exit(0)\nos.setsid()\n"
                       "signal.signal(signal.SIGTERM,signal.SIG_IGN)\n"
                       "Path('pid').write_text(str(os.getpid()))\ntime.sleep(20)\n")
        try:
            limit = time.monotonic() + 4
            while not (self.root / "pid").exists() and time.monotonic() < limit:
                time.sleep(.02)
            pid = int((self.root / "pid").read_text())
            p.terminate()
            self.assertEqual(p.wait(timeout=1.4), 130)
            self.assertFalse(Path(f"/proc/{pid}").exists())
            self.assertIsNone(unrelated.poll())
        finally:
            if p.poll() is None:
                p.terminate(); p.wait(timeout=3)
            unrelated.terminate(); unrelated.wait(timeout=3)

    def test_real_exit_failure_is_not_success(self):
        p = self.start("raise SystemExit(23)\n")
        self.assertEqual(p.wait(timeout=3), 23)

    def test_outside_game_symlink_rejected(self):
        exe = self.root / "escape"
        exe.symlink_to(sys.executable)
        with self.assertRaises(ValueError):
            module.command(dict(executable=str(exe),installRoot=str(self.root),runner="native"))

    def test_arguments_are_not_shell_code(self):
        p = self.start("import sys\nfrom pathlib import Path\nPath('args').write_text(str(sys.argv))\n")
        self.assertEqual(p.wait(timeout=3),0)
        self.assertIn("game with spaces",(self.root / "args").read_text())

    def test_native_xdg_state_is_separate_from_game_files(self):
        exe=self.root / "game"
        exe.write_text("#!/bin/sh\nexit 0\n"); exe.chmod(0o700)
        data=self.root / "personal" / "game-id"
        _,_,env=module.command(dict(executable=str(exe),installRoot=str(self.root),runner="native",dataRoot=str(data)))
        self.assertEqual(env["XDG_CONFIG_HOME"],str(data / "config"))
        self.assertEqual(env["XDG_DATA_HOME"],str(data / "share"))
        self.assertTrue(data.is_dir())
        self.assertEqual(env["TRAINEROS_PC_MANAGED"], "1")


if __name__ == "__main__":
    unittest.main()
