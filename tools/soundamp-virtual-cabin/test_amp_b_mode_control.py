"""Pointer-only decoded execution oracle; no DSP audio or full ABI emulation."""
import json
import math
import re
import struct
import unittest
import numpy as np
from numpy.testing import assert_allclose
from amp_b_pipeline import crossmix
from amp_b_mode_control import apply_mode_control,resolve_mode_parameters
from test_amp_b_control import OUT


def decoded_crossmix(left,right,amount):
    """Execute12E487 caller instructions; vector callees use host float math.

    Explicit ABI bridge skips call prologue/return; not bit-exact SIMD execution.
    Input/output/scratch arrays are separate, matching successful wrapper use.
    """
    code={}
    for line in (OUT/'first_b_spectral_crossmix.asm').read_text().splitlines():
        m=re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)',line)
        if m:code[int(m[1],16)]=(len(m[2].split()),m[3])
    def words(z):return np.asarray(z,complex).view(float).copy()
    n=len(left);arrays={1000:words(left),2000:words(right),3000:np.zeros(2*n),
                       4000:np.zeros(2*n),5000:np.zeros(2*n),6000:np.zeros(2*n)}
    r={f'R{i}':0 for i in range(16)}|{'I6':10000,'I7':9998,'M5':0,'M6':1,'M7':-1}
    r.update(R4=7000,R8=n,R12=3000)
    memory={7000:amount,10001:4000,10002:5000,10003:1000,10004:2000}
    pc=0x12E487;calls=[]
    while pc<0x12E510:
        size,insn=code[pc]
        if insn.startswith('CJUMP '):
            target=int(insn.split()[1],16);calls.append(target)
            if target==0x12FCE1:
                count=memory[r['I7']+1];arrays[r['R12']][:count]=r['R4']*arrays[r['R8']][:count]
            elif target==0x12EC7A:
                count=memory[r['I7']+1];arrays[r['R12']][:count]=arrays[r['R4']][:count]+arrays[r['R8']][:count]
            elif target==0x132E08:
                arrays[r['R8']][:r['R12']]=arrays[r['R4']][:r['R12']]
            else:raise AssertionError('unexpected crossmix callee')
            pc+=size
            for _ in range(2):pc+=code[pc][0]
            continue
        old=r.copy();writes={};stores={}
        def value(v):
            if v.startswith('F'):return old['R'+v[1:]]
            if v in old:return old[v]
            raw=int(v,0)
            return 1.0 if raw==0x3f800000 else raw
        def address(spec):
            a,b=spec.split(',')
            if a=='I7':writes[a]=old[a]+value(b);return old[a]
            return value(a)+value(b)
        for part in insn.split(' || '):
            dest,expr=part.split('=',1)
            if dest.startswith('DM('):stores[address(dest[3:-1])]=value(expr);continue
            if expr.startswith('MODIFY('):
                a,b=expr[7:-1].split(',');out=value(a)+value(b)
            elif expr.startswith('DM('):out=memory[address(expr[3:-1])]
            elif expr.startswith('PASS '):out=value(expr[5:])
            elif re.fullmatch(r'\w+[+\-]\w+',expr):
                a,op,b=re.split(r'([+\-])',expr);out=value(a)+value(b) if op=='+' else value(a)-value(b)
            else:out=value(expr)
            writes['R'+dest[1:] if dest.startswith('F') else dest]=out
        r.update(writes);memory.update(stores);pc+=size
    return arrays[1000].view(complex),arrays[2000].view(complex),calls


