"""Application lifecycle checks without a real user's windows or controller."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import types

# These tests fake X11, /proc and leases. Importing their definitions on the
# Windows build host does not claim Linux input/process acceptance.
if sys.platform == 'win32':
    sys.modules.setdefault('fcntl', types.ModuleType('fcntl'))

source=Path(__file__).resolve().parents[1]/'packaging/integrations'
sys.path.insert(0,str(source))
spec=importlib.util.spec_from_file_location('apps_service',source/'apps-service.py')
apps=importlib.util.module_from_spec(spec);spec.loader.exec_module(apps)


class X:
    def active(self):return 11
    def atom(self,name):return 1
    def prop(self,*_):return [1]
    def owns(self,w,pid,start):return w==12 and pid==80 and start=='root'
class Windows:
    def __init__(self):self.x=X();self.live={};self.mapped=[];self.closed=[]
    def all(self):return self.live
    def map(self,rows,visible):self.mapped.append((list(rows),visible))
    def close(self,window,row):self.closed.append(window);return True

def window(pid=90,start='100',transient=0,mapped=True):
    return dict(pid=pid,start=start,transient=transient,mapped=mapped,classes=['test'])


class Lifecycle(unittest.TestCase):
    def test_native_home_chord_opens_keyboard_without_controls_or_first_key(self):
        import apps_input
        from keyboard_input import HomeChord,KeyboardNavigation
        lease=apps_input.Lease.__new__(apps_input.Lease)
        class Device:
            value=1
            def mode(self,value=None):
                if value is not None:self.value=value
                return self.value
        sample=dict(neutral=True,home=False,keyboardX=False,confirm=False,back=False,left=False,right=False,up=False,down=False)
        lease.device=Device();lease.pad=types.SimpleNamespace(sample=lambda:sample)
        lease.guard=types.SimpleNamespace(poll=lambda:None);lease.mapper=None;lease.keyboard=False
        lease.menu=False;lease.armed=False;lease.held='';lease.last=0;lease.resume_pending=False
        lease.chord=HomeChord();lease.navigation=KeyboardNavigation();events=[]
        def emit(event,**data):events.append((event,data))
        sample.update(home=True,neutral=False);lease.device.mode(2);lease.poll(emit)
        self.assertEqual(events,[])
        sample['keyboardX']=True;lease.poll(emit);lease.poll(emit)
        self.assertEqual(events,[('keyboard',{})])
        sample.update(home=False,keyboardX=False,neutral=True);lease.poll(emit)
        sample.update(confirm=True,neutral=False);lease.poll(emit);lease.poll(emit)
        self.assertEqual(events[-1],('keyboard-action',{'action':'confirm'}));self.assertEqual(len(events),2)
        sample.update(confirm=False,neutral=True);lease.poll(emit)
        sample.update(home=True,neutral=False);lease.poll(emit)
        sample.update(home=False,neutral=True);lease.poll(emit)
        self.assertEqual(events[-2:],[('keyboard-closed',{}),('controls',{})]);self.assertTrue(lease.menu);self.assertFalse(lease.keyboard)
    def test_installer_mapping_uses_shared_neutral_and_home_release(self):
        import apps_input
        mapper=apps_input.Mapper(apps_input.installer_profile())
        sample=dict(connected=True,neutral=True,buttons=[],sticks=[0,0,0,0],home=False)
        self.assertFalse(mapper.frame(sample,True,0)['held'])
        sample.update(neutral=False,buttons=['south'],sticks=[0,0,1,0])
        frame=mapper.frame(sample,True,.02)
        self.assertEqual(frame['held'],['MouseLeft']);self.assertGreater(frame['motion'][0],0)
        sample['home']=True
        self.assertFalse(mapper.frame(sample,True,.04)['held'])
        sample['home']=False
        self.assertFalse(mapper.frame(sample,True,.06)['held']) # Release/neutral required after Home.
    @patch.object(apps,'emit')
    def test_idle_does_not_enumerate_unowned_desktop_windows(self,emit):
        s=self.service();s.sessions={};s.current=''
        with patch.object(s.windows,'all',side_effect=AssertionError('Unnecessary X11 scan')):
            s.scan();s.scan()
        emit.assert_called_once_with('state',current='',running=[],launching='')
    def service(self):
        s=apps.Service.__new__(apps.Service);s.windows=Windows();s.current='a';s.pending=None;s.last=None;s.lease=None
        s.sessions={'a':dict(windows={11:window()},owners={(90,'100')},row={'id':'a','name':'Test'})}
        s.save_sessions=lambda:None;s.acquire=lambda *args:None
        return s
    @patch.object(apps,'emit')
    def test_close_is_request_dialog_and_cancel_remain_live(self,emit):
        s=self.service();s.windows.live={11:window()}
        s.command({'command':'close','id':'a'})
        self.assertEqual(s.windows.closed,[11]);self.assertEqual(s.current,'a')
        s.windows.live[12]=window(transient=11);s.scan()
        self.assertEqual(set(s.sessions['a']['windows']),{11,12})
        del s.windows.live[12];s.scan() # Cancel dialog, original window remains.
        self.assertEqual(set(s.sessions['a']['windows']),{11})
        s.windows.live={};s.scan();self.assertFalse(s.sessions);self.assertEqual(s.current,'')
        emit.assert_any_call('shell')
    @patch.object(apps,'emit')
    def test_minimize_restore_keeps_identity(self,emit):
        s=self.service();s.windows.live={11:window()}
        s.command({'command':'minimize'});self.assertEqual(s.current,'')
        self.assertEqual(s.windows.mapped[-1],([11],False));self.assertFalse(s.windows.closed)
        s.command({'command':'launch','id':'a'});self.assertEqual(s.current,'a')
        self.assertEqual(s.windows.mapped[-1],([11],True))
    @patch.object(apps,'emit')
    def test_reused_pid_does_not_inherit_ownership(self,emit):
        s=self.service();s.windows.live={11:window(start='200')};s.scan()
        self.assertFalse(s.sessions);self.assertFalse(s.windows.closed)
    @patch.object(apps,'emit')
    def test_background_dialog_does_not_steal_foreground(self,emit):
        s=self.service();s.current='';s.windows.live={11:window(mapped=False),12:window(transient=11)};s.scan()
        self.assertIn(([12],False),s.windows.mapped)
    @patch.object(apps,'emit')
    @patch.object(apps,'identity',return_value='root')
    def test_installer_waits_between_windows_and_tracks_new_child(self,identity,emit):
        s=self.service();s.sessions['a']['root']=(80,'root');s.windows.live={}
        s.scan();self.assertIn('a',s.sessions) # Parent setup can disappear between phases.
        s.windows.live={12:window(pid=81),13:window(pid=91)};s.scan()
        self.assertEqual(set(s.sessions['a']['windows']),{12})
        s.command({'command':'minimize'});s.windows.live[14]=window(pid=81,transient=12);s.scan()
        self.assertIn(([14],False),s.windows.mapped)
        identity.return_value='reused';s.windows.live={};s.scan();self.assertFalse(s.sessions)


if __name__=='__main__':unittest.main()
