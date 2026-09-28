"""Content-free checks of Wi-Fi activation/rollback and bounded requests."""
import importlib.util
from pathlib import Path
import types
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('network', Path(__file__).resolve().parents[1]/'packaging/integrations/network-control.py')
n = importlib.util.module_from_spec(spec); spec.loader.exec_module(n)

class NetworkTests(unittest.TestCase):
    def test_protocol_and_security(self):
        self.assertEqual(n.wifi_security({'RsnFlags': 0x100}), 'wpa-psk')
        self.assertEqual(n.wifi_security({'RsnFlags': 0x400}), 'sae')
        self.assertEqual(n.wifi_security({'RsnFlags': 0x200}), 'unsupported')
        self.assertEqual(n.wifi_security({'Flags': 1}), 'unsupported')
        self.assertEqual(n.wifi_security({}), 'open')
        for value in ({'op':'shell','kind':'wifi'}, {'op':'pair','kind':'wifi'},
                      {'op':'connect','kind':'wifi','password':'x'*65},
                      {'op':'connect','kind':'wifi','id':[]},
                      {'op':'list','kind':'wifi','command':'bad'}):
            with self.assertRaises(ValueError): n.validate(value)

    def activation(self, *, saved=False, state=2, cancel=False, stale=False):
        events=[]
        class Manager:
            def CheckpointCreate(self,*args,**kw):events.append('protect');return '/checkpoint'
            def ActivateConnection(self,*args,**kw):events.append('activate-saved');return '/active'
            def AddAndActivateConnection2(self,config,device,ap,options,**kw):
                self.options=options;events.append('activate-memory');return '/new','/active',{}
            def CheckpointRollback(self,*args,**kw):events.append('rollback')
            def CheckpointDestroy(self,*args,**kw):events.append('unprotect')
            def Save(self,**kw):events.append('save')
            def Delete(self,**kw):events.append('delete-new')
        manager=Manager();p=object.__new__(n.Platform)
        p.d=types.SimpleNamespace(Array=lambda v,**kw:v,ObjectPath=str,UInt32=int,Boolean=bool,
                                  ByteArray=bytes,Dictionary=lambda v,**kw:v,DBusException=RuntimeError)
        row={'id':'target','title':'Fixture network','connected':False,'available':True,
             'security':'wpa-psk','_profile':'/saved' if saved else None,'_ap':'/ap','_ssid':b'Fixture'}
        p.wifi=lambda:(manager,'/device','/old',[] if stale else [row])
        p.iface=lambda *args:manager
        p.props=lambda *args:{'State':state}
        with patch.object(n,'check_cancel',side_effect=n.Cancelled() if cancel else None):
            if state!=2 or cancel or stale:
                with self.assertRaises((n.UserError,n.Cancelled)):
                    p.wifi_action({'op':'connect','id':'target','password':'fixture-password'})
            else:p.wifi_action({'op':'connect','id':'target','password':'fixture-password'})
        return events,manager

    def test_new_profile_persists_only_after_connection(self):
        events,manager=self.activation()
        self.assertEqual(manager.options,{'persist':'memory'})
        self.assertEqual(events,['protect','activate-memory','save','unprotect'])

    def test_failure_and_cancel_restore_old_connection_and_discard_new_profile(self):
        for kwargs in ({'state':4},{'cancel':True}):
            events,_=self.activation(**kwargs)
            self.assertEqual(events,['protect','activate-memory','rollback','unprotect','delete-new'])
        events,_=self.activation(saved=True,state=4)
        self.assertEqual(events,['protect','activate-saved','rollback','unprotect'])

    def test_stale_selection_never_mutates(self):
        self.assertEqual(self.activation(stale=True)[0],[])

if __name__ == '__main__':unittest.main()
