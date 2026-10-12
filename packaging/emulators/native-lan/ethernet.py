# SPDX-License-Identifier: GPL-3.0-or-later
"""A bounded private Ethernet segment; no emulator or game protocol parsing."""
PREFIX = b'\x02\x54\x4c\x41\x4e'
IP_PREFIX = b'\x0a\xcb\x00'
BROADCAST = b'\xff'*6


def mac(slot):
    if type(slot) is not int or not 1 <= slot <= 4:
        raise ValueError('native LAN slot')
    return PREFIX+bytes([slot])


def address(slot):
    mac(slot)
    return '10.203.0.'+str(slot)


def frame(data, seats, source=None):
    """Return source/destination slots, rejecting forged peer addresses.

    Zero destination denotes the private segment broadcast. The namespace has
    no default route and this bridge never forwards a physical interface.
    """
    if not isinstance(data, bytes) or not 42 <= len(data) <= 1514:
        raise ValueError('Ethernet frame bounds')
    sender = data[11]
    if sender not in seats or data[6:12] != mac(sender) or (source is not None and sender != source):
        raise ValueError('Ethernet sender')
    broadcast = data[:6] == BROADCAST
    destination = 0 if broadcast else data[5]
    if not broadcast and (destination not in seats or data[:6] != mac(destination)):
        raise ValueError('Ethernet destination')
    kind = data[12:14]
    if kind == b'\x08\x00':
        length = (data[14] & 15)*4
        total = int.from_bytes(data[16:18], 'big')
        if data[14] >> 4 != 4 or not 20 <= length <= 60 or not length <= total <= len(data)-14:
            raise ValueError('IPv4 frame bounds')
        if data[26:30] != IP_PREFIX+bytes([sender]):
            raise ValueError('IPv4 sender')
        target = data[30:34]
        expected = IP_PREFIX+bytes([destination]) if destination else IP_PREFIX+b'\x07'
        if target != expected and not (destination == 0 and target == b'\xff'*4):
            raise ValueError('IPv4 destination')
    elif kind == b'\x08\x06':
        if (data[14:20] != b'\x00\x01\x08\x00\x06\x04'
                or data[20:22] not in (b'\x00\x01', b'\x00\x02')
                or data[22:28] != mac(sender) or data[28:32] != IP_PREFIX+bytes([sender])
                or data[38:41] != IP_PREFIX or data[41] not in seats
                or (destination and data[41] != destination)):
            raise ValueError('ARP peer address')
    else:
        raise ValueError('native LAN protocol')
    return sender, destination


def targets(data, seats, local, incoming=None):
    sender, destination = frame(data, seats, incoming if local == 1 else None)
    if incoming is None and sender != local:
        raise ValueError('local frame sender')
    if incoming is not None and local != 1 and incoming != 1:
        raise ValueError('guest channel topology')
    candidates = set(seats) if destination == 0 else {destination}
    candidates.discard(sender)
    # Guests send all remote traffic through the admitted host, never a mesh
    # of unconsented extra connections. Only the host forwards other members.
    if local != 1:
        return ({1} if candidates and incoming is None else ({local} & candidates))
    return candidates
