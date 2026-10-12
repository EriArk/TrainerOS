#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""An admitted two-player channel for a game's existing native LAN traffic."""
import array
import asyncio
import base64
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import socket
import sys

spec = importlib.util.spec_from_file_location('batch', str(Path(__file__).with_name('packets.py')))
batch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(batch)


def emit(value):
    print('TRAINEROS_VITA '+base64.b64encode(json.dumps(value,separators=(',',':')).encode()).decode(),flush=True)


def read_config(path):
    path=Path(path)
    if not path.is_absolute() or path.is_symlink() or not path.is_file() or path.stat().st_size>8192:
        raise ValueError('session file')
    value=json.loads(path.read_text())
    if not isinstance(value,dict) or set(value)!={'channel','python','helper','runtime','runtimeRoot','user','title','mac'}:
        raise ValueError('session fields')
    channel=value['channel']
    if (not isinstance(channel,dict) or type(channel.get('host')) is not bool
        or channel.get('mode') not in ('nearby','online')
        or not re.fullmatch('[a-f0-9]{64}',str(channel.get('identity','')))):
        raise ValueError('channel identity')
    if not re.fullmatch('[A-Z0-9]{9}',str(value['title'])):
        raise ValueError('installed title')
    mac=value['mac']
    if (not isinstance(mac,str) or not re.fullmatch('[a-f0-9]{2}(?::[a-f0-9]{2}){5}',mac)
        or mac=='00:00:00:00:00:00' or int(mac[:2],16)&1):
        raise ValueError('own console address')
    for key in ('python','helper','runtime'):
        if not isinstance(value[key],str) or not Path(value[key]).is_absolute() or not Path(value[key]).is_file():
            raise ValueError('runtime path')
    runtime_root=Path(value['runtimeRoot'])
    if not runtime_root.is_absolute() or not runtime_root.is_dir() or not Path(value['runtime']).resolve().is_relative_to(runtime_root.resolve()):
        raise ValueError('runtime directory')
    user=Path(value['user'])
    if not user.is_absolute() or user.is_symlink() or not user.is_dir() or user.resolve().parent!=path.resolve().parent:
        raise ValueError('private settings directory')
    settings=user/'config/Vita3K/config.yml'
    if settings.is_symlink() or not settings.is_file() or settings.stat().st_size>131072:
        raise ValueError('private native settings')
    return value


