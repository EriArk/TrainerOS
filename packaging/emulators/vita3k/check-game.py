#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Installed-title identity and owned-settings regression checks."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import yaml
import game


def metadata(title):
    keys=b'TITLE_ID\0';data=title.encode()+b'\0'
    return struct.pack('<5I',0x46535000,0x101,36,36+len(keys),1)+struct.pack('<HHIII',0,0x204,len(data),16,0)+keys+data


class InstalledGame(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.data=self.root/'owned';self.bundle=self.root/'bundle'
        (self.bundle/'transport').mkdir(parents=True)
        self.app=self.data/'ux0/app/QUAK00001';(self.app/'sce_sys').mkdir(parents=True)
        (self.app/'sce_sys/param.sfo').write_bytes(metadata('QUAK00001'))
        (self.app/'eboot.bin').write_bytes(b'owned homebrew code')
        self.link=self.root/'arbitrary display name.psvita';self.link.write_text('QUAK00001\n')
        self.extra=self.data/'ux0/data/Quake/id1';self.extra.mkdir(parents=True)
        (self.extra/'pak0.pak').write_bytes(b'owned game assets')
        profiles={'QUAK00001':{'ebootSha256':game.digest(self.app/'eboot.bin'),
            'nativeSettings':{'backend-renderer':'OpenGL'},'extraContent':[{'path':'ux0/data/Quake',
            'extensions':['.pak','.dat'],'required':['id1/pak0.pak']}]}}
        (self.bundle/'transport/profiles.json').write_text(json.dumps(profiles))
        self.settings={'pref-path':str(self.data),'backend-renderer':'Vulkan','user-id':'02','audio-volume':37}
        self.addCleanup(patch.stopall)
        patch.object(game,'bundle',return_value='a'*64).start()
        patch.object(game,'owned_settings',side_effect=lambda:(copy.deepcopy(self.settings),self.data)).start()

    def identity(self):return game.identity(self.bundle,self.link)

    def test_link_name_does_not_select_a_different_game(self):
        self.assertEqual(game.installed_app(self.link,self.data)[0],'QUAK00001')
        (self.app/'sce_sys/param.sfo').write_bytes(metadata('OTHER0001'))
        with self.assertRaises(ValueError):self.identity()

    def test_link_cannot_supply_arguments_or_a_path(self):
        for value in ['../QUAK00001','QUAK00001 -r OTHER0001','QUAK00001\nOTHER0001']:
            self.link.write_text(value)
            with self.subTest(value=value),self.assertRaises(ValueError):game.title_id(self.link)

    def test_ambiguous_or_out_of_bounds_metadata_is_rejected(self):
        valid=metadata('QUAK00001')
        for value in [valid[:18],valid[:16]+struct.pack('<I',129)+valid[20:],valid[:20]+struct.pack('<HHIII',0,0x204,10,16,65535)+valid[36:]]:
            (self.app/'sce_sys/param.sfo').write_bytes(value)
            with self.assertRaises(ValueError):game.sfo_title(self.app/'sce_sys/param.sfo')

    def test_external_game_assets_and_updates_change_identity(self):
        before=self.identity()['content']
        (self.extra/'progs.dat').write_bytes(b'changed game rules')
        self.assertNotEqual(self.identity()['content'],before)
        current=self.identity()['content']
        update=self.data/'ux0/patch/QUAK00001';update.mkdir(parents=True)
        (update/'eboot.bin').write_bytes(b'update')
        self.assertNotEqual(self.identity()['content'],current)

    def test_user_save_and_controls_are_not_compared_between_players(self):
        before=self.identity()
        (self.extra/'s0.sav').write_bytes(b'personal progress')
        (self.extra/'config.cfg').write_text('personal controls')
        ordinary=self.data/'ux0/user/02/savedata/QUAK00001';ordinary.mkdir(parents=True)
        (ordinary/'progress').write_bytes(b'more personal progress')
        self.assertEqual(before,self.identity())

    def test_private_settings_retain_own_progress_account_and_volume(self):
        user=self.root/'session/user'
        original=copy.deepcopy(self.settings)
        self.assertEqual(game.prepare_settings(self.bundle,self.link,user,network=True),'QUAK00001')
        value=yaml.safe_load((user/'config/Vita3K/config.yml').read_text())
        self.assertEqual(value['pref-path'],str(self.data));self.assertEqual(value['user-id'],'02')
        self.assertEqual(value['audio-volume'],37);self.assertEqual(value['backend-renderer'],'OpenGL')
        self.assertEqual(value['controller-binds'][5],-1);self.assertEqual(value['adhoc-addr'],0)
        self.assertEqual(self.settings,original)
        (self.app/'eboot.bin').write_bytes(b'unreviewed executable')
        with self.assertRaises(ValueError):self.identity()
        other=self.root/'ordinary/user';game.prepare_settings(self.bundle,self.link,other)
        self.assertEqual(yaml.safe_load((other/'config/Vita3K/config.yml').read_text())['backend-renderer'],'Vulkan')


if __name__=='__main__':unittest.main()
