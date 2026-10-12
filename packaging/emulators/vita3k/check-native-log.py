#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
import native_log


class NativeLog(unittest.TestCase):
    def test_short_output_is_visible_before_game_closes(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'native.log'
            child=subprocess.Popen([sys.executable,'-u','-c',"import sys;print('started');sys.stdin.read()"],
                stdin=subprocess.PIPE,stdout=subprocess.PIPE)
            try:
                thread=native_log.capture(child.stdout,path)
                deadline=time.monotonic()+3
                while not path.read_bytes() and time.monotonic()<deadline:time.sleep(.01)
                self.assertIsNone(child.poll());self.assertEqual(path.read_bytes().replace(b'\r\n',b'\n'),b'started\n')
            finally:
                child.stdin.close();child.wait(timeout=3);thread.join(timeout=3)

    def test_log_limit_does_not_limit_game_files_or_block_output(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);save=root/'progress';log=root/'native.log'
            child=subprocess.Popen([sys.executable,'-u','-c',
                "import sys;from pathlib import Path;Path(sys.argv[1]).write_bytes(b's'*34000000);sys.stdout.buffer.write(b'l'*1000000)",str(save)],stdout=subprocess.PIPE)
            thread=native_log.capture(child.stdout,log,limit=64)
            self.assertEqual(child.wait(timeout=10),0);thread.join(timeout=3)
            self.assertFalse(thread.is_alive());self.assertEqual(save.stat().st_size,34000000)
            self.assertEqual(log.read_bytes(),b'l'*64)


if __name__=='__main__':unittest.main()
