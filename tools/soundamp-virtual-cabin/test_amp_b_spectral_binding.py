import copy
import json
import re
import unittest

import numpy as np

from amp_b_spectral_binding import (load_spectral_calibration, bind_spectral_parameters,
    build_spectral_reference, LOADER, ROOT)
from amp_b_bound_scenario import retained_config
from amp_b_spatial_binding import build_effective_spatial
from inspect_amp_visa import load_shortwords, decode

OUT=ROOT/'output/r2/soundamp/remaining-blocks-20260920-v3'


def caller_trace(start,end):
    """Independent decoded caller address execution; callees return success.

    Captures parameter pointers rather than importing binder offset constants.
    Does not emulate audio, allocation failure or hardware float instructions.
    """
    memcode=load_shortwords(LOADER.read_bytes());code={};pc=start
    while pc<end:
        n,t,_=decode(memcode,pc);code[pc]=(n,t);pc+=n
    reg={f'{t}{i}':0 for t in ('R','I','M') for i in range(16)}
    reg.update(R4=10000,R8=10059,R12=513,I6=40000,I7=39999,M5=0,M6=1,M7=-1,M14=1)
    mem={39999:0}|{40000+i:60000+512*i for i in range(1,15)}
    mem.update({10000+i:70000+i for i in range(59)})
    mem[11161]=20331;mem[11166]=20302
    pc=start;flag=0;pending=None;calls=[]
    for _ in range(2000):
        n,op=code[pc]
        if op.startswith('JUMP (M14,I12)'):return calls
        if op.startswith('CJUMP '):
            calls.append((int(op.split()[1],16),reg['R4'],reg['R8'],reg['R12']))
            reg['R0']=0;pc+=n
            for _ in range(2):pc+=code[pc][0]
            continue
        old=reg.copy();oldflag=flag;writes={};stores={};nextpc=pc+n;newpending=None
        def value(v):return old[v] if v in old else int(v,0)
        def address(s):
            a,b=s.split(',')
            if a.startswith('I'):
                result=value(a);writes[a]=result+value(b);return result
            return value(a)+value(b)
        for part in op.split(' || '):
            if part.startswith('IF EQ '):
                if oldflag!=0:continue
                part=part[6:]
            if part.startswith('IF NE '):
                if oldflag==0:continue
                part=part[6:]
            if part=='NOP':continue
            if part.startswith('JUMP '):
                target=int(part.split()[1],16)
                if '(DB)' in part:newpending=[2,target]
                else:nextpc=target
                continue
            dest,expr=part.split('=',1)
            if dest.startswith('DM('):stores[address(dest[3:-1])]=value(expr);continue
            if expr.startswith('DM('):v=mem[address(expr[3:-1])]
            elif expr.startswith('MODIFY('):
                a,b=expr[7:-1].split(',');v=value(a)+value(b)
            elif expr.startswith('PASS '):v=value(expr[5:]);flag=(v>0)-(v<0)
            elif '+' in expr:
                a,b=expr.split('+');v=value(a)+value(b)
            else:v=value(expr)
            writes[dest]=v
        reg.update(writes);mem.update(stores)
        if pending:
            pending[0]-=1
            if not pending[0]:nextpc=pending[1];pending=None
        if newpending:pending=newpending
        pc=nextpc
    raise AssertionError('caller did not terminate')


class SpectralBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cal={s:load_spectral_calibration(source=s) for s in ('loader','b01')}
        cls.config=json.loads((OUT/'b_config_control.json').read_text())
        cls.mode=json.loads((OUT/'b_mode_control.json').read_text())

    def bind(self,source='b01',bank=1,mode='20:default'):
        pointers={int(k):v for name in ('16:default',mode) for k,v in self.mode['pointer_maps'][name].items()}
        return bind_spectral_parameters(self.cal[source],
            effective_config=retained_config(self.config,source=source,bank=bank),pointers=pointers)

    def test_retained_nonuniform_band_map_and_whole_source_validation(self):
        for cal in self.cal.values():
            self.assertEqual(len(cal['edges']),75)
            self.assertEqual(cal['edges'][:40],list(range(40)))
            self.assertEqual(cal['edges'][-5:],[311,347,387,431,513])
            self.assertGreater(len(set(np.diff(cal['edges']))),10)
            self.assertFalse(cal['metadata']['actual_vehicle_state_proven'])

    def test_decoded_parameter_callers_not_offset_table(self):
        pre=caller_trace(0x12E1BB,0x12E30A)
        main=caller_trace(0x12E30A,0x12E3E6)
        post=caller_trace(0x12E521,0x12E80E)
        self.assertEqual([r[1] for r in pre if r[0]==0x12ADFF],[10098])
        self.assertEqual([r[2] for r in main if r[0]==0x12F250],[10211])
        self.assertEqual([r[2] for r in post if r[0]==0x12F90A],[10104,10104])
        self.assertEqual([r[2] for r in post if r[0]==0x12D355],[10120,10120])
        self.assertEqual([r[2] for r in post if r[0]==0x12F250],[10123])
        self.assertEqual([r[1] for r in post if r[0]==0x12E487],[20331])

    def test_header_binding_real_selectors_and_values(self):
        b=self.bind();l=self.bind('loader')
        self.assertFalse(b['main_enabled']);self.assertTrue(l['main_enabled'])
        self.assertTrue(b['post_enabled']);self.assertEqual(b['post_selector'],0)
        self.assertEqual(b['pre_subtraction']['low_bin'],50)
        self.assertEqual(l['pre_subtraction']['low_bin'],12)
        self.assertEqual(b['pre_subtraction']['high_bin'],512)
        self.assertAlmostEqual(b['input_mix_gain'],.5011872053146362)
        self.assertEqual(l['input_mix_gain'],.5)
        self.assertEqual(b['level_params'][14:16],[1.,.5])
        self.assertAlmostEqual(b['smoother']['history_gain'],.1944862604)
        self.assertTrue(b['smoothing_enabled'])
        self.assertNotEqual(b['post_params'][2:76],l['post_params'][2:76])

    def test_header_independent_of_four_calibration_banks_crossfeed_follows_pointer(self):
        for source in self.cal:
            for bank in range(1,5):
                for mode in ('20:default','20:3'):
                    b=self.bind(source,bank,mode)
                    self.assertEqual(b['main_params'],self.bind(source)['main_params'])
                    offset=31 if mode=='20:default' else 426
                    self.assertEqual(b['metadata']['crossfeed_config_offset'],offset)
                    cfg=retained_config(self.config,source=source,bank=bank)
                    self.assertEqual(b['crossfeed'],cfg[offset])

    def test_mode5_last_write_selects_actual_493_crossfeed(self):
        cfg=retained_config(self.config,source='b01',bank=1)
        pointers={int(k):v for name in ('20:3','16:5') for k,v in self.mode['pointer_maps'][name].items()}
        b=bind_spectral_parameters(self.cal['b01'],effective_config=cfg,pointers=pointers)
        self.assertEqual(b['metadata']['crossfeed_config_offset'],493)
        self.assertEqual(b['crossfeed'],cfg[493])
        self.assertEqual(pointers[1166],439)  # Spatial suffix still from API20.

    def test_decoded_simd_header_copy_addresses(self):
        # Execute12B2BF copy body including its103-loop SIMD pairs +scalar tail.
        m=load_shortwords(LOADER.read_bytes());pc=0x12B2BF
        r=dict(R4=10059,R8=20038,I6=40000,I7=39999,I5=0,M6=1,M7=-1)
        memory={20000+i:i for i in range(245)};memory[39999]=0
        simd=False;loop=None
        while pc<0x12B2E1:
            n,op,_=decode(m,pc);nextpc=pc+n
            if op=='NOP':pass
            elif op.startswith('BIT SET '):simd=True
            elif op.startswith('BIT CLR '):simd=False
            elif op.startswith('LCNTR='):
                match=re.fullmatch(r'LCNTR=(\d+), DO (0x[0-9a-f]+) UNTIL LCE',op)
                loop=[int(match[1]),nextpc,int(match[2],16)]
            elif op in ('R2=DM(I5,+2)','R2=DM(I5,M6)'):
                r['R2']=memory[r['I5']]
                if simd:r['S2']=memory[r['I5']+1]
                r['I5']+=2 if simd else 1
            elif op in ('DM(I4,+2)=R2','DM(I4,M6)=R2'):
                memory[r['I4']]=r['R2']
                if simd:memory[r['I4']+1]=r['S2']
                r['I4']+=2 if simd else 1
            elif op.startswith('DM(-2,I6)='):memory[r['I6']-2]=r['R2']
            elif 'MODIFY' in op:
                match=re.fullmatch(r'(I\d+)=MODIFY\((I\d+),([+-]?\d+)\)',op)
                r[match[1]]=r[match[2]]+int(match[3])
            else:
                dest,source=op.split('=');r[dest]=r[source]
            if loop and pc==loop[2]:
                loop[0]-=1
                if loop[0]:nextpc=loop[1]
            pc=nextpc
        self.assertEqual([memory[10000+i] for i in range(92,299)],list(range(38,245)))
        self.assertNotIn(10299,memory)

    def test_no_invalid_configuration_or_integrity_fallback(self):
        cfg=retained_config(self.config,source='b01',bank=1)
        for bad in ({},{1161:-1},{1161:829},{1161:True}):
            with self.assertRaises(ValueError):bind_spectral_parameters(self.cal['b01'],effective_config=cfg,pointers=bad)
        damaged=copy.deepcopy(self.cal['b01']);damaged['header_words'][69]^=1
        with self.assertRaises(ValueError):bind_spectral_parameters(damaged,effective_config=cfg,pointers={1161:31})
        damaged=copy.deepcopy(self.cal['b01']);damaged['edges'][-1]=512
        with self.assertRaises(ValueError):bind_spectral_parameters(damaged,effective_config=cfg,pointers={1161:31})
        with self.assertRaises(ValueError):load_spectral_calibration(source='car')

    def test_builder_fresh_state_and_real_not_neutral_masks(self):
        # Spatial is explicitly disabled for this nonspatial test only.
        class DisabledSpatial:
            bins=513
            def process(self,left,right,*,enabled):
                assert not enabled
                return left,right
        b=self.bind();spatial=DisabledSpatial()
        ref=build_spectral_reference(b,spatial=spatial,spatial_enabled=False,zero_histories=True)
        self.assertTrue(ref.smooth_left.first)
        self.assertFalse(ref.smooth_left is ref.smooth_right)
        self.assertFalse(ref.level_sum is ref.level_difference)
        np.testing.assert_array_equal(ref.main_transient.history(),0)
        np.testing.assert_array_equal(ref.level_sum.gain,0)
        x=np.ones(513,complex)
        out=ref.spectral_process(x,x,mode=5)
        np.testing.assert_array_equal(out['main'][0],x)
        self.assertLess(abs(out['pre'][0]).max(),.09)
        self.assertLess(abs(out['post'][0]).max(),.2)
        self.assertFalse(ref.smooth_left.first)
        with self.assertRaises(ValueError):build_spectral_reference(b,spatial=spatial,spatial_enabled=False,zero_histories=False)

    def test_integrated_retained_spatial_and_transform_tables(self):
        for source in ('loader','b01'):
            cfg=retained_config(self.config,source=source,bank=1)
            pointers={int(k):v for name in ('20:default','16:5') for k,v in self.mode['pointer_maps'][name].items()}
            b=bind_spectral_parameters(self.cal[source],effective_config=cfg,pointers=pointers)
            spatial,state=build_effective_spatial(effective_config=cfg,pointers=pointers,edges=b['edges'],
                assume_successful_fresh_initialization=True)
            ref=build_spectral_reference(b,spatial=spatial,spatial_enabled=state['enabled'],zero_histories=True)
            self.assertIn('transform_tables',ref.binding_metadata)
            self.assertEqual(len(ref.twiddles),256)
            np.testing.assert_array_equal(ref.window[:512],ref.window[:511:-1])
            x=np.sin(np.arange(512)*.137)*.02
            for _ in range(3):
                for branch in ref.process(x,x*.6,mode=5).values():
                    self.assertEqual(branch.shape,(2,512))
                    self.assertTrue(np.isfinite(branch).all())


if __name__=='__main__':unittest.main()
