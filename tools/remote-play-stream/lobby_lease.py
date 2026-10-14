# SPDX-License-Identifier: GPL-3.0-or-later
"""Consume an inherited host-lobby grant; never accept a client-selected port."""
import os
import socket
import struct
import time


class LobbyRevoked(RuntimeError):
    pass


class LobbyLease:
    TIMEOUT_NS = 350_000_000

    def __init__(self, fd):
        self.socket = socket.socket(fileno=fd)
        try:
            if (fd < 3 or self.socket.family != socket.AF_UNIX
                    or self.socket.getsockopt(socket.SOL_SOCKET, socket.SO_TYPE) != socket.SOCK_SEQPACKET
                    or struct.unpack('3i', self.socket.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))[1] != os.getuid()):
                raise RuntimeError('Expected inherited host-lobby grant')
            self.socket.setblocking(False)
            self.last = 0
        except BaseException:
            self.socket.close()
            raise

    def current(self):
        for _ in range(16):
            try:
                packet, _, flags, _ = self.socket.recvmsg(9)
            except BlockingIOError:
                break
            if not packet:
                raise LobbyRevoked('Lobby input permission ended')
            if len(packet) != 9 or flags & socket.MSG_TRUNC or packet[0:1] != b'1':
                raise RuntimeError('Invalid lobby input permission')
            sent, = struct.unpack('=Q', packet[1:])
            # The producer may send after we enter current(), before recvmsg.
            if sent <= self.last or sent > time.monotonic_ns():
                raise RuntimeError('Lobby input permission expired')
            self.last = sent
        # Missing or delayed first grant never becomes implicit permission.
        return self.last > 0 and time.monotonic_ns() - self.last <= self.TIMEOUT_NS

    def close(self):
        self.socket.close()
