"""Real filesystem transactions, corruption/cancellation and active-process leases."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
import subprocess
import sys
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('installer',os.environ.get('TRAINEROS_TEST_CRATES_INSTALLER',
    str(Path(__file__).parents[1]/'packaging/crates/installer.py')))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.home=patch.dict(os.environ,{'HOME':str(self.root)});self.home.start()
        self.store=m.Store(self.root/'packages')
    def tearDown(self): self.home.stop();self.temp.cleanup()
    def package(self,version='1',bad=None,content=b'test'):
        path=self.root/(version+'.tar.gz')
        with tarfile.open(path,'w:gz') as tar:
            info=tarfile.TarInfo('bin/app');info.size=len(content);info.mode=0o755;tar.addfile(info,io.BytesIO(content))
            if bad: tar.addfile(bad)
        record={'id':'fixture','kind':'game','version':version,'title':'Fixture','architecture':os.uname().machine,
                'entry':'bin/app','size':path.stat().st_size,'unpackedSize':1024,
                'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        return path,record
    def test_pause_keeps_partial_and_resume_checks_range(self):
        data=b'a'*(128*1024)+b'b'*(128*1024)
        target=self.root/'partial';record={'url':'https://github.com/fixture','size':len(data)}
        class Response(io.BytesIO):
            status=200
            headers={}
        opener=unittest.mock.Mock();opener.open.return_value=Response(data)
        with patch.object(m.urllib.request,'build_opener',return_value=opener):
            with self.assertRaises(m.DownloadPaused):
                m.download(record,target,lambda p:None,lambda:False,lambda:target.exists() and target.stat().st_size>=128*1024)
        self.assertEqual(target.read_bytes(),data[:128*1024])
        response=Response(data[128*1024:]);response.status=206;response.headers={'Content-Range':f'bytes 131072-262143/262144'}
        opener.open.return_value=response
        with patch.object(m.urllib.request,'build_opener',return_value=opener):m.download(record,target,lambda p:None,lambda:False)
        self.assertEqual(opener.open.call_args.args[0].get_header('Range'),'bytes=131072-')
        self.assertEqual(target.read_bytes(),data)
    def test_range_ignored_restarts_and_bad_range_does_not_append(self):
        target=self.root/'partial';target.write_bytes(b'old')
        record={'url':'https://github.com/fixture','size':6}
        class Response(io.BytesIO):
            status=206
            headers={'Content-Range':'bytes 0-5/6'}
        opener=unittest.mock.Mock();opener.open.return_value=Response(b'abcdef')
        with patch.object(m.urllib.request,'build_opener',return_value=opener):
            with self.assertRaises(ValueError):m.download(record,target,lambda p:None,lambda:False)
        self.assertEqual(target.read_bytes(),b'old')
        response=Response(b'abcdef');response.status=200;opener.open.return_value=response
        with patch.object(m.urllib.request,'build_opener',return_value=opener):m.download(record,target,lambda p:None,lambda:False)
        self.assertEqual(target.read_bytes(),b'abcdef')
    def test_update_and_downgrade_keep_data(self):
        p,r=self.package();first=self.store.activate(p,r)
        save=self.store.root/'data'/'progress';save.write_text('keep')
        p2,r2=self.package('2');self.store.activate(p2,r2)
        self.assertEqual(self.store.receipt('fixture')['version'],'2')
        self.store.activate(p,r);self.assertEqual(self.store.receipt('fixture')['version'],'1')
        self.assertTrue((self.store.root/'objects'/first['object']).exists())
        self.assertEqual(save.read_text(),'keep')
    def test_checksum_failure_preserves_previous(self):
        p,r=self.package();self.store.activate(p,r)
        p2,r2=self.package('2');r2['sha256']='0'*64
        with self.assertRaises(ValueError): self.store.activate(p2,r2)
        self.assertEqual(self.store.receipt('fixture')['version'],'1')
    def test_archive_paths_and_links(self):
        for name,kind in [('../escape',tarfile.REGTYPE),('/escape',tarfile.REGTYPE),('link',tarfile.SYMTYPE),('hard',tarfile.LNKTYPE)]:
            bad=tarfile.TarInfo(name);bad.type=kind;bad.linkname='/tmp/escape'
            p,r=self.package(bad=bad)
            with self.assertRaises(ValueError): self.store.activate(p,r)
            self.assertIsNone(self.store.receipt('fixture'))
    def test_cancel_before_activation(self):
        p,r=self.package();self.store.activate(p,r)
        p2,r2=self.package('2')
        with self.assertRaises(InterruptedError): self.store.activate(p2,r2,cancelled=lambda:True)
        self.assertEqual(self.store.receipt('fixture')['version'],'1')
    def test_busy_lease_protects_update_and_removal(self):
        p,r=self.package();self.store.activate(p,r)
        with self.store.lock('fixture',exclusive=False):
            with self.assertRaises(BlockingIOError): self.store.remove('fixture')
            p2,r2=self.package('2')
            with self.assertRaises(BlockingIOError): self.store.activate(p2,r2)
        self.assertEqual(self.store.receipt('fixture')['version'],'1')
    def test_uninstall_preserves_user_files(self):
        p,r=self.package();self.store.activate(p,r)
        data=self.store.root/'data'/'progress';data.write_text('keep')
        self.store.remove('fixture');self.assertIsNone(self.store.receipt('fixture'))
        self.assertEqual(data.read_text(),'keep');self.assertEqual(list((self.store.root/'objects').iterdir()),[])
    def crash(self, phase, archive, record, operation='install'):
        # Kill a real worker without exception/finally cleanup; test hooks never
        # enter the production helper.
        script = r'''
import json,os,signal,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1]);import installer as m
root,phase,archive,record,operation=sys.argv[2:];record=json.loads(record)
def die(): os.kill(os.getpid(),signal.SIGKILL)
original=m.atomic_json
def atomic(path,value):
    if path.parent.name=='active' and phase=='receipt-before':die()
    original(path,value)
    if path.parent.name=='transactions' and phase=='journal':die()
    if path.parent.name=='active' and phase=='receipt-after':die()
m.atomic_json=atomic
original_sync=m.sync_directory
def sync(path):
    original_sync(path)
    if Path(path).name=='objects' and phase=='object':die()
m.sync_directory=sync
original_unlink=m.durable_unlink
def unlink(path):
    if path.parent.name=='active' and path.suffix=='.json' and phase=='remove-before':die()
    original_unlink(path)
    if path.parent.name=='active' and path.suffix=='.json' and phase=='remove-after':die()
m.durable_unlink=unlink
original_desktop=m.Store.desktop
def desktop(self,record):
    original_desktop(self,record)
    if phase=='desktop':die()
m.Store.desktop=desktop
original_rmtree=m.shutil.rmtree
def rmtree(path,*args,**kwargs):
    original_rmtree(path,*args,**kwargs)
    if Path(path).parent.name=='objects' and phase=='cleanup':die()
m.shutil.rmtree=rmtree
store=m.Store(root)
if operation=='remove':store.remove('fixture')
else:store.activate(archive,record,lambda stage:die() if stage=='Installing' and phase=='extracted' else None)
'''
        result=subprocess.run([sys.executable,'-c',script,str(Path(m.__file__).parent),str(self.store.root),
                               phase,str(archive),json.dumps(record),operation],capture_output=True,text=True)
        self.assertEqual(result.returncode,-9,result.stderr)

    def recover_worker(self, expected=0):
        result=subprocess.run([sys.executable,m.__file__,'missing-offline-registry',str(self.store.root),
                               'recover','-','-'],capture_output=True,text=True)
        self.assertEqual(result.returncode,expected,result.stdout+result.stderr)

    def test_killed_update_recovers_both_sides_of_commit_offline(self):
        old,r1=self.package('1',content=b'old');r1['kind']='app'
        new,r2=self.package('2',content=b'new');r2['kind']='app'
        for phase in ('journal','extracted','object','receipt-before','receipt-after','desktop'):
            with self.subTest(phase=phase):
                self.store.remove('fixture');self.store.activate(old,r1)
                save=self.store.root/'data'/'fixture'/'owners'/('a'*64)/'save'
                save.parent.mkdir(parents=True,exist_ok=True);save.write_bytes(b'owner progress')
                self.crash(phase,new,r2)
                with self.store.lock('fixture',exclusive=False):
                    self.recover_worker(75)
                    self.assertTrue((self.store.root/'transactions/fixture.json').exists())
                self.recover_worker();self.recover_worker()
                committed=phase in ('receipt-after','desktop')
                receipt=self.store.receipt('fixture')
                self.assertEqual(receipt['version'],'2' if committed else '1')
                self.assertEqual((self.store.root/'objects'/receipt['object']/'bin/app').read_bytes(),b'new' if committed else b'old')
                self.assertIn(receipt['object'],self.store.desktop_path('fixture').read_text())
                self.assertEqual(save.read_bytes(),b'owner progress')
                self.assertFalse(list((self.store.root/'transactions').glob('*.json')))
                self.assertFalse((self.store.root/'staging/unpack-fixture').exists())
                if not committed:self.assertFalse((self.store.root/'objects'/('fixture-'+r2['sha256'])).exists())

    def test_killed_first_install_never_exposes_uncommitted_package(self):
        archive,record=self.package(content=b'first install');record['kind']='app'
        for phase in ('receipt-before','receipt-after'):
            with self.subTest(phase=phase):
                self.store.remove('fixture')
                self.crash(phase,archive,record)
                self.recover_worker()
                committed=phase=='receipt-after'
                self.assertEqual(self.store.receipt('fixture') is not None,committed)
                self.assertEqual(self.store.desktop_path('fixture').exists(),committed)
                self.assertEqual(bool(list((self.store.root/'objects').iterdir())),committed)

    def test_killed_remove_and_reinstall_keeps_each_owners_data(self):
        archive,record=self.package(content=b'installed');record['kind']='app'
        for phase in ('journal','remove-before','remove-after','cleanup'):
            with self.subTest(phase=phase):
                self.store.activate(archive,record)
                saves=[]
                for owner in ('a'*64,'b'*64):
                    save=self.store.root/'data/fixture/owners'/owner/'progress'
                    save.parent.mkdir(parents=True,exist_ok=True);save.write_text(owner);saves.append(save)
                self.crash(phase,archive,record,'remove')
                self.recover_worker();self.recover_worker()
                committed=phase in ('remove-after','cleanup')
                self.assertEqual(self.store.receipt('fixture') is None,committed)
                self.assertEqual(self.store.desktop_path('fixture').exists(),not committed)
                if committed:self.assertFalse(list((self.store.root/'objects').iterdir()))
                self.store.activate(archive,record)
                for save in saves:self.assertEqual(save.read_text(),save.parent.name)
                self.assertTrue(self.store.desktop_path('fixture').exists())

    def test_bad_architecture_does_not_activate(self):
        p,r=self.package();r['architecture']='impossible'
        with self.assertRaises(ValueError): self.store.activate(p,r)
        self.assertIsNone(self.store.receipt('fixture'))
    def test_game_launcher_requires_owner_and_isolates_progress(self):
        p,r=self.package(content=b'#!/bin/sh\nprintf ok > "$TRAINEROS_PACKAGE_DATA/probe"\n')
        self.store.activate(p,r)
        launch=Path(m.__file__).with_name('launch.py')
        command=[sys.executable,str(launch),str(self.store.root),'fixture']
        self.assertNotEqual(subprocess.run(command,capture_output=True).returncode,0)
        for owner in ('a'*64,'b'*64):
            result=subprocess.run(command+[owner],capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual((self.store.root/'data/fixture/owners'/owner/'probe').read_text(),'ok')

if __name__=='__main__': unittest.main()
