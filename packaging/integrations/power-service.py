#!/usr/bin/python3 -I
"""Fixed-protocol logind/physical power boundary; installed root-owned.

No commands, filenames or device paths are accepted from the graphical client.
Inhibitors are FD leases: loss of the client releases them automatically.
"""
import json
import os
from pathlib import Path
import signal
import struct
import sys
import time

EVENT = struct.Struct("llHHi")
NAMES = {"pm8941_pwrkey", "pmic_pwrkey", "gpio-keys-lid"}


class Button:
    """One menu per hold, no sleep on release after a long press or wake."""
    def __init__(self):
        self.down = None
        self.long = False
        self.ignore_until = 0

    def event(self, value, now):
        if now < self.ignore_until:
            self.down = None
            return None
        if value == 1 and self.down is None:
            self.down, self.long = now, False
        elif value == 0 and self.down is not None:
            held = now - self.down
            self.down = None
            if not self.long:
                return "menu" if held >= 2 else "sleep"
        return None

    def tick(self, now):
        if self.down is not None and not self.long and now - self.down >= 2:
            self.long = True
            return "menu"

    def woke(self, now):
        self.down = None
        self.ignore_until = now + 1.5


def main():
    if os.geteuid() != 0 or len(sys.argv) != 1:
        return 2
    import dbus
    import fcntl
    from dbus.mainloop.glib import DBusGMainLoop
    from gi.repository import GLib
    DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    manager = dbus.Interface(bus.get_object("org.freedesktop.login1", "/org/freedesktop/login1"),
                             "org.freedesktop.login1.Manager")
    loop = GLib.MainLoop()
    button = Button()
    devices = {}
    monotonic_fds = set()
    state = {"awake": False, "sleeping": False, "pending": 0, "lid": False}
    leases = {}
    blanked = {}

    def panel(closed):
        # Only panel blanking, never overwrite the user's brightness level.
        if closed:
            for path in Path("/sys/class/backlight").glob("*/bl_power"):
                if path in blanked:
                    continue
                try:
                    blanked[path] = path.read_text()
                    path.write_text("4")
                except OSError:
                    blanked.pop(path, None)
        else:
            for path, previous in list(blanked.items()):
                try:
                    path.write_text(previous)
                except OSError:
                    pass
                blanked.pop(path, None)

    def emit(event, **values):
        try:
            print(json.dumps({"event": event, **values}), flush=True)
        except BrokenPipeError:
            loop.quit()

    def inhibit(key, what):
        if key not in leases:
            leases[key] = manager.Inhibit(what, "TrainerOS", "Power controls and protected writes", "block").take()

    def release(key):
        if key in leases:
            os.close(leases.pop(key))

    def request(reason):
        if state["sleeping"] or state["pending"] or (reason == "lid" and state["awake"]):
            return
        state["pending"] = time.monotonic()
        emit("sleep-request", reason=reason)

    def action(kind):
        if kind == "menu":
            emit("menu")
        elif kind == "sleep":
            request("button")

    def wake(start):
        if start:
            state["sleeping"] = True
            emit("sleeping")
        else:
            state["sleeping"] = False
            state["pending"] = 0
            button.woke(time.monotonic())
            inhibit("sleep", "sleep")
            emit("awake")

    def failed(error):
        state["sleeping"] = False
        state["pending"] = 0
        inhibit("sleep", "sleep")
        emit("error", message=str(error))

    def read_input(fd, condition):
        if condition & (GLib.IO_HUP | GLib.IO_ERR):
            devices.pop(fd, None)
            os.close(fd)
            return False
        try:
            raw = os.read(fd, EVENT.size * 32)
            for offset in range(0, len(raw) - EVENT.size + 1, EVENT.size):
                seconds, micros, kind, code, value = EVENT.unpack_from(raw, offset)
                if kind == 1 and code == 116:
                    # Preserve the duration even when press/release were queued
                    # together while the process was descheduled.
                    timestamp = seconds + micros / 1_000_000
                    if fd not in monotonic_fds:
                        timestamp += time.monotonic() - time.time()
                    action(button.event(value, timestamp))
                elif kind == 5 and code == 0:
                    state["lid"] = bool(value)
                    panel(bool(value))
                    if value:
                        request("lid")
                    emit("lid", closed=bool(value))
        except BlockingIOError:
            pass
        except OSError:
            devices.pop(fd, None)
            os.close(fd)
            return False
        return True

    def scan():
        for item in Path("/sys/class/input").glob("event*"):
            try:
                if (item / "device/name").read_text().strip() not in NAMES:
                    continue
                path = "/dev/input/" + item.name
                if path in devices.values():
                    continue
                fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC | os.O_NOFOLLOW)
                monotonic_fds.discard(fd)
                try:
                    # EVIOCSCLOCKID, CLOCK_MONOTONIC (Linux evdev).
                    fcntl.ioctl(fd, 0x400445A0, struct.pack("i", time.CLOCK_MONOTONIC))
                    monotonic_fds.add(fd)
                except OSError:
                    pass
                devices[fd] = path
                GLib.io_add_watch(fd, GLib.IO_IN | GLib.IO_HUP | GLib.IO_ERR, read_input)
            except OSError:
                pass
        return True

    buffer = bytearray()
    def commands(fd, condition):
        if condition & (GLib.IO_HUP | GLib.IO_ERR):
            loop.quit()
            return False
        chunk = os.read(fd, 4096)
        if not chunk:
            loop.quit()
            return False
        buffer.extend(chunk)
        if len(buffer) > 16384:
            loop.quit()
            return False
        while b"\n" in buffer:
            line, _, rest = buffer.partition(b"\n")
            buffer[:] = rest
            try:
                command = json.loads(line)
                verb = command.get("command")
                if verb == "keep-awake":
                    state["awake"] = command.get("enabled") is True
                    emit("state", keepAwake=state["awake"])
                elif verb == "request":
                    request("menu")
                elif verb == "reject":
                    state["pending"] = 0
                elif verb == "sleep" and state["pending"] and time.monotonic() - state["pending"] < 10:
                    state["pending"] = 0
                    state["sleeping"] = True
                    release("sleep")
                    manager.Suspend(False, reply_handler=lambda: None, error_handler=failed)
            except (ValueError, TypeError, AttributeError):
                pass
        return True

    def tick():
        action(button.tick(time.monotonic()))
        if state["pending"] and time.monotonic() - state["pending"] > 10:
            state["pending"] = 0
            emit("error", message="Sleep preparation timed out. Please try again.")
        return True

    manager.connect_to_signal("PrepareForSleep", wake)
    inhibit("keys", "handle-power-key:handle-suspend-key:handle-lid-switch")
    inhibit("sleep", "sleep")
    scan()
    GLib.timeout_add_seconds(3, scan)
    GLib.timeout_add(100, tick)
    GLib.io_add_watch(sys.stdin.fileno(), GLib.IO_IN | GLib.IO_HUP | GLib.IO_ERR, commands)
    for sig in (signal.SIGTERM, signal.SIGINT):
        GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, sig, lambda: loop.quit())
    emit("ready", canSleep=str(manager.CanSuspend()) in ("yes", "challenge"))
    try:
        loop.run()
    finally:
        panel(False)
        for key in list(leases):
            release(key)
        for fd in devices:
            os.close(fd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
