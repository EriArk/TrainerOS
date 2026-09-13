"""Real Linux descendant/restart tests; no game files or compositor required."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class SupervisorTests(unittest.TestCase):
    def run_probe(self, source, attempts=3, ready_timeout=None):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / "probe.py").write_text(source)
            wrapper = f"""
import importlib.util, sys
spec=importlib.util.spec_from_file_location('client',{str(ROOT / 'packaging/session/client.py')!r})
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
sys.exit(module.supervise([sys.executable,{str(folder / 'probe.py')!r}],attempts={attempts},ready_timeout={ready_timeout!r}))
"""
            result = subprocess.run([sys.executable, "-c", wrapper], cwd=folder, capture_output=True, timeout=12)
            contents = {p.name: p.read_text() for p in folder.glob("*.txt")}
            return result, contents

    def test_success_does_not_restart(self):
        result, files = self.run_probe("from pathlib import Path\nPath('runs.txt').open('a').write('x')\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(files["runs.txt"], "x")

    def test_crash_loop_is_bounded(self):
        result, files = self.run_probe("from pathlib import Path\nimport os\nPath('runs.txt').open('a').write('x')\nos._exit(2)\n")
        self.assertEqual(result.returncode, 70, result.stderr)
        self.assertEqual(files["runs.txt"], "xxx")

    def test_startup_hang_has_a_deadline(self):
        result, _ = self.run_probe("import time\ntime.sleep(10)\n", ready_timeout=0.2)
        self.assertEqual(result.returncode, 70, result.stderr)

    def test_render_readiness_accepts_running_application(self):
        result, _ = self.run_probe("import os,time\nos.write(int(os.environ['TRAINEROS_READY_FD']), b'R')\ntime.sleep(0.4)\n", ready_timeout=0.2)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_orphan_finishes_before_shell_restarts(self):
        result, files = self.run_probe("""
from pathlib import Path
import os,subprocess,sys
if Path('runs.txt').exists():
    assert Path('saved.txt').read_text() == 'completed'
    Path('runs.txt').open('a').write('r')
else:
    Path('runs.txt').write_text('c')
    subprocess.Popen([sys.executable,'-c',"import time;from pathlib import Path;time.sleep(1.5);Path('saved.txt').write_text('completed')"])
    os._exit(9)
""")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(files["runs.txt"], "cr")
        self.assertEqual(files["saved.txt"], "completed")


class TransitionTests(unittest.TestCase):
    def test_boot_default_does_not_restart_display_manager(self):
        spec = importlib.util.spec_from_file_location("control", ROOT / "packaging/session/control.py")
        control = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(control)
        with tempfile.TemporaryDirectory() as temporary:
            control.OVERRIDE = Path(temporary) / "autologin.conf"
            control.OVERRIDE.write_text("[Autologin]\nSession=gamescope-session-steam.desktop\n")
            with patch.object(control.os, "geteuid", return_value=0), patch.object(control.sys, "argv", ["control.py", "default-traineros"]), patch.object(control.Path, "is_file", return_value=True), patch.dict(os.environ), patch.object(control.subprocess, "call") as execute:
                self.assertEqual(control.main(), 0)
                execute.assert_not_called()
            self.assertEqual(control.OVERRIDE.read_text(), "[Autologin]\nSession=traineros.desktop\n")

    def test_marker_belongs_only_to_old_session(self):
        spec = importlib.util.spec_from_file_location("control", ROOT / "packaging/session/control.py")
        control = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(control)
        with tempfile.TemporaryDirectory() as temporary:
            control.MARKER = Path(temporary) / "transition"
            with patch.object(control.time, "monotonic", return_value=100):
                control.atomic_write(control.MARKER, json.dumps({"pids": [42], "time": 99}))
                self.assertFalse(control.consume_transition(43))
                self.assertTrue(control.consume_transition(42))
                self.assertFalse(control.consume_transition(42))
                control.atomic_write(control.MARKER, json.dumps({"pids": [42], "time": 1}))
                self.assertFalse(control.consume_transition(42))


if __name__ == "__main__":
    unittest.main()
