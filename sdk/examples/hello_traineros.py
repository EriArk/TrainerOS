"""Real optional context + interactive host widget; no third-party packages."""
from pathlib import Path
import argparse
import json
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from traineros import Client, HostError, HostUnavailable

parser = argparse.ArgumentParser()
parser.add_argument("--endpoint")
parser.add_argument("--app-id", default="org.example.hello")
parser.add_argument("--context-only", action="store_true")
args = parser.parse_args()

try:
    with Client(args.app_id, args.endpoint) as host:
        print("Available services:", ", ".join(host.capabilities), flush=True)
        print(json.dumps(host.subscribe(), ensure_ascii=False), flush=True)
        clicks = 0

        def update():
            if not args.context_only and host.context.get("profile") and host.supports("ui.start.widget"):
                host.widget("hello", "Hello TrainerOS", f"Press A or tap. Activations: {clicks}")

        update()
        if not args.context_only:
            print("Start > Edit > Add > Hello TrainerOS; close this program to remove it."
                  if host.context.get("profile") else
                  "Select/create a local Trainer, then restart this example to register its widget.", flush=True)
        while True:
            event = host.poll(1.0)
            if not event:
                continue
            if event["event"] in ("context.revoked", "host.stopping"):
                print(event["event"], "— reconnect only for the intended profile", flush=True)
                break
            print(json.dumps(event, ensure_ascii=False), flush=True)
            if event["event"] == "widget.activated":
                clicks += 1
                update()
except HostUnavailable as exc:
    print(f"Optional integration unavailable: {exc}. The application can continue standalone.")
except HostError as exc:
    print("Host rejected the operation:", exc.code)
    sys.exit(1)
except KeyboardInterrupt:
    pass
