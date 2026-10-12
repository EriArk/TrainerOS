# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded frame batching reused from the maintained Azahar channel bridge."""
import re
MAX_DATAGRAM = 1514
MAX_RECORD = 16384

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
