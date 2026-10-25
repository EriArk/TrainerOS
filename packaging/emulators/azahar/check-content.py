#!/usr/bin/env python3
"""Real content promotion/rollback with a native metadata/install fixture.
Run on Linux. The separate native checker exercises actual CIA/ZCIA readers.
"""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('azahar_content',Path(__file__).with_name('content.py'))
content=importlib.util.module_from_spec(spec);spec.loader.exec_module(content)
TITLE='0004000000001234'


class ContentTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.nand=self.root/'personal-nand';self.sdmc=self.root/'personal-sdmc'
        self.source=self.root/'input.cia';self.source.write_bytes(b'owned package')
        self.request={'source':str(self.source),'runtime':'/unused/azahar','nand':str(self.nand),
                      'sdmc':str(self.sdmc),'state':str(self.root/'state')}
        self.target=self.title(self.sdmc,TITLE)/'content';self.target.mkdir(parents=True)
        (self.target/'00000000.app').write_bytes(b'old game')
        self.save=self.target.parent/'data/00000001/save.bin';self.save.parent.mkdir(parents=True)
        self.save.write_bytes(b'personal save sentinel')
        self.calls=0
        self.native=patch.object(content,'native',side_effect=self.fake);self.native.start();self.addCleanup(self.native.stop)
        self.guard=patch.object(content,'no_game');self.guard.start();self.addCleanup(self.guard.stop)

    @staticmethod
    def title(sdmc,title):return sdmc/'Nintendo 3DS'/('0'*32)/('0'*32)/'title'/title[:8]/title[8:]

    def fake(self,runtime,source,user,nand,sdmc,install=False):
        title=TITLE if source.name!='update.cia' else '0004000e00001234'
        target=self.title(sdmc,title)/'content';tickets=nand/'dbs/ticket.db'
        if install:
            self.calls+=1;target.mkdir(parents=True)
            (target/'00000000.app').write_bytes(source.read_bytes()+b' installed')
            tickets.mkdir(parents=True)
            (tickets/(title.upper()+'.0000000000000001.tik')).write_bytes(b'own ticket')
        return {'protocol':1,'title':title,'version':0,'package':True,'executable':title==TITLE,
                'extras':[str(self.title(sdmc,k+TITLE[8:])/'content') for k in ('0004000e','0004008c')] if title==TITLE else [],
                'contentRoot':str(target),'entry':str(target/'00000000.app'),'tickets':str(tickets)}

    def tearDown(self):self.assertEqual(self.save.read_bytes(),b'personal save sentinel')

    def test_inspection_has_no_install_or_personal_writes(self):
        before=list(self.root.rglob('*'));value=content.inspect(self.request)
        self.assertTrue(value['executable']);self.assertEqual(self.calls,0)
        self.assertEqual(list(self.root.rglob('*')),before)

    def test_library_source_symlink_remains_supported(self):
        alias=self.root/'linked.cia';alias.symlink_to(self.source)
        linked=dict(self.request,source=str(alias))
        self.assertEqual(content.inspect(linked)['fingerprint'],content.inspect(self.request)['fingerprint'])
        content.install(linked);self.assertTrue(alias.is_symlink())
        self.assertEqual(self.source.read_bytes(),b'owned package')

    def test_update_and_dlc_changes_change_base_identity(self):
        before=content.inspect(self.request)
        update=self.title(self.sdmc,'0004000e00001234')/'content';update.mkdir(parents=True)
        (update/'00000000.app').write_bytes(b'update v1')
        after=content.inspect(self.request);self.assertNotEqual(before['fingerprint'],after['fingerprint'])
        self.assertIn(str(update),before['observed'])
        (update/'00000000.app').write_bytes(b'update v2')
        self.assertNotEqual(after['fingerprint'],content.inspect(self.request)['fingerprint'])

    def test_install_backup_index_and_idempotence(self):
        before=content.inspect(self.request);self.request['expected']=before['fingerprint']
        value=content.install(self.request)
        self.assertEqual(value['fingerprint'],before['fingerprint'])
        self.assertEqual((self.target/'00000000.app').read_bytes(),b'owned package installed')
        self.assertTrue(any(p.is_dir() and (p/'00000000.app').read_bytes()==b'old game' for p in self.target.parent.glob('.traineros-before-*')))
        receipt=json.loads((self.root/'state'/f'{TITLE}.json').read_text())
        self.assertEqual(receipt['entry'],str(self.target/'00000000.app'))
        content.install(self.request);self.assertEqual(self.calls,1)
        self.assertFalse((self.root/'state/install-journal.json').exists())

    def test_update_install_preserves_base_and_saves(self):
        source=self.root/'update.cia';source.write_bytes(b'new update');self.request['source']=str(source)
        result=content.install(self.request);self.assertFalse(result['executable'])
        self.assertEqual((self.target/'00000000.app').read_bytes(),b'old game')
        self.assertEqual((self.title(self.sdmc,'0004000e00001234')/'content/00000000.app').read_bytes(),b'new update installed')

    def test_failure_after_first_promote_restores_content(self):
        rename=Path.rename
        def fail(path,target):
            if path.name.startswith('.traineros-next-') and str(target).endswith('.tik'):
                raise OSError('simulated filesystem failure')
            return rename(path,target)
        with patch.object(Path,'rename',fail),self.assertRaises(OSError):content.install(self.request)
        self.assertEqual((self.target/'00000000.app').read_bytes(),b'old game')
        self.assertFalse(list(self.nand.rglob('*.tik')))
        content.install(self.request);self.assertEqual(self.calls,2)

    def test_source_change_during_install_keeps_old_content(self):
        native=self.fake
        def change(*args,**kwargs):
            value=native(*args,**kwargs)
            if args[-1] is True:self.source.write_bytes(b'replaced package')
            return value
        with patch.object(content,'native',side_effect=change),self.assertRaises(ValueError):content.install(self.request)
        self.assertEqual((self.target/'00000000.app').read_bytes(),b'old game')

    def test_stale_expected_identity_never_installs(self):
        self.request['expected']='0'*64
        with self.assertRaises(ValueError):content.install(self.request)
        self.assertEqual(self.calls,0)

    def test_symlink_dependency_is_rejected(self):
        update=self.title(self.sdmc,'0004000e00001234');update.mkdir(parents=True)
        (update/'content').symlink_to(self.target,target_is_directory=True)
        with self.assertRaises(ValueError):content.inspect(self.request)

    def test_interrupted_rename_recovered_before_retry(self):
        state=self.root/'state';state.mkdir()
        pending=self.target.with_name('.traineros-next-fixture');backup=self.target.with_name('.traineros-before-fixture')
        self.target.rename(backup);pending.mkdir();(pending/'broken.app').write_bytes(b'partial')
        content.atomic(state/'install-journal.json',{'moves':[{'target':str(self.target),'pending':str(pending),
                      'backup':str(backup),'existed':True}]})
        content.recover(state/'install-journal.json',(self.nand,self.sdmc,state))
        self.assertEqual((self.target/'00000000.app').read_bytes(),b'old game')
        self.assertFalse(pending.exists());self.assertFalse(backup.exists())

    def test_external_change_after_interruption_is_not_overwritten(self):
        state=self.root/'state';state.mkdir()
        pending=self.target.with_name('.traineros-next-fixture');backup=self.target.with_name('.traineros-before-fixture')
        self.target.rename(backup);self.target.mkdir();(self.target/'00000000.app').write_bytes(b'newer external content')
        content.atomic(state/'install-journal.json',{'moves':[{'target':str(self.target),'pending':str(pending),
                      'backup':str(backup),'existed':True,'publishedHash':'different'}]})
        with self.assertRaises(ValueError):content.recover(state/'install-journal.json',(self.nand,self.sdmc,state))
        self.assertEqual((self.target/'00000000.app').read_bytes(),b'newer external content')
        self.assertEqual((backup/'00000000.app').read_bytes(),b'old game')
        self.assertTrue((state/'install-journal.json').exists())


if __name__=='__main__':unittest.main()
