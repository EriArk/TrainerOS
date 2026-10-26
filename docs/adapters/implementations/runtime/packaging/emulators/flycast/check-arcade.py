#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real archive/dependency/staging checks; no commercial game data."""
import ctypes as C
import ctypes.util
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
import zlib

spec=importlib.util.spec_from_file_location('arcade',Path(__file__).with_name('arcade.py'))
arcade=importlib.util.module_from_spec(spec);spec.loader.exec_module(arcade)


def zipped(path,entries):
    path.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as f:
        for name,content in entries.items():f.writestr(name,content)


def seven(path,entries):
    lib=C.CDLL(ctypes.util.find_library('archive'))
    for name,result,args in [
        ('archive_write_new',C.c_void_p,[]),('archive_write_set_format_7zip',C.c_int,[C.c_void_p]),
        ('archive_write_open_filename',C.c_int,[C.c_void_p,C.c_char_p]),
        ('archive_entry_new',C.c_void_p,[]),('archive_entry_set_pathname',None,[C.c_void_p,C.c_char_p]),
        ('archive_entry_set_size',None,[C.c_void_p,C.c_int64]),('archive_entry_set_filetype',None,[C.c_void_p,C.c_uint]),
        ('archive_entry_set_perm',None,[C.c_void_p,C.c_int]),('archive_write_header',C.c_int,[C.c_void_p,C.c_void_p]),
        ('archive_write_data',C.c_ssize_t,[C.c_void_p,C.c_void_p,C.c_size_t]),
        ('archive_entry_free',None,[C.c_void_p]),('archive_write_close',C.c_int,[C.c_void_p]),
        ('archive_write_free',C.c_int,[C.c_void_p])]:
        f=getattr(lib,name);f.restype=result;f.argtypes=args
    handle=lib.archive_write_new()
    try:
        assert lib.archive_write_set_format_7zip(handle)==0
        assert lib.archive_write_open_filename(handle,str(path).encode())==0
        for name,content in entries.items():
            entry=lib.archive_entry_new()
            try:
                lib.archive_entry_set_pathname(entry,name.encode());lib.archive_entry_set_size(entry,len(content))
                lib.archive_entry_set_filetype(entry,0o100000);lib.archive_entry_set_perm(entry,0o600)
                assert lib.archive_write_header(handle,entry)==0
                assert lib.archive_write_data(handle,content,len(content))==len(content)
            finally:lib.archive_entry_free(entry)
        assert lib.archive_write_close(handle)==0
    finally:lib.archive_write_free(handle)


def fixture(root):
    root.mkdir(parents=True,exist_ok=True)
    (root/'firmware').mkdir(exist_ok=True)
    blob={'name':'rom.bin','length':64,'crc':0,'kind':'Normal'}
    bios=dict(blob,name='bios.bin',region=1,length=32)
    games={p:{'platform':p,'parent':None,'bios':'testbios','disc':None,'blobs':[blob]}
           for p in ('naomi','naomi2','atomiswave')}
    catalog={'protocol':1,'source':arcade.SOURCE,'games':games,'bios':{'testbios':[bios]}}
    (root/'arcade-catalog.json').write_text(json.dumps(catalog))
    for name in games:zipped(root/(name+'.zip'),{'rom.bin':b'r'*64})
    zipped(root/'firmware/testbios.zip',{'bios.bin':b'b'*32})
    return catalog


