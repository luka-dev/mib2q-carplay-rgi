"""Bounded decoded V850 writers and SHARC indexing; no hardware emulation."""
import copy
import json
from pathlib import Path
import re
import struct
import unittest
import numpy as np
from amp_matrix_control import coefficient_targets, pair_row_step

OUT=Path(__file__).resolve().parents[2]/'assets/soundamp/remaining-blocks-20260919'


def decoded_writer(binding,matrix,current,level,processing,b01,state,asad,coordinate,entry):
    """Execute integer V850 producer instructions. Transport calls capture words.

    Callees have explicit stand-ins; no scheduler, SPI or concurrent mutations.
    Valid finite calibration domain only. RAM/ROM supplied, no file/device writes.
    """
    code={}
    for name in ('coefficient_matrix_writer','outer_api18_writer'):
        for line in (OUT/(name+'.asm')).read_text().splitlines():
            m=re.match(r'0x([0-9a-f]+)\s+([0-9a-f]+)\s+(.*)',line)
            if m:code[int(m[1],16)]=(len(bytes.fromhex(m[2])),m[3].split(';')[0].strip())
    gp=0x10000000;memory={};r={f'r{i}':0 for i in range(32)}
    r.update(gp=gp,sp=0x20000000,r20=255,r21=65535)
    def put(addr,value,width):
        for i in range(width):memory[addr+i]=(value>>(i*8))&255
    def get(addr,width,signed=False):
        raw=sum(memory[addr+i]<<(i*8) for i in range(width))
        return raw-(1<<(width*8)) if signed and raw>>(width*8-1) else raw
    for delta,val in [(-17771,processing),(-17756,b01),(-16783,level),(-16784,state),
                      (-15905,asad),(-15840,coordinate)]:put(gp+delta,val,1)
    for i,v in enumerate(sum(matrix,[])):put(gp-15904+i,v,1)
    for i,v in enumerate(current):put(gp-16804+i,v,1)
    for i,v in enumerate(sum(binding['pair_rows']['0'],[])):put(gp-0x135F8+i,v,1)
    for table in binding['tables'].values():
        for i,v in enumerate(table['tenths_db']):put(gp+table['ram_relative']+2*i,v,2)
    for i,v in enumerate(binding['gain_lookup']['linear']):
        put(0x69398-4*(i-1000),struct.unpack('<I',struct.pack('<f',v))[0],4)
    value=lambda s:r[s] if s in r else int(s,0)
    def addr(s):
        m=re.fullmatch(r'(-?\d+)\[(\w+)\]',s)
        return value(m[2])+int(m[1])
    pc=entry;flag=0;writes={}
    for _ in range(30000):
        n,op=code[pc];p=op.replace(',','').split();mn=p[0];nextpc=pc+n
        if mn=='dispose':break
        if mn=='mov':r[p[2]]=value(p[1])
        elif mn in ('movea','addi'):r[p[3]]=value(p[1])+value(p[2])
        elif mn=='movhi':r[p[3]]=(value(p[1])<<16)+value(p[2])
        elif mn=='add':r[p[2]]+=value(p[1])
        elif mn=='shl':r[p[2]]<<=value(p[1])
        elif mn=='and':r[p[2]]&=value(p[1])
        elif mn=='andi':r[p[3]]=value(p[1])&value(p[2])
        elif mn=='not':r[p[2]]=~value(p[1])
        elif mn=='mul':r[p[2]]*=value(p[1]) # low product; high destination r0 discarded
        elif mn in ('sxb','sxh'):
            bits=8 if mn=='sxb' else 16;r[p[1]]=(r[p[1]]+(1<<(bits-1)))%(1<<bits)-(1<<(bits-1))
        elif mn.startswith('ld.'):
            width={'ld.b':1,'ld.bu':1,'ld.h':2,'ld.w':4}[mn]
            r[p[2]]=get(addr(p[1]),width,mn in ('ld.b','ld.h'))
        elif mn.startswith('st.'):put(addr(p[2]),value(p[1]),{'st.b':1,'st.w':4}[mn])
        elif mn=='cmp':
            a,b=value(p[2]),value(p[1]);flag=(a>b)-(a<b)
        elif mn in ('br','jr'):nextpc=int(p[1],0)
        elif mn in ('be','bne','blt','ble','bge','bgt'):
            if {'be':flag==0,'bne':flag!=0,'blt':flag<0,'ble':flag<=0,'bge':flag>=0,'bgt':flag>0}[mn]:
                nextpc=int(p[1],0)
        elif mn=='jarl':
            target=int(p[1],0)
            if target==0xE34D4:
                base=gp-17216;count=get(base+12,4);dest=get(base+16,4)
                for i in range(count):writes[dest+i]=get(base+20+4*i,4)
            elif target==0xEBCA4:r['r6']=struct.unpack('<I',struct.pack('<f',r['r6']))[0]
            elif target not in (0xA8BB4,0xA8D04,0xA8C6C,0xAC648):raise AssertionError(op)
        else:raise AssertionError(f'unhandled {pc:x}: {op}')
        r['r0']=0;pc=nextpc
    else:raise AssertionError('writer did not terminate')
    floats={a:struct.unpack('<f',struct.pack('<I',v))[0] for a,v in writes.items()}
    return floats,[get(gp-16804+i,1,True) for i in range(20)],get(gp-16784,1)


class MatrixControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.b=json.loads((OUT/'matrix_control.json').read_text())

    def test_full_decoded_matrix_writer_all_coordinates_and_flags(self):
        matrix=[[-90+8*i+j for j in range(8)] for i in range(8)] # deliberately asymmetric
        # Distinct synthetic lookup curves expose priority/axis mistakes.
        for synthetic in (False,True):
            b=copy.deepcopy(self.b)
            if synthetic:
                b['tables']['matrix_input']['tenths_db']=list(range(-45,46,5))
                b['tables']['matrix_output']['tenths_db']=list(range(-90,91,10))
            for asad in (0,1,0xC0,0xC1,0xFC):
                for processing in (0x5A,0xA5):
                    for coordinate in range(-9,10):
                        result=coefficient_targets(b,matrix_codes=matrix,pair_codes=[0]*20,
                            row=1,asad=asad,processing_flag=processing,coordinate=coordinate)
                        actual,_,_=decoded_writer(b,matrix,[0]*20,0,processing,0xA5,0x5A,asad,coordinate,0xA9040)
                        self.assertEqual([actual[0xC022A+i] for i in range(64)],result['matrix_words'])

    def test_pair_state_and_full_decoded_writer(self):
        matrix=[[0]*8 for _ in range(8)]
        for state in (0x5A,0xAA,0xFF,0):
            for level in range(4):
                for processing,b01 in ((0xA5,0xA5),(0x5A,0xA5),(0xA5,0x5A)):
                    for asad in (0,0xC0):
                        for coordinate in (-9,0,9):
                            prior=[-50]*20
                            step=pair_row_step(self.b['pair_rows']['0'],level=level,processing_flag=processing,
                                b01_flag=b01,state=state,current=prior)
                            target=coefficient_targets(self.b,matrix_codes=matrix,pair_codes=step['current'],
                                row=step['row'],asad=asad,processing_flag=processing,coordinate=coordinate)
                            actual,current,newstate=decoded_writer(self.b,matrix,prior,level,processing,b01,state,
                                                                  asad,coordinate,0xAA648)
                            self.assertEqual([actual[0xE0584+i] for i in range(20)],target['pair_words'])
                            self.assertEqual((current,newstate),(step['current'],step['state']))

    def test_ramp_completion_requires_one_no_movement_call(self):
        rows=self.b['pair_rows']['0'];target=rows[1];prior=[v-1 for v in target]
        # Keep values in the supported source domain.
        prior=[max(-100,v) for v in prior]
        step=pair_row_step(rows,level=0,processing_flag=0xA5,b01_flag=0xA5,state=0xAA,current=prior)
        self.assertEqual(step['current'],target);self.assertEqual(step['state'],0xAA)
        step=pair_row_step(rows,level=0,processing_flag=0xA5,b01_flag=0xA5,state=0xAA,current=target)
        self.assertEqual(step['state'],0x5A)

    def test_zero_correction_does_not_remove_branch_clamp(self):
        args=dict(matrix_codes=[[0]*8 for _ in range(8)],pair_codes=[-100]*20,
                  row=1,processing_flag=0xA5,coordinate=0)
        ordinary=coefficient_targets(self.b,asad=0,**args)
        special=coefficient_targets(self.b,asad=0xC0,**args)
        self.assertEqual(ordinary['pair_words'],[0]*20)
        self.assertEqual(special['pair_words'][0],0)
        self.assertAlmostEqual(special['pair_words'][1],10**(-99/20),places=11)

    def test_sharc_decoded_coefficient_index_is_input_major(self):
        # Execute the address slice from actual decoded consumer instructions.
        lines=(OUT/'coefficient_matrix_consumer.asm').read_text().splitlines()
        code={}
        for line in lines:
            m=re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)',line)
            if m:code[int(m[1],16)]=m[3]
        for source in range(8):
            for output in range(8):
                r={'R7':source,'R2':8,'R3':output};memory=list(range(64))
                for pc in (0x126A02,0x126A04,0x126A05,0x126A07):
                    op=code[pc]
                    m=re.fullmatch(r'(R\d+)=(R\d+)\*(R\d+) \(SSI\)',op)
                    if m:r[m[1]]=r[m[2]]*r[m[3]];continue
                    m=re.fullmatch(r'(R\d+)\+=(R\d+)',op)
                    if m:r[m[1]]+=r[m[2]];continue
                    if op=='M4=R4':r['M4']=r['R4'];continue
                    self.assertEqual(op,'R4=DM(M4,I3)');r['R4']=memory[r['M4']]
                self.assertEqual(r['R4'],8*source+output)

    def test_reject_missing_or_out_of_domain_state(self):
        with self.assertRaises(ValueError):pair_row_step(self.b['pair_rows']['0'],level=4,
            processing_flag=0xA5,b01_flag=0xA5,state=0x5A,current=[0]*20)
        with self.assertRaises(ValueError):coefficient_targets(self.b,matrix_codes=[[0]*8 for _ in range(8)],
            pair_codes=[0]*20,row=1,asad=0,processing_flag=0xA5,coordinate=10)

    def test_ampchain_matrix_matches_decoded_writer_for_all_64_routes(self):
        from amp_dsp_chain import AmpChain,AUDIT
        from tempfile import TemporaryDirectory
        audit=json.loads(AUDIT.read_text());matrix=audit['known_electronic_stages']['input_matrix']['db']
        writes,_,_=decoded_writer(self.b,matrix,[0]*20,0,0xA5,0xA5,0x5A,0,0,0xA9040)
        actual=AmpChain().M8
        expected=np.array([[writes[0xC022A+8*source+output] for source in range(8)]
                           for output in range(8)])
        np.testing.assert_array_equal(actual,expected)
        self.assertNotEqual(actual[0,3],actual[3,0]) # detects old transpose
        audit['known_electronic_stages']['version']=1
        with TemporaryDirectory() as directory:
            path=Path(directory)/'old.json';path.write_text(json.dumps(audit))
            with self.assertRaises(ValueError):AmpChain(audit=path)


if __name__=='__main__':unittest.main()