def decoded_pointer_writes(api,value):
    """Execute switch bodies with old parallel operands and two delay slots.

    The ABI bridge to the suffix helper is substituted with its explicit
    arguments. Stop before frame restoration; do not emulate allocation/replay.
    No expected pointer map is used by this oracle.
    """
    code={}
    for name in ('first_b_mode_pointer_switch','first_b_mode_spatial_pointers','first_b_mode_pair_gain_pointers'):
        for line in (OUT/f'{name}.asm').read_text().splitlines():
            m=re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)',line)
            if m:code[int(m[1],16)]=(len(m[2].split()),m[3])
    bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
    flt=lambda v:struct.unpack('<f',struct.pack('<I',v&0xffffffff))[0]
    r={f'R{i}':0 for i in range(16)} | {f'I{i}':0 for i in range(16)}
    r.update(M5=0,M6=1,M7=-1,M13=0,M14=1,M15=-1,I6=40000,R4=10000,R8=20000,R12=api)
    memory={40001:bits(value),39999:0};flag=0;sz=False
    for stage in range(2):
        if stage==0:pc=0x12B34B
        else:
            r.update(R4=10000+(15 if api==16 else 10),R8=20000,R12=bits(value))
            pc=0x12B232 if api==16 else 0x12B1E7
        pending=None
        for _ in range(300):
            if pc in (0x12B3D3,0x12B478):break
            n,op=code[pc]
            if op.startswith('JUMP (M14,I12)'):break
            old=r.copy();oldflag=flag;oldsz=sz;writes={};stores={};nextpc=pc+n;newpending=None
            def val(v):
                if v.startswith('F'):return flt(old['R'+v[1:]])
                return old[v] if v in old else int(v,0)
            def addr(spec):
                a,b=spec.split(',')
                if a.startswith('I'):
                    address=val(a);writes[a]=address+val(b);return address
                return val(a)+val(b)
            for part in op.split(' || '):
                if part.startswith('IF '):
                    _,cond,part=part.split(' ',2)
                    if not {'EQ':oldflag==0,'NE':oldflag!=0,'GE':oldflag>=0,'SZ':oldsz}[cond]:continue
                if part=='NOP':continue
                m=re.fullmatch(r'JUMP (0x[0-9a-f]+)( \(DB\))?',part)
                if m:
                    if m[2]:newpending=[2,int(m[1],16)]
                    else:nextpc=int(m[1],16)
                    continue
                m=re.fullmatch(r'COMP\((\w+),(\w+)\)',part)
                if m:
                    a,b=val(m[1]),val(m[2]);flag=(a>b)-(a<b);continue
                m=re.fullmatch(r'DM\(([^)]+)\)=(\w+)',part)
                if m:stores[addr(m[1])]=val(m[2]);continue
                part=re.sub(r'^(\w+)\+=(\w+)$',r'\1=\1+\2',part)
                m=re.fullmatch(r'(\w+)=(.*)',part)
                if not m:raise AssertionError(f'unhandled {pc:x}: {part}')
                dest,expr=m.groups();q=re.fullmatch(r'DM\(([^)]+)\)',expr)
                if q:value_out=memory[addr(q[1])]
                elif expr.startswith('TRUNC '):value_out=math.trunc(val(expr[6:]))
                elif expr.startswith('BTGL '):
                    a,b=expr[5:].split(' BY ');value_out=val(a)^(1<<val(b));sz=value_out==0
                elif re.fullmatch(r'\w+[+\-]\w+',expr):
                    a,operator,b=re.split(r'([+\-])',expr);value_out=val(a)+val(b) if operator=='+' else val(a)-val(b)
                    flag=(value_out>0)-(value_out<0)
                else:value_out=val(expr)
                writes['R'+dest[1:] if dest.startswith('F') else dest]=bits(value_out) if dest.startswith('F') else value_out
            r.update(writes);memory.update(stores)
            if pending:
                pending[0]-=1
                if pending[0]==0:nextpc=pending[1];pending=None
            if newpending:pending=newpending
            pc=nextpc
        else:raise AssertionError('bounded pointer execution did not terminate')
    return {1156+i:memory[10000+i]-20000 for i in range(18) if 10000+i in memory}


