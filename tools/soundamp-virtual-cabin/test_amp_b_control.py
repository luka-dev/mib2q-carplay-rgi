"""Explicit curve tests plus bounded execution of decoded SHARC control flow."""
import json
import math
from pathlib import Path
import re
import random
import struct
import unittest
from amp_b_control import curve_contribution, update_parameter_slot, branch_gain_targets

OUT=Path(__file__).resolve().parents[2]/'output/r2/soundamp/remaining-blocks-20260919'


def decoded_curve(x,records,base,slots):
    """12B04B..12B147/1E7, finite successful paths; not a general emulator.

    Execute decoded moves, float32 arithmetic, comparisons, branches and loops.
    Respect parallel old operands and two delay slots. Only divide/sqrt library
    calls are substituted with host math (not SHARC bit-exact helper execution).
    Entry assumes the already-saved ABI frame; exit precedes frame restoration.
    """
    code={}
    for line in (OUT/'first_b_control_curve_kernel.asm').read_text().splitlines():
        m=re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)',line)
        if m:code[int(m[1],16)]=(len(m[2].split()),m[3])
    bits=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
    flt=lambda w:struct.unpack('<f',struct.pack('<I',w & 0xffffffff))[0]
    r={f'R{i}':0 for i in range(16)}
    r.update({f'I{i}':0 for i in range(16)})
    r.update(M5=0,M6=1,M7=-1,M13=0,M14=1,M15=-1,
             I6=10000,I7=9950,I2=20000,I3=30000,R7=bits(x),R9=6,
             R11=bits(0),R13=bits(5),R1=len(records))
    mem={20001:40000,20002:bits(base),40000:bits(-42)}
    mem.update({20003+i:bits(v) for i,v in enumerate(slots)})
    for i,row in enumerate(records):
        mem[30000+i]=31000+3*i
        for j,v in enumerate(row):mem[31000+3*i+j]=bits(v)
    flag=0;sz=False;pending=None;loop=None;pc=0x12B1DD if x==0 else 0x12B04B
    for _ in range(2000):
        if pc==0x12B147:return flt(mem[40000]),[flt(mem[20003+i]) for i in range(9)]
        n,op=code[pc];nextpc=pc+n;old=r.copy();oldflag=flag;oldsz=sz
        writes={};stores={};newpending=None
        def val(v):
            if v.startswith('F'):return flt(old['R'+v[1:]])
            return old[v] if v in old else int(v,0)
        def addr(spec):
            a,b=spec.split(',')
            if a.startswith('I'):
                address=val(a);writes[a]=address+val(b)
            else:address=val(a)+val(b)
            return address
        def cond(c):return {'EQ':oldflag==0,'NE':oldflag!=0,'GE':oldflag>=0,
                            'GT':oldflag>0,'LT':oldflag<0,'SZ':oldsz}[c]
        for part in op.split(' || '):
            if part.startswith('IF '):
                _,c,part=part.split(' ',2)
                if not cond(c):continue
            if part=='NOP':continue
            m=re.fullmatch(r'LCNTR=R(\d+), DO (0x[0-9a-f]+) UNTIL LCE',part)
            if m:loop=[nextpc,int(m[2],16),val('R'+m[1])];continue
            m=re.match(r'(?:C?JUMP|CALL) (0x[0-9a-f]+)(.*)',part)
            if m:
                target=int(m[1],16)
                if '(DB)' in m[2]:newpending=[2,target,part.startswith(('CALL','CJUMP'))]
                else:nextpc=target
                continue
            m=re.fullmatch(r'COMP(U?)\((\w+),(\w+)\)',part)
            if m:
                a,b=val(m[2]),val(m[3]);flag=(a>b)-(a<b);continue
            m=re.fullmatch(r'DM\(([^)]+)\)=(\w+)',part)
            if m:stores[addr(m[1])]=val(m[2]);continue
            part=re.sub(r'^(\w+)([+*\-])=(\w+)$',r'\1=\1\2\3',part)
            m=re.fullmatch(r'(\w+)=(.*)',part)
            if not m:raise AssertionError(f'unhandled {pc:x}: {part}')
            dest,expr=m.groups()
            q=re.fullmatch(r'DM\(([^)]+)\)',expr)
            if q:value=mem[addr(q[1])]
            elif expr.startswith('MODIFY('):
                a,b=expr[7:-1].split(',');value=val(a)+val(b)
            elif expr.startswith('PASS '):
                value=val(expr[5:]);flag=(value>0)-(value<0)
            elif expr.startswith('TRUNC '):value=math.trunc(val(expr[6:]))
            elif expr.startswith('ABS '):value=abs(val(expr[4:]))
            elif expr.startswith('ABS('):
                a,b=expr[4:-1].split('-');value=abs(val(a)-val(b))
            elif expr.startswith('BTGL '):
                a,b=expr[5:].split(' BY ');value=val(a)^(1<<val(b));sz=value==0
            elif re.fullmatch(r'\w+[+*\-]\w+',expr):
                a,operator,b=re.split(r'([+*\-])',expr);a,b=val(a),val(b)
                value={'+':lambda:a+b,'-':lambda:a-b,'*':lambda:a*b}[operator]()
                flag=(value>0)-(value<0)
            else:value=val(expr)
            if dest.startswith('F'):writes['R'+dest[1:]]=bits(value)
            else:writes[dest]=value
        r.update(writes);mem.update(stores)
        if loop and pc==loop[1]:
            loop[2]-=1
            if loop[2]>0:nextpc=loop[0]
            else:loop=None
        if pending:
            pending[0]-=1
            if pending[0]==0:
                _,target,is_call=pending
                if is_call:
                    if target==0x124354:r['R0']=bits(flt(r['R4'])/flt(r['R8']))
                    elif target==0x1244A6:r['R0']=bits(math.sqrt(flt(r['R4'])))
                    else:raise AssertionError(f'unhandled call {target:x}')
                else:nextpc=target
                pending=None
        if newpending:pending=newpending
        pc=nextpc
    raise AssertionError('bounded curve execution did not terminate')


