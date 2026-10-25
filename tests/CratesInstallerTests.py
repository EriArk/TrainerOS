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

spec=importlib.util.spec_from_file_location('installer',Path(__file__).parents[1]/'packaging/crates/installer.py')
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
    def test_bad_architecture_does_not_activate(self):
        p,r=self.package();r['architecture']='impossible'
        with self.assertRaises(ValueError): self.store.activate(p,r)
        self.assertIsNone(self.store.receipt('fixture'))
    def test_game_launcher_requires_owner_and_isolates_progress(self):
        p,r=self.package(content=b'#!/bin/sh\nprintf ok > "$TRAINEROS_PACKAGE_DATA/probe"\n')
        self.store.activate(p,r)
        launch=Path(__file__).parents[1]/'packaging/crates/launch.py'
        command=[sys.executable,str(launch),str(self.store.root),'fixture']
        self.assertNotEqual(subprocess.run(command,capture_output=True).returncode,0)
        for owner in ('a'*64,'b'*64):
            result=subprocess.run(command+[owner],capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual((self.store.root/'data/fixture/owners'/owner/'probe').read_text(),'ok')

if __name__=='__main__': unittest.main()
