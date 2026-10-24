#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One owned-game capture/encoder, independent inherited grants for P2-P4."""
import array
import json
import os
from pathlib import Path
import select
import signal
import socket
import struct
import subprocess
import sys
import time
import uuid
from lobby_lease import LobbyLease, LobbyRevoked


def request(control):
    data, ancillary, flags, _ = control.recvmsg(4096, socket.CMSG_SPACE(16), socket.MSG_CMSG_CLOEXEC)
    fds = []
    for level, kind, value in ancillary:
        if level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS:
            numbers = array.array('i');numbers.frombytes(value);fds.extend(numbers)
    try:
        if not data:
            raise EOFError
        value = json.loads(data)
        if flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC) or not isinstance(value, dict):
            raise ValueError('Invalid local group request')
        connection = value.get('connection', '')
        if str(uuid.UUID(connection)) != connection:
            raise ValueError('Invalid connection identity')
        if value.get('kind') == 'remove' and set(value) == {'kind', 'connection'} and not fds:
            return value, None
        if (value.get('kind') != 'add' or set(value) != {'kind', 'connection', 'port', 'config'}
                or type(value['port']) is not int or not 2 <= value['port'] <= 4 or len(fds) != 1
                or not isinstance(value['config'], str) or len(value['config']) > 1024
                or not Path(value['config']).is_absolute() or '\n' in value['config'] or '\x00' in value['config']):
            raise ValueError('Invalid local controller request')
        return value, fds.pop()
    finally:
        for fd in fds:os.close(fd)


class Grants:
    def __init__(self, host):
        self.host, self.peers = host, {}

    def remove(self, connection):
        peer = self.peers.pop(connection, None)
        if peer is not None:
            lease, writer, _ = peer
            lease.close();writer.close()
            self.host.send(('remove\n' + connection).encode())

    def add(self, value, fd):
        lease = LobbyLease(fd)
        writer = child = None
        try:
            connection, port = value['connection'], value['port']
            if connection in self.peers or len(self.peers) >= 3 or any(p[2] == port for p in self.peers.values()):
                raise ValueError('Controller is occupied')
            writer, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            writer.setblocking(False)
            packet = f"add\n{port}\n{connection}\n{value['config']}".encode()
            self.host.sendmsg([packet], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [child.fileno()]))])
            self.peers[connection] = lease, writer, port
        except BaseException:
            lease.close()
            if writer is not None:writer.close()
            raise
        finally:
            if child is not None:child.close()

    def tick(self, focused):
        for connection, (lease, writer, _) in list(self.peers.items()):
            try:
                permitted = lease.current()
                writer.send((b'1' if permitted and focused else b'0') + struct.pack('=Q', time.monotonic_ns()))
            except BlockingIOError:
                pass  # Slow receivers lose their grant, never stall other pads.
            except (RuntimeError, BrokenPipeError, ConnectionResetError):
                self.remove(connection)

    def close(self):
        for lease, writer, _ in self.peers.values():lease.close();writer.close()
        self.peers.clear()


def main():
    if len(sys.argv) != 3 or not sys.argv[1].isdigit() or int(sys.argv[1]) <= 1:
        raise SystemExit('Expected owned GAME_PID STREAM_HOST')
    # Reuse the same local credentials/socket validation as controller grants.
    owner = LobbyLease(int(os.environ['TRAINEROS_GROUP_FD']));control = owner.socket
    sys.path.insert(0, '/var/opt/traineros/integrations')
    from game_capture import GameCapture
    from overlay_support import identity
    here = Path(__file__).resolve().parent
    children = [];game = grants = parent = child = None
    def stop(*_):raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    try:
        pid = int(sys.argv[1]);start = identity(pid);deadline = time.monotonic() + 15
        while game is None:
            if identity(pid) != start:raise RuntimeError('Owned game ended before capture')
            try:game = GameCapture(pid)
            except RuntimeError as error:
                if str(error) != 'Game capture unavailable' or time.monotonic() >= deadline:raise
                time.sleep(.05)
        for name in ('capture-game.py', 'capture-game-audio.py'):
            children.append(subprocess.Popen([sys.executable, str(here / name), str(pid)], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE))
        video, audio = children
        parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        env = dict(os.environ, TRAINEROS_GROUP_FD=str(child.fileno()), TRAINEROS_GAME_AUDIO_FD=str(audio.stdout.fileno()))
        env.pop('TRAINEROS_GAME_PAD_FD', None);env.pop('TRAINEROS_LOBBY_INPUT_FD', None)
        host = subprocess.Popen([sys.argv[2], '--group'], env=env, stdin=video.stdout, pass_fds=[child.fileno(), audio.stdout.fileno()])
        children.append(host);child.close();child = None;video.stdout.close();audio.stdout.close()
        grants = Grants(parent)
        while host.poll() is None:
            if not game.alive():raise RuntimeError('Owned game ended')
            if select.select([control], [], [], .05)[0]:
                try:value, fd = request(control)
                except EOFError:break
                if value['kind'] == 'add':grants.add(value, fd)
                else:grants.remove(value['connection'])
            grants.tick(game.x.owns(game.x.active(), game.pid, game.start))
            if any(p.poll() is not None for p in (video, audio)):raise RuntimeError('Owned capture ended')
        return host.returncode or 0
    except KeyboardInterrupt:
        return 130
    finally:
        if grants is not None:grants.close()
        if parent is not None:parent.close()
        if child is not None:child.close()
        # Closing the control endpoint asks the group to release its encoder.
        for process in reversed(children):
            try:process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()
            if process.stdout is not None:process.stdout.close()
        if game is not None:game.close()
        owner.close()


if __name__ == '__main__':
    raise SystemExit(main())