class BModeControlTests(unittest.TestCase):
    def setUp(self):
        self.binding=json.loads((OUT/'b_mode_control.json').read_text())
        self.configs=json.loads((OUT/'b_config_control.json').read_text())

    def apply(self,api,value,*,state=None,deferred=False,auto=0):
        state=state or {'pointers':{p:10 for p in range(1156,1174)},'shadow':{16:2,20:1},'effective':{16:2,20:1}}
        return apply_mode_control(self.binding,api=api,value=value,deferred=deferred,
            effective_config487=auto,**{k:state[k] for k in ('pointers','shadow','effective')})

    def test_all_nonautomatic_paths_against_decoded_instructions(self):
        for api,values in [(16,[2,5,6]),(20,[1,3])]:
            for value in values:
                with self.subTest(api=api,value=value):
                    self.assertEqual(self.apply(api,value)['pointer_writes'],decoded_pointer_writes(api,value))

    def test_order_matters_despite_identical_effective_selectors(self):
        a=self.apply(20,3,state=self.apply(16,5))
        b=self.apply(16,5,state=self.apply(20,3))
        self.assertEqual(a['effective'],b['effective'])
        self.assertEqual(a['pointers'][1156],421);self.assertEqual(b['pointers'][1156],488)
        self.assertEqual(a['pointers'][1166],b['pointers'][1166]);self.assertEqual(a['pointers'][1171],b['pointers'][1171])

    def test_auto_resolves_before_deferred_shadow_write(self):
        for auto,wanted in [(1,1),(0,3),(3,3),(1.00001,3)]:
            r=self.apply(20,6,deferred=True,auto=auto)
            self.assertEqual(r['shadow'][20],wanted);self.assertEqual(r['effective'][20],1)
            self.assertFalse(r['replay_required']);self.assertEqual(r['pointer_writes'],{})
            self.assertEqual(set(r['pointers'].values()),{10})

    def test_auto_after_resolution_matches_decoded_pointer_helper(self):
        for auto,resolved in [(1,1),(0,3)]:
            r=self.apply(20,6,auto=auto)
            self.assertEqual(r['effective'][20],resolved)
            self.assertEqual(r['pointer_writes'],decoded_pointer_writes(20,resolved))

    def test_independent_suffixes_are_retained_and_inputs_unchanged(self):
        state=self.apply(20,3);before=json.dumps(state,sort_keys=True)
        r=self.apply(16,2,state=state)
        self.assertEqual(json.dumps(state,sort_keys=True),before)
        for p in range(1166,1171):self.assertEqual(r['pointers'][p],state['pointers'][p])
        r=self.apply(20,1,state=self.apply(16,5))
        self.assertEqual([r['pointers'][p] for p in range(1171,1174)],[506,507,508])

    def test_mode16_defer_keeps_prior_pointers_and_effective(self):
        r=self.apply(16,5,deferred=True)
        self.assertEqual(r['shadow'][16],5);self.assertEqual(r['effective'][16],2)
        self.assertFalse(r['pointer_writes']);self.assertFalse(r['replay_required'])

    def test_resolves_real_b01_bank1_to_gains_and_delays(self):
        f=lambda w:struct.unpack('<f',struct.pack('<I',w))[0]
        config=list(map(f,self.configs['sources']['b01']['banks'][0]['words']))
        a=self.apply(16,5,state=self.apply(20,3));b=self.apply(20,3,state=self.apply(16,5))
        x=resolve_mode_parameters(self.binding,effective_config=config,pointers=a['pointers'])
        y=resolve_mode_parameters(self.binding,effective_config=config,pointers=b['pointers'])
        self.assertAlmostEqual(x['four_tail_final_gain'],.630957365)
        self.assertAlmostEqual(y['four_tail_final_gain'],.446683586)
        self.assertEqual(x['six_common_delay'],355);self.assertEqual(x['two_common_delay'],105)
        self.assertEqual(len(x['spatial_parameters']),28);self.assertEqual(len(x['six_nested_L1']),5)

    def test_all_image_banks_resolve_auto_to_three(self):
        for banks in self.binding['sources'].values():
            for bank in banks:self.assertEqual(self.apply(20,6,auto=bank['config487'])['resolved_value'],3)

    def params(self,source,bank,state):
        words=self.configs['sources'][source]['banks'][bank-1]['words']
        config=[struct.unpack('<f',struct.pack('<I',w))[0] for w in words]
        return resolve_mode_parameters(self.binding,effective_config=config,pointers=state['pointers'])

    def test_all_image_crossfeed_choices_through_pointer_resolution(self):
        for source in ('loader','b01'):
            for bank in range(1,5):
                for api,value,key in [(16,2,'16:default'),(16,5,'16:5'),(20,1,'20:default'),(20,3,'20:3')]:
                    p=self.params(source,bank,self.apply(api,value))
                    self.assertEqual(p['post_crossfeed'],self.binding['sources'][source][bank-1]['choices'][key]['1161'][0])

    def test_crossfeed_matches_bounded_decoded_call_sequence(self):
        rng=np.random.default_rng(1161);a=rng.normal(size=513)+1j*rng.normal(size=513)
        b=rng.normal(size=513)+1j*rng.normal(size=513)
        for amount in (0,.15,.2,.25,.27,.28,.3,.5,1,2,-.2):
            x,y,calls=decoded_crossmix(a,b,amount)
            assert_allclose(crossmix(a,b,amount=amount),(x,y),rtol=0,atol=0)
            self.assertEqual(calls,[0x12FCE1,0x12FCE1,0x12EC7A,0x12FCE1,0x12FCE1,0x132E08,0x12EC7A])

    def test_extracted_crossfeed_preserves_mono_and_scales_difference(self):
        z=np.array([1+2j,-3+4j,0j])
        for source in ('loader','b01'):
            for bank in range(1,5):
                for state in (self.apply(16,2),self.apply(16,5),self.apply(20,3)):
                    c=self.params(source,bank,state)['post_crossfeed']
                    assert_allclose(crossmix(z,z,amount=c),(z,z),atol=1e-15)
                    assert_allclose(crossmix(z,-z,amount=c),((1-2*c)*z,-(1-2*c)*z),atol=1e-15)

    def test_b01_bank2_crossfeed_order_is_not_bank1_assumption(self):
        a=self.apply(16,5,state=self.apply(20,3));b=self.apply(20,3,state=self.apply(16,5))
        self.assertEqual(a['effective'],b['effective'])
        self.assertAlmostEqual(self.params('b01',2,a)['post_crossfeed'],.27)
        self.assertAlmostEqual(self.params('b01',2,b)['post_crossfeed'],.28)

    def test_unbound_pointer_fields_do_not_gain_invented_audio_roles(self):
        self.assertEqual(self.binding['unbound_pointer_consumers'],[1162,1163,1164,1165])
        self.assertEqual(len(self.binding['consumers']),14)
        self.assertEqual(len(self.params('b01',1,self.apply(16,5))),14)

    def test_rejects_unvalidated_and_incomplete_inputs(self):
        for api,value in [(16,1),(20,5),(18,0),(16,float('nan'))]:
            with self.assertRaises(ValueError):self.apply(api,value)
        with self.assertRaises(ValueError):self.apply(20,6,auto=float('nan'))
        with self.assertRaises(ValueError):resolve_mode_parameters(self.binding,effective_config=[0]*828,pointers={})


if __name__=='__main__':unittest.main()
