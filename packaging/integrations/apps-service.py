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
from keyboard_input import KeyboardWriter


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


def browser_identity(app_id, record):
    desktop_id=app_id.removesuffix('.desktop')
    if desktop_id.casefold() in [value.casefold() for value in record['classes']]:return True
    # Flatpak Firefox may expose Navigator/firefox instead of its desktop ID.
    # Its sandbox records the actual application ID independently of window titles.
    try:
        import configparser
        info=configparser.ConfigParser()
        info.read_string((Path('/proc')/str(record['pid'])/'root/.flatpak-info').read_text()[:16384])
        return info.get('Application','name',fallback='')==desktop_id
    except (OSError,ValueError,configparser.Error):return False


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
                root=session.get('root')
                if root:
                    try:
                        if identity(root[0])!=root[1]:root=None
                    except OSError:root=None
                matched = {int(w):windows[int(w)] for w,r in session['windows'].items()
                           if int(w) in windows and windows[int(w)]['pid']==r['pid'] and windows[int(w)]['start']==r['start']}
                if matched or root:
                    self.sessions[app_id] = dict(windows=matched,owners={(r['pid'],r['start']) for r in matched.values()},row=session['row'])
                    if root:self.sessions[app_id]['root']=root
                    self.windows.map(matched,False)
        except (OSError,ValueError,KeyError,TypeError): pass

    def save_sessions(self):
        saved = {app_id:dict(windows=session['windows'],row=session['row'],root=session.get('root')) for app_id,session in self.sessions.items()}
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
        # Unmanaged desktop windows cannot become an owned app by discovery.
        # Launch captures its own before-set; restore_sessions scans at startup.
        windows = self.windows.all() if self.sessions or self.pending else {}
        for app_id, session in list(self.sessions.items()):
            root = session.get('root')
            root_alive = False
            if root:
                try: root_alive = identity(root[0]) == root[1]
                except OSError: pass
            session['windows'] = {w: r for w, r in session['windows'].items()
                                  if w in windows and windows[w]['pid'] == r['pid'] and windows[w]['start'] == r['start']}
            # Include new windows/dialogs from the exact same application process,
            # plus direct transients. PID reuse must never inherit window ownership.
            for w, r in windows.items():
                owned_child = root_alive and self.windows.x.owns(w, *root)
                if r['mapped'] and (owned_child or (r['pid'], r['start']) in session['owners'] or r['transient'] in session['windows']):
                    session['windows'][w] = r
                    if app_id != self.current: self.windows.map({w:r},False)
            if not session['windows'] and not root_alive:
                del self.sessions[app_id]
                if self.current == app_id:
                    self.current = ''; self.release(); emit('shell')
        if self.pending:
            app_id, before, pids, deadline, reuse_browser = self.pending
            app, row = self.apps[app_id]
            candidates = {}
            for w, r in windows.items():
                if not r['mapped']: continue
                owned = any(self.windows.x.owns(w, pid, start) for pid, start in pids)
                matches = (row['startupClass'] and row['startupClass'] in r['classes']) or (reuse_browser and browser_identity(app_id,r))
                # An explicit URL launch may reuse the default browser's window.
                # Keep its observed process identity; never adopt it on idle scans.
                reused = reuse_browser and matches and w in before and before[w]['pid']==r['pid'] and before[w]['start']==r['start']
                if (w not in before and (owned or matches)) or reused:
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
        installer=self.sessions[app_id].get('root')
        if self.lease and getattr(self.lease,'installer',None)!=installer:self.release()
        self.acquire(installer)
        for other, session in self.sessions.items():
            if other != app_id: self.windows.map(session['windows'], False)
        self.windows.map(self.sessions[app_id]['windows'], True)
        self.current = app_id
        session = self.sessions[app_id]
        pid = session['root'][0] if session.get('root') else next(iter(session['windows'].values()))['pid']
        emit('foreground', id=app_id, pid=pid)

    def acquire(self, installer=None):
        if self.lease is None:
            from apps_input import Lease
            self.lease = Lease(installer)

    def release(self):
        self.close_keyboard_output()
        if self.lease:
            self.lease.close(); self.lease = None

    def close_keyboard_output(self):
        output=getattr(self,'keyboard_output',None)
        if output:output.close();self.keyboard_output=None

    def keyboard_focus(self):
        session=self.sessions.get(self.current)
        if not session:return False
        active=self.windows.x.active()
        record=session['windows'].get(active)
        return bool(record and self.windows.valid(active,record))

    def command(self, message):
        op, app_id = message.get('command'), message.get('id', '')
        if op == 'keyboard-close':
            self.close_keyboard_output()
            if self.lease and self.lease.keyboard:self.lease.resume()
            return
        if op == 'keyboard-input':
            if self.lease and self.lease.keyboard and self.keyboard_focus():
                record=self.sessions[self.current]['windows'][self.windows.x.active()]
                root=(record['pid'],record['start'])
                if getattr(self,'keyboard_output',None) and self.keyboard_output.root!=root:self.close_keyboard_output()
                if not getattr(self,'keyboard_output',None):self.keyboard_output=KeyboardWriter(root)
                self.keyboard_output.apply(message)
            return
        if op == 'watch-installer':
            # The shell owns ProcessService and the subreaper. This service owns
            # only windows/input, and never executes a supplied command.
            pid = int(message['pid'])
            proc = Path('/proc') / str(pid)
            if pid <= 1 or proc.stat().st_uid != os.getuid():
                raise ValueError('Installer process is unavailable.')
            command = (proc/'cmdline').read_bytes().split(b'\0')
            if b'/var/opt/traineros/pc/launch.py' not in command:
                raise ValueError('This is not a managed PC installer.')
            app_id = 'traineros-pc-installer'
            row = dict(id=app_id, name=message.get('title', 'Game installer')[:96],
                       description='Game installation', icon='package-x-generic', startupClass='')
            self.sessions[app_id] = dict(windows={}, owners=set(), row=row, root=(pid,identity(pid)))
            self.publish(); self.activate(app_id)
        elif op == 'resume-installer': self.activate('traineros-pc-installer')
        elif op == 'finish-installer':
            session = self.sessions.pop('traineros-pc-installer', None)
            if session:
                if self.current == 'traineros-pc-installer':
                    self.current = ''; self.release(); emit('shell')
                self.publish()
        elif op == 'ping':
            self.heartbeat = time.monotonic()
            if self.lease: self.lease.ping()
        elif op == 'refresh': self.apps = catalogue(self.Gio); self.publish()
        elif op in ('launch', 'open-file', 'open-url') and not self.pending:
            uris = []
            if op in ('open-file', 'open-url'):
                from urllib.parse import urlsplit
                uri = message.get('url', '')
                parsed = urlsplit(uri)
                if op == 'open-url':
                    if parsed.scheme != 'https' or not parsed.hostname or parsed.username is not None or parsed.password is not None:
                        raise ValueError('Unsupported browser address.')
                    handler = self.Gio.AppInfo.get_default_for_uri_scheme('https')
                else:
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
            if op == 'open-url' and app_id in self.sessions and self.sessions[app_id]['windows']:
                self.activate(app_id)
            else:
                self.pending = (app_id, before, pids, time.monotonic() + 20, op == 'open-url')
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
                if self.lease:
                    self.lease.poll(emit)
                    if self.lease.keyboard and not self.keyboard_focus():
                        self.lease.resume();emit('keyboard-closed')
                    if not self.lease.keyboard:self.close_keyboard_output()
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
