#!/usr/bin/python3
"""Desktop applications in the existing Gamescope session. JSON-lines IPC.

GIO owns desktop-entry precedence, visibility, localization and Exec expansion.
Window control is restricted to observed local XRes clients; it never signals a
process or borrows the game's save/exit policy. Native Wayland is not managed.
"""
import argparse
import ctypes as c
import json
import os
from pathlib import Path
import select
import sys
import tempfile
import time
from overlay_support import X11, identity


def emit(event, **data):
    print(json.dumps(dict(event=event, **data), ensure_ascii=False), flush=True)


def catalogue(Gio):
    result = {}
    for app in Gio.AppInfo.get_all():
        if not hasattr(app, 'get_startup_wm_class') or not app.should_show():
            continue
        app_id = app.get_id()
        if not app_id or app.get_boolean('Terminal'):
            continue  # A terminal host is a separate integration, not a hidden failed launch.
        if app_id.startswith(('traineros', 'armada-return-', 'armada-installer')):
            continue
        icon = app.get_icon()
        icon_name = icon.get_names()[0] if isinstance(icon,Gio.ThemedIcon) and icon.get_names() else (
            icon.get_file().get_path() if isinstance(icon,Gio.FileIcon) else '')
        result[app_id] = (app, dict(id=app_id, name=app.get_display_name(),
            description=app.get_description() or '', icon=icon_name or '',
            startupClass=app.get_startup_wm_class() or ''))
    return result


class Windows:
    def __init__(self):
        self.x = X11()
        class Attributes(c.Structure):
            _fields_ = [('x',c.c_int),('y',c.c_int),('width',c.c_int),('height',c.c_int),
                ('border_width',c.c_int),('depth',c.c_int),('visual',c.c_void_p),('root',c.c_ulong),
                ('klass',c.c_int),('bit_gravity',c.c_int),('win_gravity',c.c_int),('backing_store',c.c_int),
                ('backing_planes',c.c_ulong),('backing_pixel',c.c_ulong),('save_under',c.c_int),
                ('colormap',c.c_ulong),('map_installed',c.c_int),('map_state',c.c_int),
                ('all_event_masks',c.c_long),('your_event_mask',c.c_long),('do_not_propagate_mask',c.c_long),
                ('override_redirect',c.c_int),('screen',c.c_void_p)]
        self.Attributes = Attributes
        self.x.x.XGetWindowAttributes.argtypes = [c.c_void_p,c.c_ulong,c.POINTER(Attributes)]
        for name in ('XMapWindow', 'XRaiseWindow'):
            getattr(self.x.x, name).argtypes = [c.c_void_p, c.c_ulong]

    def all(self):
        x = self.x
        root, parent, children, count = c.c_ulong(), c.c_ulong(), c.POINTER(c.c_ulong)(), c.c_uint()
        found = set(x.prop(x.root, '_NET_CLIENT_LIST'))
        if x.x.XQueryTree(x.display, x.root, c.byref(root), c.byref(parent), c.byref(children), c.byref(count)):
            try: found.update(children[:count.value])
            finally:
                if children: x.x.XFree(children)
        result = {}
        for window in found:
            try:
                pid = x.pid(window)
                if pid <= 1 or (Path('/proc') / str(pid)).stat().st_uid != os.getuid(): continue
                # Exclude supporting/input-only windows. Dialogs with no title are
                # subsequently associated through WM_TRANSIENT_FOR.
                title = x.prop(window, '_NET_WM_NAME') or x.prop(window, 'WM_NAME')
                classes = x.prop(window, 'WM_CLASS')
                if not isinstance(classes, bytes): classes = b''
                transient = next(iter(x.prop(window, 'WM_TRANSIENT_FOR')), 0)
                if not title and not transient: continue
                if not transient and x.atom('WM_DELETE_WINDOW') not in x.prop(window, 'WM_PROTOCOLS'): continue
                attributes = self.Attributes()
                if not x.x.XGetWindowAttributes(x.display,window,c.byref(attributes)): continue
                result[window] = dict(pid=pid, start=identity(pid),
                    classes=classes.decode(errors='replace').strip('\0').split('\0'),
                    transient=transient, mapped=attributes.map_state==2)
            except (OSError, ValueError): pass
        return result

    def valid(self, window, record):
        try:
            return self.x.pid(window) == record['pid'] and identity(record['pid']) == record['start']
        except OSError: return False

    def map(self, windows, visible):
        for window, record in windows.items():
            if self.valid(window, record):
                (self.x.x.XMapWindow if visible else self.x.x.XUnmapWindow)(self.x.display, window)
                if visible: self.x.x.XRaiseWindow(self.x.display, window)
        self.x.x.XFlush(self.x.display)

    def close(self, window, record):
        # Ordinary WM_DELETE only. In particular do not call the emulator helper's
        # close(), whose supported runtimes have special signal/confirmation rules.
        x = self.x
        if not self.valid(window, record) or x.atom('WM_DELETE_WINDOW') not in x.prop(window, 'WM_PROTOCOLS'):
            return False
        class Data(c.Union):
            _fields_ = [('l', c.c_long * 5), ('b', c.c_char * 20)]
        class Message(c.Structure):
            _fields_ = [('type', c.c_int), ('serial', c.c_ulong), ('send_event', c.c_int),
                        ('display', c.c_void_p), ('window', c.c_ulong), ('message_type', c.c_ulong),
                        ('format', c.c_int), ('data', Data)]
        class Event(c.Union):
            _fields_ = [('client', Message), ('pad', c.c_long * 24)]
        event = Event()
        event.client = Message(33, 0, True, x.display, window, x.atom('WM_PROTOCOLS'), 32, Data())
        event.client.data.l[0] = x.atom('WM_DELETE_WINDOW')
        x.x.XSendEvent.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_long, c.POINTER(Event)]
        ok = x.x.XSendEvent(x.display, window, False, 0, c.byref(event))
        x.x.XFlush(x.display)
        return bool(ok)


