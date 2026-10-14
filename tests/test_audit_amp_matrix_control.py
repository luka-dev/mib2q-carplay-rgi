"""Image-bound matrix and pair coefficient provenance."""
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_amp_matrix_control import matrix_control
from audit_amp_output_bindings import loader_dm
from extract_amp_sharc_app import parse_blocks
from inspect_amp_visa import load_shortwords
import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
IMAGE=Path(_WS + '/Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin')
OUT=ROOT/'output/r2/soundamp/remaining-blocks-20260919'


@unittest.skipUnless(IMAGE.exists(),'fixed image required')
class MatrixAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=(ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        cls.data=IMAGE.read_bytes();cls.mem=load_shortwords(raw);cls.dm=loader_dm(raw,parse_blocks(raw))

    def test_export_reproduces(self):
        self.assertEqual(matrix_control(self.data,self.dm,self.mem),json.loads((OUT/'matrix_control.json').read_text()))

    def test_tables_zero_zero_signed_nine(self):
        r=matrix_control(self.data,self.dm,self.mem)
        self.assertEqual(r['tables']['pair_odd']['tenths_db'],[0]*19)
        self.assertEqual(r['tables']['matrix_input']['tenths_db'],[0]*19)
        self.assertEqual(r['tables']['matrix_output']['tenths_db'],list(range(-90,91,10)))
        self.assertEqual(r['tables']['matrix_output']['file_offset'],'0x102cc4')

    def test_hash_and_descriptor_mutations_fail_closed(self):
        for pc in (0x12694C,0x126A02,0x126A04,0x126A10):
            mem=self.mem.copy();mem[pc]^=1
            with self.assertRaises(ValueError):matrix_control(self.data,self.dm,mem)
        dm=self.dm.copy();dm[0xC4AED+9]+=1
        with self.assertRaises(ValueError):matrix_control(self.data,dm,self.mem)
        with self.assertRaises(ValueError):matrix_control(self.data[:-1],self.dm,self.mem)

    def test_lookup_zero_floor_and_profiles(self):
        r=matrix_control(self.data,self.dm,self.mem);g=r['gain_lookup']['linear']
        self.assertEqual(g[0],0);self.assertAlmostEqual(g[10],10**(-99/20),places=11)
        self.assertEqual(g[1000],1)
        self.assertEqual(r['pair_rows']['0'][1][12],-6)
        self.assertEqual(r['pair_rows']['1'][1][12],-4)


if __name__=='__main__':unittest.main()
