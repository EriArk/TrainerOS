#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run with the installed helper's Python. No game data or public service required."""
import base64
import importlib.util
import json
from pathlib import Path
import selectors
import subprocess
import sys
import unittest

HELPER = Path(__file__).with_name("linked-save.py")
spec = importlib.util.spec_from_file_location("linked_save", HELPER)
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.children = []

    def tearDown(self):
        for child in self.children:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=5)
            for stream in (child.stdin, child.stdout, child.stderr):
                stream.close()

    def start(self, config):
        child = subprocess.Popen([sys.executable, "-I", str(HELPER)],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
        self.children.append(child)
        child.stdin.write(json.dumps(config).encode() + b"\n")
        child.stdin.flush()
        return child

    def event(self, child):
        with selectors.DefaultSelector() as selector:
            selector.register(child.stdout, selectors.EVENT_READ)
            self.assertTrue(selector.select(15), "helper timed out")
        line = child.stdout.readline(262145)
        self.assertLessEqual(len(line), 262144)
        return json.loads(line)

    def host(self, size=131072):
        config = dict(host=True, mode="nearby", address="127.0.0.1",
                      content="a" * 64, size=size)
        child = self.start(config)
        ready = self.event(child)
        self.assertEqual(ready["event"], "ready")
        return child, dict(config, **ready["endpoint"])

    def guest(self, config, **changes):
        data = bytes(range(256)) * (config["size"] // 256)
        guest = dict(config, host=False, data=base64.b64encode(data).decode())
        guest.update(changes)
        return self.start(guest), data

    def test_largest_save_and_retry_after_wrong_certificate(self):
        host, config = self.host()
        wrong, _ = self.guest(config, fingerprint="0" * 64)
        self.assertEqual(self.event(wrong), {"event": "error"})
        self.assertNotEqual(wrong.wait(timeout=5), 0)
        guest, expected = self.guest(config)
        received = self.event(host)
        self.assertEqual(received["event"], "done")
        self.assertEqual(base64.b64decode(received["data"]), expected)
        self.assertEqual(self.event(guest), {"event": "done", "data": ""})
        self.assertEqual(host.wait(timeout=5), 0)
        self.assertEqual(guest.wait(timeout=5), 0)

    def test_wrong_identity_does_not_consume_host(self):
        host, config = self.host(8192)
        wrong, _ = self.guest(config, content="b" * 64)
        self.assertEqual(self.event(wrong), {"event": "error"})
        self.assertNotEqual(wrong.wait(timeout=5), 0)
        guest, expected = self.guest(config)
        self.assertEqual(base64.b64decode(self.event(host)["data"]), expected)
        self.assertEqual(self.event(guest)["event"], "done")

    def test_parent_exit_cancels_listener(self):
        host, _ = self.host()
        host.stdin.close()
        self.assertEqual(host.wait(timeout=5), 0)
        self.assertEqual(host.stdout.read(), b"")

    def test_invalid_seed_and_size_rejected_before_connection(self):
        config = dict(host=False, mode="online", content="a" * 64,
                      size=8192, data=base64.b64encode(b"short").decode())
        for changed in ({}, {"size": 131073}, {"data": "not base64"},
                        {"content": "wrong"}, {"host": "yes"}):
            with self.subTest(changed=changed):
                child = self.start(dict(config, **changed))
                self.assertEqual(self.event(child), {"event": "error"})
                self.assertNotEqual(child.wait(timeout=5), 0)


if __name__ == "__main__":
    unittest.main()
