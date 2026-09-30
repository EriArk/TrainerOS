"""No radios required: identity, pipe framing and owned-connection cleanup."""
import importlib.util
import json
from pathlib import Path
import types
import unittest
import uuid
import tempfile
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('nearby', Path(__file__).resolve().parents[1]/'packaging/integrations/nearby-control.py')
n = importlib.util.module_from_spec(spec); spec.loader.exec_module(n)


class NearbyTests(unittest.TestCase):
    def radio(self):
        app=object.__new__(n.Nearby)
        app.key='11111111-1111-4111-8111-111111111111'; app.name='Misty'; app.visible=True
        app.path='/radio'; app.device='/device'; app.active=app.profile=None
        app.target=''; app.deadline=0; app.incoming=app.ready=app.network_started=False
        app.last_scan=0; app.retry_after=0; app.declined={}; app.matches=[]; app.old={}
        app.direct_enabled=True
        peer='22222222-2222-4222-8222-222222222222'
        app.peers={peer:{'id':peer,'name':'Brock','path':'/peer','mac':'00:11:22:33:44:55','seen':100}}
        calls=[]
        app.d=types.SimpleNamespace(Dictionary=lambda v,**kw:v,Int32=int,UInt32=int,ObjectPath=str,
            String=str,Array=lambda v,**kw:v,ByteArray=bytes,DBusException=RuntimeError)
        app.api=types.SimpleNamespace(StopFind=lambda **kw:calls.append('stop-find'),
            ExtendedListen=lambda v,**kw:calls.append(('listen',v)),Cancel=lambda **kw:calls.append('cancel'))
        app.manager=types.SimpleNamespace(AddAndActivateConnection2=lambda *a,**kw:('/profile','/owned',{}),
            DeactivateConnection=lambda p,**kw:calls.append(('deactivate',p)))
        app.iface=lambda *a:types.SimpleNamespace(Set=lambda *a,**kw:calls.append('identity'))
        app.props=lambda *a:{'State':1}
        app.found=lambda *a:None
        return app,peer,calls

    def test_only_traineros_identity_is_discovered(self):
        key = str(uuid.uuid4())
        raw = n.MAGIC + uuid.UUID(key).bytes + 'Misty'.encode()
        self.assertEqual(n.decode_peer({'VendorExtension': [b'Other console', raw]}), {'id': key, 'name': 'Misty'})
        for raw in (b'Other console', n.MAGIC, n.MAGIC+uuid.UUID(key).bytes+b'Bad\nname', n.MAGIC+uuid.UUID(key).bytes+b'\xff'):
            self.assertIsNone(n.decode_peer({'VendorExtension': [raw]}))
        for name in ('', 'x'*33, '\x00Trainer'):
            with self.assertRaises(ValueError): n.identity({'id':key,'name':name})

    def test_direct_policy_defaults_on_and_rejects_malformed_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            policy=Path(directory)/'nearby.json'
            self.assertTrue(n.direct_enabled(policy))
            for data,expected in ((b'{"enabled":false}',False),(b'{"enabled":true}',True),
                                  (b'{"enabled":"false"}',False),(b'[]',False),
                                  (b'{',False),(b'\xff',False),(b'x'*1025,False)):
                policy.write_bytes(data)
                with patch.object(n,'diagnostic'):self.assertEqual(n.direct_enabled(policy),expected)

    def test_disabled_direct_keeps_pipe_alive_without_touching_the_radio(self):
        app,peer,calls=self.radio();app.direct_enabled=False;app.path=None;app.device=None
        app.initialize=lambda:self.fail('Disabled transport touched the radio')
        with patch.object(n,'emit') as event, patch.object(n,'diagnostic'):
            app.command(json.dumps({'op':'configure','id':app.key,'name':app.name,'visible':True}))
            app.tick();self.assertFalse(app.visible);self.assertEqual(calls,[])
            self.assertTrue(any(c.args[0]=='unavailable' for c in event.call_args_list))
            app.command(json.dumps({'op':'invite','peer':peer}));self.assertIsNone(app.active)
            app.command(json.dumps({'op':'disconnect'}));self.assertEqual(calls,[])

    def test_diagnostic_does_not_record_credentials_or_exception_text(self):
        messages=[]
        logger=types.SimpleNamespace(LOG_PID=1,LOG_DAEMON=2,LOG_INFO=3,
                                     openlog=lambda *a:None,syslog=lambda level,value:messages.append(value))
        with patch.dict('sys.modules',{'syslog':logger}):
            n.diagnostic('negotiation-failed',status=7,passphrase='secret',pin='1234',peer_object='/peer')
        self.assertEqual(json.loads(messages[0]),{'stage':'negotiation-failed','status':7})
        app,peer,calls=self.radio();app.active='/owned'
        with patch.object(n,'diagnostic') as log:
            app.device_state(120,50,11)
            log.assert_called_once_with('device-state',state=120,reason=11)
            self.assertEqual(app.active,'/owned') # Observation is not a network mutation.

    def test_partial_and_batched_commands_survive_pipe_buffering(self):
        app = object.__new__(n.Nearby); app.buffer=b''; commands=[]; app.command=commands.append
        with patch.object(n.os,'read',side_effect=[b'{"op":"dis',b'connect"}\n{"op":"configure"}\n']):
            self.assertTrue(app.input(None,None)); self.assertEqual(commands,[])
            self.assertTrue(app.input(None,None))
        self.assertEqual(commands,[b'{"op":"disconnect"}',b'{"op":"configure"}'])

    def test_release_deactivates_only_its_own_connection(self):
        app=object.__new__(n.Nearby); app.active='/traineros/active'; calls=[]
        app.target='';app.incoming=False;app.path=None
        app.manager=types.SimpleNamespace(DeactivateConnection=lambda path,**kw:calls.append(path))
        app.d=types.SimpleNamespace(DBusException=RuntimeError)
        app.release();app.release()
        self.assertEqual(calls,['/traineros/active']);self.assertIsNone(app.active)
        self.assertEqual(app.target,'');self.assertFalse(app.ready)

    def test_accept_gets_its_own_network_deadline_and_is_idempotent(self):
        app,peer,calls=self.radio()
        with patch.object(n.time,'monotonic',return_value=100), patch.object(n,'emit'):
            app.requested('/peer'); self.assertEqual(app.deadline,150)
        with patch.object(n.time,'monotonic',return_value=149), patch.object(n,'emit') as event:
            app.command(json.dumps({'op':'accept','peer':peer}))
            self.assertEqual(app.deadline,149+n.NETWORK_SECONDS); self.assertTrue(app.network_started)
            event.assert_called_with('connecting',peer=peer,phase='network')
            app.command(json.dumps({'op':'accept','peer':peer}))
            self.assertEqual(app.active,'/owned');self.assertNotIn(('deactivate','/owned'),calls)
        with patch.object(n.time,'monotonic',return_value=151), patch.object(n,'emit'):
            app.tick();self.assertEqual(app.target,peer) # Old response deadline has elapsed.

    def test_failed_activation_can_be_retried_immediately(self):
        app,peer,calls=self.radio()
        def fail(*a,**kw):raise RuntimeError('activation failed')
        app.manager.AddAndActivateConnection2=fail
        with patch.object(n.time,'monotonic',return_value=100), patch.object(n,'emit') as event:
            app.command(json.dumps({'op':'invite','peer':peer}))
            self.assertEqual(app.target,'');self.assertEqual(app.deadline,0)
            self.assertEqual(event.call_args.args[0],'error')
            app.manager.AddAndActivateConnection2=lambda *a,**kw:('/profile','/owned',{})
            app.command(json.dumps({'op':'invite','peer':peer}));self.assertEqual(app.active,'/owned')

    def test_negotiation_extends_once_and_ignores_another_peer(self):
        app,peer,calls=self.radio()
        with patch.object(n.time,'monotonic',return_value=100), patch.object(n,'emit'):
            app.connect(peer);old=app.deadline
            app.lost('/peer');self.assertIn(peer,app.peers)
            app.negotiated({'peer_object':'/someone-else'});self.assertEqual(app.deadline,old)
        with patch.object(n.time,'monotonic',return_value=140), patch.object(n,'emit') as event:
            app.negotiated({'peer_object':'/peer'});self.assertEqual(app.deadline,140+n.NETWORK_SECONDS)
            event.assert_called_once_with('connecting',peer=peer,phase='network')
        with patch.object(n.time,'monotonic',return_value=190), patch.object(n,'emit') as event:
            app.negotiated({'peer_object':'/peer'});self.assertEqual(app.deadline,140+n.NETWORK_SECONDS)
            app.tick();self.assertIn(peer,app.peers);self.assertEqual(app.active,'/owned')
        with patch.object(n.time,'monotonic',return_value=206), patch.object(n,'emit') as event:
            app.tick();self.assertIsNone(app.active)
            self.assertTrue(any(c.args[0]=='closed' and 'connecting' in c.kwargs.get('error','') for c in event.call_args_list))

    def test_pausing_visibility_keeps_connection_and_disables_listen_correctly(self):
        app,peer,calls=self.radio();app.active='/owned';app.target=peer
        with patch.object(n.time,'monotonic',return_value=100), patch.object(n,'emit'):
            app.command(json.dumps({'op':'configure','id':app.key,'name':app.name,'visible':False}))
            self.assertEqual(app.active,'/owned');self.assertNotIn(('deactivate','/owned'),calls)
            self.assertIn(('listen',{}),calls)

    def test_negotiation_failure_cleans_only_matching_attempt(self):
        app,peer,calls=self.radio();app.active='/owned';app.target=peer
        with patch.object(n,'emit') as event:
            app.negotiation_failed({'peer_object':'/other'});self.assertEqual(app.active,'/owned')
            app.negotiation_failed({'peer_object':'/peer'});self.assertIsNone(app.active)
            self.assertIn(('deactivate','/owned'),calls);self.assertEqual(app.target,'')
            event.assert_called_once_with('closed',error='Could not connect. Try inviting your friend again.')

    def test_discovery_setting_failure_does_not_end_established_connection(self):
        app,peer,calls=self.radio();app.active='/owned';app.target=peer;app.ready=True
        def fail():raise RuntimeError('discovery settings unavailable')
        app.advertise=fail
        with patch.object(n,'emit') as event:
            app.command(json.dumps({'op':'configure','id':app.key,'name':app.name,'visible':False}))
            self.assertEqual(app.active,'/owned');self.assertNotIn(('deactivate','/owned'),calls)
            event.assert_called_once_with('searching',active=False)

    def test_radio_failure_backs_off_before_reinitializing(self):
        app,peer,calls=self.radio();app.path=None;attempts=[]
        def fail():attempts.append(1);raise RuntimeError('radio unavailable')
        app.initialize=fail;app.close=lambda:None
        with patch.object(n,'emit'), patch.object(n.time,'monotonic',return_value=100):app.tick()
        with patch.object(n,'emit'), patch.object(n.time,'monotonic',return_value=101):app.tick()
        self.assertEqual(len(attempts),1)
        with patch.object(n,'emit'), patch.object(n.time,'monotonic',return_value=116):app.tick()
        self.assertEqual(len(attempts),2)


if __name__ == '__main__':unittest.main()
