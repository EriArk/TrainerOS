#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One accepted party's opaque SRAM preparation; never reads a user file.

Online uses Magic Wormhole's authenticated, encrypted ordered-message API.
At most eight 16 KiB data messages, one offer and one receipt per preparation.
Nearby uses a one-shot TLS upload pinned to the accepted invitation's certificate.
Input/output are private, bounded JSON pipes owned by TrainerOS, not log streams.
"""
import base64
import hashlib
import ipaddress
import json
import os
import re
import signal
import sys
import threading
from contextlib import closing

MAX_SIZE = 131072
CHUNK = 16384
APP_ID = "org.traineros.linked-save/v1"


def emit(value):
    print(json.dumps(value, separators=(",", ":")), flush=True)


def validate(config):
    if not isinstance(config, dict) or type(config.get("host")) is not bool:
        raise ValueError("config")
    if config.get("size") not in (8192, 32768, 65536, 131072):
        raise ValueError("size")
    if not re.fullmatch(r"[0-9a-f]{64}", config.get("content", "")):
        raise ValueError("content")
    if config.get("mode") not in ("online", "nearby"):
        raise ValueError("mode")
    data = b""
    if not config["host"]:
        data = base64.b64decode(config.get("data", ""), validate=True)
        if len(data) != config["size"]:
            raise ValueError("data")
    return data


async def online(reactor, config, data):
    from wormhole import create
    from wormhole.cli.public_relay import RENDEZVOUS_RELAY
    w = create(APP_ID, RENDEZVOUS_RELAY, reactor)
    try:
        if config["host"]:
            w.allocate_code(8)
            emit({"event": "ready", "endpoint": {"mode": "online", "code": await w.get_code()}})
        else:
            code = config.get("code", "")
            if len(code) > 160 or not re.fullmatch(r"[0-9]+(?:-[a-z]+){8}", code):
                raise ValueError("code")
            w.set_code(code)
        await w.get_verifier()
        if config["host"]:
            raw = await w.get_message()
            if len(raw) > 1024:
                raise ValueError("offer")
            offer = json.loads(raw)
            if offer.get("content") != config["content"] or offer.get("size") != config["size"]:
                raise ValueError("identity")
            chunks = []
            for offset in range(0, config["size"], CHUNK):
                chunk = await w.get_message()
                if len(chunk) != min(CHUNK, config["size"] - offset):
                    raise ValueError("length")
                chunks.append(chunk)
            data = b"".join(chunks)
            if hashlib.sha256(data).hexdigest() != offer.get("sha256"):
                raise ValueError("digest")
            w.send_message(b"received")
            if await w.get_message() != b"done":
                raise ValueError("receipt")
        else:
            w.send_message(json.dumps({"content": config["content"], "size": len(data),
                                       "sha256": hashlib.sha256(data).hexdigest()}).encode())
            for offset in range(0, len(data), CHUNK):
                w.send_message(data[offset:offset + CHUNK])
            if await w.get_message() != b"received":
                raise ValueError("receipt")
            w.send_message(b"done")
    finally:
        await w.close()
    emit({"event": "done", "data": base64.b64encode(data).decode() if config["host"] else ""})


def nearby(config, data):
    import http.client
    import http.server
    import secrets
    import socket
    import ssl
    import tempfile
    from pathlib import Path

    address = str(ipaddress.IPv4Address(config["address"]))
    if not config["host"]:
        port = config.get("port", 0)
        fingerprint, token = config.get("fingerprint", ""), config.get("token", "")
        if not 1 <= port <= 65535 or not re.fullmatch(r"[0-9a-f]{64}", fingerprint) or not re.fullmatch(r"[0-9a-f]{32}", token):
            raise ValueError("endpoint")
        # Pin BEFORE sending any application bytes; system CAs are irrelevant
        # to this ephemeral, invitation-bound certificate.
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        with closing(http.client.HTTPSConnection(address, port, timeout=10, context=context)) as conn:
            conn.connect()
            if hashlib.sha256(conn.sock.getpeercert(binary_form=True)).hexdigest() != fingerprint:
                raise ValueError("certificate")
            conn.request("POST", "/save/" + token, data,
                         {"X-TrainerOS-Content": config["content"], "Content-Type": "application/octet-stream"})
            response = conn.getresponse()
            if response.status != 200 or response.read(16) != b"received":
                raise ValueError("receipt")
    else:
        from cryptography import x509
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import ec
        from cryptography.x509.oid import NameOID
        from datetime import datetime, timedelta, timezone
        key = ec.generate_private_key(ec.SECP256R1())
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "TrainerOS session")])
        now = datetime.now(timezone.utc)
        cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
                .serial_number(x509.random_serial_number()).not_valid_before(now - timedelta(minutes=1))
                .not_valid_after(now + timedelta(minutes=5)).sign(key, hashes.SHA256()))
        token = secrets.token_hex(16)
        received = []

        class Upload(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_POST(self):
                if (self.path != "/save/" + token or self.headers.get("X-TrainerOS-Content") != config["content"]
                        or self.headers.get("Content-Length") != str(config["size"])
                        or self.headers.get("Transfer-Encoding") is not None or received):
                    self.send_error(400)
                    return
                body = self.rfile.read(config["size"])
                if len(body) != config["size"]:
                    self.send_error(400)
                    return
                self.send_response(200)
                self.send_header("Content-Length", "8")
                self.end_headers()
                self.wfile.write(b"received")
                self.wfile.flush()
                received.append(body)

        with tempfile.TemporaryDirectory(prefix="traineros-link-tls-") as folder:
            os.chmod(folder, 0o700)
            pem = Path(folder) / "session.pem"
            pem.write_bytes(cert.public_bytes(serialization.Encoding.PEM) +
                            key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                              serialization.NoEncryption()))
            os.chmod(pem, 0o600)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(pem)

            class Server(http.server.HTTPServer):
                def get_request(self):
                    sock, peer = self.socket.accept()
                    sock.settimeout(5)
                    try:
                        return context.wrap_socket(sock, server_side=True), peer
                    except Exception:
                        sock.close()
                        raise

                def handle_error(self, request, client_address):
                    pass  # No private endpoint or data in stderr.

            with Server((address, 0), Upload) as server:
                emit({"event": "ready", "endpoint": {"mode": "nearby", "port": server.server_port,
                      "fingerprint": cert.fingerprint(hashes.SHA256()).hex(), "token": token}})
                while not received:
                    server.handle_request()
            data = received[0]
    emit({"event": "done", "data": base64.b64encode(data).decode() if config["host"] else ""})


def main():
    # Losing the owning application closes the pipe and ends this preparation.
    signal.signal(signal.SIGALRM, lambda *_: os._exit(2))
    signal.alarm(70)
    line = sys.stdin.buffer.readline(262145)
    if len(line) > 262144 or not line.endswith(b"\n"):
        raise ValueError("config")
    config = json.loads(line)
    data = validate(config)
    def parent_closed():
        os.read(0, 1)
        os._exit(0)
    threading.Thread(target=parent_closed, daemon=True).start()
    if config["mode"] == "nearby":
        nearby(config, data)
    else:
        from twisted.internet.task import react
        react(online, [config, data])


if __name__ == "__main__":
    try:
        main()
    except Exception:
        emit({"event": "error"})
        sys.exit(1)
