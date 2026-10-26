#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded regression checks for the transport adapter, not game evidence."""
import asyncio
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("room_session", Path(__file__).with_name("room-session.py"))
room = importlib.util.module_from_spec(spec)
spec.loader.exec_module(room)


class AdmissionTests(unittest.TestCase):
    def test_group_roster_and_individual_admission(self):
        identity="a"*64
        for players in (2,3,4):
            host={"host":True,"identity":identity,"mode":"nearby","address":"127.0.0.1","players":players,"seat":1}
            seen=set()
            for seat in range(2,players+1):
                guest=dict(host,host=False,seat=seat)
                expected=room.peer_config(host,seat)
                self.assertEqual(expected["identity"],room.peer_config(guest,seat)["identity"])
                self.assertNotIn(expected["identity"],seen);seen.add(expected["identity"])
                self.assertNotIn("seat",expected);self.assertNotIn("players",expected)
                with self.assertRaises(ValueError):room.peer_config(guest,1)
        for players,seat,host in [(1,1,True),(5,1,True),(3,4,False),(3,0,False),(3,1,False),(3,2,True),(True,1,True),(3,2.5,False)]:
            with self.assertRaises(ValueError):room.roster({"host":host,"players":players,"seat":seat})

    def test_burst_keeps_native_boundaries_order_and_control_marker(self):
        queue=asyncio.Queue()
        expected=[bytes([n])*1400 for n in range(20)]
        for packet in expected[1:]: queue.put_nowait(b"D"+packet)
        queue.put_nowait(b"J")
        pending=b"D"+expected[0];received=[];records=0
        while pending != b"J":
            record,pending=room.batch_datagrams(pending,queue)
            self.assertLessEqual(len(record),room.MAX_RECORD)
            received.extend(room.decode_datagrams(record));records+=1
        self.assertEqual(received,expected)
        self.assertEqual(records,2)
        self.assertTrue(queue.empty())
        self.assertEqual(room.batch_datagrams(pending,queue),(b"J",None))

    def test_batch_rejects_truncation_oversize_and_excess_count(self):
        for value in (b"B",b"B\x00",b"B\x00\x00",b"B\x00\x02x",
                      b"B\x10\x01"+b"x"*4097,b"B"+(b"\x00\x01x"*129),
                      b"D"+b"x"*4097,b"B"+b"x"*room.MAX_RECORD):
            with self.assertRaises(ValueError):room.decode_datagrams(value)
        queue=asyncio.Queue()
        for _ in range(128):queue.put_nowait(b"Dx")
        record,pending=room.batch_datagrams(b"Dx",queue)
        self.assertEqual(len(room.decode_datagrams(record)),128)
        self.assertEqual(pending,b"Dx")

    def test_real_helper_endpoint_shapes(self):
        endpoint = {"mode": "nearby", "port": 12345, "fingerprint": "a" * 64, "token": "b" * 32}
        self.assertEqual(room.ready_endpoint({"event": "ready", "endpoint": endpoint}, "nearby"), endpoint)
        code = "1-able-baker-cable-delta-eagle-fable-gamma-hotel"
        self.assertEqual(room.ready_endpoint({"event": "ready", "code": code}, "online"), {"mode": "online", "code": code})
        with self.assertRaises(ValueError): room.ready_endpoint({"event": "ready", "mode": "nearby"}, "nearby")
        with self.assertRaises(ValueError): room.ready_endpoint({"event": "ready", "endpoint": endpoint}, "online")

    def test_private_directory_and_unknown_fields(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder).resolve()
            user = root / "user"
            user.mkdir()
            content = root / "game"
            content.write_bytes(b"3DSX")
            config = {"channel": {"identity": "a" * 64}, "user": str(user), "sysdata": str(user)}
            config.update({key: str(content) for key in ("content", "runtime", "python", "helper")})
            filename = root / "network.json"
            def write(): filename.write_text(json.dumps(config))
            write()
            self.assertEqual(room.read_config(filename)[1], root)
            config["user"] = str(root.parent)
            write()
            with self.assertRaises(ValueError): room.read_config(filename)
            config["user"] = str(user)
            config["unexpected"] = True
            write()
            with self.assertRaises(ValueError): room.read_config(filename)
            del config["unexpected"]
            config["channel"]["identity"] = ""
            write()
            with self.assertRaises(ValueError): room.read_config(filename)

    def test_udp_peer_pin_and_overload_bounds(self):
        queue = asyncio.Queue(maxsize=2)
        bridge = room.DatagramBridge(False, queue, asyncio.Event())
        bridge.datagram_received(b"untrusted", ("192.0.2.1", 123))
        self.assertIsNone(bridge.peer)
        bridge.datagram_received(b"a" * (room.MAX_DATAGRAM + 1), ("127.0.0.1", 123))
        self.assertIsNone(bridge.peer)
        bridge.datagram_received(b"first", ("127.0.0.1", 123))
        bridge.datagram_received(b"wrong port", ("127.0.0.1", 124))
        bridge.datagram_received(b"second", ("127.0.0.1", 123))
        bridge.datagram_received(b"overload", ("127.0.0.1", 123))
        self.assertEqual(queue.qsize(), 2)
        self.assertEqual(queue.get_nowait(), b"Dfirst")
        self.assertEqual(queue.get_nowait(), b"Dsecond")
        host = room.DatagramBridge(True, queue, asyncio.Event())
        host.datagram_received(b"unsolicited", ("127.0.0.1", 123))
        self.assertTrue(queue.empty())
        host.peer = ("127.0.0.1", 125)
        host.datagram_received(b"native room", host.peer)
        self.assertEqual(queue.get_nowait(), b"Dnative room")


if __name__ == "__main__":
    unittest.main()
