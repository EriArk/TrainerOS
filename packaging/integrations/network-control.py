#!/usr/bin/python3 -I
"""Bounded network operations. JSON stdin, redacted errors, no shell execution."""
import json
import os
import re
import select
import sys
import time
import uuid

NM = 'org.freedesktop.NetworkManager'
BLUEZ = 'org.bluez'
PROPS = 'org.freedesktop.DBus.Properties'
OPS = {'list', 'scan', 'connect', 'disconnect', 'forget', 'pair'}


def read_message():
    raw = sys.stdin.buffer.readline(8193)
    if not raw or len(raw) > 8192 or not raw.endswith(b'\n'):
        raise ValueError('Invalid request')
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError('Invalid request')
    return value


def emit(event, **value):
    print(json.dumps({'event': event, **value}, ensure_ascii=True), flush=True)


def validate(value):
    if set(value) - {'op', 'kind', 'id', 'password'} or value.get('op') not in OPS or value.get('kind') not in ('wifi', 'bluetooth'):
        raise ValueError('Invalid request')
    if not isinstance(value.get('id', ''), str) or len(value.get('id', '')) > 256:
        raise ValueError('Invalid target')
    password = value.get('password', '')
    if not isinstance(password, str) or len(password.encode('utf8')) > 64 or '\0' in password:
        raise ValueError('Invalid password')
    if value['kind'] == 'wifi' and value['op'] == 'pair':
        raise ValueError('Invalid operation')
    return value


class Cancelled(Exception):
    pass


class UserError(Exception):
    pass


def check_cancel():
    if select.select([sys.stdin.buffer], [], [], 0)[0]:
        read_message()  # EOF, malformed input and cancellation all abort the operation.
        raise Cancelled()


def wifi_security(ap):
    flags = int(ap.get('WpaFlags', 0)) | int(ap.get('RsnFlags', 0))
    if flags & 0x200:  # 802.1X; existing configured profiles remain usable.
        return 'unsupported'
    if flags & 0x100:
        return 'wpa-psk'
    if flags & 0x400:
        return 'sae'
    return 'unsupported' if int(ap.get('Flags', 0)) & 1 else 'open'


