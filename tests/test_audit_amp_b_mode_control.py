import json
import unittest
from test_audit_amp_b_config_control import ROOT,OUT,IMAGE
from audit_amp_b_mode_control import mode_control,PROOFS,CALLER_PROOFS,pointer_literal_census
from audit_amp_output_bindings import loader_dm
from extract_amp_sharc_app import parse_blocks
from inspect_amp_visa import load_shortwords


@unittest.skipUnless(IMAGE.exists(),'fixed CodeFlash required')
class BModeProvenanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=(OUT.parent/'J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin').read_bytes()
        cls.dm=loader_dm(raw,parse_blocks(raw));cls.mem=load_shortwords(raw);cls.data=IMAGE.read_bytes()
        cls.report=json.loads((OUT/'b_mode_control.json').read_text())

    def test_generated_report_and_pointer_domains(self):
        self.assertEqual(mode_control(self.data,self.dm,self.mem),self.report)
        maps=self.report['pointer_maps']
        self.assertEqual(set(map(int,maps['16:5'])),set(range(1156,1166))|set(range(1171,1174)))
        self.assertEqual(set(map(int,maps['20:3'])),set(range(1156,1171)))

    def test_each_code_proof_fails_closed(self):
        for a,_,_ in [(a,b,h) for _,a,b,h in PROOFS]+CALLER_PROOFS:
            mem=self.mem.copy();mem[a]^=1
            with self.assertRaises(ValueError):mode_control(self.data,self.dm,mem)

    def test_image_and_jump_table_mutation_rejected(self):
        with self.assertRaises(ValueError):mode_control(self.data[:-1],self.dm,self.mem)
        dm=self.dm.copy();dm[0xB914B+4]^=1
        with self.assertRaises(ValueError):mode_control(self.data,dm,self.mem)

    def test_consumer_pin_and_auto_sources(self):
        mem=self.mem.copy();mem[0x12E6A7]^=1
        with self.assertRaises(ValueError):mode_control(self.data,self.dm,mem)
        self.assertEqual([b['config487'] for b in self.report['sources']['loader']],[3]*4)
        self.assertEqual([b['config487'] for b in self.report['sources']['b01']],[0]*4)
        self.assertEqual(self.report['api20_auto']['reads_effective_object_offset'],786)

    def test_crossfeed_base_chain_and_remaining_boundary(self):
        self.assertEqual(self.report['consumers']['1161'],
            {'pc':'0x12e683','role':'post_crossfeed','words':1})
        self.assertEqual(self.report['unbound_pointer_consumers'],[1162,1163,1164,1165])
        self.assertEqual(self.report['post_crossfeed']['config_offsets'],
            {'default':31,'16:5':493,'20:3':426})
        from inspect_amp_visa import decode
        for pc,insn in [(0x1315AE,'R0=0x3b'),
                       (0x1315B0,'R8=R11+R0 || DM(I7,M7)=R6'),
                       (0x12E841,'R5=R3+R4 || DM(-2,I6)=R8'),
                       (0x12EA39,'R2=DM(-2,I6)'),
                       (0x12EA3B,'R8=PASS R2 || DM(I7,M7)=R11'),
                       (0x12E556,'R3=R8'),
                       (0x12E672,'R8=PASS R15 || I5=R3'),
                       (0x12E48E,'R13=DM(M5,I4)')]:
            self.assertEqual(decode(self.mem,pc)[1],insn)

    def test_aligned_literal_census_and_injected_read(self):
        c=self.report['pointer_literal_census']
        self.assertEqual(c['instructions'],16093);self.assertEqual(c['unknown'],0)
        self.assertEqual(len(c['immediate_offset_reads']),14)
        self.assertTrue(set([1162,1163,1164,1165,1103,1104,1105,1106])<=set(c['not_found_offsets']))
        mem=self.mem.copy();mem[0x12E685]=1103
        changed=pointer_literal_census(mem)
        self.assertNotIn(1103,changed['not_found_offsets'])
        self.assertIn({'pc':'0x12e683','instruction':'R4=DM(+1103,I5)'},changed['immediate_offset_reads'])
        with self.assertRaises(ValueError):mode_control(self.data,self.dm,mem)

    def test_all_b01_crossfeed_banks(self):
        rows=self.report['sources']['b01']
        for name,expected in [('16:default',[.25,.25,.25,.2]),('16:5',[.27,.27,.3,0]),
                              ('20:3',[.27,.28,.3,0])]:
            for bank,value in zip(rows,expected):
                self.assertAlmostEqual(bank['choices'][name]['1161'][0],value)


if __name__=='__main__':unittest.main()
