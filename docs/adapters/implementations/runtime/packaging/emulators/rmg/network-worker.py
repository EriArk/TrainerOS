#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the unchanged RMG server/core inside one admitted private native LAN."""
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import socket
import subprocess
import sys
import threading
import websocket

import island
import native_log
from native_lobby import join
from session import read_config


def main(path, control_fd, outside_namespace):
    config = read_config(path)
    runtime = config['runtime']
    if set(runtime) != {'root', 'rom', 'content', 'runtime', 'token', 'renderer', 'user'}:
        raise ValueError('native runtime fields')
    root, rom, user = (Path(runtime[key]) for key in ('root', 'rom', 'user'))
    folder = Path(path).parent
    if (not root.is_absolute() or not root.is_dir() or not rom.is_absolute() or not rom.is_file()
            or not 4096 <= rom.stat().st_size <= 64*1024**2
            or hashlib.sha256(rom.read_bytes()).hexdigest() != runtime['content']
            or runtime['renderer'] not in ('vulkan', 'opengl')
            or user.is_symlink() or not user.is_absolute() or not user.is_dir()
            or user.resolve().parent != folder.resolve()):
        raise ValueError('native runtime identity')
    for name in ('bin/RMG', 'bin/rmg-netplay-server'):
        executable = root/name
        if not executable.is_file() or not executable.resolve().is_relative_to(root.resolve()):
            raise ValueError('native runtime path')
    os.umask(0o077)
    control = island.open_interface(config['slot'], control_fd, outside_namespace)
    native = server = ws = None
    parent = os.getpid()
    # These processes own only disposable N64 progress. A crashed worker must
    # not leave an invisible emulator or private server detached from its party.
    guard = island.death_guard(parent, signal.SIGKILL)

    def cancelled():
        ready, _, _ = select.select([control], [], [], 0)
        return bool(ready)

    def send(value):
        control.send(json.dumps(value, separators=(',', ':')).encode())

    try:
        control.settimeout(180)
        if control.recv(32) != b'start':
            raise ValueError('native start cancelled')
        control.settimeout(None)
        if config['slot'] == 1:
            server = subprocess.Popen([str(root/'bin/rmg-netplay-server'), '--name', 'TrainerOS',
                '--disable-broadcast', '--verify-ip', '--close-on-finish', '--timeout', '2'],
                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                start_new_session=True, preexec_fn=guard)
            native_log.capture(server.stdout, folder/'server.log')
        ws, admission = join(dict(runtime, slot=config['slot'], seats=config['seats']), cancelled,
                             lambda: send({'room': True}))
        settings = user/'session.json'
        settings.write_text(json.dumps(dict(protocol=1, address='10.203.0.1', port=45001,
                                            controller=admission.controller, renderer=runtime['renderer'])))
        env = dict(os.environ)
        for key in ('QT_QUICK_BACKEND', 'QSG_RHI_BACKEND', 'QT_QPA_PLATFORM', 'VK_ICD_FILENAMES', 'LD_LIBRARY_PATH'):
            env.pop(key, None)
        env.update(XDG_CONFIG_HOME=str(user/'config'), XDG_DATA_HOME=str(user/'data'),
                   XDG_CACHE_HOME=str(user/'cache'), TRAINEROS_RMG_SESSION=str(settings))
        args = [str(root/'bin/RMG'), '--lib-path', str(root/'lib/RMG'),
                '--core-path', str(root/'lib/RMG/Core'), '--plugin-path', str(root/'lib/RMG/Plugin'),
                '--shared-data-path', str(root/'share/RMG'), '-f', '-n', '-q', str(rom)]
        native = subprocess.Popen(args, cwd=user, env=env, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True, preexec_fn=guard)
        started, failed = threading.Event(), threading.Event()
        pending = bytearray()

        def observe(data):
            pending.extend(data)
            while b'\n' in pending:
                end = pending.index(b'\n')
                line = bytes(pending[:end])
                del pending[:end+1]
                if line == b'[TrainerOSRmg] running':
                    started.set()
                elif line == b'[TrainerOSRmg] error':
                    failed.set()
            if len(pending) > 4096:
                pending.clear()

        native_log.capture(native.stdout, folder/'native.log', observe=observe)
        send({'native': native.pid})
        announced = False
        while native.poll() is None and not failed.is_set():
            if started.is_set() and not announced:
                announced = True
                send({'running': True})
            readable, _, _ = select.select([control, ws.sock], [], [], .25)
            if control in readable:
                break
            if ws.sock in readable:
                try:
                    data = ws.recv()
                except websocket.WebSocketTimeoutException:
                    continue
                if not isinstance(data, str) or len(data) > 16384:
                    raise ValueError('native lobby frame')
                if admission.receive(json.loads(data)) is not None:
                    raise ValueError('native room changed after start')
            if server is not None and server.poll() is not None:
                raise ValueError('native server ended')
        if native.poll() is not None or failed.is_set():
            send({'exited': 1 if failed.is_set() else native.returncode})
    finally:
        # Stop the emulator before waiting for a lobby socket that may be gone.
        island.stop_owned(native)
        island.stop_owned(server)
        if ws is not None:
            ws.close(timeout=1)
        control.close()


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]), sys.argv[3])