async def main(path):
    config = read_config(path)
    folder = Path(path).parent
    loop = asyncio.get_running_loop()
    stop = asyncio.Event()
    for sig in (signal.SIGTERM, signal.SIGINT): loop.add_signal_handler(sig, stop.set)
    outgoing = asyncio.Queue(maxsize=128)
    credits = asyncio.Semaphore(64)
    tasks = []
    children = []
    tap = None
    control = None
    stats = {'sent': 0, 'received': 0, 'dropped': 0}
    connected = False
    peer_ready = False
    native_ready = False
    peer_native_ready = False
    announced = False
    started = False
    deadline = loop.time()+180

    def tap_readable():
        try: data = os.read(tap, 2048)
        except BlockingIOError: return
        if not 14 <= len(data) <= 1514 or data[12:14] not in (b'\x08\x00', b'\x08\x06'): return
        if outgoing.full(): stats['dropped'] += 1
        else: outgoing.put_nowait(b'D'+data)

    async def write_channel(value):
        channel.stdin.write(json.dumps(value,separators=(',',':')).encode()+b'\n')
        await channel.stdin.drain()

    async def send_records():
        pending = None
        while True:
            data = pending if pending is not None else await outgoing.get()
            await credits.acquire()
            if data[:1] == b'D': await asyncio.sleep(.001)
            data, pending = batch.batch_datagrams(data,outgoing)
            if data[:1] in (b'D',b'B'): stats['sent'] += len(batch.decode_datagrams(data))
            await write_channel({'send':base64.b64encode(data).decode()})

    async def keep_alive():
        # An idle native LAN menu may send nothing. Exercise the channel's
        # existing receipt deadline so a vanished peer cannot remain forever.
        while True:
            await asyncio.sleep(3)
            await outgoing.put(b'H')

    def start_if_ready():
        nonlocal started
        if tap is not None and peer_ready and not started:
            started=True
            control.send(b'start')

    def announce_if_ready():
        nonlocal announced
        if native_ready and peer_native_ready and not announced:
            announced=True
            emit({'event':'running'})

    async def make_island():
        nonlocal tap, control
        control, worker_socket = socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
        worker = await asyncio.create_subprocess_exec('/usr/bin/unshare','--user','--map-root-user','--net',
            '/usr/bin/python3',str(Path(__file__).with_name('network-worker.py')),str(path),str(worker_socket.fileno()),os.readlink('/proc/self/ns/net'),
            pass_fds=(worker_socket.fileno(),),stdout=asyncio.subprocess.DEVNULL,
            stderr=open(folder/'worker.log','wb'))
        children.append(worker)
        worker_socket.close()
        control.settimeout(12)
        message, ancillary, flags, address = await asyncio.to_thread(control.recvmsg,128,socket.CMSG_SPACE(4))
        assert message == b'ready' and ancillary
        fds=array.array('i');fds.frombytes(ancillary[0][2][:4]);tap=fds[0]
        control.setblocking(False)
        loop.add_reader(tap,tap_readable)
        await outgoing.put(b'R')
        start_if_ready()
        async def read_worker():
            nonlocal native_ready
            message=await loop.sock_recv(control,128)
            value=json.loads(message)
            if set(value)!={'native'} or type(value['native']) is not int or value['native']<2:
                raise ValueError('native process acknowledgement')
            native_ready=True
            await outgoing.put(b'J')
            announce_if_ready()
            message=await loop.sock_recv(control,128)
            if message:
                value=json.loads(message)
                if set(value)!={'exited'} or type(value['exited']) is not int:
                    raise ValueError('native exit acknowledgement')
                if value['exited'] not in (0,-signal.SIGTERM):emit({'event':'error'})
            stop.set()
        tasks.append(asyncio.create_task(read_worker()))
        async def monitor():
            await worker.wait()
            stop.set()
        tasks.append(asyncio.create_task(monitor()))

    try:
        channel=await asyncio.create_subprocess_exec(config['python'],config['helper'],
            stdin=asyncio.subprocess.PIPE,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.DEVNULL,limit=32768)
        children.append(channel)
        await write_channel(config['channel'])
        async def read_channel():
            nonlocal connected, deadline, peer_ready, peer_native_ready
            while line:=await channel.stdout.readline():
                value=json.loads(line);event=value.get('event')
                if event=='ready': emit({'event':'ready','endpoint':batch.ready_endpoint(value,config['channel']['mode'])})
                elif event=='connected' and not connected:
                    connected=True;deadline=loop.time()+45
                    tasks.append(asyncio.create_task(send_records()))
                    tasks.append(asyncio.create_task(keep_alive()))
                    await make_island()
                elif event=='data' and connected:
                    data=base64.b64decode(value['data'],validate=True)
                    if data==b'R' and not peer_ready: peer_ready=True;start_if_ready()
                    elif data==b'J' and not peer_native_ready: peer_native_ready=True;announce_if_ready()
                    elif data==b'H': pass
                    else:
                        for packet in batch.decode_datagrams(data):
                            assert 14<=len(packet)<=1514 and packet[12:14] in (b'\x08\x00',b'\x08\x06')
                            try: os.write(tap,packet);stats['received']+=1
                            except BlockingIOError: stats['dropped']+=1
                    await write_channel({'consume':value['sequence']})
                elif event=='ack': credits.release()
                elif event=='sent': pass
                else:
                    if event!='closed': emit({'event':'error'})
                    stop.set();return
            stop.set()
        tasks.append(asyncio.create_task(read_channel()))
        while not stop.is_set():
            if not announced and loop.time()>deadline: raise TimeoutError('admission')
            for task in tasks:
                if task.done(): task.result();stop.set()
            (folder/'metrics.json').write_text(json.dumps(stats))
            try: await asyncio.wait_for(stop.wait(),.5)
            except asyncio.TimeoutError: pass
    finally:
        if tap is not None: loop.remove_reader(tap);os.close(tap)
        if control is not None: control.close()
        for task in tasks: task.cancel()
        await asyncio.gather(*tasks,return_exceptions=True)
        # Dilation can reconnect after an abrupt transport loss. Deliberately
        # end an ordinary exit instead of leaving the other game waiting.
        if children and children[0].returncode is None:
            try:
                await asyncio.wait_for(write_channel({'close':True}),1)
                channel.stdin.close()
            except (BrokenPipeError,ConnectionResetError,asyncio.TimeoutError):pass
        # EOF on the local control socket closes the owned native process group.
        for child in reversed(children):
            if child.returncode is None:
                try: await asyncio.wait_for(child.wait(),6)
                except asyncio.TimeoutError:
                    child.terminate()
                    try:await asyncio.wait_for(child.wait(),3)
                    except asyncio.TimeoutError:child.kill();await child.wait()
        (folder/'metrics.json').write_text(json.dumps(stats))

if __name__=='__main__':
    try:
        if len(sys.argv)!=2: raise ValueError('arguments')
        asyncio.run(main(sys.argv[1]))
    except Exception:
        emit({'event':'error'})
        sys.exit(1)
