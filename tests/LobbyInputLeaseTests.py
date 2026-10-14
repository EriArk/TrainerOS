"""Local IPC lifetime checks; no network or game required."""
import os
from pathlib import Path
import socket
import struct
import sys
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/remote-play-stream'))
from lobby_lease import LobbyLease


@unittest.skipUnless(sys.platform == 'linux', 'Linux inherited SEQPACKET lease')
class LeaseTests(unittest.TestCase):
    def setUp(self):
        self.writer, reader = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.lease = LobbyLease(reader.detach())

    def tearDown(self):
        self.writer.close()
        self.lease.close()

    def send(self, stamp):
        self.writer.send(b'1' + struct.pack('=Q', stamp))

    def test_no_grant_and_expired_grant_never_authorize(self):
        self.assertFalse(self.lease.current())
        now = time.monotonic_ns()
        self.send(now)
        self.assertTrue(self.lease.current())
        with patch('lobby_lease.time.monotonic_ns', return_value=now + LobbyLease.TIMEOUT_NS + 1):
            self.assertFalse(self.lease.current())

    def test_closed_authority_revokes_even_with_queued_permission(self):
        self.send(time.monotonic_ns())
        self.writer.close()
        with self.assertRaises(RuntimeError):
            self.lease.current()

    def test_startup_backlog_needs_a_fresh_grant(self):
        now = time.monotonic_ns()
        self.send(now - LobbyLease.TIMEOUT_NS - 1)
        self.assertFalse(self.lease.current())
        self.send(now)
        self.assertTrue(self.lease.current())

    def test_replay_does_not_extend_grant(self):
        now = time.monotonic_ns()
        self.send(now)
        self.assertTrue(self.lease.current())
        self.send(now)
        with self.assertRaises(RuntimeError):
            self.lease.current()

    def test_timestamp_is_checked_after_receiving(self):
        now = time.monotonic_ns()
        underlying = self.lease.socket
        class ArrivingSocket:
            def recvmsg(self, size):
                # A heartbeat can arrive between entering current and recvmsg.
                clock.return_value = now + 100
                return underlying.recvmsg(size)
        with patch('lobby_lease.time.monotonic_ns', return_value=now) as clock:
            self.send(now + 100)
            self.lease.socket = ArrivingSocket()
            try:
                self.assertTrue(self.lease.current())
            finally:
                self.lease.socket = underlying

    def test_bad_future_and_truncated_packets(self):
        for packet in (b'0' + bytes(8), b'1' + bytes(9), b'1' + struct.pack('=Q', time.monotonic_ns() + 10**10)):
            self.writer.send(packet)
            with self.assertRaises(RuntimeError):
                self.lease.current()

    def test_stream_socket_is_not_a_permission_channel(self):
        writer, reader = socket.socketpair()
        try:
            with self.assertRaises(RuntimeError):
                LobbyLease(reader.detach())
        finally:
            writer.close()


if __name__ == '__main__':
    unittest.main()
