#!/usr/bin/env python3
"""Actual descriptor/grant isolation for the shared Remote Play supervisor."""
import array
import importlib.util
import json
import os
from pathlib import Path
import socket
import struct
import sys
import time
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1] / 'tools/remote-play-stream'
sys.path.insert(0, str(ROOT))
spec = importlib.util.spec_from_file_location('group', ROOT / 'run-group.py')
group = importlib.util.module_from_spec(spec);spec.loader.exec_module(group)


class GroupTests(unittest.TestCase):
    def test_received_add_owns_exactly_one_descriptor(self):
        a, b = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        writer, lease = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        with a, b, writer, lease:
            value = dict(kind='add', connection=str(uuid.uuid4()), port=3, config='/tmp/private/session.conf')
            a.sendmsg([json.dumps(value).encode()], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [lease.fileno()]))])
            packet, fd = group.request(b)
            self.assertEqual(packet, value)
            with socket.socket(fileno=fd) as received:
                writer.send(b'owned');self.assertEqual(received.recv(20), b'owned')

    def test_rejected_requests_close_received_descriptors(self):
        a, b = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        writer, lease = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        with a, b, writer, lease:
            for port in (1, 5, True):
                before = len(list(Path('/proc/self/fd').iterdir()))
                value = dict(kind='add', connection=str(uuid.uuid4()), port=port, config='/tmp/private/session.conf')
                a.sendmsg([json.dumps(value).encode()], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [lease.fileno()]))])
                with self.assertRaises(ValueError):group.request(b)
                self.assertEqual(len(list(Path('/proc/self/fd').iterdir())), before)

    def test_one_revoked_or_malformed_grant_keeps_other_controller(self):
        for malformed in (False, True):
            host, receiver = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            grants = group.Grants(host);writers, children, ids = [], [], []
            try:
                for port in (2, 3):
                    writer, lease = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET);writers.append(writer)
                    connection = str(uuid.uuid4());ids.append(connection)
                    grants.add(dict(connection=connection, port=port, config='/tmp/private/session.conf'), lease.detach())
                    packet, ancillary, _, _ = receiver.recvmsg(2048, socket.CMSG_SPACE(4))
                    self.assertTrue(packet.startswith(f'add\n{port}\n'.encode()))
                    numbers = array.array('i');numbers.frombytes(ancillary[0][2]);children.append(socket.socket(fileno=numbers[0]))
                    writer.send(b'1' + struct.pack('=Q', time.monotonic_ns()))
                grants.tick(True)
                for child in children:self.assertEqual(child.recv(9)[0:1], b'1')
                if malformed:writers[0].send(b'bad')
                else:writers[0].close()
                grants.tick(True)
                self.assertEqual(receiver.recv(2048), ('remove\n' + ids[0]).encode())
                self.assertEqual(set(grants.peers), {ids[1]})
                self.assertEqual(children[0].recv(9), b'')
                self.assertEqual(children[1].recv(9)[0:1], b'1')
                grants.tick(False);self.assertEqual(children[1].recv(9)[0:1], b'0')
                grants.remove(ids[0]);self.assertEqual(set(grants.peers), {ids[1]})
            finally:
                grants.close();host.close();receiver.close()
                for item in writers + children:item.close()


if __name__ == '__main__':unittest.main()
