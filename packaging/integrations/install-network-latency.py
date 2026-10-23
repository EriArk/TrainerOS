#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install the NetworkManager Wi-Fi latency default without reconnecting.

Requires root. Existing per-connection overrides are preserved. The running
interface is changed only when its active profile uses NetworkManager defaults.
"""
import os
import configparser
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(*args):
    return subprocess.check_output(args, text=True, env={**os.environ, "LC_ALL": "C"}).strip()


def main():
    if os.geteuid() != 0:
        raise SystemExit("Root required")
    name = "99-zz-traineros-network-latency.conf"
    data = Path(__file__).with_name(name).read_bytes().replace(b"\r\n", b"\n")
    target = Path("/etc/NetworkManager/conf.d") / name
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_symlink():
        raise RuntimeError("Refusing a symlinked configuration")
    backup = Path("/var/lib/traineros/network-latency-v2-before")
    backup.mkdir(parents=True, exist_ok=True, mode=0o700)
    if not (backup / "recorded").exists():
        if target.exists():
            shutil.copy2(target, backup / name)
        (backup / "recorded").write_text("present\n" if target.exists() else "absent\n")
    fd, temporary = tempfile.mkstemp(dir=target.parent)
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)
        stream.flush()
        os.fchmod(stream.fileno(), 0o644)
        os.fsync(stream.fileno())
    os.replace(temporary, target)
    subprocess.run(["nmcli", "general", "reload", "conf"], check=True)
    effective = configparser.ConfigParser(interpolation=None, strict=False)
    effective.read_string(run("NetworkManager", "--print-config"))
    if effective.get("connection", "wifi.powersave", fallback="") != "2":
        raise RuntimeError("Another NetworkManager configuration overrides the latency default")
    # Do not bounce the connection, change its saved profile, or override an
    # explicit on/off/ignore preference. Reading one field avoids SSID parsing.
    for device in run("nmcli", "-g", "DEVICE", "device").splitlines():
        if not (Path("/sys/class/net") / device / "wireless").exists():
            continue
        uuid = run("nmcli", "-g", "GENERAL.CON-UUID", "device", "show", device)
        if not uuid or uuid == "--":
            continue
        setting = run("nmcli", "-g", "802-11-wireless.powersave", "connection", "show", "uuid", uuid)
        if setting.split()[0] in ("0", "default"):
            subprocess.run(["iw", "dev", device, "set", "power_save", "off"], check=True)
            print(device, run("iw", "dev", device, "get", "power_save"))
        else:
            print(device, "explicit profile preference preserved")
    print("Wi-Fi latency default installed; connections were not restarted")


if __name__ == "__main__":
    main()