class BControlTests(unittest.TestCase):
    def test_zero_is_special_not_interpolated(self):
        self.assertEqual(curve_contribution(0,[(0,4,0),(100,2,0)]),0)

    def test_shapes_both_signs(self):
        for sign in (-1,1):
            for shape,wanted in [(0,1),(1,.25),(2,2)]:
                records=[(0,0,0),(sign*100,4,shape)]
                self.assertAlmostEqual(curve_contribution(sign*25,records),wanted)

    def test_bracket_order_duplicates_and_clamping(self):
        self.assertEqual(curve_contribution(150,[(10,2,0),(100,4,0)]),4)
        self.assertEqual(curve_contribution(25,[(0,0,0),(100,1,0),(100,3,0)]),.75)
        self.assertEqual(curve_contribution(-150,[(-10,2,0),(-100,4,0)]),4)
        self.assertEqual(curve_contribution(50,[(-50,4,0),(0,1,0)]),1)

    def test_slot_keeps_other_controls_and_clamps(self):
        slots=[.5]*9
        r=update_parameter_slot(base=1.6,contributions=slots,slot=6,coordinate=80,
                                records=[(0,0,0),(0,0,0)],lower=0,upper=5)
        self.assertEqual(slots,[.5]*9);self.assertEqual(r['contributions'][6],0)
        self.assertAlmostEqual(r['unclamped'],5.6);self.assertEqual(r['target'],5)

    def test_image_curves_no_level_contribution(self):
        report=json.loads((OUT/'b_level_control.json').read_text())['curve_bindings']
        self.assertTrue(report['zero_curve_words'])
        for source in ('loader','b01'):
            for row in report['bindings']:
                records=[[v[source+'_float'] for v in p] for p in row['curve_records']]
                for x in (-200,-100,-.01,0,.01,70,126,200):
                    self.assertEqual(curve_contribution(x,records),0)

    def test_invalid_domain_and_shape(self):
        for x in (401,float('nan')):
            with self.assertRaises(ValueError):curve_contribution(x,[(0,0,0)])
        with self.assertRaises(ValueError):curve_contribution(1,[(0,0,0),(2,1,3)])

    def test_decoded_kernel_agrees_with_reference(self):
        cases=[]
        for x in (-200,-126,-70,-.1,0,.1,70,126,200):cases.append((x,[(0,0,0),(0,0,0)]))
        for sign in (-1,1):
            for shape in (0,1,2):
                for x in (.1,25,99,100,150):
                    cases.append((sign*x,[(0,0,0),(sign*100,2,shape)]))
        rng=random.Random(25)
        for _ in range(80):
            cases.append((rng.uniform(-200,200),
                          [(rng.uniform(-200,200),rng.uniform(-2,3),rng.randrange(3)) for _ in range(4)]))
        for x in (201,250,399,400):cases.append((x,[(120,0,0),(180,.7,0)]))
        for x,records in cases:
            with self.subTest(x=x,records=records):
                slots=[.01*i for i in range(9)]
                actual,updated=decoded_curve(x,records,1,slots)
                wanted=update_parameter_slot(base=1,contributions=slots,slot=6,
                        coordinate=x,records=records,lower=0,upper=5)
                self.assertAlmostEqual(actual,wanted['target'],places=5)
                self.assertAlmostEqual(updated[6],wanted['contributions'][6],places=5)

    def gains(self,source='b01',changes=None,state=None):
        binding=json.loads((OUT/'b_level_control.json').read_text())['gain_controls']
        coords={r['api']:0 for r in binding['slots']};coords.update(changes or {})
        return branch_gain_targets(binding,source=source,coordinates=coords,
                contributions=state if state is not None else {d:[0]*9 for d in (46,50,51)})

    def test_extracted_nonzero_b01_slots(self):
        r=self.gains(changes={18:9,19:100,27:180})
        self.assertAlmostEqual(r['targets'][46],1)
        self.assertAlmostEqual(r['targets'][50],3.2,places=6)
        self.assertAlmostEqual(r['targets'][51],1.8,places=6)
        r=self.gains(changes={18:-9,19:-50})
        self.assertEqual(r['targets'][46],0);self.assertEqual(r['targets'][50],0)
        self.assertAlmostEqual(r['targets'][51],1.3,places=6)

    def test_loader_is_not_b01(self):
        r=self.gains('loader',{18:9,19:100,22:9,27:180})
        self.assertEqual(r['targets'][46],1);self.assertEqual(r['targets'][50],1)
        self.assertAlmostEqual(r['targets'][51],1.6,places=6)

    def test_api22_symmetric_and_api27_extended_domain(self):
        for v in (-9,9):
            r=self.gains(changes={22:v})
            self.assertAlmostEqual(r['targets'][50],.5)
            self.assertAlmostEqual(r['targets'][51],1.2,places=6)
        for x,gain in [(0,1),(120,1),(150,1.35),(180,1.7),(250,1.7),(400,1.7)]:
            self.assertAlmostEqual(self.gains(changes={27:x})['targets'][50],gain,places=6)

    def test_untouched_slot_and_explicit_inputs(self):
        state={d:[0]*9 for d in (46,50,51)};state[51][1]=.25
        r=self.gains(state=state)
        self.assertAlmostEqual(r['targets'][51],1.85,places=6)
        self.assertEqual(r['contributions'][51][1],.25);self.assertEqual(state[51][1],.25)
        with self.assertRaises(ValueError):self.gains(changes={18:201})
        binding=json.loads((OUT/'b_level_control.json').read_text())['gain_controls']
        with self.assertRaises(ValueError):branch_gain_targets(binding,source='b01',coordinates={},contributions=state)

    def test_decoded_actual_b01_records(self):
        binding=json.loads((OUT/'b_level_control.json').read_text())['gain_controls']
        for row in binding['slots']:
            for branch in row['branches']:
                records=[[v['b01'] for v in p] for p in branch['records']]
                for x in (-100,-9,-1,0,1,9,100,150,180,200):
                    with self.subTest(slot=row['slot'],descriptor=branch['descriptor'],x=x):
                        _,slots=decoded_curve(x,records,1,[0]*9)
                        self.assertAlmostEqual(slots[6],curve_contribution(x,records),places=5)


if __name__=='__main__':unittest.main()
