#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A private native LAN over the existing admitted, bounded record transport.

One host channel per accepted guest forms a star, with no public game sockets,
physical-interface forwarding, default route or emulator synchronization here.
"""
import array
import asyncio
import base64
import json
import os
from pathlib import Path
import re
import signal
import socket
import sys

import ethernet
import packets


def emit(value):
    print('TRAINEROS_NATIVE_LAN '+base64.b64encode(json.dumps(value, separators=(',', ':')).encode()).decode(), flush=True)


def read_config(path):
    path = Path(path)
    if not path.is_absolute() or path.is_symlink() or not path.is_file() or path.stat().st_size > 16384:
        raise ValueError('private session file')
    value = json.loads(path.read_text())
    if not isinstance(value, dict) or set(value) != {'version', 'slot', 'seats', 'channels', 'worker', 'helper', 'runtime'}:
        raise ValueError('private session fields')
    slot, seats, channels = value['slot'], value['seats'], value['channels']
    if (value['version'] != 1 or type(slot) is not int or not isinstance(seats, list)
            or not 2 <= len(seats) <= 4 or any(type(v) is not int or not 1 <= v <= 4 for v in seats)
            or len(set(seats)) != len(seats) or slot not in seats or 1 not in seats
            or not isinstance(channels, dict) or not isinstance(value['runtime'], dict)):
        raise ValueError('private session roster')
    peers = set(seats)-{1} if slot == 1 else {1}
    if set(channels) != {str(v) for v in peers}:
        raise ValueError('private session channels')
    for config in channels.values():
        if (not isinstance(config, dict) or type(config.get('host')) is not bool
                or config['host'] != (slot == 1) or config.get('mode') not in ('online', 'nearby')
                or not re.fullmatch('[a-f0-9]{64}', str(config.get('identity', '')))):
            raise ValueError('private channel identity')
    for key in ('worker', 'helper'):
        target = Path(value[key])
        if not target.is_absolute() or not target.is_file() or target.resolve().parent != Path(__file__).resolve().parent:
            raise ValueError('installed native helper')
    return value


async def main(path):
    config = read_config(path)
    slot, seats = config['slot'], config['seats']
    folder = Path(path).parent
    loop = asyncio.get_running_loop()
    stop = asyncio.Event()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, stop.set)
    peers = {int(peer): {'config': settings, 'queue': asyncio.Queue(maxsize=128),
                        'credits': asyncio.Semaphore(64), 'connected': False, 'ready': False,
                        'native': False, 'process': None}
             for peer, settings in config['channels'].items()}
    tasks, children = [], []
    tap = control = None
    worker_started = False
    native_ready = announced = room_ready = False
    deadline = loop.time()+180
    stats = {'sent': 0, 'received': 0, 'dropped': 0}

    async def write_channel(peer, value):
        process = peers[peer]['process']
        process.stdin.write(json.dumps(value, separators=(',', ':')).encode()+b'\n')
        await process.stdin.drain()

    def enqueue(peer, data):
        queue = peers[peer]['queue']
        if queue.full():
            stats['dropped'] += 1
        else:
            queue.put_nowait(data)

    def route(data, incoming=None):
        targets = ethernet.targets(data, seats, slot, incoming)
        for destination in targets:
            if destination == slot:
                try:
                    os.write(tap, data)
                except BlockingIOError:
                    stats['dropped'] += 1
            else:
                enqueue(destination, b'D'+data)

    def tap_readable():
        try:
            data = os.read(tap, 2048)
        except BlockingIOError:
            return
        # Native IPv6/multicast discovery outside this reviewed IPv4 segment
        # is simply not carried. Invalid authenticated peer frames fail below.
        try:
            route(data)
        except ValueError:
            return

    async def send_records(peer):
        state = peers[peer]
        pending = None
        while True:
            data = pending if pending is not None else await state['queue'].get()
            await state['credits'].acquire()
            if data[:1] == b'D':
                await asyncio.sleep(.001)
            data, pending = packets.batch_datagrams(data, state['queue'])
            if data[:1] in (b'D', b'B'):
                stats['sent'] += len(packets.decode_datagrams(data))
            await write_channel(peer, {'send': base64.b64encode(data).decode()})

    async def keep_alive(peer):
        while True:
            await asyncio.sleep(3)
            await peers[peer]['queue'].put(b'H')

    async def announce_if_ready():
        nonlocal announced
        if native_ready and all(v['native'] for v in peers.values()) and not announced:
            announced = True
            if slot == 1:
                for peer in peers:
                    # Control markers must not be dropped behind a packet burst.
                    await peers[peer]['queue'].put(b'G')
            emit({'event': 'running'})

    def start_if_ready():
        nonlocal worker_started
        if (control is not None and not worker_started and all(v['ready'] for v in peers.values())
                and (slot == 1 or room_ready)):
            worker_started = True
            control.send(b'start')

    async def make_island():
        nonlocal tap, control
        control, child_socket = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        try:
            with (folder/'worker.log').open('wb') as log:
                worker = await asyncio.create_subprocess_exec('/usr/bin/unshare', '--user',
                    '--map-current-user', '--keep-caps', '--net', sys.executable, config['worker'],
                    str(path), str(child_socket.fileno()), os.readlink('/proc/self/ns/net'),
                    pass_fds=(child_socket.fileno(),), stdout=asyncio.subprocess.DEVNULL, stderr=log)
            children.append(worker)
        finally:
            child_socket.close()
        control.settimeout(12)
        message, ancillary, flags, address = await asyncio.to_thread(control.recvmsg, 128, socket.CMSG_SPACE(4))
        if (message != b'ready' or flags or len(ancillary) != 1
                or ancillary[0][:2] != (socket.SOL_SOCKET, socket.SCM_RIGHTS)):
            raise ValueError('native interface acknowledgement')
        fds = array.array('i')
        fds.frombytes(ancillary[0][2])
        if len(fds) != 1:
            for fd in fds:
                os.close(fd)
            raise ValueError('native interface descriptor')
        tap = fds[0]
        control.setblocking(False)
        loop.add_reader(tap, tap_readable)
        for peer in peers:
            await peers[peer]['queue'].put(b'R')
        start_if_ready()

        async def read_worker():
            nonlocal native_ready, room_ready
            native_pid = None
            while message := await loop.sock_recv(control, 128):
                value = json.loads(message)
                if value == {'room': True} and slot == 1 and not room_ready:
                    room_ready = True
                    for peer in peers:
                        await peers[peer]['queue'].put(b'L')
                elif (set(value) == {'native'} and native_pid is None
                      and type(value['native']) is int and value['native'] > 1):
                    native_pid = value['native']
                elif value == {'running': True} and native_pid is not None and not native_ready:
                    native_ready = True
                    if slot != 1:
                        await peers[1]['queue'].put(b'J')
                    await announce_if_ready()
                elif set(value) == {'exited'} and type(value['exited']) is int:
                    if value['exited'] not in (0, -signal.SIGTERM):
                        emit({'event': 'error'})
                    break
                else:
                    raise ValueError('native process acknowledgement')
            stop.set()

        tasks.append(asyncio.create_task(read_worker()))

        async def monitor():
            await worker.wait()
            stop.set()
        tasks.append(asyncio.create_task(monitor()))

    async def read_channel(peer):
        nonlocal room_ready
        state = peers[peer]
        while line := await state['process'].stdout.readline():
            value = json.loads(line)
            event = value.get('event')
            if event == 'ready' and slot == 1:
                emit({'event': 'ready', 'slot': peer,
                      'endpoint': packets.ready_endpoint(value, state['config']['mode'])})
            elif event == 'connected' and not state['connected']:
                state['connected'] = True
                tasks.append(asyncio.create_task(send_records(peer)))
                tasks.append(asyncio.create_task(keep_alive(peer)))
            elif event == 'data' and state['connected']:
                data = base64.b64decode(value['data'], validate=True)
                if data == b'R' and not state['ready']:
                    state['ready'] = True
                    start_if_ready()
                elif data == b'L' and slot != 1 and not room_ready:
                    room_ready = True
                    start_if_ready()
                elif ((data == b'J' and slot == 1) or (data == b'G' and slot != 1)) and not state['native']:
                    state['native'] = True
                    await announce_if_ready()
                elif data != b'H':
                    if tap is None:
                        raise ValueError('frame before interface readiness')
                    for packet in packets.decode_datagrams(data):
                        route(packet, peer)
                        stats['received'] += 1
                await write_channel(peer, {'consume': value['sequence']})
            elif event == 'ack':
                state['credits'].release()
            elif event != 'sent':
                if event != 'closed':
                    emit({'event': 'error'})
                stop.set()
                return
        stop.set()

    try:
        for peer, state in peers.items():
            process = await asyncio.create_subprocess_exec(sys.executable, config['helper'],
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL, limit=32768)
            state['process'] = process
            await write_channel(peer, state['config'])
            tasks.append(asyncio.create_task(read_channel(peer)))
        await make_island()
        while not stop.is_set():
            if not announced and loop.time() > deadline:
                raise TimeoutError('native admission')
            for task in tuple(tasks):
                if task.done():
                    task.result()
                    stop.set()
            (folder/'metrics.json').write_text(json.dumps(stats))
            try:
                await asyncio.wait_for(stop.wait(), .5)
            except asyncio.TimeoutError:
                pass
    finally:
        if tap is not None:
            loop.remove_reader(tap)
            os.close(tap)
        if control is not None:
            control.close()
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        for peer, state in peers.items():
            process = state['process']
            if process is not None and process.returncode is None:
                try:
                    await asyncio.wait_for(write_channel(peer, {'close': True}), 1)
                    process.stdin.close()
                except (BrokenPipeError, ConnectionResetError, asyncio.TimeoutError):
                    pass
        for child in children+[v['process'] for v in peers.values() if v['process'] is not None]:
            if child.returncode is None:
                try:
                    await asyncio.wait_for(child.wait(), 6)
                except asyncio.TimeoutError:
                    child.terminate()
                    try:
                        await asyncio.wait_for(child.wait(), 3)
                    except asyncio.TimeoutError:
                        child.kill()
                        await child.wait()
        (folder/'metrics.json').write_text(json.dumps(stats))


if __name__ == '__main__':
    try:
        if len(sys.argv) != 2:
            raise ValueError('arguments')
        asyncio.run(main(sys.argv[1]))
    except Exception:
        emit({'event': 'error'})
        sys.exit(1)
