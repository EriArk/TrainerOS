import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "sdk/python"))
from traineros import Client, HostError

with Client("org.test.process", sys.argv[1]) as host:
    assert host.subscribe()["profile"] == "alice"
    assert not host.supports("network")
    try:
        host.call("wallet.credit", amount=999)
        raise AssertionError("Unbound service succeeded")
    except HostError as error:
        assert error.code == "unsupported_method"
    host.widget("hello", "Process widget", "Waiting for host input")
    print("registered", flush=True)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        event = host.poll(0.5)
        if event and event["event"] == "widget.activated":
            assert event["id"] == "hello"
            host.widget("hello", "Process widget", "Callback received")
            print("activated", flush=True)
        if event and event["event"] == "context.changed":
            assert event["context"]["selected"]["id"] == "game-b"
            print("changed", flush=True)
        if event and event["event"] == "context.revoked":
            print("revoked", flush=True)
            break
    else:
        raise AssertionError("Missing lifecycle event")
