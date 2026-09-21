"""Source identity, CRC cross-checks and explicit four-bank provenance."""
import binascii
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_amp_b_config_control import config_control,config_crc,PROOFS
from audit_amp_b_gain_controls import gain_controls
from audit_amp_output_bindings import loader_dm
from extract_amp_sharc_app import parse_blocks
from inspect_amp_visa import load_shortwords
IMAGE=Path('/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin')
OUT=ROOT/'output/r2/soundamp/remaining-blocks-20260919'


@unittest.skipUnless(IMAGE.exists(),'fixed CodeFlash required')
class BConfigControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=(OUT.parent/'J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        cls.dm=loader_dm(raw,parse_blocks(raw));cls.mem=load_shortwords(raw);cls.data=IMAGE.read_bytes()
        cls.report=json.loads((OUT/'b_config_control.json').read_text())

    def test_export_matches_sources(self):
        self.assertEqual(config_control(self.data,self.dm,self.mem),self.report)

    def test_all_banks_crc_matches_independent_byte_implementation(self):
        for source in self.report['sources'].values():
            for bank in source['banks']:
                words=bank['words'];payload=struct.pack('<828I',*words[:-1])
                self.assertEqual(binascii.crc_hqx(payload,0),words[-1])
                self.assertEqual(config_crc(words[:-1],self.report['crc_table']),words[-1])
        self.assertEqual(config_crc([],self.report['crc_table']),0)

    def test_bank_differences_are_not_matrix_banks(self):
        self.assertEqual([len(b['different_offsets_from_bank1']) for b in self.report['sources']['loader']['banks']],[0,2,2,2])
        self.assertEqual([len(b['different_offsets_from_bank1']) for b in self.report['sources']['b01']['banks']],[0,31,53,271])
        self.assertEqual(self.report['api17']['accepted'],[1,2,3,4])
        self.assertEqual(self.report['v850_selector']['table'][0],self.report['v850_selector']['table'][1])

    def test_bindings_use_selected_bank_addresses(self):
        for i,b in enumerate(self.report['control_bindings_by_bank']):
            self.assertEqual(b['config_bank'],i+1)
            self.assertEqual(int(b['bases']['46']['b01_file_offset'],16),0x1203E0+4*(829*i+122))
            self.assertEqual(int(b['bases']['46']['loader_address'],16),0xC3D5F+829*i+122)
        for bad in (0,5,1.0):
            with self.assertRaises(ValueError):gain_controls(self.data,self.dm,self.mem,config_bank=bad)

    def test_corrupt_image_or_bank_fails_closed(self):
        with self.assertRaises(ValueError):config_control(self.data[:-1],self.dm,self.mem)
        dm=self.dm.copy();dm[0xC3D5F+829+35]^=1
        with self.assertRaises(ValueError):config_control(self.data,dm,self.mem)

    def test_every_config_code_proof_rejects_mutation(self):
        for name,a,_,_ in PROOFS:
            mem=self.mem.copy();mem[a]^=1
            with self.subTest(proof=name):
                with self.assertRaises(ValueError):config_control(self.data,self.dm,mem)

    def test_replay_offsets_order_and_retained_baseline(self):
        r=self.report['replay']
        self.assertEqual(r['api_order'],[19,18,22,21,23,24,25,27,29])
        self.assertEqual(r['effective_offsets'],[1131,1130,1134,1133,1135,1136,1137,1139,1141])
        for source in self.report['sources'].values():
            self.assertTrue(all(b['words'][791]==0 for b in source['banks']))

    def test_zero_eq_design_is_preserved_across_all_bank_images(self):
        for b in self.report['control_bindings_by_bank']:
            for eq in b['parameter_controls']['eq_rebuilds']:
                for source in ('loader','b01'):
                    self.assertEqual([v[source] for v in eq['coefficient_words']],[1,0,0,0,0])
                    self.assertEqual(eq['design_words'][2][source],0)


if __name__=='__main__':unittest.main()