class Tests(unittest.TestCase):
    def setUp(self):
        t=tempfile.TemporaryDirectory();self.addCleanup(t.cleanup);self.root=Path(t.name)
        self.catalog=fixture(self.root)
        self.request={'source':str(self.root/'naomi.zip'),'platform':'naomi','bundle':str(self.root)}
    def save(self):
        (self.root/'arcade-catalog.json').write_text(json.dumps(self.catalog))
    def test_all_three_platforms_and_region(self):
        for p in ('naomi','naomi2','atomiswave'):
            v=arcade.inspect(dict(self.request,platform=p,source=str(self.root/(p+'.zip'))))
            self.assertEqual(v['platform'],p);self.assertEqual(v['region'],1)
    def test_real_seven_zip(self):
        path=self.root/'naomi.7z';seven(path,{'rom.bin':b'r'*64})
        self.assertEqual(arcade.archive(path)[0]['crc'],zlib.crc32(b'r'*64))
        self.assertTrue(arcade.inspect(dict(self.request,source=str(path)))['content'])
        self.assertEqual(arcade.inspect(self.request)['content'],arcade.inspect(dict(self.request,source=str(path)))['content'])
    def test_wrong_platform_and_unknown_set(self):
        with self.assertRaises(arcade.Invalid):arcade.inspect(dict(self.request,platform='atomiswave'))
        with self.assertRaises(arcade.Invalid):arcade.inspect(dict(self.request,source=str(self.root/'unknown.zip')))
    def test_parent_and_merged_sets(self):
        self.catalog['games']['naomi']['parent']='parent';self.save()
        self.assertTrue(arcade.inspect(self.request)['content']) # Merged, parent unnecessary.
        zipped(self.root/'naomi.zip',{'extra':b'x'})
        with self.assertRaises(arcade.Invalid):arcade.inspect(self.request)
        zipped(self.root/'parent.zip',{'rom.bin':b'r'*64})
        v=arcade.inspect(self.request);self.assertIn(str(self.root/'parent.zip'),v['observed'])
        self.assertIn(str(self.root/'parent.7z'),v['observed'])
    def test_crc_lookup_before_filename(self):
        self.catalog['games']['naomi']['blobs'][0]['crc']=zlib.crc32(b'r'*64);self.save()
        zipped(self.root/'naomi.zip',{'renamed.bin':b'r'*64})
        self.assertTrue(arcade.inspect(self.request)['content'])
    def test_optional_eeprom_and_required_key(self):
        extra={'name':'optional.bin','length':8,'crc':0,'kind':'Eeprom'}
        self.catalog['games']['naomi']['blobs'].append(extra);self.save()
        self.assertTrue(arcade.inspect(self.request)['content'])
        extra['kind']='Key';self.save()
        with self.assertRaises(arcade.Invalid):arcade.inspect(self.request)
    def test_missing_bios_observations_and_recovery(self):
        (self.root/'firmware/testbios.zip').unlink()
        with self.assertRaises(arcade.Invalid) as error:arcade.inspect(self.request)
        self.assertIn(str(self.root/'firmware/testbios.zip'),error.exception.args[1])
        zipped(self.root/'firmware/testbios.zip',{'bios.bin':b'b'*32})
        self.assertTrue(arcade.inspect(self.request)['content'])
    def test_bios_and_rom_changes_change_identity(self):
        before=arcade.inspect(self.request)['content']
        zipped(self.root/'firmware/testbios.zip',{'bios.bin':b'c'*32})
        after=arcade.inspect(self.request)['content'];self.assertNotEqual(before,after)
        zipped(self.root/'naomi.zip',{'rom.bin':b's'*64})
        self.assertNotEqual(after,arcade.inspect(self.request)['content'])
    def test_unrelated_bios_members_do_not_split_compatible_peers(self):
        before=arcade.inspect(self.request)['content']
        zipped(self.root/'firmware/testbios.zip',{'bios.bin':b'b'*32,'other-console.bin':b'other'})
        self.assertEqual(before,arcade.inspect(self.request)['content'])
    def test_short_and_unsafe_entries_rejected(self):
        for entries in ({'rom.bin':b'r'}, {'rom.bin':b'r'*64,'../escape':b'x'}):
            zipped(self.root/'naomi.zip',entries)
            with self.assertRaises(arcade.Invalid):arcade.inspect(self.request)
    def test_duplicate_entries_rejected(self):
        with zipfile.ZipFile(self.root/'naomi.zip','w') as f:
            f.writestr('rom.bin',b'r'*64);f.writestr('rom.bin',b'r'*64)
        with self.assertRaises(arcade.Invalid):arcade.inspect(self.request)
    def test_gdrom_dependency_and_parent_layout(self):
        game=self.catalog['games']['naomi'];game['disc']='gdl-test';game['parent']='parent';self.save()
        with self.assertRaises(arcade.Invalid):arcade.inspect(self.request)
        folder=self.root/'parent';folder.mkdir()
        (folder/'gdl-test.chd').write_bytes(bytes.fromhex('4d436f6d707248440000007c00000005')+b'\0'*108)
        v=arcade.inspect(self.request);self.assertIn(str(folder/'gdl-test.chd'),v['observed'])
        (folder/'gdl-test.chd').write_bytes(b'invalid')
        with self.assertRaises(arcade.Invalid):arcade.inspect(self.request)
    def test_private_stage_and_stale_rejection(self):
        value=arcade.inspect(self.request);stage=self.root/'stage';stage.mkdir()
        (self.root/'naomi.eeprom').write_bytes(b'personal')
        with self.assertRaises(arcade.Invalid):arcade.prepare(dict(self.request,stage=str(stage),expected='stale'))
        result=arcade.prepare(dict(self.request,stage=str(stage),expected=value['content']))
        self.assertTrue(Path(result['entry']).is_file());self.assertFalse(list(stage.rglob('*.eeprom')))
        (stage/'data/naomi.eeprom').write_bytes(b'private progress')
        self.assertEqual((self.root/'naomi.eeprom').read_bytes(),b'personal')
        with self.assertRaises(arcade.Invalid):arcade.prepare(dict(self.request,stage=str(stage),expected=value['content']))


if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--fixture':fixture(Path(sys.argv[2]))
    else:unittest.main()
