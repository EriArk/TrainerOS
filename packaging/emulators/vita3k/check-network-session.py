#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Boundaries for admitted native LAN sessions; no network or personal data used."""
import asyncio
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('vita_session',Path(__file__).with_name('network-session.py'))
session=importlib.util.module_from_spec(spec)
spec.loader.exec_module(session)


class Frames(unittest.TestCase):
    def test_batch_keeps_exact_frames_and_control_order(self):
        q=asyncio.Queue()
        frames=[bytes([n])*1500 for n in range(14)]
        for value in frames[1:]:q.put_nowait(b'D'+value)
        q.put_nowait(b'J')
        pending=b'D'+frames[0];found=[]
        while pending!=b'J':
            record,pending=session.batch.batch_datagrams(pending,q)
            self.assertLessEqual(len(record),16384)
            found+=session.batch.decode_datagrams(record)
            if pending is None:pending=q.get_nowait()
        self.assertEqual(frames,found)
        self.assertTrue(q.empty())

    def test_truncated_oversized_and_empty_records_rejected(self):
        for data in [b'',b'B',b'B\x00',b'B\x00\x02x',b'B\x00\x00',b'D',b'D'+b'x'*1515,b'J']:
            with self.subTest(data=data[:8]),self.assertRaises(ValueError):
                session.batch.decode_datagrams(data)

    def test_endpoint_does_not_accept_an_extra_network_or_path(self):
        value={'endpoint':{'mode':'nearby','port':45000,'fingerprint':'a'*64,'token':'b'*32}}
        self.assertEqual(session.batch.ready_endpoint(value,'nearby'),value['endpoint'])
        value['endpoint']['path']='/unrelated'
        with self.assertRaises(ValueError):session.batch.ready_endpoint(value,'nearby')


class Config(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        user=self.root/'user';(user/'config/Vita3K').mkdir(parents=True)
        (user/'config/Vita3K/config.yml').write_text('pref-path: /own\n')
        runtime=self.root/'runtime';runtime.mkdir()
        for name in ('AppRun','python','channel.py'):(runtime/name).write_text('fixture')
        self.value={'channel':{'host':True,'mode':'online','identity':'a'*64},
            'runtime':str(runtime/'AppRun'),'runtimeRoot':str(runtime),'python':str(runtime/'python'),
            'helper':str(runtime/'channel.py'),'user':str(user),'title':'VQUP00001','mac':'02:01:02:03:04:05'}
        self.path=self.root/'session.json'

    def read(self,value):
        self.path.write_text(json.dumps(value))
        return session.read_config(self.path)

    def test_valid_private_session(self):
        self.assertEqual(self.read(self.value),self.value)

    def test_title_and_mac_cannot_be_paths_or_shared_broadcast_identity(self):
        for key,values in {'title':['../app','VQUP00001 --flag','vqUP00001'],
                           'mac':['ff:ff:ff:ff:ff:ff','00:00:00:00:00:00','02:01:02:03:04:05;command']}.items():
            for value in values:
                candidate=copy.deepcopy(self.value);candidate[key]=value
                with self.subTest(key=key,value=value),self.assertRaises(ValueError):self.read(candidate)

    def test_unrelated_directory_and_runtime_escape_rejected(self):
        other=self.root/'another';other.mkdir()
        for key,value in [('runtime',str(self.root/'user/config/Vita3K/config.yml')),('user',str(self.root))]:
            candidate=copy.deepcopy(self.value);candidate[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.read(candidate)

    def test_credentials_stay_in_expected_local_schema(self):
        candidate=copy.deepcopy(self.value);candidate['launchCommand']='unrelated command'
        with self.assertRaises(ValueError):self.read(candidate)
        for key,value in [('host',1),('identity','a'*63),('mode','unreviewed')]:
            candidate=copy.deepcopy(self.value);candidate['channel'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.read(candidate)


if __name__=='__main__':unittest.main()
