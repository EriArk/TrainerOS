#!/usr/bin/python3
"""Keep the compositor alive for an orphaned Adventure before restarting its shell."""
import ctypes
import logging
from logging.handlers import RotatingFileHandler
import os
from pathlib import Path
import subprocess
import select
import sys
import time


def supervise(command, attempts=3, ready_timeout=None):
    # Linux reparents orphaned grandchildren here instead of PID 1. Never kill
    # them just because the graphical shell crashed: they may be saving a game.
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise OSError(ctypes.get_errno(), "Cannot protect Adventure descendants")
    failures = 0
    while failures < attempts:
        started = time.monotonic()
        startup_timed_out = False
        if ready_timeout is None:
            child = subprocess.Popen(command)
        else:
            reader, writer = os.pipe()
            environment = os.environ.copy()
            environment["TRAINEROS_READY_FD"] = str(writer)
            try:
                child = subprocess.Popen(command, env=environment, pass_fds=(writer,))
            finally:
                os.close(writer)
            try:
                readable, _, _ = select.select([reader], [], [], ready_timeout)
                ready = bool(readable) and os.read(reader, 1) == b"R"
                if ready:
                    logging.info("First frame received")
                if not ready and child.poll() is None:
                    try:
                        child.wait(timeout=0.2)  # EOF can precede process reaping.
                    except subprocess.TimeoutExpired:
                        startup_timed_out = True
                        logging.error("No rendered frame before startup deadline")
                        child.kill()
            finally:
                os.close(reader)
        result = child.wait()
        logging.info("TrainerOS exited: %s", result)
        while True:
            try:
                pid, status = os.waitpid(-1, 0)
                logging.info("Adventure descendant finished: pid=%s exit=%s", pid, os.waitstatus_to_exitcode(status))
            except ChildProcessError:
                break
            except InterruptedError:
                continue
        if result == 0:
            return 0
        if startup_timed_out:
            return 70
        failures = 0 if time.monotonic() - started >= 120 else failures
        failures += 1
        time.sleep(1)
    return 70


if __name__ == "__main__":
    logs = Path.home() / ".local/state/traineros"
    logs.mkdir(parents=True, exist_ok=True, mode=0o700)
    handler = RotatingFileHandler(logs / "session.log", maxBytes=262144, backupCount=2)
    handler.setFormatter(logging.Formatter("%(asctime)s %(message)s"))
    logging.basicConfig(level=logging.INFO, handlers=[handler])
    # There is no root execution or user-supplied shell command in this client.
    executable = str(Path.home() / ".local/bin/traineros")
    os.environ["TRAINEROS_SESSION"] = "1"
    os.environ["QT_QPA_PLATFORM"] = "xcb"
    # Match Armada's X11 application environment. With WAYLAND_DISPLAY set,
    # Flatpak's fallback-x11 permission hides DISPLAY; RetroArch's Qt companion
    # then aborts because QT_QPA_PLATFORM requests xcb. Wayland remains available
    # explicitly to adapters through the Gamescope-specific variable.
    if os.environ.get("WAYLAND_DISPLAY"):
        os.environ["GAMESCOPE_WAYLAND_DISPLAY"] = os.environ.pop("WAYLAND_DISPLAY")
    subprocess.run(["dbus-update-activation-environment", "--systemd", "DISPLAY",
                    "GAMESCOPE_WAYLAND_DISPLAY", "WAYLAND_DISPLAY="], check=True)
    sys.exit(supervise([executable], ready_timeout=45))
