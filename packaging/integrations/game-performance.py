#!/usr/bin/python3
"""Session-scoped host power lease. No root, sysfs writes or host config edits.

Armada Power1 and power-profiles-daemon are the authorities. stdin EOF (including
shell death) releases the lease. A private journal recovers interrupted writes.
"""
import json
import os
from pathlib import Path
import signal
import sys


class Host:
    def __init__(self):
        import gi
        gi.require_version('Gio', '2.0')
        from gi.repository import Gio, GLib
        self.Gio, self.GLib = Gio, GLib
        self.bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
        last = None
        for provider in [('org.armada.Power', '/org/armada/Power', 'org.armada.Power1'),
                         ('org.freedesktop.UPower.PowerProfiles', '/org/freedesktop/UPower/PowerProfiles', 'org.freedesktop.UPower.PowerProfiles'),
                         ('net.hadess.PowerProfiles', '/net/hadess/PowerProfiles', 'net.hadess.PowerProfiles')]:
            self.name, self.path, self.iface = provider
            try:
                self.read()
                return
            except Exception as error:
                last = error
        raise RuntimeError('No supported power service is available') from last

    def call(self, method, args):
        return self.bus.call_sync(self.name, self.path, 'org.freedesktop.DBus.Properties',
                                  method, args, None, self.Gio.DBusCallFlags.NONE, 4000, None).unpack()

    def read(self):
        return self.call('GetAll', self.GLib.Variant('(s)', (self.iface,)))[0]

    def write(self, key, value):
        self.call('Set', self.GLib.Variant('(ssv)', (self.iface, key,
                  self.GLib.Variant('u' if key == 'ManualGpuClock' else 's', value))))

    @property
    def profile_key(self):
        return 'Profile' if self.name == 'org.armada.Power' else 'ActiveProfile'


def capabilities(host):
    s = host.read()
    return dict(provider=host.name, profiles=s.get('AvailableProfiles', [p['Profile'] for p in s.get('Profiles', [])]),
                current=s.get(host.profile_key, ''), governors=s.get('AvailableCpuScalingGovernors', []),
                governor=s.get('CpuScalingGovernor', ''), gpuModes=s.get('AvailableGpuPerformanceLevels', []),
                gpuMin=s.get('ManualGpuClockMin', 0), gpuMax=s.get('ManualGpuClockMax', 0),
                gpuClock=s.get('ManualGpuClock', 0), gpuMode=s.get('GpuPerformanceLevel', ''),
                temperature=s.get('Temperature', 0), fanRpm=s.get('FanRpm', 0))


def plan(host, config):
    c = capabilities(host)
    result = []
    if config.get('power'):
        if config['power'] not in c['profiles']:
            raise ValueError('The saved power mode is unavailable on this host')
        result.append((host.profile_key, config['power']))
    if config.get('governor'):
        if config['governor'] not in c['governors']:
            raise ValueError('The saved CPU governor is unavailable')
        result.append(('CpuScalingGovernor', config['governor']))
    clock = config.get('gpu', 0)
    if type(clock) is not int or clock < 0:
        raise ValueError('Invalid GPU clock')
    if clock:
        if 'manual' not in c['gpuModes'] or not c['gpuMin'] <= clock <= c['gpuMax']:
            raise ValueError('The saved GPU clock is outside the host range')
        # Set clock while still in auto; switching to manual applies it.
        result += [('ManualGpuClock', clock), ('GpuPerformanceLevel', 'manual')]
    return result


