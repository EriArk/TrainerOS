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
import shutil
import tempfile


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
    def relocated(value):
        path = Path(value).resolve(strict=True)
        original = config.get("contentPrefix")
        if original and config.get("runner") != "native":
            base = Path(original).resolve()
            if path.is_relative_to(base):
                path = (Path(config["prefix"]) / path.relative_to(base)).resolve(strict=True)
        return path
    exe = relocated(config["executable"])
    root = relocated(config["installRoot"])
    maintenance = config.get("mode") == "maintenance"
    installing = config.get("mode") == "installer" or maintenance
    external = config.get("mode") == "installer" or (maintenance and config.get("operation") == "patch")
    if not exe.is_file() or (not external and not exe.is_relative_to(root)):
        raise ValueError("Game executable is outside its installation")
    cwd = relocated(config["workingDirectory"]) if config.get("workingDirectory") else exe.parent
    if not cwd.is_dir() or not cwd.is_relative_to(root):
        raise ValueError("Game working directory is unavailable")
    env = os.environ.copy()
    for key in ("SteamAppId", "SteamGameId", "SteamOverlayGameId"):
        env.pop(key, None)
        value = config.get("steamEnvironment", {}).get(key)
        if isinstance(value, str) and value.isascii() and value.isdigit() and len(value) <= 20:
            env[key] = value
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
    elif config["runner"] in ("proton", "wine"):
        prefix = Path(config["prefix"])
        if not prefix.is_absolute() or prefix == Path.home() or prefix == Path("/"):
            raise ValueError("Invalid game prefix")
        prefix.mkdir(parents=True, exist_ok=True, mode=0o700)
        env["WINEPREFIX"] = str(prefix)
        # A shell inherited from another launcher must not choose our prefix or runner.
        for key in ("PROTONPATH", "WINE", "WINESERVER", "WINELOADER", "WINETRICKS", "WINETRICKS_DOWNLOADER",
                    "UMU_NO_PROTON", "WINEARCH", "WINEDLLOVERRIDES"):
            env.pop(key, None)
        if config["runner"] == "proton":
            proton = Path(config["proton"])
            umu = Path(config["umu"])
            if not (proton / "proton").is_file() or not umu.is_file():
                raise ValueError("The selected Windows runtime is unavailable")
            env.update(PROTONPATH=str(proton), GAMEID="umu-traineros", UMU_RUNTIME_UPDATE="0", PROTON_VERB="waitforexitandrun")
            runner = [sys.executable, str(umu)]
        else:
            wine = Path(config["wine"])
            if not wine.is_file() or not os.access(wine, os.X_OK):
                raise ValueError("The selected Wine is unavailable")
            runner = [str(wine)]
        operation = config.get("operation") if maintenance else None
        if operation == "component":
            verb = config.get("operationValue")
            if verb not in ("vcrun2022", "d3dx9", "dotnet48"):
                raise ValueError("Unsupported component")
            if config["runner"] == "proton":
                # UMU's supported prefix-bound Winetricks entry point. No system Wine fallback.
                if not (proton / "protonfixes" / "winetricks").is_file():
                    raise ValueError("This Proton build does not provide Winetricks")
                argv = runner + ["winetricks", verb]
            else:
                import shutil
                winetricks = shutil.which("winetricks")
                if not winetricks:
                    raise ValueError("Winetricks is not installed")
                server = wine.parent / "wineserver"
                if not server.is_file():
                    raise ValueError("This Wine build has no matching wineserver")
                env.update(WINE=str(wine), WINESERVER=str(server))
                argv = [winetricks, verb]
        elif operation == "tool":
            tool = config.get("operationValue")
            if tool not in ("winecfg", "uninstaller"):
                raise ValueError("Unsupported Windows tool")
            argv = runner + [tool]
        elif installing and config.get("installerFormat") == "msi":
            # UMU passes non-host executable names through to the selected
            # Proton prefix. MSI is a package, never an executable itself.
            argv = runner + ["msiexec", "/i", str(exe)]
        else:
            argv = runner + [str(exe)]
    else:
        raise ValueError("Unsupported PC runner")
    args = config.get("arguments", [])
    if not isinstance(args, list) or not all(isinstance(arg, str) for arg in args):
        raise ValueError("Invalid game arguments")
    return argv + args, str(cwd), env


def prepare_prefix(config):
    source = config.get("prefixSource")
    if not source or config.get("runner") == "native":
        return
    destination = Path(config["prefix"])
    source = Path(source)
    if not destination.is_absolute() or not source.is_absolute() or destination == source or destination.is_relative_to(source):
        raise ValueError("Invalid environment migration")
    destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    # Each migration has its own UUID. Failure leaves the prior prefix intact,
    # and no partly copied directory is published as the new environment.
    with (destination.parent / (destination.name + ".prepare.lock")).open("a+") as preparation:
        fcntl.flock(preparation, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if destination.is_symlink():
            raise ValueError("Invalid environment destination")
        if destination.exists():
            if not (destination / ".traineros-prefix-ready").is_file():
                raise ValueError("The new environment is incomplete; choose the previous runtime")
            return
        if not source.exists():
            if config.get("prefixSourceExisted", True):
                raise ValueError("Reconnect the previous environment's storage before changing runtimes")
            destination.mkdir(mode=0o700)
            (destination / ".traineros-prefix-ready").write_text("fresh")
            return
        if source.is_symlink() or not source.is_dir():
            raise ValueError("The previous environment is unavailable")
        source_lock = source / ".traineros-session.lock"
        if source_lock.is_symlink():
            raise ValueError("Invalid environment lease")
        with source_lock.open("a+") as lease:
            try:
                fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise RuntimeError("Close the game before changing its runtime") from None
            temporary = Path(tempfile.mkdtemp(prefix=".prefix-copy-", dir=destination.parent))
            try:
                # Keep Wine's dosdevices links as links; never follow a drive link
                # to copy the host filesystem. copytree errors keep the old prefix.
                shutil.copytree(source, temporary, symlinks=True, dirs_exist_ok=True,
                                ignore=lambda path, names: [n for n in names if n.startswith(".traineros-")])
                (temporary / ".traineros-prefix-ready").write_text("copied")
                os.replace(temporary, destination)
            finally:
                if temporary.exists():
                    shutil.rmtree(temporary)


def run(config):
    prepare_prefix(config)
    # A shared prefix lease covers games and maintenance, including after a shell
    # restart. Do not unlink the lock inode while other processes may hold it.
    prefix = config.get("prefix") if config.get("runner") != "native" else config.get("installRoot")
    if prefix:
        directory = Path(prefix)
        if not directory.is_absolute() or directory in (Path.home(), Path("/")):
            raise ValueError("Invalid game prefix")
        directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        lock_path = directory / ".traineros-session.lock"
        if lock_path.is_symlink():
            raise ValueError("Invalid session lock")
        with lock_path.open("a+") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise RuntimeError("This game's environment is already in use") from None
            return supervise(*command(config))
    return supervise(*command(config))


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
                return run(config)
            finally:
                lock_path.unlink(missing_ok=True)
    return run(config)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print("PC launch failed:", error, file=sys.stderr)
        sys.exit(1)