class Platform:
    def __init__(self):
        import dbus
        self.d = dbus
        self.bus = dbus.SystemBus()

    def iface(self, service, path, interface):
        return self.d.Interface(self.bus.get_object(service, path), interface)

    def props(self, service, path, interface):
        return self.iface(service, path, PROPS).GetAll(interface, timeout=4)

    def wifi(self):
        manager = self.iface(NM, '/org/freedesktop/NetworkManager', NM)
        devices = [str(p) for p in manager.GetDevices(timeout=4)
                   if int(self.props(NM, p, NM+'.Device')['DeviceType']) == 2]
        if not devices:
            raise UserError('Wi-Fi is unavailable on this device.')
        device = devices[0]
        wireless = self.props(NM, device, NM+'.Device.Wireless')
        active_ap = str(wireless.get('ActiveAccessPoint', '/'))
        devprops = self.props(NM, device, NM+'.Device')
        active_path = str(devprops.get('ActiveConnection', '/'))
        active_profile = str(self.props(NM, active_path, NM+'.Connection.Active').get('Connection', '/')) if active_path != '/' else '/'
        profiles = []
        settings = self.iface(NM, '/org/freedesktop/NetworkManager/Settings', NM+'.Settings')
        for path in settings.ListConnections(timeout=4):
            data = self.iface(NM, path, NM+'.Settings.Connection').GetSettings(timeout=4)
            if data.get('connection', {}).get('type') != '802-11-wireless':
                continue
            ssid = bytes(data.get('802-11-wireless', {}).get('ssid', []))
            security = str(data.get('802-11-wireless-security', {}).get('key-mgmt', 'open'))
            profiles.append({'id': str(data['connection']['uuid']), 'path': str(path), 'ssid': ssid,
                             'security': security, 'title': ssid.decode('utf8', 'replace') or str(data['connection']['id'])})
        found = {}
        for path in self.iface(NM, device, NM+'.Device.Wireless').GetAllAccessPoints(timeout=4):
            try:
                ap = self.props(NM, path, NM+'.AccessPoint')
            except self.d.DBusException:
                continue  # AP disappeared during scan.
            ssid = bytes(ap['Ssid'])
            if not ssid:
                continue
            security = wifi_security(ap)
            saved = next((p for p in profiles if p['ssid'] == ssid and
                          (p['security'] == security or security == 'unsupported' and p['security'] in ('wpa-eap', 'ieee8021x'))), None)
            key = (ssid, security)
            row = {'id': 'saved:'+saved['id'] if saved else 'ap:'+str(path), 'title': ssid.decode('utf8', 'replace'),
                   'saved': bool(saved), 'connected': str(path) == active_ap and active_profile != '/',
                   'strength': int(ap['Strength']), 'security': security, 'available': True,
                   '_ap': str(path), '_ssid': ssid, '_profile': saved['path'] if saved else None}
            previous = found.get(key)
            if not previous or row['connected'] or not previous['connected'] and row['strength'] > previous['strength']:
                found[key] = row
        rows = list(found.values())
        for saved in profiles:
            if not any(r['id'] == 'saved:'+saved['id'] for r in rows):
                rows.append({'id': 'saved:'+saved['id'], 'title': saved['title'], 'saved': True,
                             'connected': saved['path'] == active_profile, 'strength': 0, 'security': saved['security'],
                             'available': False, '_ap': '/', '_ssid': saved['ssid'], '_profile': saved['path']})
        rows.sort(key=lambda r: (not r['connected'], not r['available'], not r['saved'], -r['strength'], r['title'].casefold()))
        for row in rows:
            row['detail'] = 'Connected' if row['connected'] else 'Saved · out of range' if not row['available'] else 'Saved' if row['saved'] else 'Open network' if row['security'] == 'open' else 'Secured network'
            if row['available']:
                row['detail'] += ' · '+str(row['strength'])+'%'
        return manager, device, active_path, rows

    def bluetooth(self):
        objects = self.iface(BLUEZ, '/', 'org.freedesktop.DBus.ObjectManager').GetManagedObjects(timeout=4)
        adapters = [str(p) for p, v in objects.items() if BLUEZ+'.Adapter1' in v]
        if not adapters:
            raise UserError('Bluetooth is unavailable on this device.')
        rows = []
        for path, interfaces in objects.items():
            if BLUEZ+'.Device1' not in interfaces:
                continue
            data = interfaces[BLUEZ+'.Device1']
            if str(data.get('Adapter')) != adapters[0]:
                continue
            paired = bool(data.get('Paired', False)); connected = bool(data.get('Connected', False))
            rows.append({'id': str(path), 'title': str(data.get('Alias') or data.get('Name') or data.get('Address')),
                         'saved': paired, 'connected': connected,
                         'detail': 'Connected' if connected else 'Paired' if paired else 'Nearby', 'available': True})
        rows.sort(key=lambda r: (not r['connected'], not r['saved'], r['title'].casefold()))
        return adapters[0], rows

    def snapshot(self, kind):
        rows = self.wifi()[3] if kind == 'wifi' else self.bluetooth()[1]
        emit('snapshot', rows=[{k: v for k, v in row.items() if not k.startswith('_')} for row in rows[:256]])

    def wifi_action(self, request):
        manager, device, active, rows = self.wifi()
        operation = request['op']
        if operation == 'list':
            return
        if operation == 'scan':
            self.iface(NM, device, NM+'.Device.Wireless').RequestScan(self.d.Dictionary({}, signature='sv'), timeout=5)
            for _ in range(20):
                check_cancel(); time.sleep(.2)
            return
        row = next((r for r in rows if r['id'] == request.get('id')), None)
        if not row:
            raise UserError('This network changed. Refresh and choose it again.')
        if operation == 'forget':
            if not row['_profile']:
                raise UserError('This network is not saved.')
            self.iface(NM, row['_profile'], NM+'.Settings.Connection').Delete(timeout=5)
            return
        if operation == 'disconnect':
            if not row['connected']:
                raise UserError('This network is no longer connected.')
            self.iface(NM, device, NM+'.Device').Disconnect(timeout=5)
            return
        if row['connected']:
            return
        if not row['available']:
            raise UserError('This network is out of range. Refresh and try again.')
        password = request.pop('password', '')
        if not row['_profile'] and row['security'] == 'unsupported':
            raise UserError('Set up this network in Desktop Mode first.')
        if not row['_profile'] and row['security'] != 'open':
            if not (8 <= len(password.encode('utf8')) <= 63 or row['security'] == 'wpa-psk' and re.fullmatch('[0-9a-fA-F]{64}', password)):
                raise UserError('Check the Wi-Fi password and try again.')
        # NM restores the previous connection even if this helper/application dies.
        checkpoint = manager.CheckpointCreate(self.d.Array([self.d.ObjectPath(device)], signature='o'), self.d.UInt32(90), self.d.UInt32(2), timeout=5)
        created = None
        try:
            if row['_profile']:
                activation = manager.ActivateConnection(row['_profile'], device, row['_ap'], timeout=10)
            else:
                config = {'connection': {'id': row['title'], 'uuid': str(uuid.uuid4()), 'type': '802-11-wireless', 'autoconnect': self.d.Boolean(True)},
                          '802-11-wireless': {'ssid': self.d.ByteArray(row['_ssid']), 'mode': 'infrastructure'},
                          'ipv4': {'method': 'auto'}, 'ipv6': {'method': 'auto'}}
                if row['security'] != 'open':
                    config['802-11-wireless-security'] = {'key-mgmt': row['security'], 'psk': password}
                config = self.d.Dictionary({k: self.d.Dictionary(v, signature='sv') for k, v in config.items()}, signature='sa{sv}')
                created, activation, _ = manager.AddAndActivateConnection2(config, device, row['_ap'], self.d.Dictionary({'persist': 'memory'}, signature='sv'), timeout=10)
                password = ''; config = None
            deadline = time.monotonic()+65
            while time.monotonic() < deadline:
                check_cancel()
                state = int(self.props(NM, activation, NM+'.Connection.Active')['State'])
                if state == 2:
                    if created:
                        self.iface(NM, created, NM+'.Settings.Connection').Save(timeout=5)
                    manager.CheckpointDestroy(checkpoint, timeout=5)
                    checkpoint = None
                    return
                if state in (3, 4):
                    break
                time.sleep(.25)
            raise UserError('Could not connect. Check the password or signal and try again.')
        finally:
            if checkpoint:
                try:
                    manager.CheckpointRollback(checkpoint, timeout=5)
                finally:
                    try: manager.CheckpointDestroy(checkpoint, timeout=5)
                    except self.d.DBusException: pass
                if created:
                    try: self.iface(NM, created, NM+'.Settings.Connection').Delete(timeout=5)
                    except self.d.DBusException: pass

    def bluetooth_action(self, request):
        adapter, rows = self.bluetooth(); operation = request['op']
        if operation == 'list':
            return
        controller = self.iface(BLUEZ, adapter, BLUEZ+'.Adapter1')
        if operation == 'scan':
            controller.StartDiscovery(timeout=5)
            try:
                for _ in range(40):
                    check_cancel(); time.sleep(.2)
            finally:
                controller.StopDiscovery(timeout=5)
            return
        row = next((r for r in rows if r['id'] == request.get('id')), None)
        if not row:
            raise UserError('This device is no longer nearby. Search again.')
        device = self.iface(BLUEZ, row['id'], BLUEZ+'.Device1')
        if operation == 'forget':
            controller.RemoveDevice(self.d.ObjectPath(row['id']), timeout=5)
        elif operation == 'disconnect':
            device.Disconnect(timeout=8)
        else:
            pair_device(self, row, operation == 'pair' and not row['saved'])


