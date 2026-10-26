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
    sys.modules.setdefault("fcntl", types.ModuleType("fcntl"))

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
