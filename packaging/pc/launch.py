#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""PC session supervisor. No shell evaluation, Steam mutation or global Wine kill."""
import ctypes
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import fcntl


def children():
    """All descendants, including double-forked children adopted by this subreaper."""
    result = set()
    pending = [os.getpid()]
    while pending:
        pid = pending.pop()
        try:
            ids = Path(f"/proc/{pid}/task/{pid}/children").read_text().split()
        except OSError:
            continue
        for item in ids:
            child = int(item)
            if child not in result:
                result.add(child)
                pending.append(child)
    return result


def supervise(argv, cwd, env):
    if not sys.platform.startswith("linux"):
        raise RuntimeError("PC sessions require Linux process supervision")
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise OSError(ctypes.get_errno(), "Cannot supervise game children")
    stopping = [None]

    def stop(_sig, _frame):
        if stopping[0] is None:
            stopping[0] = time.monotonic()

    for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(sig, stop)
    process = subprocess.Popen(argv, cwd=cwd, env=env, start_new_session=True)
    result = None
    while True:
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if not pid:
                    break
                code = os.waitstatus_to_exitcode(status)
                if pid == process.pid:
                    result = code or result
                    process.returncode = code
                elif code and not result:
                    result = code
        except ChildProcessError:
            return 130 if stopping[0] is not None else (result or 0)
        if stopping[0] is not None:
            sig = signal.SIGKILL if time.monotonic() - stopping[0] > .5 else signal.SIGTERM
            for pid in children():
                try:
                    os.kill(pid, sig)
                except ProcessLookupError:
                    pass
        time.sleep(.025)


def command(config):
    exe = Path(config["executable"]).resolve(strict=True)
    root = Path(config["installRoot"]).resolve(strict=True)
    installing = config.get("mode") == "installer"
    if not exe.is_file() or (not installing and not exe.is_relative_to(root)):
        raise ValueError("Game executable is outside its installation")
    cwd = Path(config.get("workingDirectory") or exe.parent).resolve(strict=True)
    if not cwd.is_dir() or not cwd.is_relative_to(root):
        raise ValueError("Game working directory is unavailable")
    env = os.environ.copy()
    # The existing Home overlay still checks this launch's exact process tree
    # and WM_DELETE_WINDOW; the marker admits ordinary game executable names.
    env["TRAINEROS_PC_MANAGED"] = "1"
    # Reuse the Apps Xwayland window owner, including child installer dialogs.
    if installing:
        env.update(QT_QPA_PLATFORM="xcb", GDK_BACKEND="x11",
                   TRAINEROS_INSTALL_DIRECTORY=str(root))
    # The shell's renderer/plugin paths must not leak into games.
    for key in ("QT_QUICK_BACKEND", "QT_SCALE_FACTOR", "QSG_RHI_BACKEND", "QT_PLUGIN_PATH", "QML2_IMPORT_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH"):
        env.pop(key, None)
    if config["runner"] == "native":
        script = installing and config.get("installerFormat") == "script"
        if not script and not os.access(exe, os.X_OK):
            raise ValueError("Game executable is not executable")
        if config.get("dataRoot"):
            data = Path(config["dataRoot"])
            data.mkdir(parents=True, exist_ok=True, mode=0o700)
            env.update(XDG_CONFIG_HOME=str(data / "config"), XDG_DATA_HOME=str(data / "share"),
                       XDG_CACHE_HOME=str(data / "cache"))
        if script:
            with exe.open("rb") as stream:
                shebang = stream.readline(256).rstrip(b"\n")
            interpreters = {b"#!/bin/sh": "/bin/sh", b"#!/bin/bash": "/bin/bash",
                            b"#!/usr/bin/env bash": "/bin/bash"}
            if shebang not in interpreters:
                raise ValueError("Unsupported installer interpreter")
            argv = [interpreters[shebang], str(exe)]
        else:
            argv = [str(exe)]
    elif config["runner"] == "proton":
        proton = Path(config["proton"])
        umu = Path(config["umu"])
        if not (proton / "proton").is_file() or not umu.is_file():
            raise ValueError("The selected Windows runtime is unavailable")
        prefix = Path(config["prefix"])
        prefix.mkdir(parents=True, exist_ok=True, mode=0o700)
        env.update(WINEPREFIX=str(prefix), PROTONPATH=str(proton), GAMEID="umu-traineros",
                   UMU_RUNTIME_UPDATE="0", PROTON_VERB="waitforexitandrun")
        if installing and config.get("installerFormat") == "msi":
            # UMU passes non-host executable names through to the selected
            # Proton prefix. MSI is a package, never an executable itself.
            argv = [sys.executable, str(umu), "msiexec", "/i", str(exe)]
        else:
            argv = [sys.executable, str(umu), str(exe)]
    else:
        raise ValueError("Unsupported PC runner")
    args = config.get("arguments", [])
    if not isinstance(args, list) or not all(isinstance(arg, str) for arg in args):
        raise ValueError("Invalid game arguments")
    return argv + args, str(cwd), env


def main():
    if len(sys.argv) != 2:
        raise ValueError("Expected a PC launch configuration")
    config = json.loads(sys.argv[1])
    if config.get("mode") == "installer":
        # A shell restart may leave this supervisor running. Recovery and
        # discard must distinguish a live prefix from an abandoned import.
        root = Path(config["installRoot"]).resolve(strict=True)
        lock_path = Path(config["activityFile"])
        if not root.is_relative_to(lock_path.parent.resolve(strict=True)) or lock_path.is_symlink():
            raise ValueError("Invalid installation activity file")
        with lock_path.open("a+") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            lock.seek(0);lock.truncate()
            start = Path(f"/proc/{os.getpid()}/stat").read_text().rsplit(")", 1)[1].split()[19]
            json.dump(dict(pid=os.getpid(), start=start), lock);lock.flush();os.fsync(lock.fileno())
            try:
                return supervise(*command(config))
            finally:
                lock_path.unlink(missing_ok=True)
    return supervise(*command(config))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print("PC launch failed:", error, file=sys.stderr)
        sys.exit(1)
