# SPDX-License-Identifier: GPL-3.0-or-later
import base64
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'packaging/crates'))
import catalogue as c
import catalogue_server as server
import publish_catalogue as publisher
from publish_release import add_project_presentations
from installer import select_release


class CatalogueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder=tempfile.TemporaryDirectory();cls.path=Path(cls.folder.name)
        cls.key=cls.path/'key.pem';cls.pub=cls.path/'pub.pem'
        subprocess.run(['openssl','genpkey','-algorithm','ED25519','-out',str(cls.key)],check=True,capture_output=True)
        subprocess.run(['openssl','pkey','-in',str(cls.key),'-pubout','-out',str(cls.pub)],check=True,capture_output=True)
        cls.base={'schema':1,'revision':1,'crates':json.loads((ROOT/'data/crates/showcase.json').read_text()),
            'catalogue':json.loads((ROOT/'data/crates/catalogue.json').read_text()),
            'packages':json.loads((ROOT/'packaging/crates/packages.json').read_text())['packages'],'assets':{}}

    @classmethod
    def tearDownClass(cls): cls.folder.cleanup()

    def signed(self,value): return {'signed':copy.deepcopy(value),'signature':c.signature(value,self.key)}

    def test_first_release_adds_reviewed_project_and_embedded_art(self):
        value=copy.deepcopy(self.base)
        ids={'session-timer','pocket-artillery'}
        value['catalogue']['projects']=[p for p in value['catalogue']['projects'] if p['id'] not in ids]
        for crate,members in value['catalogue']['contents'].items():
            value['catalogue']['contents'][crate]=[id for id in members if id not in ids]
        value['catalogue']['projects'][0]['summary']='Retained live editorial change'
        records=[{'id':id} for id in sorted(ids)]
        add_project_presentations(value,records,self.base['catalogue'],ROOT)
        c.validate(value)
        self.assertEqual(value['catalogue']['projects'][0]['summary'],'Retained live editorial change')
        remaining=[id for id in self.base['catalogue']['contents']['workshop'] if id not in ids]
        self.assertEqual(value['catalogue']['contents']['workshop'],['session-timer','pocket-artillery']+remaining)
        self.assertEqual(len(value['assets']),2)
        self.assertEqual(value['packages'],self.base['packages'])
        for p in value['catalogue']['projects']:
            if p['id'] in ids:
                self.assertTrue(p['hero'].startswith('asset:'))
                self.assertIn(p['hero'][6:],value['assets'])
        before=copy.deepcopy(value)
        add_project_presentations(value,records,self.base['catalogue'],ROOT)
        self.assertEqual(value,before)
        with self.assertRaises(ValueError):
            add_project_presentations(value,[{'id':'unreviewed'}],self.base['catalogue'],ROOT)

    def next(self):
        value=copy.deepcopy(self.base);value['revision']=2
        value['catalogue']['projects'][0]['summary']='A revised description'
        value['catalogue']['projects'][1]['summary']='A second updated project'
        value['crates'][2:4]=reversed(value['crates'][2:4])
        value['packages'][0]['withdrawn']=True
        value['catalogue']['contents']['whats-new'].reverse()
        return value

    def test_delta_two_projects_reorder_and_withdrawal(self):
        before,after=self.signed(self.base),self.signed(self.next())
        change=c.delta(before,after)
        self.assertEqual(len(change['patch']['projects']['upsert']),2)
        self.assertLess(len(c.canonical(change)),len(c.canonical(after)))
        self.assertEqual(c.apply(change,before,self.pub),after)
        unchanged=c.delta(after,after)
        self.assertLess(len(c.canonical(unchanged)),130)
        self.assertEqual(c.apply(unchanged,after,self.pub),after)

    def test_corrupt_signature_conflict_and_rollback_rejected(self):
        before,after=self.signed(self.base),self.signed(self.next())
        change=c.delta(before,after);change['patch']['projects']['upsert'][0]['title']='Tampered'
        with self.assertRaises(ValueError):c.apply(change,before,self.pub)
        change=c.delta(before,after);change['baseDigest']='0'*64
        with self.assertRaises(ValueError):c.apply(change,before,self.pub)
        with self.assertRaises(ValueError):c.apply({'kind':'snapshot','envelope':before},after,self.pub)
        conflict=copy.deepcopy(self.base);conflict['crates'][0]['title']='Conflict'
        with self.assertRaises(ValueError):c.apply({'kind':'snapshot','envelope':self.signed(conflict)},before,self.pub)

    def test_full_snapshot_unknown_cursor_and_unchanged(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d);before,after=self.signed(self.base),self.signed(self.next())
            c.atomic(path/'1.json',before);c.atomic(path/'current.json',after)
            self.assertEqual(server.response(path,'1',c.digest(self.base))['kind'],'delta')
            self.assertEqual(server.response(path,'2',c.digest(after['signed']))['kind'],'unchanged')
            self.assertEqual(server.response(path,'999','0'*64)['kind'],'snapshot')
            self.assertEqual(server.response(path,'../../private','')['kind'],'snapshot')

    def test_no_executable_or_foreign_metadata(self):
        for change in ('command','url','path','duplicate','image'):
            bad=copy.deepcopy(self.base)
            if change=='command':bad['packages'][0]['install']='rm -rf /'
            if change=='url':bad['packages'][0]['url']='https://github.com.evil.test/archive'
            if change=='path':bad['packages'][0]['entry']='../../evil'
            if change=='duplicate':bad['catalogue']['projects'].append(bad['catalogue']['projects'][0])
            if change=='image':bad['catalogue']['projects'][0]['hero']='file:///etc/passwd'
            with self.subTest(change=change),self.assertRaises(ValueError):c.verified(self.signed(bad),self.pub)
        with self.assertRaises(ValueError):c.decode(b'{"schema":1,"schema":2}')

    def test_publication_cannot_replace_bytes_of_released_version(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);c.atomic(p/'old.json',self.signed(self.base))
            bad=self.next();bad['packages'][0]['sha256']='0'*64;c.atomic(p/'input.json',bad)
            with self.assertRaises(ValueError):publisher.publish(p/'input.json',self.key,p/'output.json',p/'old.json',self.pub)
            self.assertFalse((p/'output.json').exists())

    def test_same_version_has_independent_architecture_builds(self):
        value=copy.deepcopy(self.base);value['revision']=2
        arm=copy.deepcopy(value['packages'][0]);arm['architecture']='aarch64'
        desktop=dict(arm,architecture='x86_64',sha256='1'*64)
        value['packages']=[desktop,arm]+value['packages'][1:]
        c.validate(value)
        for arch,expected in [('aarch64',arm),('x86_64',desktop)]:
            self.assertEqual(select_release(value['packages'],arm['id'],arm['version'],arch),expected)
        with self.assertRaises(ValueError):select_release([desktop],arm['id'],arm['version'],'aarch64')
        with self.assertRaises(ValueError):select_release([dict(arm,withdrawn=True)],arm['id'],arm['version'],'aarch64')
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);c.atomic(p/'old.json',self.signed(self.base));c.atomic(p/'input.json',value)
            publisher.publish(p/'input.json',self.key,p/'output.json',p/'old.json',self.pub)
            before=c.load(p/'output.json',self.pub)
            self.assertEqual(c.apply(c.delta(self.signed(self.base),before),self.signed(self.base),self.pub),before)
            for index in (0,1):
                bad=copy.deepcopy(value);bad['revision']=3;bad['packages'][index]['sha256']='2'*64
                c.atomic(p/'input.json',bad)
                with self.assertRaises(ValueError):publisher.publish(p/'input.json',self.key,p/'next.json',p/'output.json',self.pub)
        value['packages'].append(arm)
        with self.assertRaises(ValueError):c.validate(value)

    def test_bounded_hashed_artwork(self):
        data=(ROOT/'assets/crates/examples/2048.png').read_bytes();key=c.hashlib.sha256(data).hexdigest()
        value=copy.deepcopy(self.base);value['assets'][key]=base64.b64encode(data).decode();value['catalogue']['projects'][0]['hero']='asset:'+key
        envelope=self.signed(value);c.verified(envelope,self.pub)
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);c.materialize(p,envelope)
            self.assertEqual((p/'assets'/(key+'.png')).read_bytes(),data)
            self.assertEqual(c.load(p/'current.json',self.pub),envelope)
        value['assets'][key]=base64.b64encode(data+b'tamper').decode()
        with self.assertRaises(ValueError):c.verified(self.signed(value),self.pub)

    @unittest.skipUnless(sys.platform=='linux','Production transaction locks use flock')
    def test_one_request_atomic_offline_and_concurrent_entry(self):
        import fcntl
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);calls=[]
            def fetch(query):calls.append(query);return {'kind':'snapshot','envelope':self.signed(self.base)}
            c.sync(p,public_key=self.pub,fetch=fetch);self.assertEqual(len(calls),1)
            original=(p/'current.json').read_bytes()
            def offline(query):calls.append(query);raise OSError('Offline')
            with self.assertRaises(OSError):c.sync(p,public_key=self.pub,fetch=offline)
            self.assertEqual((p/'current.json').read_bytes(),original)
            with (p/'sync.lock').open('a') as lock:
                fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
                with self.assertRaises(BlockingIOError):c.sync(p,public_key=self.pub,fetch=fetch)
                self.assertEqual(len(calls),2)
            (p/'current.json').write_text('{broken')
            c.sync(p,public_key=self.pub,fetch=fetch);self.assertIn('revision=0',calls[-1])


if __name__=='__main__':unittest.main()
