#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One accepted DS party's private process pipe over Magic Wormhole Dilation.

No game/save paths, credentials, ROMs, emulator state or game protocol are parsed
here. The coordinator supplies an already consented session identity and bounded
records. Codes and records are private IPC, never application log output.
"""
import base64
import json
import re
import struct
import sys
import time
from collections import OrderedDict, deque

MAX_RECORD = 16384
WINDOW = 64
APP_ID = "org.traineros.ds-channel/v1"
SUBPROTOCOL = "traineros-ds-records-v1"


def validate(config):
    if (not isinstance(config, dict) or type(config.get("host")) is not bool
            or type(config.get("relayOnly", False)) is not bool
            or not isinstance(config.get("identity"), str)
            or not re.fullmatch(r"[0-9a-f]{64}", config["identity"])
            or set(config) - {"host", "identity", "code", "relayOnly"}):
        raise ValueError("config")
    if not config["host"]:
        code = config.get("code", "")
        if not isinstance(code, str) or len(code) > 160 or not re.fullmatch(r"[0-9]+(?:-[a-z]+){8}", code):
            raise ValueError("code")
    elif "code" in config:
        raise ValueError("host code")
    return bytes.fromhex(config["identity"])


class Records:
    """Bound both directions, including the library's durable reconnect queue.

    Receive credit is returned only after the parent explicitly consumes that
    sequence. Wire receipts alone cannot bypass a stalled coordinator.
    """
    def __init__(self, identity, write, emit, now=time.monotonic):
        self.identity, self.write, self.emit, self.now = identity, write, emit, now
        self.verified = False
        self.sent = self.received = 0
        self.outgoing, self.incoming = OrderedDict(), OrderedDict()

    def start(self):
        self.write(b"H" + self.identity)

    def receive(self, record):
        if not isinstance(record, bytes) or not record or len(record) > MAX_RECORD + 5:
            raise ValueError("record bounds")
        if not self.verified:
            if record != b"H" + self.identity:
                raise ValueError("session identity")
            self.verified = True
            self.emit({"event": "connected"})
            return
        if record[:1] == b"D" and len(record) > 5:
            sequence, = struct.unpack("!I", record[1:5])
            if sequence != self.received or len(self.incoming) >= WINDOW:
                raise ValueError("receive window")
            self.received += 1
            self.incoming[sequence] = self.now()
            self.emit({"event": "data", "sequence": sequence,
                       "data": base64.b64encode(record[5:]).decode("ascii")})
        elif record[:1] == b"A" and len(record) == 5:
            sequence, = struct.unpack("!I", record[1:])
            if not self.outgoing or sequence != next(iter(self.outgoing)):
                raise ValueError("receipt order")
            del self.outgoing[sequence]
            self.emit({"event": "ack", "sequence": sequence})
        else:
            raise ValueError("record kind")

    def command(self, value):
        if not self.verified or not isinstance(value, dict) or len(value) != 1:
            raise ValueError("unadmitted command")
        if "send" in value:
            encoded = value["send"]
            if not isinstance(encoded, str) or len(encoded) > ((MAX_RECORD + 2) // 3) * 4:
                raise ValueError("send bounds")
            data = base64.b64decode(encoded, validate=True)
            if not data or len(data) > MAX_RECORD or len(self.outgoing) >= WINDOW or self.sent > 0xFFFFFFFF:
                raise ValueError("send window")
            sequence = self.sent
            self.sent += 1
            self.outgoing[sequence] = self.now()
            self.write(b"D" + struct.pack("!I", sequence) + data)
            self.emit({"event": "sent", "sequence": sequence})
        elif "consume" in value:
            sequence = value["consume"]
            if type(sequence) is not int or not self.incoming or sequence != next(iter(self.incoming)):
                raise ValueError("consume order")
            del self.incoming[sequence]
            self.write(b"A" + struct.pack("!I", sequence))
        else:
            raise ValueError("command kind")

    def check_timeout(self):
        for pending in (self.incoming, self.outgoing):
            if pending and self.now() - next(iter(pending.values())) > 12:
                raise ValueError("coordinator or peer stalled")


def main():
    from twisted.internet import reactor, stdio
    from twisted.internet.defer import ensureDeferred
    from twisted.internet.protocol import Factory, Protocol
    from twisted.protocols.basic import LineReceiver
    from wormhole import create
    from wormhole.cli.public_relay import RENDEZVOUS_RELAY, TRANSIT_RELAY

    class Owner(LineReceiver):
        delimiter = b"\n"
        MAX_LENGTH = 32768

        def __init__(self):
            self.config = self.wormhole = self.records = None
            self.closing = False
            self.exit_code = 1
            self.started = time.monotonic()
            self.output, self.output_bytes = deque(), 0
            self.output_paused = self.drain_scheduled = False

        def connectionMade(self):
            self.transport.registerProducer(self, True)

        def pauseProducing(self):
            self.output_paused = True

        def resumeProducing(self):
            self.output_paused = False
            self.schedule_drain()

        def stopProducing(self):
            self.stop(False)

        def schedule_drain(self):
            if not self.drain_scheduled:
                self.drain_scheduled = True
                reactor.callLater(0, self.drain)

        def drain(self):
            self.drain_scheduled = False
            while self.output and not self.output_paused:
                line = self.output.popleft()
                self.output_bytes -= len(line)
                self.sendLine(line)

        def emit(self, value):
            # Honor StandardIO backpressure as well as record credits. A parent
            # that keeps sending but stops reading cannot grow stdout forever.
            if not self.closing:
                line = json.dumps(value, separators=(",", ":")).encode("ascii")
                if self.output_bytes + len(line) > 2 * 1024 * 1024:
                    raise ValueError("parent output stalled")
                self.output.append(line)
                self.output_bytes += len(line)
                self.schedule_drain()

        def lineReceived(self, line):
            if self.closing:
                return
            try:
                value = json.loads(line)
                if self.config is None:
                    identity = validate(value)
                    self.config = value
                    ensureDeferred(self.connect_peer(identity)).addErrback(self.connect_failed)
                elif value == {"close": True}:
                    self.stop(True)
                elif self.records:
                    self.records.command(value)
                else:
                    raise ValueError("not connected")
            except Exception:
                self.stop(False)

        def lineLengthExceeded(self, _):
            self.stop(False)

        def connectionLost(self, _):
            self.stop(False)

        def connect_failed(self, failure):
            self.stop(False, "connect-" + failure.type.__name__)

        def stop(self, success, category="channel"):
            if self.closing:
                return
            if not success:
                self.output.clear()
                self.output_bytes = 0
            self.emit({"event": "closed"} if success else {"event": "error", "category": category})
            self.closing = True
            self.exit_code = 0 if success else 1
            # Shutdown is bounded even if mailbox/transit has disappeared.
            reactor.callLater(2, lambda: reactor.running and reactor.stop())
            if self.wormhole:
                self.wormhole.close().addBoth(lambda _: reactor.running and reactor.stop())
            else:
                reactor.callLater(0, reactor.stop)

        async def connect_peer(self, identity):
            self.wormhole = w = create(APP_ID, RENDEZVOUS_RELAY, reactor, dilation=True)
            if self.config["host"]:
                w.allocate_code(8)
                code = await w.get_code()
                self.emit({"event": "ready", "code": code})
            else:
                w.set_code(self.config["code"])
            await w.get_verifier()
            if self.closing:
                return
            owner = self

            class Channel(Protocol):
                def connectionMade(self):
                    if owner.closing or owner.records is not None:
                        self.transport.loseConnection()
                        owner.stop(False)
                        return
                    owner.records = Records(identity, self.transport.write, owner.emit)
                    owner.records.start()

                def dataReceived(self, data):
                    if owner.closing:
                        return
                    try:
                        owner.records.receive(data)
                    except Exception as error:
                        owner.stop(False, "record-" + type(error).__name__)

                def connectionLost(self, _):
                    owner.stop(False)

            dilated = w.dilate(transit_relay_location=TRANSIT_RELAY,
                               no_listen=self.config.get("relayOnly", False),
                               ping_interval=5.0, expected_subprotocols=[SUBPROTOCOL])
            factory = Factory.forProtocol(Channel)
            if self.config["host"]:
                await dilated.listener_for(SUBPROTOCOL).listen(factory)
            else:
                await dilated.connector_for(SUBPROTOCOL).connect(factory)

        def tick(self):
            if self.closing:
                return
            try:
                if not self.records or not self.records.verified:
                    if time.monotonic() - self.started > 45:
                        raise ValueError("admission timeout")
                else:
                    self.records.check_timeout()
            except Exception:
                self.stop(False)
            if not self.closing:
                reactor.callLater(1, self.tick)

    owner = Owner()
    stdio.StandardIO(owner)
    reactor.callLater(1, owner.tick)
    reactor.run()
    return owner.exit_code


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        # Avoid exception objects: dependencies may include private endpoints.
        print('{"event":"error"}', flush=True)
        raise SystemExit(1)
