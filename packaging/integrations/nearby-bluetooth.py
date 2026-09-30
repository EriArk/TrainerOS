"""BlueZ RFCOMM transport for the existing, consent-gated TrainerOS Link protocol.

The localhost bridge never listens on a LAN address. Discovery belongs to this
D-Bus client; existing bonds, other profiles and radio power are left alone.
"""
import json
import os
import selectors
import signal
import socket
import sys
import threading
import time
import uuid

SERVICE = 'org.bluez'
ADAPTER = SERVICE + '.Adapter1'
DEVICE = SERVICE + '.Device1'
PROPS = 'org.freedesktop.DBus.Properties'
PROFILE = '91b83720-0abc-4a62-90ad-1bf7c3305d04'
PROFILE_PATH = '/org/traineros/nearby'
LINK_PORT = 47845
LIMIT = 65536


def emit(event, **fields):
    print(json.dumps({'event': event, **fields}, ensure_ascii=True), flush=True)


def diagnostic(stage, **fields):
    try:
        import syslog
        syslog.openlog('traineros-bluetooth', syslog.LOG_PID, syslog.LOG_DAEMON)
        record = {'stage': stage}
        if 'error_type' in fields: record['error_type'] = fields['error_type']
        syslog.syslog(syslog.LOG_INFO, json.dumps(record))
    except (ImportError, OSError):
        pass


def identity(value):
    name = value.get('name')
    if not isinstance(value.get('id'), str) or not isinstance(name, str) or not name.strip() or len(name) > 32 or any(ord(c) < 32 for c in name):
        raise ValueError('Invalid Trainer name')
    return str(uuid.UUID(value['id'])), name


def discovered(properties):
    if PROFILE not in [str(x).lower() for x in properties.get('UUIDs', [])]:
        return None
    address = str(properties.get('Address', ''))
    if len(address) != 17 or any(len(x) != 2 or any(c not in '0123456789abcdefABCDEF' for c in x)
                                for x in address.split(':')) or len(address.split(':')) != 6:
        return None
    key = str(uuid.uuid5(uuid.UUID(PROFILE), address.upper()))
    name = str(properties.get('Name', ''))
    try:
        identity({'id': key, 'name': name})
    except ValueError:
        return None
    return {'id': key, 'name': name, 'transport': 'bluetooth'}


