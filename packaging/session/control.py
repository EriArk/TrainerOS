#!/usr/bin/python3 -I
"""Root-owned, fixed-verb Armada session boundary. Never load user Python code."""
import os
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

OVERRIDE = Path("/etc/sddm.conf.d/zz-steamos-autologin.conf")
ARMADA = "/usr/libexec/armada/session-control"
MARKER = Path("/run/traineros-transition")


def session_pids():
    result = []
    for process in Path("/proc").glob("[0-9]*"):
        try:
            arguments = (process / "cmdline").read_bytes().split(b"\0")
            if arguments[:2] == [b"/bin/bash", b"/var/opt/traineros/session/start-session.sh"]:
                result.append(int(process.name))
        except OSError:
            pass
    return result


def consume_transition(pid):
    try:
        value = json.loads(MARKER.read_text())
        if time.monotonic() - value["time"] > 30 or pid not in value["pids"]:
            return False
        value["pids"].remove(pid)
        atomic_write(MARKER, json.dumps(value))
        return True
    except (OSError, ValueError, KeyError, TypeError):
        return False


def atomic_write(path, content):
    fd, temporary = tempfile.mkstemp(prefix=".traineros-", dir=path.parent)
    try:
        with os.fdopen(fd, "w") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
            os.fchmod(stream.fileno(), 0o644)
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def main():
    if os.geteuid() != 0 or len(sys.argv) not in {2, 3}:
        return 2
    action = sys.argv[1]
    if action not in {"traineros", "default-traineros", "desktop", "steam", "recover", "reboot", "poweroff"}:
        return 2
    if (action == "recover" and (len(sys.argv) != 3 or not sys.argv[2].isascii() or not sys.argv[2].isdigit())) or (action != "recover" and len(sys.argv) != 2):
        return 2
    os.environ.clear()
    os.environ.update(PATH="/usr/sbin:/usr/bin:/sbin:/bin", LANG="C.UTF-8")
    if action == "recover":
        # Only the old session may consume its own transition. A new session's
        # startup failure must still recover, including after a manual switch.
        if consume_transition(int(sys.argv[2])):
            return 0
        if OVERRIDE.is_file() and "Session=traineros.desktop" not in OVERRIDE.read_text():
            return 0
        return subprocess.call([ARMADA, "switch-desktop"])
    if action in {"traineros", "default-traineros"}:
        if not Path("/etc/traineros/wayland-sessions/traineros.desktop").is_file():
            return 3
        atomic_write(OVERRIDE, "[Autologin]\nSession=traineros.desktop\n")
        if action == "default-traineros":
            return 0
    atomic_write(MARKER, json.dumps({"pids": session_pids(), "time": time.monotonic()}))
    if action in {"desktop", "steam"}:
        result = subprocess.call([ARMADA, "switch-desktop" if action == "desktop" else "switch-gamemode"])
    else:
        result = subprocess.call(["/usr/bin/systemctl", "--no-block", "restart", "sddm"] if action == "traineros"
                                 else ["/usr/bin/systemctl", "--no-block", action])
    if result and MARKER.exists():
        MARKER.unlink()
    return result


if __name__ == "__main__":
    sys.exit(main())
