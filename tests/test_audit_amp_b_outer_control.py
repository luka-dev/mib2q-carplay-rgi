"""Static sender provenance; no vehicle or protocol communication."""
import json
from pathlib import Path
import re
import struct
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_amp_b_outer_control import outer_control, function_operations, direct_lp_candidates
IMAGE=Path('/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/'
           'uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin')
OUT=ROOT/'output/r2/soundamp/remaining-blocks-20260919'


@unittest.skipUnless(IMAGE.exists(),'fixed CodeFlash required')
class OuterControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=IMAGE.read_bytes();cls.report=outer_control(cls.data)

    def test_report_reproduces(self):
        self.assertEqual(self.report,json.loads((OUT/'b_outer_control.json').read_text()))

    def test_fail_closed_on_changed_image(self):
        for address in (0xAA87A,0x96468,0xCA732,0xD58A4):
            changed=bytearray(self.data);changed[address]^=1
            with self.subTest(address=hex(address)):
                with self.assertRaises(ValueError):outer_control(changed)
        with self.assertRaises(ValueError):outer_control(self.data[:-1])

    def test_outer_tables_and_handlers(self):
        r=self.report
        self.assertEqual([(v['operation'],v['flags'],v['first'],v['second'])
            for v in r['main']['operations']],[(1,2,'0x0','0xd4180'),(2,208,'0xd40c8','0x0')])
        self.assertEqual([(v['operation'],v['first'],v['second'])
            for v in r['alternate']['operations']],[(0,'0xd4130','0x0'),(1,'0x0','0xd42dc')])
        self.assertEqual([(v['operation'],v['first']) for v in r['api18']['operations']],
                         [(0,'0xca4b4'),(1,'0x0'),(2,'0xca4b4')])
        self.assertEqual(struct.unpack_from('<III',self.data,0x4021C),
                         (0xF5,0x3FF29F7,0x965E4))
        self.assertEqual(struct.unpack_from('<III',self.data,0x40204),
                         (0x22,0x3FF29F5,0x95A24))
        self.assertIsNone(function_operations(self.data,0x965E4,0xFFF0))

    def test_direct_sender_census_is_bounded(self):
        r=self.report['sender_census']
        self.assertEqual(len(r['direct_lp_candidates']),17)
        self.assertEqual(r['direct_lp_candidates'][-3:],['0xaa886','0xac6de','0xac756'])
        self.assertEqual(direct_lp_candidates(self.data,0xD57C8),[0xA29C8,0xA29E2])
        self.assertEqual(direct_lp_candidates(self.data,0xAA644),[0xA77CA,0xA77D6])

    def test_decoded_event_routes(self):
        # Execute only the move/compare/branch selection tree, no callees.
        code={}
        for line in (OUT/'outer_event_dispatch.asm').read_text().splitlines():
            m=re.match(r'0x([0-9a-f]+)\s+([0-9a-f]+)\s+(.*)',line)
            if m:code[int(m[1],16)]=(len(bytes.fromhex(m[2])),m[3].split(';')[0].strip())
        targets={22:0xA77CA,368:0xA77D6,369:0xA782E,370:0xA7850,371:0xA7858}
        for event,target in targets.items():
            regs={'r0':0,'r13':event,'r29':event};pc=0xA795E;flag=0
            value=lambda s:regs[s] if s in regs else int(s,0)
            for _ in range(100):
                if pc in targets.values():break
                n,op=code[pc];nextpc=pc+n;parts=op.replace(',','').split()
                if parts[0]=='movea':regs[parts[3]]=value(parts[1])+value(parts[2])
                elif parts[0]=='cmp':
                    a,b=value(parts[2]),value(parts[1]);flag=(a>b)-(a<b)
                elif parts[0] in ('br','jr'):nextpc=int(parts[1],0)
                elif parts[0] in ('be','bne','bgt','blt','bge','ble'):
                    take={'be':flag==0,'bne':flag!=0,'bgt':flag>0,'blt':flag<0,
                          'bge':flag>=0,'ble':flag<=0}[parts[0]]
                    if take:nextpc=int(parts[1],0)
                else:self.fail(f'unhandled {pc:x}: {op}')
                pc=nextpc
            self.assertEqual(pc,target)

    def test_api18_direct_gp_store_census(self):
        # Byte-store gp-relative pattern only: does not exclude indirect stores.
        stores=[]
        for pc in range(0x80000,0xE0000,2):
            lo,hi=struct.unpack_from('<HH',self.data,pc)
            if hi==0xC220 and lo&0x7FF==0x744:stores.append(pc)
        self.assertEqual(stores,[0xB6DD0,0xCA732,0xCA73E,0xCA744])

    def test_source_clamp_and_raw_forwarding_pins(self):
        pins={0xAA86A:'048f20c2',0xAA87A:'20361200',0xAC748:'2606feaf0008',
              0xCA6F4:'6c62',0xCA6F8:'7462',0xCA72C:'7762',0xCA738:'6962',
              0xD4106:'20367101',0xD416A:'20367101',0xD4302:'20367201'}
        for pc,raw in pins.items():self.assertEqual(self.data[pc:pc+len(raw)//2].hex(),raw)


if __name__=='__main__':unittest.main()