class Session:
    """One bounded worker: control handshake, consent, then an opaque byte relay."""
    def __init__(self, radio, incoming, device, fd):
        self.radio, self.incoming, self.device = radio, incoming, device
        self.remote = socket.socket(fileno=fd)
        self.remote.settimeout(1)
        self.local = self.listener = None
        self.cancelled = threading.Event()
        self.decision = threading.Event()
        self.accepted = False
        self.peer = self.name = ''
        self.buffer = b''
        self.thread = threading.Thread(target=self.run, name='nearby-rfcomm', daemon=True)

    def stop(self):
        self.cancelled.set(); self.decision.set()
        for endpoint in (self.remote, self.local, self.listener):
            if endpoint:
                try: endpoint.shutdown(socket.SHUT_RDWR)
                except OSError: pass
                endpoint.close()

    def notify(self, event, **fields):
        self.radio.g.idle_add(self.radio.session_event, self, event, fields)

    def line(self, seconds):
        deadline = time.monotonic() + seconds
        while not self.cancelled.is_set() and time.monotonic() < deadline:
            if b'\n' in self.buffer:
                data, self.buffer = self.buffer.split(b'\n', 1)
                if len(data) > 1024: raise ValueError('Invalid nearby handshake')
                value = json.loads(data)
                if not isinstance(value, dict): raise ValueError('Invalid nearby handshake')
                return value
            try: data = self.remote.recv(4096)
            except socket.timeout: continue
            if not data: raise ConnectionError('Nearby connection closed')
            self.buffer += data
            if len(self.buffer) > LIMIT or (b'\n' not in self.buffer and len(self.buffer) > 1024):
                raise ValueError('Invalid nearby handshake')
        raise TimeoutError('Nearby request timed out')

    def write_control(self, value):
        self.remote.sendall(json.dumps(value, ensure_ascii=True).encode() + b'\n')

    def relay(self):
        remote, local = self.remote, self.local
        remote.setblocking(False); local.setblocking(False)
        queued = {remote: bytearray(), local: bytearray(self.buffer)}
        self.buffer = b''
        with selectors.DefaultSelector() as poll:
            for endpoint in (remote, local): poll.register(endpoint, selectors.EVENT_READ)
            while not self.cancelled.is_set():
                for endpoint in (remote, local):
                    other = local if endpoint is remote else remote
                    flags = (selectors.EVENT_READ if len(queued[other]) < LIMIT else 0)
                    if queued[endpoint]: flags |= selectors.EVENT_WRITE
                    if flags:
                        if endpoint in poll.get_map(): poll.modify(endpoint, flags)
                        else: poll.register(endpoint, flags)
                    elif endpoint in poll.get_map(): poll.unregister(endpoint)
                for key, flags in poll.select(1):
                    endpoint = key.fileobj
                    other = local if endpoint is remote else remote
                    if flags & selectors.EVENT_READ:
                        try: data = endpoint.recv(min(4096, LIMIT-len(queued[other])))
                        except BlockingIOError: continue
                        if not data: return
                        queued[other].extend(data)
                    if flags & selectors.EVENT_WRITE and queued[endpoint]:
                        try: count = endpoint.send(queued[endpoint])
                        except BlockingIOError: continue
                        if not count: return
                        del queued[endpoint][:count]

    def run(self):
        error = ''
        try:
            self.write_control({'btLink': 1, 'id': self.radio.key, 'name': self.radio.name})
            value = self.line(15)
            if type(value.get('btLink')) is not int or value['btLink'] != 1: raise ValueError('Incompatible nearby version')
            self.peer, self.name = identity(value)
            if self.peer == self.radio.key: raise ValueError('Invalid nearby identity')
            if self.incoming:
                self.notify('invite', peer=self.peer, name=self.name)
                if not self.decision.wait(45) or not self.accepted or self.cancelled.is_set():
                    self.write_control({'accepted': False}); return
                self.write_control({'accepted': True})
                self.local = socket.create_connection(('127.0.0.1', LINK_PORT), timeout=5)
                self.notify('ready', peer=self.peer, incoming=True, transport='bluetooth')
            else:
                if self.line(50).get('accepted') is not True:
                    error = 'Your friend declined the invitation.'; return
                self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.listener.bind(('127.0.0.1', 0)); self.listener.listen(1); self.listener.settimeout(1)
                self.notify('ready', peer=self.radio.target, identity=self.peer, name=self.name,
                            port=self.listener.getsockname()[1], incoming=False, transport='bluetooth')
                deadline = time.monotonic()+15
                while not self.cancelled.is_set() and time.monotonic() < deadline:
                    try: self.local, _ = self.listener.accept(); break
                    except socket.timeout: continue
                if not self.local: raise TimeoutError('Local Link did not connect')
                self.listener.close(); self.listener = None
            diagnostic('relay-ready')
            self.relay()
        except (OSError, ValueError, KeyError, BufferError):
            error = 'Bluetooth connection ended. Invite your friend again.'
        finally:
            self.stop(); self.notify('closed', error=error)


