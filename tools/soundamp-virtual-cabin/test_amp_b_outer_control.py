"""Isolated normal API18 coordinate, decoded clamp oracle; no request I/O."""
from pathlib import Path
import re
import unittest
from amp_b_control import api18_request_coordinate

OUT=Path(__file__).resolve().parents[2]/'output/r2/soundamp/remaining-blocks-20260919'


class API18SourceTests(unittest.TestCase):
    def test_decoded_signed_domain_and_clamp_all_bytes(self):
        code={}
        for line in (OUT/'outer_api18_request.asm').read_text().splitlines():
            m=re.match(r'0x([0-9a-f]+)\s+([0-9a-f]+)\s+(.*)',line)
            if m:code[int(m[1],16)]=(len(bytes.fromhex(m[2])),m[3].split(';')[0].strip())
        for raw in range(256):
            pc=0xCA6F2;r={'r0':0,'r12':raw};flag=0;stores={}
            val=lambda s:r[s] if s in r else int(s,0)
            for _ in range(40):
                if pc in (0xCA6FC,0xCA748):break
                n,op=code[pc];nextpc=pc+n;p=op.replace(',','').split()
                if p[0]=='sxb':r[p[1]]=(r[p[1]]+128)%256-128
                elif p[0]=='cmp':
                    a,b=val(p[2]),val(p[1]);flag=(a>b)-(a<b)
                elif p[0]=='mov':r[p[2]]=val(p[1])
                elif p[0]=='movea':r[p[3]]=val(p[1])+val(p[2])
                elif p[0]=='ld.b':
                    self.assertEqual(p[1],'-14973[gp]');r[p[2]]=0 # explicit old raw coordinate
                elif p[0]=='st.b':stores[p[2]]=val(p[1])
                elif p[0]=='br':nextpc=int(p[1],0)
                elif p[0] in ('bgt','bge','ble','be'):
                    if {'bgt':flag>0,'bge':flag>=0,'ble':flag<=0,'be':flag==0}[p[0]]:
                        nextpc=int(p[1],0)
                else:self.fail(f'unhandled {pc:x}: {op}')
                pc=nextpc
            signed=(raw+128)%256-128
            with self.subTest(raw=raw):
                if pc==0xCA6FC:
                    with self.assertRaises(ValueError):api18_request_coordinate(signed)
                else:
                    self.assertEqual(pc,0xCA748)
                    self.assertEqual(stores['-14973[gp]'],signed)
                    self.assertEqual(api18_request_coordinate(signed),stores['-15840[gp]'])

    def test_zero_no_offset_and_reject_noninteger(self):
        self.assertEqual(api18_request_coordinate(0),0.0)
        self.assertEqual(api18_request_coordinate(-12),-9.0)
        self.assertEqual(api18_request_coordinate(12),9.0)
        for value in (0.0,True,None,'0',13,-13):
            with self.assertRaises(ValueError):api18_request_coordinate(value)


if __name__=='__main__':unittest.main()
