#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux protocol test: four native RMG lobbies over three private TLS channels.

Run with the staged transport's Python and --runtime BUNDLE. No ROM or emulator
is executed. This checks admission, controller ordering and party teardown; it
does not establish four-device gameplay, internet traversal or performance.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import secrets
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def worker(path, control_fd, outside_namespace):
    import websocket
    import island
    from native_lobby import join
    from session import read_config
    config = read_config(path)
    runtime = config['runtime']
    control = island.open_interface(config['slot'], control_fd, outside_namespace)
    server = ws = None

    def send(value):
        control.send(json.dumps(value).encode())

    def cancelled():
        return bool(select.select([control], [], [], 0)[0])
    try:
        if control.recv(32) != b'start':
            raise ValueError('cancelled fixture')
        if config['slot'] == 1:
            server = subprocess.Popen([str(Path(runtime['root'])/'bin/rmg-netplay-server'),
                '--name', 'TrainerOS-four-lobby-test', '--disable-broadcast', '--verify-ip',
                '--close-on-finish', '--timeout', '2'],
                stdout=(path.parent/'server.log').open('wb'), stderr=subprocess.STDOUT,
                start_new_session=True, preexec_fn=island.death_guard(os.getpid(), signal.SIGKILL))
        else:
            time.sleep({4: 0, 3: .4, 2: .8}[config['slot']])
        ws, state = join(dict(runtime, slot=config['slot'], seats=config['seats']), cancelled,
                         lambda: send({'room': True}))
        (path.parent/'proof.json').write_text(json.dumps(dict(slot=config['slot'], controller=state.controller,
                                                            members=len(state.roster))))
        # Protocol-only worker acknowledgement, never an emulator readiness claim.
        send({'native': os.getpid()})
        send({'running': True})
        while not cancelled():
            readable, _, _ = select.select([control, ws.sock], [], [], .2)
            if control in readable:
                break
            if ws.sock in readable:
                try:
                    data = ws.recv()
                except websocket.WebSocketTimeoutException:
                    continue
                if not data:
                    raise ValueError('closed native lobby')
                state.receive(json.loads(data))
    finally:
        island.stop_owned(server)
        if ws is not None:
            ws.close(timeout=1)
        control.close()


def exercise(bundle, root):
    manifest = json.loads((bundle/'manifest.json').read_text())
    server = bundle/'bin/rmg-netplay-server'
    if hashlib.sha256(server.read_bytes()).hexdigest() != manifest['files']['bin/rmg-netplay-server']:
        raise ValueError('changed native server')
    transport = root/'transport'
    transport.mkdir()
    for path in (bundle/'transport').glob('*.py'):
        shutil.copyfile(path, transport/path.name)
    shutil.copyfile(__file__, transport/'check-party.py')
    token = secrets.token_hex(16)
    processes = {}

    def launch(slot, endpoints=None):
        folder = root/str(slot)
        folder.mkdir(mode=0o700)
        channels = {}
        for peer in ([2, 3, 4] if slot == 1 else [1]):
            guest = peer if slot == 1 else slot
            settings = dict(host=slot == 1, mode='nearby', address='127.0.0.1',
                            identity=hashlib.sha256((token+':'+str(guest)).encode()).hexdigest())
            if endpoints:
                settings.update(endpoints[slot])
            channels[str(peer)] = settings
        config = dict(version=1, slot=slot, seats=[1, 2, 3, 4], channels=channels,
                      worker=str(transport/'check-party.py'), helper=str(transport/'secure-channel.py'),
                      runtime=dict(root=str(bundle), token=token, content='b'*64, runtime='c'*64))
        (folder/'network.json').write_text(json.dumps(config))
        with (folder/'output.log').open('wb') as log:
            processes[slot] = subprocess.Popen([sys.executable, str(transport/'session.py'), str(folder/'network.json')],
                stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)

    def events(slot):
        result = []
        for line in (root/str(slot)/'output.log').read_bytes().splitlines():
            if line.startswith(b'TRAINEROS_NATIVE_LAN '):
                result.append(json.loads(base64.b64decode(line.split(b' ', 1)[1])))
        return result

    try:
        launch(1)
        endpoints = {}
        deadline = time.monotonic()+30
        while time.monotonic() < deadline:
            for event in events(1):
                if event['event'] == 'ready':
                    endpoints[event['slot']] = event['endpoint']
            if len(endpoints) == 3:
                break
            time.sleep(.1)
        if len(endpoints) != 3:
            raise TimeoutError('private endpoint preparation')
        for slot in [4, 3, 2]:
            launch(slot, endpoints)
        deadline = time.monotonic()+80
        while time.monotonic() < deadline:
            if all(any(value['event'] == 'running' for value in events(slot)) for slot in [1, 2, 3, 4]):
                break
            if any(process.poll() is not None for process in processes.values()):
                raise RuntimeError('private coordinator failed')
            time.sleep(.2)
        else:
            raise TimeoutError('four native lobbies')
        proofs = [json.loads((root/str(slot)/'proof.json').read_text()) for slot in [1, 2, 3, 4]]
        if [value['controller'] for value in proofs] != [1, 4, 3, 2] or any(value['members'] != 4 for value in proofs):
            raise ValueError('native controller assignment')
        print('Four native lobby clients admitted in reverse order; assigned controllers 1/4/3/2.', flush=True)
        started = time.monotonic()
        processes[2].terminate()
        for process in processes.values():
            process.wait(timeout=25)
        print(f'All four coordinators closed after one guest left: {time.monotonic()-started:.2f}s.', flush=True)
    finally:
        for process in processes.values():
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=12)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == '__main__':
    if len(sys.argv) == 4 and not sys.argv[1].startswith('--'):
        worker(Path(sys.argv[1]), int(sys.argv[2]), sys.argv[3])
    else:
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument('--runtime', type=Path, required=True)
        args = parser.parse_args()
        os.umask(0o077)
        with tempfile.TemporaryDirectory(prefix='traineros-four-lobby-') as folder:
            exercise(args.runtime.resolve(strict=True), Path(folder))
