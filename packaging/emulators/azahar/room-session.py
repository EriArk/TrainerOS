#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Carry native Azahar ENet datagrams through an admitted two-to-four-person group.

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


def roster(channel):
    host = channel.get("host")
    players, seat = channel.get("players", 2), channel.get("seat", 1 if host else 2)
    if (("players" in channel) != ("seat" in channel)
            or type(host) is not bool or type(players) is not int or type(seat) is not int
            or not 2 <= players <= 4 or not 1 <= seat <= players or host != (seat == 1)):
        raise ValueError("room roster")
    return host, players, seat


def peer_config(channel, seat):
    host, players, owner = roster(channel)
    if not 2 <= seat <= players or (not host and seat != owner):
        raise ValueError("channel seat")
    value = {k: v for k, v in channel.items() if k not in ("players", "seat")}
    if "players" in channel:
        # A capability for one guest must never admit a different seat/session.
        value["identity"] = hashlib.sha256(
            ("traineros-azahar-group-v2:" + channel["identity"] + ":" + str(players) + ":" + str(seat)).encode()).hexdigest()
    return value


async def run(config, root):
    loop = asyncio.get_running_loop()
    stop = asyncio.Event()
    for signum in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(signum, stop.set)
    host, players, owner = roster(config["channel"])
    children, tasks, edges = [], [], {}
    native = None
    native_joined = announced = False
    room_port = None
    deadline = loop.time() + 180
    password = hashlib.sha256(("traineros-azahar-room-v1:" + config["channel"]["identity"]).encode()).hexdigest()

    async def channel_write(edge, value):
        edge["process"].stdin.write(json.dumps(value, separators=(",", ":")).encode() + b"\n")
        await edge["process"].stdin.drain()

    async def send_records(edge):
        pending = None
        while True:
            data = pending if pending is not None else await edge["queue"].get()
            await edge["credits"].acquire()
            if data[:1] == b"D":
                await asyncio.sleep(0.001)
            data, pending = batch_datagrams(data, edge["queue"])
            if data == b"Q":
                edge["close_sequence"] = edge["written"]
            edge["written"] += 1
            await channel_write(edge, {"send": base64.b64encode(data).decode()})

    async def maybe_running():
        nonlocal announced
        if not announced and native_joined and all(e["joined"] for e in edges.values()):
            announced = True
            if host:
                for edge in edges.values():
                    await edge["queue"].put(b"G")
            emit({"event": "running"})

    async def start_native(port):
        nonlocal native
        if native is not None:
            raise ValueError("duplicate client")
        native_config = root / "native-room.json"
        with native_config.open("x") as stream:
            os.chmod(native_config, 0o600)
            json.dump({"host": host, "user": config["user"], "sysdata": config["sysdata"], "port": port,
                       "password": password, "nickname": "Player" + str(owner), "players": players}, stream)
        env = os.environ.copy()
        env["TRAINEROS_AZAHAR_SESSION"] = str(native_config)
        env["QT_QPA_PLATFORM"] = "xcb"
        native = await asyncio.create_subprocess_exec(config["runtime"], "-f", config["content"],
            cwd=root, env=env, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
            limit=32768)
        children.append(native)

        async def read_native():
            nonlocal native_joined
            room_ready = False
            while line := await native.stdout.readline():
                line = line.strip()
                if line == b"[TrainerOS3DS] room-ready" and host and not room_ready:
                    room_ready = True
                    for edge in edges.values():
                        await edge["queue"].put(b"R")
                elif line == b"[TrainerOS3DS] running" and not native_joined:
                    native_joined = True
                    if not host:
                        await edges[owner]["queue"].put(b"J")
                    await maybe_running()
                elif line == b"[TrainerOS3DS] error":
                    raise ValueError("native room")
            code = await native.wait()
            if code:
                raise ValueError("native exit")
            stop.set()
        tasks.append(asyncio.create_task(read_native()))

    async def read_channel(seat, edge):
        nonlocal deadline, room_port
        while line := await edge["process"].stdout.readline():
            value = json.loads(line)
            event = value.get("event")
            if event == "ready" and host and not edge["ready"] and not edge["connected"]:
                edge["ready"] = True
                endpoint = ready_endpoint(value, config["channel"]["mode"])
                emit({"event": "ready", "endpoint": endpoint, "seat": seat})
            elif event == "connected" and not edge["connected"]:
                edge["connected"] = True
                tasks.append(asyncio.create_task(send_records(edge)))
                if all(e["connected"] for e in edges.values()):
                    deadline = loop.time() + 60
                    if host:
                        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                            probe.bind(("127.0.0.1", 0))
                            room_port = probe.getsockname()[1]
                        for e in edges.values():
                            e["bridge"].peer = ("127.0.0.1", room_port)
                        await start_native(room_port)
            elif event == "data" and edge["connected"]:
                if type(value.get("sequence")) is not int or value["sequence"] != edge["received"]:
                    raise ValueError("receipt sequence")
                edge["received"] += 1
                data = base64.b64decode(value["data"], validate=True)
                if data == b"R" and not host and native is None:
                    await start_native(edge["transport"].get_extra_info("sockname")[1])
                elif data == b"J" and host and native is not None and not edge["joined"]:
                    edge["joined"] = True
                    await maybe_running()
                elif data == b"G" and not host and native_joined and not edge["joined"]:
                    edge["joined"] = True
                    await maybe_running()
                elif data == b"Q":
                    # Explicit end-of-room survives Dilation's reconnect policy.
                    # Consume it below so the departing peer sees its receipt.
                    stop.set()
                elif data[:1] in (b"D", b"B"):
                    packets = decode_datagrams(data)
                    if edge["bridge"].peer is not None:
                        for packet in packets:
                            edge["transport"].sendto(packet, edge["bridge"].peer)
                else:
                    raise ValueError("room record")
                await channel_write(edge, {"consume": value["sequence"]})
            elif event == "ack" and edge["connected"]:
                if value.get("sequence") != edge["acked"] or edge["acked"] >= edge["sent"]:
                    raise ValueError("ack sequence")
                edge["acked"] += 1
                edge["credits"].release()
                if value["sequence"] == edge["close_sequence"]:
                    edge["close_ack"].set()
            elif event == "sent" and edge["connected"]:
                if value.get("sequence") != edge["sent"]:
                    raise ValueError("send sequence")
                edge["sent"] += 1
            elif event == "closed":
                # End the complete native room, including sockets and every helper.
                stop.set()
                return
            else:
                raise ValueError("channel event")
        stop.set()

    try:
        # Construct every edge before any reader can start the room.
        for seat in (range(2, players + 1) if host else (owner,)):
            queue = asyncio.Queue(maxsize=128)
            transport, bridge = await loop.create_datagram_endpoint(
                lambda q=queue: DatagramBridge(host, q, stop), local_addr=("127.0.0.1", 0))
            edge = {"queue": queue, "credits": asyncio.Semaphore(WINDOW), "bridge": bridge,
                    "transport": transport, "ready": False, "connected": False, "joined": False,
                    "sent": 0, "acked": 0, "received": 0, "written": 0,
                    "close_sequence": None, "close_ack": asyncio.Event()}
            edges[seat] = edge
            child = await asyncio.create_subprocess_exec(config["python"], config["helper"],
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL, limit=32768)
            children.append(child)
            edge["process"] = child
            await channel_write(edge, peer_config(config["channel"], seat))
        for seat, edge in edges.items():
            tasks.append(asyncio.create_task(read_channel(seat, edge)))
        while not stop.is_set():
            if not announced and loop.time() > deadline:
                raise TimeoutError("room admission")
            for task in list(tasks):
                if task.done():
                    task.result()
                    stop.set()
            try:
                await asyncio.wait_for(stop.wait(), 0.1)
            except asyncio.TimeoutError:
                pass
    finally:
        for edge in edges.values():
            edge["transport"].close()
        if native is not None and native.returncode is None:
            native.terminate()
        async def end_room(edge):
            if not edge["connected"]:
                return
            try:
                async def notify():
                    await edge["queue"].put(b"Q")
                    await edge["close_ack"].wait()
                await asyncio.wait_for(notify(), 2)
            except asyncio.TimeoutError:
                pass
        # Keep the readers/senders alive until the normal receipt arrives.
        # All edges share this bound, including an already broken connection.
        await asyncio.gather(*(end_room(edge) for edge in edges.values()))
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        # Tell Dilation to close the admitted subchannel before terminating the
        # helper. SIGTERM alone can leave the peer waiting for reconnect/credit
        # timeout, especially when the native room has become idle.
        async def close_channel(edge):
            process = edge.get("process")
            if process is None or process.returncode is not None:
                return
            try:
                await asyncio.wait_for(channel_write(edge, {"close": True}), 0.5)
                await asyncio.wait_for(process.wait(), 3)
            except (asyncio.TimeoutError, BrokenPipeError, ConnectionResetError):
                pass
        await asyncio.gather(*(close_channel(edge) for edge in edges.values()))
        for child in children:
            if child.returncode is None:
                child.terminate()
        async def reap(child):
            try:
                await asyncio.wait_for(child.wait(), 5)
            except asyncio.TimeoutError:
                child.kill()
                await child.wait()
        await asyncio.gather(*(reap(child) for child in children))


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("arguments")
        asyncio.run(run(*read_config(sys.argv[1])))
    except Exception:
        emit({"event": "error"})
        sys.exit(1)
