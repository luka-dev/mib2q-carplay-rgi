import json
import unittest
from test_audit_amp_b_config_control import OUT,IMAGE
from audit_amp_b_indexed_control import indexed_control,PROOFS,CALLER_PROOFS
from audit_amp_output_bindings import loader_dm
from extract_amp_sharc_app import parse_blocks
from inspect_amp_visa import load_shortwords


@unittest.skipUnless(IMAGE.exists(),'fixed local images required')
class BIndexedProvenanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=(OUT.parent/'J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        cls.mem=load_shortwords(raw);cls.dm=loader_dm(raw,parse_blocks(raw));cls.data=IMAGE.read_bytes()

    def test_report_and_direct_control_overlap(self):
        r=indexed_control(self.data,self.dm,self.mem)
        self.assertEqual(r,json.loads((OUT/'b_indexed_control.json').read_text()))
        overlap=r['canonical_writes']['0x14000000|index']['overlap']
        self.assertEqual([x['object_offset'] for x in overlap],list(range(1128,1142)))
        self.assertEqual(overlap[0]['target_address_in_normal_init'],'0xb8cfd')

    def test_all_code_proofs_and_caller_fail_closed(self):
        for pc in [a for _,a,_,_ in PROOFS]+[a for a,_,_ in CALLER_PROOFS]+[0x12DE66,0x131CD1]:
            mem=self.mem.copy();mem[pc]^=1
            with self.assertRaises(ValueError):indexed_control(self.data,self.dm,mem)

    def test_wrong_image_or_dispatch_table_rejected(self):
        with self.assertRaises(ValueError):indexed_control(self.data[:-1],self.dm,self.mem)
        dm=self.dm.copy();dm[0xB912F]^=1
        with self.assertRaises(ValueError):indexed_control(self.data,dm,self.mem)


if __name__=='__main__':unittest.main()
