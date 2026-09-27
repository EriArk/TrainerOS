#!/usr/bin/python3 -I
"""Fixed-verb radio control. Installed root-owned; no user paths or commands."""
import json
import os
from pathlib import Path
import sys

VERBS = {"status", "wifi-on", "wifi-off", "bluetooth-on", "bluetooth-off", "airplane-on", "airplane-off"}
STATE = Path('/run/traineros-radio-restore.json')


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in VERBS:
        return 2
    verb = sys.argv[1]
    if verb != 'status' and os.geteuid() != 0:
        return 2
    import dbus
    bus = dbus.SystemBus()
    nm = dbus.Interface(bus.get_object('org.freedesktop.NetworkManager', '/org/freedesktop/NetworkManager'), 'org.freedesktop.DBus.Properties')
    interface = 'org.freedesktop.NetworkManager'
    objects = dbus.Interface(bus.get_object('org.bluez', '/'), 'org.freedesktop.DBus.ObjectManager').GetManagedObjects(timeout=3)
    adapters = [dbus.Interface(bus.get_object('org.bluez', path), 'org.freedesktop.DBus.Properties')
                for path, values in objects.items() if 'org.bluez.Adapter1' in values]

    def snapshot():
        wifi = bool(nm.Get(interface, 'WirelessEnabled', timeout=3))
        bluetooth = any(bool(p.Get('org.bluez.Adapter1', 'Powered', timeout=3)) for p in adapters) if adapters else None
        return {'wifi': int(wifi), 'bluetooth': int(bluetooth) if bluetooth is not None else -1,
                'airplane': int(not wifi and not bluetooth)}

    def wifi(value):
        nm.Set(interface, 'WirelessEnabled', dbus.Boolean(value), timeout=5)

    def bluetooth(value):
        if not adapters:
            raise RuntimeError('Bluetooth adapter unavailable')
        for adapter in adapters:
            adapter.Set('org.bluez.Adapter1', 'Powered', dbus.Boolean(value), timeout=5)

    if verb == 'airplane-on':
        before = snapshot()
        if not before['airplane']:
            fd = os.open(STATE, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, 'w') as output:
                json.dump(before, output)
        # Shut Wi-Fi down last so the Bluetooth operation can report failure.
        if adapters:
            bluetooth(False)
        wifi(False)
    elif verb == 'airplane-off':
        before = json.loads(STATE.read_text()) if STATE.exists() else {'wifi': 1, 'bluetooth': 1}
        wifi(before.get('wifi') == 1)
        if adapters:
            bluetooth(before.get('bluetooth') == 1)
        STATE.unlink(missing_ok=True)
    elif verb.startswith('wifi-'):
        wifi(verb.endswith('-on'))
    elif verb.startswith('bluetooth-'):
        bluetooth(verb.endswith('-on'))
    print(json.dumps(snapshot()))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception:
        # No D-Bus payloads, SSIDs or credentials in ordinary UI/log errors.
        print('Radio control unavailable', file=sys.stderr)
        sys.exit(1)
