"""Select only the current owned game's Gamescope and PulseAudio sources."""
import asyncio
import json
from pathlib import Path
from overlay_support import X11, identity


def namespace_pid(anchor, local_pid, proc=Path('/proc')):
    """Resolve one process inside the exact live Flatpak PID namespace."""
    try:
        start = (proc / str(anchor) / 'stat').read_text().rsplit(')', 1)[1].split()[19]
        namespace = (proc / str(anchor) / 'ns/pid').readlink()
        matches = []
        for entry in proc.glob('[0-9]*'):
            try:
                if (entry / 'ns/pid').readlink() != namespace:
                    continue
                ids = next(line.split()[1:] for line in (entry / 'status').read_text().splitlines()
                           if line.startswith('NSpid:'))
                if len(ids) > 1 and ids[-1] == str(local_pid) and descendant(int(entry.name), anchor, start, proc):
                    matches.append(int(entry.name))
            except (OSError, ValueError, IndexError, StopIteration):
                continue
        if (proc / str(anchor) / 'stat').read_text().rsplit(')', 1)[1].split()[19] != start:
            return 0
        return matches[0] if len(matches) == 1 else 0
    except (OSError, ValueError, IndexError):
        return 0


def audio_pid(properties, instances, resolve=namespace_pid):
    instance = str(properties.get('pipewire.access.portal.instance_id', ''))
    if instance:
        # Pulse's application.process.id is inside the Flatpak PID namespace.
        # Match the exact live instance, never merely the application name.
        pid, app = instances.get(instance, (0, ''))
        local_pid = str(properties.get('application.process.id', ''))
        return resolve(pid, int(local_pid)) if (pid > 1 and app
            and app == properties.get('pipewire.access.portal.app_id')
            and local_pid.isdigit() and int(local_pid) > 0) else 0
    if properties.get('pipewire.client.access') == 'flatpak':
        return 0
    pid = str(properties.get('application.process.id', ''))
    return int(pid) if pid.isdigit() else 0


def descendant(pid, root, start, proc=Path('/proc')):
    try:
        if (proc / str(root) / 'stat').read_text().rsplit(')', 1)[1].split()[19] != start:
            return False
        for _ in range(32):
            if pid == root:
                return True
            if pid <= 1:
                return False
            pid = int((proc / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()[1])
    except (OSError, ValueError, IndexError):
        pass
    return False


class GameCapture:
    def __init__(self, pid):
        self.pid = int(pid)
        if self.pid <= 1:
            raise ValueError('No active game')
        self.start = identity(self.pid)
        self.x = X11()
        entries = self.x.prop(self.x.root, 'GAMESCOPE_FOCUSABLE_WINDOWS')
        candidates = []
        for offset in range(0, len(entries) - 2, 3):
            window, appid, _ = entries[offset:offset + 3]
            if appid and self.x.owns(window, self.pid, self.start):
                candidates.append((window, appid))
        # An ambiguous selection must not silently share a different window.
        apps = {appid for _, appid in candidates}
        if len(apps) != 1:
            self.close()
            raise RuntimeError('Game capture unavailable')
        self.appid = apps.pop()
        self.windows = {window for window, _ in candidates}
        if any(entries[i + 1] == self.appid and not self.x.owns(entries[i], self.pid, self.start)
               for i in range(0, len(entries) - 2, 3)):
            self.close()
            raise RuntimeError('Ambiguous game capture')

    def alive(self):
        entries = self.x.prop(self.x.root, 'GAMESCOPE_FOCUSABLE_WINDOWS')
        if any(entries[i + 1] == self.appid and not self.x.owns(entries[i], self.pid, self.start)
               for i in range(0, len(entries) - 2, 3)):
            return False
        return any(entries[i] in self.windows and entries[i + 1] == self.appid
                   and self.x.owns(entries[i], self.pid, self.start)
                   for i in range(0, len(entries) - 2, 3))

    def close(self):
        import ctypes
        self.x.x.XCloseDisplay.argtypes = [ctypes.c_void_p]
        self.x.x.XCloseDisplay(self.x.display)

    async def audio_inputs(self):
        async def output(*args):
            p = await asyncio.create_subprocess_exec(*args,
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
            try:
                data, _ = await asyncio.wait_for(p.communicate(), 2)
                if p.returncode or len(data) > 1024 * 1024:
                    return b''
                return data
            except BaseException:
                if p.returncode is None:
                    p.kill()
                    await p.wait()
                raise
        async def query(kind):
            return json.loads(await output('/usr/bin/pactl', '-f', 'json', 'list', kind) or b'[]')
        inputs, sinks = await asyncio.gather(query('sink-inputs'), query('sinks'))
        instances = {}
        if any(s.get('properties', {}).get('pipewire.access.portal.instance_id') for s in inputs):
            try:
                rows = (await output('/usr/bin/flatpak', 'ps', '--columns=instance,child-pid,application')).decode().splitlines()
                for row in rows:
                    fields = row.split()
                    if len(fields) == 3 and fields[0].isdigit() and fields[1].isdigit():
                        instances[fields[0]] = (int(fields[1]), fields[2])
            except (OSError, ValueError):
                pass
        monitors = {int(s['index']): s.get('monitor_source_name', s.get('monitor_source')) for s in sinks}
        result = {}
        for stream in inputs:
            pid = audio_pid(stream.get('properties', {}), instances)
            monitor = monitors.get(int(stream['sink']))
            if pid > 1 and isinstance(monitor, str) and descendant(pid, self.pid, self.start):
                result[int(stream['index'])] = monitor
        return result
