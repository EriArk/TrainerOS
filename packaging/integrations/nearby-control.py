#!/usr/bin/python3 -I
"""Process-owned Wi-Fi Direct discovery/link. No saved Wi-Fi profiles are changed."""
import json
import os
import signal
import sys
import time
import uuid

MAGIC = b'\x02TOTrainerOS1'
NM = 'org.freedesktop.NetworkManager'
WPA = 'fi.w1.wpa_supplicant1'
PROPS = 'org.freedesktop.DBus.Properties'
P2P = WPA + '.Interface.P2PDevice'
RESPONSE_SECONDS = 50
NETWORK_SECONDS = 65


def identity(data):
    name = data.get('name', '')
    if not isinstance(name, str) or not name.strip() or len(name) > 32 or any(ord(c) < 32 for c in name):
        raise ValueError('Invalid Trainer name')
    return str(uuid.UUID(data['id'])), name


def decode_peer(props):
    for extension in props.get('VendorExtension', []):
        raw = bytes(extension)
        if raw.startswith(MAGIC) and len(raw) >= len(MAGIC) + 17:
            try:
                key = str(uuid.UUID(bytes=raw[len(MAGIC):len(MAGIC)+16]))
                name = raw[len(MAGIC)+16:].decode('utf8')
                identity({'id': key, 'name': name})
                return {'id': key, 'name': name}
            except (ValueError, UnicodeError):
                pass
    return None


def emit(event, **value):
    print(json.dumps({'event': event, **value}, ensure_ascii=True), flush=True)