def pair_device(platform, row, pair):
    import dbus.service
    from gi.repository import GLib
    target = row['id']; loop = GLib.MainLoop(); pending = {}; result = {'error': None}
    device = platform.iface(BLUEZ, target, BLUEZ+'.Device1')

    class Rejected(dbus.DBusException):
        _dbus_error_name = 'org.bluez.Error.Rejected'

    def failed(error):
        result['error'] = UserError('Could not pair or connect. Keep the device nearby and try again.')
        loop.quit()

    def connected():
        loop.quit()

    def paired():
        # Only the device explicitly selected and paired here becomes trusted.
        try:
            platform.iface(BLUEZ, target, PROPS).Set(BLUEZ+'.Device1', 'Trusted', dbus.Boolean(True), timeout=4)
            device.Connect(reply_handler=connected, error_handler=failed, timeout=25)
        except dbus.DBusException as error: failed(error)

    class Agent(dbus.service.Object):
        def ask(self, device_path, kind, reply, error, text=''):
            if str(device_path) != target or pending:
                error(Rejected()); return
            pending.update(kind=kind, reply=reply, error=error)
            emit('prompt', kind=kind, text=text)

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='ou', out_signature='', async_callbacks=('reply', 'error'))
        def RequestConfirmation(self, device_path, passkey, reply, error):
            self.ask(device_path, 'confirm', reply, error, 'Does '+row['title']+' show '+f'{int(passkey):06d}'+'?')

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='o', out_signature='s', async_callbacks=('reply', 'error'))
        def RequestPinCode(self, device_path, reply, error): self.ask(device_path, 'pin', reply, error)

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='o', out_signature='u', async_callbacks=('reply', 'error'))
        def RequestPasskey(self, device_path, reply, error): self.ask(device_path, 'passkey', reply, error)

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='o', out_signature='', async_callbacks=('reply', 'error'))
        def RequestAuthorization(self, device_path, reply, error): self.ask(device_path, 'confirm', reply, error, 'Pair with '+row['title']+'?')

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='os', out_signature='', async_callbacks=('reply', 'error'))
        def AuthorizeService(self, device_path, service, reply, error): self.ask(device_path, 'confirm', reply, error, 'Allow '+row['title']+' to connect?')

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='os', out_signature='')
        def DisplayPinCode(self, device_path, pincode):
            if str(device_path) != target: raise Rejected()
            emit('prompt', kind='display', text='Enter '+str(pincode)+' on '+row['title'])

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='ouq', out_signature='')
        def DisplayPasskey(self, device_path, passkey, entered):
            if str(device_path) != target: raise Rejected()
            emit('prompt', kind='display', text='Enter '+f'{int(passkey):06d}'+' on '+row['title']+' · '+str(entered)+'/6')

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='', out_signature='')
        def Cancel(self):
            if pending:
                pending.clear()
            emit('prompt', kind='display', text='Pairing cancelled')

        @dbus.service.method(BLUEZ+'.Agent1', in_signature='', out_signature='')
        def Release(self):
            result['error'] = UserError('Pairing ended. Try again.')
            loop.quit()

    def abort():
        if pending:
            pending.pop('error')(Rejected()); pending.clear()
        if pair:
            try: device.CancelPairing(timeout=3)
            except dbus.DBusException: pass
        if not row['connected']:
            try: device.Disconnect(timeout=3)
            except dbus.DBusException: pass
        result['error'] = Cancelled(); loop.quit()
        return False

    def answer(fd, flags):
        try:
            value = read_message()
            if value.get('cancel') or not pending: return abort()
            kind = pending['kind']; reply = pending['reply']
            if kind == 'confirm':
                if value.get('accept') is not True: return abort()
                pending.clear(); reply()
            elif kind == 'passkey':
                if not re.fullmatch('[0-9]{1,6}', value.get('value', '')): return abort()
                pending.clear(); reply(dbus.UInt32(int(value['value'])))
            else:
                pin = value.get('value', '')
                if not isinstance(pin, str) or not 1 <= len(pin) <= 16 or not pin.isascii(): return abort()
                pending.clear(); reply(pin)
        except Exception:
            return abort()
        return True

    agent = Agent(platform.bus, '/org/traineros/PairAgent')
    manager = platform.iface(BLUEZ, '/org/bluez', BLUEZ+'.AgentManager1')
    manager.RegisterAgent('/org/traineros/PairAgent', 'KeyboardDisplay', timeout=4)
    # Never replace the desktop's default agent; BlueZ uses this caller's agent.
    watch = GLib.io_add_watch(sys.stdin.fileno(), GLib.IO_IN | GLib.IO_HUP, answer)
    timer = GLib.timeout_add_seconds(85, abort)
    try:
        if pair: device.Pair(reply_handler=paired, error_handler=failed, timeout=75)
        else: device.Connect(reply_handler=connected, error_handler=failed, timeout=25)
        loop.run()
        if result['error']: raise result['error']
    finally:
        if GLib.MainContext.default().find_source_by_id(watch): GLib.source_remove(watch)
        if GLib.MainContext.default().find_source_by_id(timer): GLib.source_remove(timer)
        try: manager.UnregisterAgent('/org/traineros/PairAgent', timeout=3)
        except dbus.DBusException: pass
        agent.remove_from_connection()


def main():
    if len(sys.argv) != 1: return 2
    request = validate(read_message())
    if request['op'] != 'list' and os.geteuid() != 0: return 2
    from dbus.mainloop.glib import DBusGMainLoop
    DBusGMainLoop(set_as_default=True)
    platform = Platform(); error = ''
    try:
        (platform.wifi_action if request['kind'] == 'wifi' else platform.bluetooth_action)(request)
    except (Cancelled, BrokenPipeError):
        error = 'Cancelled'
    except UserError as failure:
        error = str(failure)
    except Exception:
        error = 'Connection unavailable. Check the radio and try again.'
    try: platform.snapshot(request['kind'])
    except Exception:
        if not error: error = 'Could not refresh connections. Try again.'
    emit('done', error=error)
    return 0


if __name__ == '__main__':
    try: sys.exit(main())
    except Exception:
        emit('done', error='Connection service unavailable.')
        sys.exit(1)