class Service:
    def __init__(self, Gio, GLib):
        self.Gio, self.GLib = Gio, GLib
        self.windows = Windows()
        self.apps = catalogue(Gio)
        self.sessions = {}
        self.journal = Path(os.environ['XDG_RUNTIME_DIR'])/'traineros-apps-windows.json'
        self.current = ''
        self.last = None
        self.pending = None
        self.lease = None
        self.buffer = b''
        self.heartbeat = time.monotonic()
        self.context = GLib.MainContext.default()
        self.monitor = Gio.AppInfoMonitor.get()
        self.monitor.connect('changed', self.catalogue_changed)
        self.restore_sessions()

    def restore_sessions(self):
        try:
            if self.journal.is_symlink() or self.journal.stat().st_uid != os.getuid() or self.journal.stat().st_size > 1024*1024: return
            saved = json.loads(self.journal.read_text())
            windows = self.windows.all()
            for app_id, session in saved.items():
                matched = {int(w):windows[int(w)] for w,r in session['windows'].items()
                           if int(w) in windows and windows[int(w)]['pid']==r['pid'] and windows[int(w)]['start']==r['start']}
                if matched:
                    self.sessions[app_id] = dict(windows=matched,owners={(r['pid'],r['start']) for r in matched.values()},row=session['row'])
                    self.windows.map(matched,False)
        except (OSError,ValueError,KeyError,TypeError): pass

    def save_sessions(self):
        saved = {app_id:dict(windows=session['windows'],row=session['row']) for app_id,session in self.sessions.items()}
        fd, temporary = tempfile.mkstemp(prefix='.traineros-apps-', dir=self.journal.parent)
        try:
            with os.fdopen(fd,'w') as stream:
                json.dump(saved,stream);stream.flush();os.fsync(stream.fileno())
            os.replace(temporary,self.journal)
        finally:
            if os.path.exists(temporary): os.unlink(temporary)

    def catalogue_changed(self, *_):
        self.apps = catalogue(self.Gio); self.publish()

    def publish(self):
        rows = {key:value[1] for key,value in self.apps.items()}
        for app_id,session in self.sessions.items(): rows.setdefault(app_id,session['row'])
        emit('catalogue', rows=list(rows.values()))

    def scan(self):
        windows = self.windows.all()
        for app_id, session in list(self.sessions.items()):
            session['windows'] = {w: r for w, r in session['windows'].items()
                                  if w in windows and windows[w]['pid'] == r['pid'] and windows[w]['start'] == r['start']}
            # Include new windows/dialogs from the exact same application process,
            # plus direct transients. PID reuse must never inherit window ownership.
            for w, r in windows.items():
                if r['mapped'] and ((r['pid'], r['start']) in session['owners'] or r['transient'] in session['windows']):
                    session['windows'][w] = r
                    if app_id != self.current: self.windows.map({w:r},False)
            if not session['windows']:
                del self.sessions[app_id]
                if self.current == app_id:
                    self.current = ''; self.release(); emit('shell')
        if self.pending:
            app_id, before, pids, deadline = self.pending
            app, row = self.apps[app_id]
            candidates = {}
            for w, r in windows.items():
                if w in before or not r['mapped']: continue
                owned = any(self.windows.x.owns(w, pid, start) for pid, start in pids)
                if owned or (row['startupClass'] and row['startupClass'] in r['classes']):
                    candidates[w] = r
            if candidates:
                owners = {(r['pid'], r['start']) for r in candidates.values()}
                self.sessions[app_id] = dict(windows=candidates, owners=owners, row=row)
                self.pending = None
                self.activate(app_id)
            elif time.monotonic() > deadline:
                self.pending = None
                self.release()
                emit('error', message='The application did not provide a manageable window. It may already be open in another session.')
        state = dict(current=self.current, running=list(self.sessions), launching=self.pending[0] if self.pending else '')
        if state != self.last:
            self.last = state; self.save_sessions(); emit('state', **state)

    def activate(self, app_id):
        if app_id not in self.sessions: return
        self.acquire()
        for other, session in self.sessions.items():
            if other != app_id: self.windows.map(session['windows'], False)
        self.windows.map(self.sessions[app_id]['windows'], True)
        self.current = app_id
        emit('foreground', id=app_id, pid=next(iter(self.sessions[app_id]['windows'].values()))['pid'])

    def acquire(self):
        if self.lease is None:
            from apps_input import Lease
            self.lease = Lease()

    def release(self):
        if self.lease:
            self.lease.close(); self.lease = None

    def command(self, message):
        op, app_id = message.get('command'), message.get('id', '')
        if op == 'ping':
            self.heartbeat = time.monotonic()
            if self.lease: self.lease.ping()
        elif op == 'refresh': self.apps = catalogue(self.Gio); self.publish()
        elif op in ('launch', 'open-file') and not self.pending:
            uris = []
            if op == 'open-file':
                from urllib.parse import urlsplit
                uri = message.get('url', '')
                parsed = urlsplit(uri)
                if parsed.scheme not in ('file', 'smb', 'sftp', 'ftp', 'dav', 'davs') or parsed.password:
                    raise ValueError('Unsupported file location.')
                content_type, uncertain = self.Gio.content_type_guess(parsed.path, None)
                # A desktop entry is a document here, never an instruction to execute.
                if content_type == 'application/x-desktop': content_type = 'text/plain'
                handler = self.Gio.AppInfo.get_default_for_type(content_type, parsed.scheme != 'file')
                if handler is None: raise ValueError('No installed application can open this file type.')
                app_id = handler.get_id()
                if app_id not in self.apps: self.apps = catalogue(self.Gio)
                uris = [uri]
            elif app_id in self.sessions: self.activate(app_id); return
            if app_id not in self.apps: raise ValueError('This application is no longer installed.')
            self.acquire()
            app = self.apps[app_id][0]
            pids = []
            before = self.windows.all()
            context = self.Gio.AppLaunchContext()
            # Use the session's Xwayland so the existing compositor/window path
            # can manage these apps. No inherited shell rendering overrides.
            context.setenv('QT_QPA_PLATFORM', 'xcb')
            context.setenv('GDK_BACKEND', 'x11')
            context.unsetenv('QT_QUICK_BACKEND'); context.unsetenv('QT_SCALE_FACTOR')
            def started(info, pid, data):
                try: pids.append((pid, identity(pid)))
                except OSError: pass
            try:
                app.launch_uris_as_manager(uris, context, self.GLib.SpawnFlags.SEARCH_PATH,
                                           None, None, started, None)
            except Exception:
                self.release(); raise
            self.pending = (app_id, before, pids, time.monotonic() + 20)
        elif op == 'controls' and self.current and self.lease:
            self.lease.open(); emit('controls')
        elif op == 'resume' and self.lease: self.lease.resume()
        elif op == 'minimize':
            if self.current in self.sessions: self.windows.map(self.sessions[self.current]['windows'], False)
            self.current = ''; self.release(); emit('shell')
        elif op == 'close' and app_id in self.sessions:
            self.activate(app_id)
            if self.lease: self.lease.resume()
            session = self.sessions[app_id]
            # Ask one top-level window; a save dialog must remain visible and
            # cancellation must leave the session running. No kill escalation.
            active = self.windows.x.active()
            top = [w for w, r in session['windows'].items() if not r['transient']]
            window = active if active in top else next((w for w in top if self.windows.x.atom('WM_DELETE_WINDOW') in self.windows.x.prop(w, 'WM_PROTOCOLS')), 0)
            if not window or not self.windows.close(window, session['windows'][window]):
                raise ValueError('Close this application using its own menu.')

    def run(self):
        self.publish(); emit('ready')
        next_scan = 0
        try:
            while time.monotonic() - self.heartbeat < 4:
                while self.context.pending(): self.context.iteration(False)
                if select.select([sys.stdin], [], [], .05)[0]:
                    data = os.read(sys.stdin.fileno(), 8192)
                    if not data: break
                    self.buffer += data
                    if len(self.buffer) > 65536: break
                    while b'\n' in self.buffer:
                        line, self.buffer = self.buffer.split(b'\n', 1)
                        try: self.command(json.loads(line))
                        except Exception as error: emit('error', message=str(error)[:240])
                if time.monotonic() >= next_scan:
                    self.scan(); next_scan = time.monotonic() + .2
                if self.lease: self.lease.poll(emit)
        finally:
            self.release()
            # A shell/helper crash must not leave live applications inaccessible.
            # Never terminate them: restore all tracked windows for recovery.
            for session in self.sessions.values(): self.windows.map(session['windows'], True)


def main():
    import gi
    gi.require_version('Gio', '2.0')
    from gi.repository import Gio, GLib
    parser = argparse.ArgumentParser()
    parser.add_argument('--catalogue', action='store_true')
    args = parser.parse_args()
    if args.catalogue:
        emit('catalogue', rows=[v[1] for v in catalogue(Gio).values()]); return
    Service(Gio, GLib).run()


if __name__ == '__main__':
    try: main()
    except Exception as error:
        emit('error', message='Applications are unavailable: ' + type(error).__name__)
        raise SystemExit(1)
