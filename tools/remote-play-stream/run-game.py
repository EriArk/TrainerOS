#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the isolated host against an already running, owned game. Never launch it.

Usage: run-game.py GAME_PID STREAM_HOST PRIVATE_CONFIG
The shared selector validates the game's identity; separate inherited pipes carry
only its video and audio. Exiting the host terminates both producers, not the game.
"""
import os
from pathlib import Path
import signal
import socket
import struct
import subprocess
import sys
import time
from lobby_lease import LobbyLease, LobbyRevoked


def main():
    if len(sys.argv) != 4 or not sys.argv[1].isdigit() or int(sys.argv[1]) <= 1:
        raise SystemExit('Expected GAME_PID STREAM_HOST PRIVATE_CONFIG')
    lobby_fd = os.environ.get('TRAINEROS_LOBBY_INPUT_FD')
    lobby = LobbyLease(int(lobby_fd)) if lobby_fd is not None else None
    sys.path.insert(0, '/var/opt/traineros/integrations')
    from game_capture import GameCapture
    from overlay_support import identity
    game=None
    here = Path(__file__).resolve().parent
    children = []
    supervisor=lease=None
    def stop(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    try:
        pid=int(sys.argv[1]);start=identity(pid);deadline=time.monotonic()+15
        while game is None:
            if identity(pid)!=start:raise RuntimeError('Owned game ended before capture')
            if lobby is not None:lobby.current()
            try:game=GameCapture(pid)
            except RuntimeError as error:
                if str(error)!='Game capture unavailable' or time.monotonic()>=deadline:raise
                time.sleep(.05)
        for name in ('capture-game.py', 'capture-game-audio.py'):
            child = subprocess.Popen([sys.executable, str(here / name), sys.argv[1]],
                                     stdin=subprocess.DEVNULL, stdout=subprocess.PIPE)
            children.append(child)
        video, audio = children
        env = dict(os.environ, TRAINEROS_GAME_AUDIO_FD=str(audio.stdout.fileno()))
        env.pop('TRAINEROS_GAME_PAD_FD',None)
        env.pop('TRAINEROS_LOBBY_INPUT_FD',None)
        descriptors=[audio.stdout.fileno()]
        if lobby is not None:
            supervisor,lease=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
            supervisor.setblocking(False)
            env['TRAINEROS_GAME_PAD_FD']=str(lease.fileno())
            descriptors.append(lease.fileno())
        host = subprocess.Popen([sys.argv[2], sys.argv[3]], env=env,
                                stdin=video.stdout, pass_fds=descriptors)
        children.append(host)
        if lease is not None:
            lease.close();lease=None
        video.stdout.close()
        audio.stdout.close()
        while True:
            if not game.alive():
                raise RuntimeError('Owned game ended')
            if supervisor is not None:
                permitted=lobby.current()
                focused=permitted and game.x.owns(game.x.active(),game.pid,game.start)
                try:
                    supervisor.send((b'1' if focused else b'0')+struct.pack('=Q',time.monotonic_ns()))
                except BlockingIOError:
                    pass # An unresponsive host lets its lease expire.
                except (BrokenPipeError,ConnectionResetError):
                    # The host closes this endpoint when the guest disconnects.
                    return host.wait(timeout=3)
            try:
                return host.wait(timeout=.05)
            except subprocess.TimeoutExpired:
                if any(p.poll() is not None for p in (video, audio)):
                    raise RuntimeError('Owned-game capture ended')
    except LobbyRevoked:
        return 0
    except KeyboardInterrupt:
        return 130
    finally:
        if supervisor is not None:supervisor.close()
        if lease is not None:lease.close()
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
        for child in reversed(children):
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
            if child.stdout is not None:
                child.stdout.close()
        if game is not None:game.close()
        if lobby is not None:lobby.close()


if __name__ == '__main__':
    raise SystemExit(main())
