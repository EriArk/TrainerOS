"""Application lifecycle checks without a real user's windows or controller."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

source=Path(__file__).resolve().parents[1]/'packaging/integrations'
sys.path.insert(0,str(source))
spec=importlib.util.spec_from_file_location('apps_service',source/'apps-service.py')
apps=importlib.util.module_from_spec(spec);spec.loader.exec_module(apps)


class X:
    def active(self):return 11
    def atom(self,name):return 1
    def prop(self,*_):return [1]
class Windows:
    def __init__(self):self.x=X();self.live={};self.mapped=[];self.closed=[]
    def all(self):return self.live
    def map(self,rows,visible):self.mapped.append((list(rows),visible))
    def close(self,window,row):self.closed.append(window);return True

def window(pid=90,start='100',transient=0,mapped=True):
    return dict(pid=pid,start=start,transient=transient,mapped=mapped,classes=['test'])


class Lifecycle(unittest.TestCase):
    @patch.object(apps,'emit')
    def test_idle_does_not_enumerate_unowned_desktop_windows(self,emit):
        s=self.service();s.sessions={};s.current=''
        with patch.object(s.windows,'all',side_effect=AssertionError('Unnecessary X11 scan')):
            s.scan();s.scan()
        emit.assert_called_once_with('state',current='',running=[],launching='')
    def service(self):
        s=apps.Service.__new__(apps.Service);s.windows=Windows();s.current='a';s.pending=None;s.last=None;s.lease=None
        s.sessions={'a':dict(windows={11:window()},owners={(90,'100')},row={'id':'a','name':'Test'})}
        s.save_sessions=lambda:None;s.acquire=lambda:None
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


if __name__=='__main__':unittest.main()
