#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Carry native Azahar ENet datagrams through an admitted two-person channel.

No 3DS wireless protocol, game state or save parsing occurs here. Each client
runs one native console; the organizer also runs Azahar's native room. UDP is
loopback-only, with bounded queues and native ENet retaining its own semantics.
"""
import asyncio
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import sys


MAX_DATAGRAM = 4096
MAX_RECORD = 16384
WINDOW = 64


def emit(event):
    print("TRAINEROS_3DS " + base64.b64encode(json.dumps(event, separators=(",", ":")).encode()).decode(), flush=True)


def ready_endpoint(value, mode):
    # The shared helper's TLS endpoint is nested; its online rendezvous code is
    # a top-level field. Preserve the actual contracts for both transports.
    if mode == "nearby":
        endpoint = value.get("endpoint")
        if (not isinstance(endpoint, dict) or set(endpoint) != {"mode", "port", "fingerprint", "token"}
                or endpoint["mode"] != "nearby" or type(endpoint["port"]) is not int
                or not 1 <= endpoint["port"] <= 65535
                or not re.fullmatch("[a-f0-9]{64}", str(endpoint["fingerprint"]))
                or not re.fullmatch("[a-f0-9]{32}", str(endpoint["token"]))):
            raise ValueError("nearby endpoint")
        return dict(endpoint)
    code = value.get("code")
    if mode != "online" or not isinstance(code, str) or len(code) > 160 or not re.fullmatch(r"[0-9]+(?:-[a-z]+){8}", code):
        raise ValueError("online endpoint")
    return {"mode": "online", "code": code}


def read_config(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 8192:
        raise ValueError("session file")
    value = json.loads(path.read_text())
    if set(value) != {"channel", "content", "runtime", "python", "helper", "user", "sysdata"}:
        raise ValueError("session fields")
    if not isinstance(value["channel"], dict) or not re.fullmatch("[a-f0-9]{64}", str(value["channel"].get("identity", ""))):
        raise ValueError("identity")
    root = path.resolve().parent
    for key in ("content", "runtime", "python", "helper"):
        if not isinstance(value[key], str) or not Path(value[key]).is_file():
            raise ValueError("session paths")
    user = Path(value["user"])
    if user.is_symlink() or not user.is_dir() or user.resolve().parent != root or str(user.resolve()) != str(user):
        raise ValueError("private user directory")
    if not isinstance(value["sysdata"], str) or not Path(value["sysdata"]).is_dir():
        raise ValueError("own system data")
    return value, root


class DatagramBridge(asyncio.DatagramProtocol):
    def __init__(self, host, queue, stop):
        self.host, self.queue, self.stop = host, queue, stop
        self.peer = None

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, address):
        if not data or len(data) > MAX_DATAGRAM or address[0] != "127.0.0.1":
            return
        if self.peer is None and not self.host:
            self.peer = address
        if address != self.peer:
            return
        # Dropping overload is legal UDP behavior. ENet's native retransmission
        # applies; never accumulate an unbounded reliable tunnel backlog.
        if not self.queue.full():
            self.queue.put_nowait(b"D" + data)

    def error_received(self, _):
        self.stop.set()


def batch_datagrams(first, queue):
    """Drain an existing burst into one receipt window slot, without waiting.

    Preserve native datagram boundaries/order and control markers. One receipt
    per tiny UDP packet needlessly throttles native ENet on a distant relay.
    """
    if first[:1] != b"D":
        return first, None
    packet = first
    result = bytearray(b"B")
    count = 0
    while True:
        payload = packet[1:]
        if not payload or len(payload) > MAX_DATAGRAM:
            raise ValueError("datagram bounds")
        result.extend(len(payload).to_bytes(2, "big"))
        result.extend(payload)
        count += 1
        if queue.empty():
            return bytes(result), None
        packet = queue.get_nowait()
        if count == 128 or packet[:1] != b"D" or len(result) + 2 + len(packet) - 1 > MAX_RECORD:
            return bytes(result), packet


def decode_datagrams(record):
    if not record or len(record) > MAX_RECORD:
        raise ValueError("datagram record bounds")
    if record[:1] == b"D" and 1 < len(record) <= MAX_DATAGRAM + 1:
        return [record[1:]]
    if record[:1] != b"B":
        raise ValueError("datagram record kind")
    packets, offset = [], 1
    while offset < len(record):
        if offset + 2 > len(record):
            raise ValueError("truncated datagram length")
        size = int.from_bytes(record[offset:offset + 2], "big")
        offset += 2
        if not 1 <= size <= MAX_DATAGRAM or offset + size > len(record):
            raise ValueError("truncated datagram")
        packets.append(record[offset:offset + size])
        offset += size
        if len(packets) > 128:
            raise ValueError("datagram count")
    if not packets:
        raise ValueError("empty batch")
    return packets


async def run(config, root):
    loop = asyncio.get_running_loop()
    stop = asyncio.Event()
    for signum in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(signum, stop.set)
    host = config["channel"]["host"]
    if type(host) is not bool:
        raise ValueError("host")
    outgoing = asyncio.Queue(maxsize=128)
    credits = asyncio.Semaphore(WINDOW)
    children, tasks = [], []
    channel = None
    native = None
    transport = None
    connected = False
    native_joined = peer_joined = announced = False
    deadline = loop.time() + 180
    password = hashlib.sha256(("traineros-azahar-room-v1:" + config["channel"]["identity"]).encode()).hexdigest()

    async def channel_write(value):
        channel.stdin.write(json.dumps(value, separators=(",", ":")).encode() + b"\n")
        await channel.stdin.drain()

    async def send_records():
        pending = None
        while True:
            data = pending if pending is not None else await outgoing.get()
            await credits.acquire()
            if data[:1] == b"D":
                # Native ENet flushes a burst across several UDP callbacks.
                # One bounded millisecond collects that burst before consuming
                # a WAN receipt slot, rather than exhausting credit on singletons.
                await asyncio.sleep(0.001)
            data, pending = batch_datagrams(data, outgoing)
            await channel_write({"send": base64.b64encode(data).decode()})

    def maybe_running():
        nonlocal announced
        if native_joined and peer_joined and not announced:
            announced = True
            emit({"event": "running"})

    async def start_native(port):
        nonlocal native
        if native is not None:
            raise ValueError("duplicate client")
        native_config = root / "native-room.json"
        with native_config.open("x") as stream:
            os.chmod(native_config, 0o600)
            json.dump({"host": host, "user": config["user"], "sysdata": config["sysdata"], "port": port,
                       "password": password, "nickname": "Player1" if host else "Player2"}, stream)
        env = os.environ.copy()
        env["TRAINEROS_AZAHAR_SESSION"] = str(native_config)
        env["QT_QPA_PLATFORM"] = "xcb"
        native = await asyncio.create_subprocess_exec(config["runtime"], "-f", config["content"],
            cwd=root, env=env, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
            limit=32768)
        children.append(native)

        async def read_native():
            nonlocal native_joined
            while line := await native.stdout.readline():
                line = line.strip()
                if line == b"[TrainerOS3DS] room-ready" and host:
                    await outgoing.put(b"R")
                elif line == b"[TrainerOS3DS] running":
                    native_joined = True
                    await outgoing.put(b"J")
                    maybe_running()
                elif line == b"[TrainerOS3DS] error":
                    emit({"event": "error"})
                    stop.set()
                    return
            code = await native.wait()
            if code:
                print("[TrainerOS3DS] native-exit=" + str(code), flush=True)
                emit({"event": "error"})
            stop.set()
        tasks.append(asyncio.create_task(read_native()))

    try:
        channel = await asyncio.create_subprocess_exec(config["python"], config["helper"],
            stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL, limit=32768)
        children.append(channel)
        await channel_write(config["channel"])
        transport, bridge = await loop.create_datagram_endpoint(
            lambda: DatagramBridge(host, outgoing, stop), local_addr=("127.0.0.1", 0))
        guest_port = transport.get_extra_info("sockname")[1]

        async def read_channel():
            nonlocal connected, deadline, peer_joined
            while line := await channel.stdout.readline():
                value = json.loads(line)
                event = value.get("event")
                if event == "ready" and host and not connected:
                    endpoint = ready_endpoint(value, config["channel"]["mode"])
                    emit({"event": "ready", "endpoint": endpoint})
                elif event == "connected" and not connected:
                    connected = True
                    deadline = loop.time() + 40
                    tasks.append(asyncio.create_task(send_records()))
                    if host:
                        # Native ENet owns its socket. Reserve an unused loopback
                        # port briefly; a bind race fails safely at Room::Create.
                        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                            probe.bind(("127.0.0.1", 0))
                            port = probe.getsockname()[1]
                        bridge.peer = ("127.0.0.1", port)
                        await start_native(port)
                elif event == "data" and connected:
                    data = base64.b64decode(value["data"], validate=True)
                    if data == b"R" and not host and native is None:
                        await start_native(guest_port)
                    elif data == b"J" and not peer_joined:
                        peer_joined = True
                        maybe_running()
                    elif data[:1] in (b"D", b"B"):
                        packets = decode_datagrams(data)
                        if bridge.peer is not None:
                            for packet in packets:
                                transport.sendto(packet, bridge.peer)
                    else:
                        raise ValueError("room record")
                    await channel_write({"consume": value["sequence"]})
                elif event == "ack" and connected:
                    credits.release()
                elif event == "sent" and connected:
                    pass
                else:
                    if event != "closed":
                        emit({"event": "error"})
                    stop.set()
                    return
            stop.set()
        tasks.append(asyncio.create_task(read_channel()))
        while not stop.is_set():
            if not announced and loop.time() > deadline:
                raise TimeoutError("room admission")
            for task in tasks:
                if task.done():
                    task.result()
                    stop.set()
            try:
                await asyncio.wait_for(stop.wait(), 0.1)
            except asyncio.TimeoutError:
                pass
    finally:
        if transport:
            transport.close()
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        for child in children:
            if child.returncode is None:
                child.terminate()
        for child in children:
            try:
                await asyncio.wait_for(child.wait(), 5)
            except asyncio.TimeoutError:
                child.kill()
                await child.wait()


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("arguments")
        asyncio.run(run(*read_config(sys.argv[1])))
    except Exception:
        emit({"event": "error"})
        sys.exit(1)