class Nearby:
    def __init__(self):
        import dbus
        from dbus.mainloop.glib import DBusGMainLoop
        from gi.repository import GLib
        DBusGMainLoop(set_as_default=True)
        self.d, self.g = dbus, GLib
        self.bus = dbus.SystemBus()
        self.loop = GLib.MainLoop()
        self.key = self.name = ''
        self.visible = False
        self.path = self.device = self.active = self.profile = None
        self.old = {}; self.peers = {}; self.target = ''; self.deadline = 0
        self.declined = {}
        self.incoming = False; self.ready = False; self.last_scan = 0; self.matches = []
        self.buffer = b''
        self.network_started = False; self.retry_after = 0
        self.manager = self.iface(NM, '/org/freedesktop/NetworkManager', NM)
        GLib.io_add_watch(sys.stdin, GLib.IO_IN | GLib.IO_HUP, self.input)
        GLib.timeout_add_seconds(3, self.tick)
        for sig in (signal.SIGTERM, signal.SIGINT):
            GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, sig, self.stop)

    def iface(self, service, path, interface):
        return self.d.Interface(self.bus.get_object(service, path), interface)

    def props(self, service, path, interface):
        return self.iface(service, path, PROPS).GetAll(interface, timeout=4)

    def initialize(self):
        devices = self.manager.GetDevices(timeout=4)
        wifi = next(p for p in devices if int(self.props(NM, p, NM+'.Device')['DeviceType']) == 2)
        self.device = next(p for p in devices if int(self.props(NM, p, NM+'.Device')['DeviceType']) == 30)
        name = self.props(NM, wifi, NM+'.Device')['Interface']
        self.path = next(p for p in self.props(WPA, '/fi/w1/wpa_supplicant1', WPA)['Interfaces']
                         if self.props(WPA, p, WPA+'.Interface')['Ifname'] == name)
        self.api = self.iface(WPA, self.path, P2P)
        self.old = self.props(WPA, self.path, P2P)['P2PDeviceConfig']
        self.matches = [self.bus.add_signal_receiver(self.found, signal_name='DeviceFound', dbus_interface=P2P, path=self.path),
                        self.bus.add_signal_receiver(self.requested, signal_name='GONegotiationRequest', dbus_interface=P2P, path=self.path),
                        self.bus.add_signal_receiver(self.negotiated, signal_name='GONegotiationSuccess', dbus_interface=P2P, path=self.path),
                        self.bus.add_signal_receiver(self.negotiation_failed, signal_name='GONegotiationFailure', dbus_interface=P2P, path=self.path),
                        self.bus.add_signal_receiver(self.lost, signal_name='DeviceLost', dbus_interface=P2P, path=self.path)]
        self.advertise()

    def lost(self, path):
        # Discovery may lose a peer while its P2P group is being created.
        # Keep the accepted attempt's identity for the negotiation signals.
        self.peers = {k:r for k,r in self.peers.items() if k == self.target or r['path'] != str(path)}

    def advertise(self):
        d = self.d
        if self.visible:
            raw = MAGIC + uuid.UUID(self.key).bytes + self.name.encode('utf8')
            config = {'DeviceName': d.String(self.name.encode('utf8')[:32].decode('utf8', 'ignore')),
                      'VendorExtension': d.Array([d.ByteArray(raw)], signature='ay')}
        else:
            config = {k: self.old.get(k, d.String('') if k == 'DeviceName' else d.Array([], signature='ay'))
                      for k in ('DeviceName', 'VendorExtension')}
        self.iface(WPA, self.path, PROPS).Set(P2P, 'P2PDeviceConfig', d.Dictionary(config, signature='sv'), timeout=4)
        if not self.visible:
            self.api.StopFind(timeout=4)
            self.api.ExtendedListen(d.Dictionary({}, signature='sv'), timeout=4)
            self.peers.clear(); emit('peers', peers=[])
        self.last_scan = 0
        emit('searching', active=self.visible)

    def found(self, path):
        try:
            props = self.props(WPA, path, WPA+'.Peer'); row = decode_peer(props)
            if not row or row['id'] == self.key or not self.visible:
                return
            row.update(path=str(path), mac=':'.join('%02X' % x for x in props['DeviceAddress']), seen=time.monotonic())
            if len(self.peers) < 16 or row['id'] in self.peers:
                self.peers[row['id']] = row
        except self.d.DBusException:
            pass

    def requested(self, path, *_):
        self.found(path)
        row = next((r for r in self.peers.values() if r['path'] == str(path)), None)
        if not self.visible or not row or self.active or self.target or self.declined.get(row['id'],0)>time.monotonic():
            return
        self.target = row['id']; self.incoming = True; self.network_started = False
        self.deadline = time.monotonic()+RESPONSE_SECONDS
        emit('invite', peer=self.target, name=row['name'])

    def negotiated(self, properties):
        row = self.peers.get(self.target)
        if not self.active or not row or str(properties.get('peer_object', '')) != row['path']:
            return
        if not self.network_started:
            self.network_started = True; self.deadline = time.monotonic()+NETWORK_SECONDS
            emit('connecting', peer=self.target, phase='network')

    def negotiation_failed(self, properties):
        row = self.peers.get(self.target)
        if not row or str(properties.get('peer_object', '')) != row['path']:
            return
        self.release(); emit('closed', error='Could not connect. Try inviting your friend again.')

    def connect(self, key):
        if self.active or key not in self.peers:
            raise ValueError('This Trainer is no longer nearby.')
        self.target = key; self.ready = False; self.network_started = self.incoming
        self.deadline = time.monotonic()+(NETWORK_SECONDS if self.incoming else RESPONSE_SECONDS)
        d = self.d
        settings = d.Dictionary({'connection': {'id': 'TrainerOS nearby', 'uuid': str(uuid.uuid4()),
                                  'type': 'wifi-p2p', 'autoconnect': False},
                    'wifi-p2p': {'peer': self.peers[key]['mac'], 'wps-method': d.UInt32(4)},
                    'ipv4': {'method': 'auto', 'never-default': True}, 'ipv6': {'method': 'disabled'}}, signature='sa{sv}')
        self.profile, self.active, _ = self.manager.AddAndActivateConnection2(settings, self.device,
            d.ObjectPath('/'), d.Dictionary({'persist': 'volatile', 'bind-activation': 'dbus-client'}, signature='sv'), timeout=6)
        emit('connecting', peer=key, phase='network' if self.incoming else 'response')

    def release(self):
        if self.active:
            try:self.manager.DeactivateConnection(self.active, timeout=4)
            except self.d.DBusException:pass
        elif self.target and self.incoming and self.path:
            self.declined[self.target] = time.monotonic()+3
            try:self.api.Cancel(timeout=4)
            except self.d.DBusException:pass
        self.active = self.profile = None
        self.target = ''; self.ready = self.incoming = False; self.deadline = 0
        self.network_started = False
        self.last_scan = 0

    def tick(self):
        try:
            now = time.monotonic()
            if self.visible and not self.path and now >= self.retry_after:
                self.initialize()
            if self.active:
                state = self.props(NM, self.active, NM+'.Connection.Active')
                if int(state['State']) == 2 and not self.ready:
                    self.ready = True; self.deadline = 0
                    device = state['Devices'][0]
                    interface = str(self.props(NM, device, NM+'.Device')['IpInterface'])
                    emit('ready', peer=self.target, interface=interface, incoming=self.incoming)
                    emit('searching', active=False)
                elif int(state['State']) >= 3:
                    self.release(); emit('closed', error='The nearby connection ended. Invite your friend again.')
            if self.deadline and now > self.deadline:
                message = 'Could not finish connecting. Try again.' if self.network_started else 'Your friend did not answer. Try inviting them again.'
                self.release(); emit('closed', error=message)
            if self.visible and self.path and not self.active and not self.target and now-self.last_scan >= 18:
                self.api.Find(self.d.Dictionary({'Timeout': self.d.Int32(5)}, signature='sv'), timeout=4)
                self.api.ExtendedListen(self.d.Dictionary({'period': self.d.Int32(300), 'interval': self.d.Int32(2500)}, signature='sv'), timeout=4)
                self.last_scan = now
                emit('searching', active=True)
            self.peers = {k:r for k,r in self.peers.items() if k == self.target or now-r['seen'] < 45}
            self.declined = {k:deadline for k,deadline in self.declined.items() if deadline > now}
            emit('peers', peers=[{k:r[k] for k in ('id','name')} for r in self.peers.values()])
        except Exception:
            if self.active or self.target:
                self.release(); emit('closed', error='Nearby connection was interrupted.')
            # Radio-off and service restarts must not create a busy error popup loop.
            self.close()
            for match in self.matches:match.remove()
            self.matches = []; self.path = self.device = None
            self.retry_after = time.monotonic()+15
            self.peers.clear(); emit('peers', peers=[]); emit('unavailable')
        return True

    def input(self, _source, condition):
        chunk = os.read(sys.stdin.fileno(), 4096)
        if not chunk:return self.stop()
        self.buffer += chunk
        if len(self.buffer) > 8192:return self.stop()
        while b'\n' in self.buffer:
            line, self.buffer = self.buffer.split(b'\n', 1)
            if len(line)>4096:return self.stop()
            self.command(line)
        return True

    def command(self, line):
        starting = False; op = None
        try:
            v = json.loads(line); op = v.get('op')
            if op == 'configure':
                key, name = identity(v)
                changed = (self.key, self.name, self.visible) != (key, name, v.get('visible') is True)
                self.key, self.name, self.visible = key, name, v.get('visible') is True
                if self.path and changed:self.advertise()
                self.tick()
            elif op == 'invite' and self.visible and not self.target:
                peer = str(uuid.UUID(v['peer']))
                if peer not in self.peers:raise ValueError('This Trainer is no longer nearby.')
                self.incoming = False; starting = True; self.connect(peer)
            elif op == 'accept' and self.incoming and v.get('peer') == self.target:
                if self.active:return
                starting = True
                self.connect(self.target)
            elif op == 'disconnect':self.release()
            else:raise ValueError('Nearby request is unavailable.')
        except Exception as e:
            if starting:self.release()
            if op == 'configure' and self.active:
                # Discovery settings are not the established connection.
                emit('searching', active=False); return
            emit('error', error=str(e) if isinstance(e, ValueError) else 'Nearby connection could not start.')

    def stop(self, *_):
        self.loop.quit(); return False

    def close(self):
        self.release()
        if self.path:
            visible = self.visible
            try:self.visible = False; self.advertise()
            except Exception:pass
            finally:self.visible = visible


if __name__ == '__main__':
    if os.geteuid() != 0 or len(sys.argv) != 1:
        sys.exit(2)
    import fcntl
    lock = os.open('/run/traineros-nearby.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:sys.exit(3)
    # The shell owns this helper through stdin; its D-Bus lifetime owns the
    # volatile connection as a second cleanup path after a crash/kill.
    app = Nearby()
    try:app.loop.run()
    finally:app.close()