class NearbyBluetooth:
    def __init__(self):
        import dbus
        import dbus.service
        from dbus.mainloop.glib import DBusGMainLoop
        from gi.repository import GLib
        DBusGMainLoop(set_as_default=True)
        self.d, self.g = dbus, GLib
        self.bus = dbus.SystemBus()
        self.loop = GLib.MainLoop()
        self.key = self.name = ''; self.visible = False
        self.adapter = None; self.old_alias = self.owned_alias = None
        self.profile = None; self.scanning = False
        self.peers = {}; self.target = ''; self.target_device = ''; self.deadline = 0
        self.session = None; self.generation = 0; self.retry = 0; self.buffer = b''
        radio = self

        class Profile(dbus.service.Object):
            @dbus.service.method(SERVICE+'.Profile1', in_signature='', out_signature='')
            def Release(self): radio.release(); radio.profile = None

            @dbus.service.method(SERVICE+'.Profile1', in_signature='oha{sv}', out_signature='')
            def NewConnection(self, device, fd, _properties):
                handle = fd.take()
                if (radio.session or not radio.adapter or not str(device).startswith(radio.adapter+'/dev_')
                        or (radio.target_device and str(device) != radio.target_device) or not radio.visible):
                    os.close(handle); raise dbus.exceptions.DBusException('Busy', name=SERVICE+'.Error.Rejected')
                incoming = not radio.target_device
                radio.target_device = str(device)
                radio.session = Session(radio, incoming, str(device), handle)
                radio.deadline = 0
                radio.stop_discovery()
                radio.session.thread.start()

            @dbus.service.method(SERVICE+'.Profile1', in_signature='o', out_signature='')
            def RequestDisconnection(self, device):
                if radio.session and radio.session.device == str(device):
                    radio.session.stop()

        self.Profile = Profile
        self.bus.add_signal_receiver(self.added, signal_name='InterfacesAdded',
                                     dbus_interface='org.freedesktop.DBus.ObjectManager')
        self.bus.add_signal_receiver(self.changed, signal_name='PropertiesChanged',
                                     dbus_interface=PROPS, path_keyword='path')
        self.bus.add_signal_receiver(self.removed, signal_name='InterfacesRemoved',
                                     dbus_interface='org.freedesktop.DBus.ObjectManager')
        GLib.io_add_watch(sys.stdin, GLib.IO_IN | GLib.IO_HUP, self.input)
        GLib.timeout_add_seconds(3, self.tick)
        for sig in (signal.SIGTERM, signal.SIGINT): GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, sig, self.stop)

    def iface(self, path, interface):
        return self.d.Interface(self.bus.get_object(SERVICE, path), interface)

    def props(self, path, interface):
        return self.iface(path, PROPS).GetAll(interface, timeout=3)

    def initialize(self):
        objects = self.iface('/', 'org.freedesktop.DBus.ObjectManager').GetManagedObjects(timeout=3)
        self.adapter = next(str(p) for p, v in objects.items() if ADAPTER in v and v[ADAPTER].get('Powered'))
        self.old_alias = self.props(self.adapter, ADAPTER)['Alias']
        self.profile = self.Profile(self.bus, PROFILE_PATH)
        self.iface('/org/bluez', SERVICE+'.ProfileManager1').RegisterProfile(PROFILE_PATH, PROFILE,
            self.d.Dictionary({'Name': 'TrainerOS Link', 'Channel': self.d.UInt16(0),
                               'RequireAuthentication': self.d.Boolean(False),
                               'RequireAuthorization': self.d.Boolean(False), 'AutoConnect': self.d.Boolean(False)},
                              signature='sv'), timeout=3)
        self.advertise()
        address = self.props(self.adapter, ADAPTER)['Address']
        emit('identity', discovery=str(uuid.uuid5(uuid.UUID(PROFILE), str(address).upper())))
        for path, interfaces in objects.items(): self.added(path, interfaces)
        diagnostic('initialized')

    def advertise(self):
        if not self.adapter: return
        if self.visible:
            self.iface(self.adapter, PROPS).Set(ADAPTER, 'Alias', self.d.String(self.name), timeout=3)
            self.owned_alias = self.name
            if not self.session: self.start_discovery()
        else:
            self.stop_discovery(); self.restore_alias()
            self.peers.clear(); emit('peers', peers=[])

    def restore_alias(self):
        if self.owned_alias is not None and self.props(self.adapter, ADAPTER).get('Alias') == self.owned_alias:
            self.iface(self.adapter, PROPS).Set(ADAPTER, 'Alias', self.old_alias, timeout=3)
        self.owned_alias = None

    def start_discovery(self):
        if self.scanning or not self.visible or self.session: return
        api = self.iface(self.adapter, ADAPTER)
        api.SetDiscoveryFilter(self.d.Dictionary({'Transport': 'bredr', 'UUIDs': self.d.Array([PROFILE], signature='s'),
                               'RSSI': self.d.Int16(-127),
                               'Discoverable': self.d.Boolean(True)}, signature='sv'), timeout=3)
        api.StartDiscovery(timeout=3); self.scanning = True
        emit('searching', active=True)

    def stop_discovery(self):
        if self.scanning:
            try: self.iface(self.adapter, ADAPTER).StopDiscovery(timeout=3)
            except self.d.DBusException: pass
            self.scanning = False
        emit('searching', active=False)

    def added(self, path, interfaces):
        if not self.visible or not self.adapter or not str(path).startswith(self.adapter+'/dev_') or DEVICE not in interfaces: return
        row = discovered(interfaces[DEVICE])
        if row and (len(self.peers) < 16 or row['id'] in self.peers):
            row.update(path=str(path), seen=time.monotonic()); self.peers[row['id']] = row

    def changed(self, interface, values, _invalidated, path):
        if interface == DEVICE and any(k in values for k in ('RSSI', 'UUIDs', 'Name')):
            try: self.added(path, {DEVICE: self.props(path, DEVICE)})
            except self.d.DBusException: pass

    def removed(self, path, interfaces):
        if DEVICE in interfaces: self.peers = {k: v for k, v in self.peers.items() if v['path'] != str(path)}

    def session_event(self, session, event, fields):
        if self.session is not session: return False
        if event == 'invite': self.target = fields['peer']
        if event == 'closed': self.release()
        diagnostic(event); emit(event, **fields)
        return False

    def release(self):
        self.generation += 1
        session, self.session = self.session, None
        if session: session.stop()
        device, self.target_device = self.target_device, ''
        self.target = ''; self.deadline = 0
        if device:
            try: self.iface(device, DEVICE).DisconnectProfile(PROFILE, reply_handler=lambda: None, error_handler=lambda _: None)
            except self.d.DBusException: pass

    def command(self, line):
        try:
            value = json.loads(line); op = value.get('op')
            if op == 'configure':
                key, name = identity(value)
                if self.key and key != self.key: self.release()
                self.key, self.name, self.visible = key, name, value.get('visible') is True
                if self.adapter: self.advertise()
                self.tick()
            elif op == 'invite' and self.visible and not self.target_device:
                key = str(uuid.UUID(value['peer'])); row = self.peers.get(key)
                if not row: raise ValueError('This Trainer is no longer nearby.')
                self.target, self.target_device = key, row['path']; self.deadline = time.monotonic()+25
                self.generation += 1; generation = self.generation
                def failure(_error):
                    if generation == self.generation and not self.session:
                        diagnostic('connect-failed', error_type=_error.get_dbus_name())
                        self.release(); emit('closed', error='Could not connect over Bluetooth. Try again.')
                self.iface(row['path'], DEVICE).ConnectProfile(PROFILE, reply_handler=lambda: None, error_handler=failure, timeout=25)
                diagnostic('connection-started')
            elif op == 'accept' and self.session and self.session.incoming and value.get('peer') == self.target:
                diagnostic('accepted')
                self.session.accepted = True; self.session.decision.set()
            elif op == 'disconnect': self.release()
            else: raise ValueError('Nearby request is unavailable.')
        except (ValueError, KeyError, TypeError, self.d.DBusException):
            emit('error', error='Bluetooth connection could not start.')

    def tick(self):
        try:
            now = time.monotonic()
            if self.visible and not self.adapter and now >= self.retry: self.initialize()
            if self.adapter and not self.props(self.adapter, ADAPTER).get('Powered'):
                raise ValueError('Bluetooth is off')
            if self.deadline and now > self.deadline:
                self.release(); emit('closed', error='Could not connect over Bluetooth. Try again.')
            if self.adapter and not self.target_device: self.start_discovery()
            # BR/EDR inquiry updates can be sparse even for a stationary peer.
            # Keep its bounded discovery row through several inquiry cycles;
            # ConnectProfile still verifies that it is actually reachable.
            self.peers = {k: v for k, v in self.peers.items() if k == self.target or now-v['seen'] < 180}
            emit('peers', peers=[{k: v[k] for k in ('id', 'name', 'transport')} for v in self.peers.values()])
        except (ValueError, StopIteration, self.d.DBusException) as error:
            self.close(); self.retry = time.monotonic()+15
            emit('identity', discovery='')
            emit('peers', peers=[]); emit('unavailable')
            diagnostic('unavailable', error_type=error.get_dbus_name() if isinstance(error, self.d.DBusException) else type(error).__name__)
        return True

    def input(self, _source, _condition):
        data = os.read(sys.stdin.fileno(), 4096)
        if not data: return self.stop()
        self.buffer += data
        if len(self.buffer) > 8192: return self.stop()
        while b'\n' in self.buffer:
            line, self.buffer = self.buffer.split(b'\n', 1)
            if len(line) > 4096: return self.stop()
            self.command(line)
        return True

    def stop(self, *_): self.loop.quit(); return False

    def close(self):
        self.release()
        if self.adapter:
            try: self.stop_discovery(); self.restore_alias()
            except self.d.DBusException: pass
        if self.profile:
            try: self.iface('/org/bluez', SERVICE+'.ProfileManager1').UnregisterProfile(PROFILE_PATH, timeout=3)
            except self.d.DBusException: pass
            self.profile.remove_from_connection(); self.profile = None
        self.adapter = None; self.peers.clear()