class Lease:
    def __init__(self, host, journal):
        self.host, self.journal = host, journal
        self.steps = []
        self.observed = {}

    def save(self):
        temporary = self.journal.with_suffix('.new')
        flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC | getattr(os, 'O_NOFOLLOW', 0)
        fd = os.open(temporary, flags, 0o600)
        with os.fdopen(fd, 'w') as f:
            json.dump(dict(provider=self.host.name, steps=self.steps, observed=self.observed), f)
            f.flush()
            os.fsync(f.fileno())
        os.replace(temporary, self.journal)

    def recover(self):
        if not self.journal.exists():
            return
        data = json.loads(self.journal.read_text())
        if data['provider'] != self.host.name:
            raise RuntimeError('Restore the previous host power service before applying a game profile')
        self.steps = data['steps']
        self.observed = data.get('observed', {})
        self.release()

    def apply(self, config):
        operations = plan(self.host, config)  # validate ALL values before changing anything
        for key, value in operations:
            state = self.host.read()
            if state[key] == value:
                continue
            step = dict(key=key, before=state[key], after=value,
                        scope=state.get(self.host.profile_key))
            if key == self.host.profile_key:
                step['clock'] = state.get('ManualGpuClock')
            self.steps.append(step)
            self.save()  # intent before side effect, also recoverable after helper crash
            self.host.write(key, value)
            step['after'] = self.host.read()[key]  # host may round clocks
            step['completed'] = True
            state = self.host.read()
            self.observed = {k: state[k] for k in (self.host.profile_key, 'CpuScalingGovernor', 'ManualGpuClock', 'GpuPerformanceLevel') if k in state}
            self.save()

    def release(self):
        # Reverse order matters: restore target profile's governor before leaving
        # that profile. Do not overwrite a later manual change in another client.
        if self.observed and self.steps and all(s.get('completed') for s in self.steps):
            current = self.host.read()
            if any(current.get(k) != v for k, v in self.observed.items()):
                # Another client has taken over. Do not undo half of its choice
                # (e.g. keep its clock but accidentally restore GPU auto mode).
                self.steps.clear()
                self.observed = {}
                self.journal.unlink(missing_ok=True)
                return
        self.observed = {}  # rollback writes are themselves recoverable
        if self.steps:
            self.save()
        external = False
        while self.steps:
            step = self.steps[-1]
            current = self.host.read()
            scoped = step['key'] == self.host.profile_key or current.get(self.host.profile_key) == step.get('scope')
            if not scoped or current.get(step['key']) not in (step['before'], step['after']):
                external = True
            if scoped and current.get(step['key']) in (step['before'], step['after']) and not (external and step['key'] == self.host.profile_key):
                if current.get(step['key']) != step['before']:
                    self.host.write(step['key'], step['before'])
                if step['key'] == self.host.profile_key and step.get('clock') is not None:
                    self.host.write('ManualGpuClock', step['clock'])
                if self.host.read()[step['key']] != step['before']:
                    raise RuntimeError('Host did not restore ' + step['key'])
            self.steps.pop()
            self.save()
        self.journal.unlink(missing_ok=True)


def emit(value):
    print(json.dumps(value), flush=True)


def main():
    lease = None
    try:
        host = Host()
        probe = sys.argv[1:] == ['probe']
        if not probe and (len(sys.argv) != 3 or sys.argv[1] != 'lease'):
            raise ValueError('Invalid request')
        import fcntl
        root = Path.home() / '.local/state/traineros-power'
        root.mkdir(parents=True, exist_ok=True, mode=0o700)
        if root.is_symlink() or root.stat().st_uid != os.getuid():
            raise RuntimeError('Invalid power lease directory')
        fd = os.open(root / 'lock', os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        with os.fdopen(fd, 'w') as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                if probe:
                    emit(capabilities(host))
                    return
                raise RuntimeError('Another game already owns the power profile') from None
            lease = Lease(host, root / 'restore.json')
            lease.recover()
            if probe:
                emit(capabilities(host))
                return
            def stop(*_):
                raise InterruptedError('Power lease interrupted')
            signal.signal(signal.SIGTERM, stop)
            signal.signal(signal.SIGINT, stop)
            try:
                lease.apply(json.loads(sys.argv[2]))
                emit(dict(ready=True, **capabilities(host)))
                sys.stdin.buffer.read(1)  # close or EOF releases; no polling
            finally:
                lease.release()
            emit(dict(restored=True))
    except Exception as error:
        emit(dict(error=str(error)[:400]))
        sys.exit(1)


if __name__ == '__main__':
    main()
