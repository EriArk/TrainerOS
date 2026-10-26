#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise full-catalogue admission using synthetic, real ISO9660 images."""
import csv
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from check_disc import entry
import launcher

ROOT=Path(__file__).resolve().parents[2]
PROFILES=json.loads(Path(__file__).with_name('profiles.json').read_text(encoding='utf-8'))


def disc_image(path,serial,variant=0):
    boot=serial[:4]+'_'+serial[5:8]+'.'+serial[8:]
    config=('BOOT2 = cdrom0:\\'+boot+';1\r\n').encode()
    elf=b'\x7fELF\x01\x01'+bytes(range(57))+bytes([variant])
    data=bytearray(32*2048);data[16*2048:16*2048+7]=b'\x01CD001\x01'
    struct.pack_into('<I',data,16*2048+158,20);struct.pack_into('<I',data,16*2048+166,2048)
    rows=entry('SYSTEM.CNF;1',21,len(config))+entry(boot+';1',22,len(elf))
    data[20*2048:20*2048+len(rows)]=rows;data[21*2048:21*2048+len(config)]=config
    data[22*2048:22*2048+len(elf)]=elf;path.write_bytes(data)


class CatalogueTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.path=Path(self.temp.name).resolve()/'arbitrary-name.iso'
        self.manifest=patch.object(launcher,'manifest',return_value={
            'runtimeSha256':'a'*64,'settings':'eclx1-namespace-own-cards-v1'})
        self.mock_manifest=self.manifest.start();self.addCleanup(self.manifest.stop)

    def test_all_owner_rows_covered_once(self):
        source=ROOT/PROFILES['catalogue']
        with source.open(encoding='utf-8-sig',newline='') as stream:rows=list(csv.reader(stream))[1:]
        self.assertEqual(PROFILES['version'],2)
        self.assertEqual(PROFILES['catalogueSha256'],hashlib.sha256(source.read_bytes()).hexdigest())
        self.assertEqual([(g['catalogueRow'],g['title']) for g in PROFILES['games']],[(int(r[0]),r[1]) for r in rows])
        serials=[r['serial'] for g in PROFILES['games'] for r in g['releases']]
        self.assertEqual(len(serials),len(set(serials)))
        self.assertEqual(len(PROFILES['games']),56)

    def test_every_catalogued_release_accepts_actual_disc_identity(self):
        for game in PROFILES['games']:
            self.assertEqual(game['availability'],'owner-test')
            self.assertTrue(game['releases'])
            for release in game['releases']:
                with self.subTest(title=game['title'],serial=release['serial']):
                    disc_image(self.path,release['serial'])
                    found=launcher.identify(self.path)
                    self.assertEqual(found['serial'],release['serial'])
                    self.assertEqual(found['content'],hashlib.sha256(self.path.read_bytes()).hexdigest())
                    self.assertEqual(found['id'],'runtime.armsx2.lan.v1')
                    self.assertEqual(found['players'],2)
                    self.assertEqual(found['runtime'],'a'*64)
                    self.assertFalse(found['lateJoin'])

    def test_unknown_disc_not_admitted_by_filename(self):
        path=self.path.with_name('Star Wars Battlefront II.iso')
        disc_image(path,'SLUS-99999')
        self.assertEqual(launcher.identify(path),{})
        self.mock_manifest.assert_not_called()

    def test_revisions_available_but_not_interchangeable_between_players(self):
        disc_image(self.path,'SLUS-21240',1);first=launcher.identify(self.path)
        disc_image(self.path,'SLUS-21240',2);second=launcher.identify(self.path)
        self.assertNotEqual(first['crc'],second['crc'])
        self.assertNotEqual(first['content'],second['content'])
        self.assertNotEqual(launcher.stable(first),launcher.stable(second))

    def test_known_region_constraints_preserved(self):
        restrictions={9:'PAL',20:'PAL',42:'NTSC-U',56:'NTSC-J'}
        for game in PROFILES['games']:
            region=restrictions.get(game['catalogueRow'])
            if region:self.assertTrue(all(r['region'].startswith(region) for r in game['releases']))

    def test_runtime_guard_still_applies_to_expanded_list(self):
        serial=PROFILES['games'][0]['releases'][0]['serial'];disc_image(self.path,serial)
        self.mock_manifest.side_effect=ValueError('Runtime replaced')
        with self.assertRaises(ValueError):launcher.identify(self.path)


if __name__=='__main__':unittest.main()
