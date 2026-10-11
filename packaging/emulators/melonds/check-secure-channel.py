#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""ROM-free application-boundary checks; no public relay or dependencies needed."""
import base64
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("secure_channel", Path(__file__).with_name("secure-channel.py"))
channel = importlib.util.module_from_spec(spec)
spec.loader.exec_module(channel)


class RecordsTests(unittest.TestCase):
    def test_nearby_is_explicit_pinned_and_has_no_cloud_fields(self):
        host = {"host": True, "identity": "a"*64, "mode": "nearby", "address": "192.168.1.2"}
        self.assertEqual(channel.validate(host), bytes.fromhex("a"*64))
        guest = dict(host, host=False, port=1234, token="b"*32, fingerprint="c"*64)
        self.assertEqual(channel.validate(guest), bytes.fromhex("a"*64))
        for changes in ({"address":"0.0.0.0"},{"address":"224.0.0.1"},{"address":1234},
                        {"port":True},{"token":"wrong"},{"fingerprint":""},{"code":"1-x"},{"relayOnly":False}):
            with self.assertRaises((ValueError, TypeError)):
                channel.validate(dict(guest, **changes))

    def pair(self):
        self.time = 0
        self.wire = [[], []]
        self.events = [[], []]
        peers = [channel.Records(b"x" * 32, self.wire[i].append, self.events[i].append,
                                 lambda: self.time) for i in range(2)]
        for peer in peers:
            peer.start()
        for i in range(2):
            peers[1-i].receive(self.wire[i].pop())
        self.assertTrue(all(p.verified for p in peers))
        return peers

    def test_credits_wait_for_parent_and_exact_order(self):
        a, b = self.pair()
        payload = bytes(range(256)) * 64
        for _ in range(channel.WINDOW):
            a.command({"send": base64.b64encode(payload).decode()})
            b.receive(self.wire[0].pop(0))
        self.assertEqual(len(a.outgoing), channel.WINDOW)
        self.assertEqual(len(b.incoming), channel.WINDOW)
        self.assertFalse(self.wire[1])
        with self.assertRaises(ValueError):
            a.command({"send": "YQ=="})
        with self.assertRaises(ValueError):
            b.command({"consume": 1})
        for i in range(channel.WINDOW):
            b.command({"consume": i})
            a.receive(self.wire[1].pop(0))
        self.assertFalse(a.outgoing or b.incoming)
        self.assertEqual(base64.b64decode(self.events[1][-1]["data"]), payload)
        a.command({"send": "YQ=="})

    def test_identity_replay_and_record_bounds(self):
        a, b = self.pair()
        for record in (b"", b"H"+b"x"*32, b"D"+struct.pack("!I",1)+b"x",
                       b"D"+b"\0"*4, b"A"+b"\0"*4, b"D"+b"\0"*(channel.MAX_RECORD+5)):
            with self.assertRaises(ValueError):
                b.receive(record)
        bad = channel.Records(b"a"*32, lambda _: None, lambda _: None)
        with self.assertRaises(ValueError):
            bad.receive(b"H"+b"b"*32)
        a.command({"send": "YQ=="})
        record = self.wire[0].pop(0)
        b.receive(record)
        with self.assertRaises(ValueError):
            b.receive(record)
        with self.assertRaises(ValueError):
            b.command({"consume": False})

    def test_stall_and_invalid_local_data(self):
        a, b = self.pair()
        for value in ({"send": "!"}, {"send": ""}, {"send": "a"*30000}, {"send": []},
                      {"send": "YQ==", "consume": 0}, {"consume": 0}):
            with self.assertRaises(ValueError):
                a.command(value)
        a.command({"send": "YQ=="})
        b.receive(self.wire[0].pop(0))
        self.time = 13
        for peer in (a, b):
            with self.assertRaises(ValueError):
                peer.check_timeout()

    def test_config_types_and_unknown_fields(self):
        valid = {"host": True, "identity": "a"*64}
        self.assertEqual(channel.validate(valid), b"\xaa"*32)
        for change in ({"host": 1}, {"identity": "A"*64}, {"identity": 1},
                       {"relayOnly": "yes"}, {"path": "/save"}, {"code": "secret"}):
            with self.assertRaises(ValueError):
                channel.validate(dict(valid, **change))
        with self.assertRaises(ValueError):
            channel.validate(dict(valid, host=False))


if __name__ == "__main__":
    unittest.main()
