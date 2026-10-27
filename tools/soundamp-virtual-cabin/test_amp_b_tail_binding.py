import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import unittest

import numpy as np

from amp_b_tail_binding import (bind_effective_tails, build_effective_mode5,
                                tail_binding_evidence, PROOFS, ROOT)
from amp_b_bound_scenario import retained_config, bind_effective_output
from amp_dsp_chain import reference_fingerprint
from amp_b_output import BSixChannelTailReference, BFourChannelTailReference, BOutputFinishReference
from amp_b_decorrelator import BTwoChannelTailReference
from test_amp_b_pipeline import pipeline

OUT=ROOT/'assets/soundamp/remaining-blocks-20260920-v3'


def caller_trace(name, *, object_base=10000, parameter_base=20000):
    """Independent decoded caller/stack execution, successful callee stand-ins.

    Derives passed addresses without consulting the binding's offset tables.
    Does not execute callee audio, errors or allocation; those are separate tests.
    """
    code={};words=[]
    for line in (OUT/(name+'.asm')).read_text().splitlines():
        match=re.fullmatch(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)',line)
        if match:
            w=[int(v,16) for v in match[2].split()];words.extend(w)
            code[int(match[1],16)]=(len(w),match[3])
    proof=next(v for v in PROOFS if v[0]==min(code))
    assert hashlib.sha256(struct.pack(f'<{len(words)}H',*words)).hexdigest()==proof[2]
    reg={f'{t}{i}':0 for t in ('R','I','M') for i in range(16)}
    reg.update(R4=object_base,R8=512,R12=512,I6=40000,I7=39999,M5=0,M6=1,M7=-1,M14=1)
    if name=='first_b_delay_gain_eq':reg['R8']=parameter_base
    mem={39999:0}|{40000+i:60000+512*i for i in range(1,9)}
    mem.update({object_base+i:70000+i for i in range(59)})
    mem.update({object_base+i:parameter_base+i for i in range(1156,1174)})
    pc=min(code);flag=0;pending=None;calls=[]
    for _ in range(1000):
        size,op=code[pc]
        if op.startswith('JUMP (M14,I12)'):return calls
        if op.startswith('CJUMP '):
            calls.append({'pc':pc,'target':int(op.split()[1],16),'R4':reg['R4'],'R8':reg['R8']})
            reg['R0']=0;pc+=size
            for _ in range(2):pc+=code[pc][0]
            continue
        old=reg.copy();writes={};stores={};oldflag=flag;nextpc=pc+size;newpending=None
        def value(v):return old[v] if v in old else int(v,0)
        def address(s):
            a,b=s.split(',')
            if a.startswith('I'):
                base=value(a);writes[a]=base+value(b);return base
            return value(a)+value(b)
        for part in op.split(' || '):
            if part.startswith('IF EQ '):
                if oldflag!=0:continue
                part=part[6:]
            if part=='NOP':continue
            if part.startswith('JUMP '):
                dest=int(part.split()[1],16)
                if '(DB)' in part:newpending=[2,dest]
                else:nextpc=dest
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
            if pending[0]==0:nextpc=pending[1];pending=None
        if newpending:pending=newpending
        pc=nextpc
    raise AssertionError('caller did not terminate')


class TailBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report=json.loads((OUT/'b_config_control.json').read_text())
        cls.mode=json.loads((OUT/'b_mode_control.json').read_text())

    def args(self, source='b01',bank=1,mode20='default'):
        pointers={int(k):v for name in ('16:default',f'20:{mode20}')
                  for k,v in self.mode['pointer_maps'][name].items()}
        return dict(effective_config=retained_config(self.report,source=source,bank=bank),
                    pointers=pointers,library_mode=5,
                    assume_successful_settled_allocations=True,zero_histories=True)

    def test_actual_loader_hash_and_instruction_pins(self):
        evidence=tail_binding_evidence()
        self.assertEqual(len(evidence['ranges']),8)
        self.assertEqual(len(evidence['instruction_pins']),33)

    def test_decoded_outer_callers_bind_all_tail_addresses(self):
        traces={key:caller_trace(f'first_b_{key}_channel_tail') for key in ('six','four','two')}
        def params(key,target,register='R8'):
            return [row[register] for row in traces[key] if row['target']==target]
        self.assertEqual(params('four',0x12EB9B),[10332,10376])
        self.assertEqual(params('four',0x12F411),[10331])
        self.assertEqual(params('six',0x12EB9B),[10444,10495,10546])
        self.assertEqual(params('six',0x12B90B),[10488,10491,10539,10542])
        self.assertEqual(params('six',0x12D10E),[21167,21168,21169,21170])
        self.assertEqual(params('six',0x132D0B,'R4'),[10422,21171,21172,21173,21157])
        self.assertEqual(params('two',0x12EB9B),[10615])
        self.assertEqual(params('two',0x12D10E),[10659,10667])
        self.assertEqual(params('two',0x12B90B),[10664,10672])
        self.assertEqual(params('two',0x132D0B,'R4'),[21158])

    def test_decoded_pair_uses_two_delays_gain_then_count(self):
        trace=caller_trace('first_b_delay_gain_eq')
        self.assertEqual([(r['target'],r['R8'] if r['target']!=0x132D0B else r['R4'])
                          for r in trace],[(0x12F411,20000),(0x12F411,20001),
                                           (0x132D0B,20002),(0x12DBDE,20003)])

    def test_all_retained_banks_bind_counts_and_explicit_zero_delay_cells(self):
        for source in ('loader','b01'):
            for bank in range(1,5):
                args=self.args(source,bank)
                b=bind_effective_tails(**args)
                counts=[len(q['eq_records']) for name in ('six_tail','four_tail') for q in b[name]['pair_configs']]
                self.assertEqual(counts,[7,5,6,2,1] if source=='loader' else [8]*5)
                self.assertFalse(b['metadata']['actual_vehicle_state_proven'])
                if source=='b01' and bank==4:
                    self.assertTrue(b['six_tail']['nested'][0][1]['allow_zero_delay_cells'])
                    self.assertEqual(b['six_tail']['nested'][0][1]['delay1'],0)

    def test_pointer_selected_gains_do_not_replace_inline_finish_gains(self):
        args=self.args();c=args['effective_config'];p=args['pointers']
        c[p[1156]]=0.25;c[122]=1.75;c[p[1157]]=0.4;c[293]=2.5
        b=bind_effective_tails(**args)
        self.assertEqual(b['four_tail']['final_gain'],.25)
        self.assertEqual(b['finish']['four_gain'],1.75)
        self.assertEqual(b['six_tail']['final_gain'],.4)
        self.assertEqual(b['finish']['six_gain'],2.5)

    def test_every_pair_field_matches_decoded_argument_addresses(self):
        for source in ('loader','b01'):
            args=self.args(source);cfg=args['effective_config'];bound=bind_effective_tails(**args)
            for name in ('four','six','two'):
                calls=caller_trace(f'first_b_{name}_channel_tail')
                bases=[r['R8']-10000-299 for r in calls if r['target']==0x12EB9B]
                pairs=bound[f'{name}_tail'].get('pair_configs',[bound[f'{name}_tail']])
                for base,pair in zip(bases,pairs,strict=True):
                    self.assertEqual(pair['channel_delays'],[round(v) for v in cfg[base:base+2]])
                    self.assertEqual(pair['pre_eq_gain'],cfg[base+2])
                    count=int(cfg[base+3]);self.assertEqual(len(pair['eq_records']),count)
                    np.testing.assert_array_equal(pair['eq_records'],np.array(cfg[base+4:base+4+count*5]).reshape(count,5))

    def test_enable_rounding_differs_and_disabled_records_are_not_executed(self):
        args=self.args();c=args['effective_config'];p=args['pointers']
        c[365:368]=[.9,0,9]  # TRUNC .9 ->0; do not validate unused delay/gain.
        c[p[1167]]=0.49;c[p[1167]+1]=0;c[p[1167]+2]=9
        b=bind_effective_tails(**args)
        self.assertIsNone(b['two_tail']['single'][0]);self.assertIsNone(b['six_tail']['nested'][0][0])
        c[p[1167]]=.5
        with self.assertRaises(ValueError):bind_effective_tails(**args)
        c[p[1167]+1:p[1167]+3]=[7.5,.25]
        b=bind_effective_tails(**args)
        self.assertEqual(b['six_tail']['nested'][0][0]['delay1'],8)
        c[365]=1
        with self.assertRaises(ValueError):bind_effective_tails(**args)

    def test_bound_mode5_matches_separately_instantiated_tails_and_finish(self):
        args=self.args();bound=bind_effective_tails(**args);spectral=pipeline()
        model=build_effective_mode5(spectral=spectral,input_mix_gain=.5,**args)
        rng=np.random.default_rng(781);pcm=rng.normal(0,.01,(3,512))
        branches=copy.deepcopy(spectral).process(pcm[0]+.5*pcm[2],pcm[1]+.5*pcm[2],mode=5)
        two=BTwoChannelTailReference(**bound['two_tail']).process(branches['main'].T)
        six=BSixChannelTailReference(**bound['six_tail']).process(branches['post'].T)
        four=BFourChannelTailReference(**bound['four_tail']).process(branches['pre'].T)
        expected=BOutputFinishReference(**bound['finish']).process(six,four,two)
        np.testing.assert_array_equal(model.process(*pcm),expected)
        np.testing.assert_array_equal(spectral.analysis_history,0)

    def test_bound_tail_streaming_partition_invariance(self):
        bound=bind_effective_tails(**self.args());x=np.random.default_rng(782).normal(0,.01,(700,2))
        for constructor,name in ((BSixChannelTailReference,'six_tail'),(BFourChannelTailReference,'four_tail'),(BTwoChannelTailReference,'two_tail')):
            a=constructor(**bound[name]);b=constructor(**bound[name])
            np.testing.assert_array_equal(a.process(x),np.concatenate([b.process(x[:31]),b.process(x[31:513]),b.process(x[513:])]))

    def test_reject_missing_state_invalid_pointer_mode_count_and_unstable_eq(self):
        mutations=[('zero_histories',False),('assume_successful_settled_allocations',False),('library_mode',6)]
        for k,v in mutations:
            args=self.args();args[k]=v
            with self.assertRaises(ValueError):bind_effective_tails(**args)
        for offset,value in ((36,0),(36,9),(33,-1),(37,float('nan')),(40,2)):
            args=self.args();args['effective_config'][offset]=value
            with self.assertRaises(ValueError):bind_effective_tails(**args)
        args=self.args();args['pointers'][1167]=828
        with self.assertRaises(ValueError):bind_effective_tails(**args)
        args=self.args();del args['pointers'][1162]
        with self.assertRaises(ValueError):bind_effective_tails(**args)

    def test_requested_zero_and_one_have_distinct_provenance_after_build(self):
        args=self.args('b01',4)
        zero=build_effective_mode5(spectral=pipeline(),input_mix_gain=.5,**args)
        for pointer in (1168,1170):args['effective_config'][args['pointers'][pointer]+1]=1
        one=build_effective_mode5(spectral=pipeline(),input_mix_gain=.5,**args)
        self.assertNotEqual(reference_fingerprint(zero),reference_fingerprint(one))
        self.assertEqual(zero.tail_binding_metadata['nested_requested_records']['1168'][1],0)
        self.assertEqual(one.tail_binding_metadata['nested_requested_records']['1168'][1],1)
        pcm=np.random.default_rng(783).normal(0,.01,(3,512))
        np.testing.assert_array_equal(zero.process(*pcm),one.process(*pcm))

    def test_float32_half_boundary_agrees_for_tail_and_finish_count(self):
        args=self.args();cfg=args['effective_config']
        below=float(np.nextafter(np.float32(.5),np.float32(0)))
        cfg[36]=cfg[294]=below
        bound=bind_effective_tails(**args)
        self.assertEqual(len(bound['four_tail']['pair_configs'][0]['eq_records']),1)
        self.assertEqual(len(bound['finish']['six_eq']),1)
        cfg[294]=float(np.nextafter(np.float32(2.5),np.float32(0)))
        # This one stays below2.5 after the float32 addition, count2.
        self.assertEqual(len(bind_effective_output(effective_config=cfg,library_mode=5,
            successful_filter_path=True)['finish_parameters']['six_eq']),2)

if __name__=='__main__':unittest.main()
