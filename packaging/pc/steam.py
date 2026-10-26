#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned Steam shortcuts and a lifetime-bound request to the running shell.

No game commands are stored in Steam. Unrecognised VDF is refused, never repaired.
"""
import hashlib
import json
import os
from pathlib import Path
import socket
import shlex
import struct
import sys
import tempfile
import uuid
import zlib


def decode(data):
    if len(data) > 8 * 1024 * 1024:
        raise ValueError("Steam shortcut database is too large")
    position = 0
    count = 0

    def take(size):
        nonlocal position
        if position + size > len(data):
            raise ValueError("Incomplete Steam shortcut database")
        value = data[position:position + size]
        position += size
        return value

    def string():
        nonlocal position
        end = data.find(b"\0", position)
        if end < 0:
            raise ValueError("Incomplete Steam string")
        value = take(end - position)
        take(1)
        return value.decode("utf-8", "surrogateescape")

    def obj(depth=0):
        nonlocal count
        if depth > 12:
            raise ValueError("Steam shortcut nesting is too deep")
        result = {}
        while True:
            kind = take(1)[0]
            if kind == 8:
                return result
            count += 1
            if count > 100000:
                raise ValueError("Too many Steam shortcut fields")
            key = string()
            if key in result:
                raise ValueError("Duplicate Steam field")
            if kind == 0:
                value = obj(depth + 1)
            elif kind == 1:
                value = string()
            elif kind in (2, 3, 4, 6):
                value = take(4)
            elif kind == 7:
                value = take(8)
            else:
                raise ValueError("Unsupported Steam database field; no changes made")
            result[key] = (kind, value)
    result = obj()
    if position != len(data):
        raise ValueError("Unexpected data after Steam shortcuts")
    return result


def encode(obj):
    result = bytearray()
    for key, (kind, value) in obj.items():
        result.extend(bytes([kind]) + key.encode("utf-8", "surrogateescape") + b"\0")
        result.extend(encode(value) if kind == 0 else value.encode("utf-8", "surrogateescape") + b"\0" if kind == 1 else value)
    return bytes(result) + b"\x08"


def quote(value):
    if any(c in value for c in ("\0", "\n", "\r")):
        raise ValueError("Invalid shortcut argument")
    return shlex.quote(value)


def steam_running():
    proc = Path("/proc")
    if not proc.is_dir():
        raise RuntimeError("Steam shortcut updates require Linux")
    for item in proc.iterdir():
        if not item.name.isdigit():
            continue
        try:
            if item.stat().st_uid == os.getuid() and (item / "comm").read_text().strip() in ("steam", "steamwebhelper"):
                return True
        except (OSError, ProcessLookupError):
            pass
    return False


def identity(request):
    game = request["id"]
    if not game.startswith("pc:") or str(uuid.UUID(game[3:])) != game[3:]:
        raise ValueError("Invalid PC game identity")
    owner = request["owner"]
    if len(owner) != 64 or any(c not in "0123456789abcdef" for c in owner):
        raise ValueError("Invalid owner identity")
    return "TrainerOS:" + owner + ":" + game


def update(request, running=steam_running):
    import fcntl
    marker = identity(request)
    config = Path(request["config"]).resolve(strict=True)
    if config.name != "config" or not config.parent.name.isdigit() or config.parent.parent.name != "userdata":
        raise ValueError("Choose an existing Steam user")
    path = config / "shortcuts.vdf"
    if path.is_symlink():
        raise ValueError("Steam shortcut database is a symbolic link")
    with (config / ".traineros-shortcuts.lock").open("a+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if running():
            raise RuntimeError("Close Steam before changing its shortcuts")
        original = path.read_bytes() if path.exists() else None
        tree = decode(original) if original is not None else {"shortcuts": (0, {})}
        if "shortcuts" not in tree or tree["shortcuts"][0] != 0:
            raise ValueError("Unrecognised Steam shortcuts database")
        shortcuts = tree["shortcuts"][1]
        helper = str(Path(__file__).resolve())
        state = str(Path(request["state"]).resolve())
        options = " ".join(map(quote, [helper, "--launch", state, request["owner"], request["id"]]))
        matches = []
        for key, (kind, shortcut) in shortcuts.items():
            if kind != 0:
                raise ValueError("Unrecognised Steam shortcut entry")
            owned = shortcut.get("DevkitGameID") == (1, marker) or (
                shortcut.get("LaunchOptions") == (1, options) and shortcut.get("exe") == (1, '"/usr/bin/python3"'))
            if owned:
                if shortcut.get("exe") != (1, '"/usr/bin/python3"') or shortcut.get("LaunchOptions") != (1, options):
                    raise ValueError("Owned shortcut was changed externally; keep it and resolve the conflict in Steam")
                matches.append(key)
        if request["operation"] == "status":
            return bool(matches)
        if len(matches) > 1:
            raise ValueError("Duplicate owned shortcuts; no changes made")
        if request["operation"] == "remove":
            for key in matches:
                del shortcuts[key]
        elif request["operation"] == "add":
            number = 0
            while str(number) in shortcuts:
                number += 1
            key = matches[0] if matches else str(number)
            entry = dict(shortcuts[key][1]) if matches else {}
            integer = lambda n: (2, struct.pack("<I", n))
            appid = zlib.crc32(marker.encode()) | 0x80000000
            if not matches and any(s.get("appid") == integer(appid) for k, s in shortcuts.values() if k == 0):
                raise ValueError("Steam shortcut ID conflict")
            entry.update(appid=integer(appid), appname=(1, request["title"]), exe=(1, '"/usr/bin/python3"'),
                         StartDir=(1, '"' + str(Path(helper).parent) + '"'), LaunchOptions=(1, options),
                         IsHidden=integer(0), AllowDesktopConfig=integer(1), AllowOverlay=integer(1),
                         OpenVR=integer(0), Devkit=integer(0), DevkitGameID=(1, marker), LastPlayTime=entry.get("LastPlayTime", integer(0)))
            tags = dict(entry.get("tags", (0, {}))[1])
            if (1, "TrainerOS") not in tags.values():
                tag = 0
                while str(tag) in tags:
                    tag += 1
                tags[str(tag)] = (1, "TrainerOS")
            entry["tags"] = (0, tags)
            shortcuts[key] = (0, entry)
        else:
            raise ValueError("Unsupported Steam operation")
        data = encode(tree)
        if decode(data) != tree:
            raise ValueError("Steam serialization check failed")
        if data == original:
            return request["operation"] == "add"
        # Back up and verify the exact old bytes before replacing the database.
        if original is not None:
            backup = config / ("shortcuts.vdf.traineros-" + hashlib.sha256(original).hexdigest()[:20] + ".bak")
            if not backup.exists():
                with backup.open("xb") as out:
                    out.write(original); out.flush(); os.fsync(out.fileno())
            if backup.read_bytes() != original:
                raise RuntimeError("Steam backup verification failed")
        with tempfile.NamedTemporaryFile(dir=config, prefix=".traineros-shortcuts-", delete=False) as out:
            temporary = Path(out.name)
            out.write(data); out.flush(); os.fsync(out.fileno())
        try:
            if running() or (path.read_bytes() if path.exists() else None) != original:
                raise RuntimeError("Steam changed its shortcuts; no changes made")
            os.replace(temporary, path)
            if path.read_bytes() != data:
                raise RuntimeError("Steam shortcut verification failed")
        finally:
            temporary.unlink(missing_ok=True)
        return request["operation"] == "add"


def launch(state, owner, game):
    identity(dict(owner=owner, id=game))
    endpoint = Path(state) / "pc" / owner / "steam-endpoint.json"
    address = json.loads(endpoint.read_text())["socket"]
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(10)
        client.connect(address)
        environment = {key: os.environ[key] for key in ("SteamAppId", "SteamGameId", "SteamOverlayGameId")
                       if os.environ.get(key, "").isascii() and os.environ.get(key, "").isdigit()}
        client.sendall(json.dumps(dict(owner=owner, id=game, environment=environment)).encode() + b"\n")
        with client.makefile("rb") as stream:
            first = stream.readline(4096)
            response = json.loads(first)
            if response.get("state") != "started":
                raise RuntimeError(response.get("error", "TrainerOS could not start this game"))
            # Steam must see the real game's lifetime, including launcher's children.
            client.settimeout(None)
            final = json.loads(stream.readline(4096))
            return int(final.get("exitCode", 1))


if __name__ == "__main__":
    try:
        if len(sys.argv) == 5 and sys.argv[1] == "--launch":
            sys.exit(launch(*sys.argv[2:]))
        if len(sys.argv) != 2:
            raise ValueError("Expected a shortcut operation")
        print(json.dumps(dict(present=update(json.loads(sys.argv[1])))))
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
