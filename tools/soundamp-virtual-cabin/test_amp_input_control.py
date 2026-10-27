"""Explicit input-control cases and a bounded interpreter of the V850 listing."""
import json
from pathlib import Path
import re
import unittest
from amp_input_control import (normal_level_target, select_level_targets,
                               gated_level_targets, eq_selector_step,
                               source_adjustment_request, override_mode_request,
                               b_level_parameter, b_level_apply)

OUT=Path(__file__).resolve().parents[2]/'assets/soundamp/remaining-blocks-20260919'
THRESHOLDS=[12,9,6,3,-3,-10,-12,-16,-22,-28,-33,-38,-44,-50,-60,-60]


def signed(value,bits):
    value &= (1<<bits)-1
    return value-(1<<bits) if value & (1<<(bits-1)) else value


def decoded_normal_target(raw,trim,active,inhibit,override):
    """Execute C6928..C69DE for lane0, not a reimplementation of the formula."""
    code={}
    for line in (OUT/'level_normal_target.asm').read_text().splitlines():
        match=re.match(r'0x([0-9a-f]+)\s+([0-9a-f]+)\s+(.*)',line)
        if match:
            address,hexbytes,op=match.groups()
            code[int(address,16)]=(len(hexbytes)//2,op.split(';')[0].strip())
    regs={f'r{i}':0 for i in range(32)}
    regs.update(gp=0x10000000,r14=0x10000000-15764,r8=0x10000000-15465,r12=0xA5)
    memory={regs['gp']+offset:value & 255 for offset,value in {
        -15764:0,-15465:active,-15056:inhibit,-15425:raw,-14992:trim,
        -17779:override,-15718:42}.items()}
    def value(text): return regs[text] if text in regs else int(text,0)
    def addr(text):
        m=re.fullmatch(r'(-?\d+)\[(\w+)\]',text)
        return regs[m[2]]+int(m[1])
    pc=0xC6928; lhs=rhs=0
    for _ in range(100):
        if pc==0xC69E0:
            return signed(memory[regs['gp']-14967],8),signed(memory[regs['gp']-15718],8)
        size,op=code[pc];parts=op.split(' ',1)
        name=parts[0];args=parts[1].split(', ') if len(parts)>1 else []
        next_pc=pc+size
        if name=='mov':regs[args[1]]=value(args[0])
        elif name=='movea':regs[args[2]]=value(args[0])+value(args[1])
        elif name=='addi':regs[args[2]]=value(args[0])+value(args[1])
        elif name=='add':regs[args[1]]=value(args[1])+value(args[0])
        elif name in ('ld.b','ld.bu'):
            v=memory[addr(args[0])];regs[args[1]]=signed(v,8) if name=='ld.b' else v
        elif name=='st.b':memory[addr(args[1])]=value(args[0]) & 255
        elif name in ('sxb','sxh'):regs[args[0]]=signed(value(args[0]),8 if name=='sxb' else 16)
        elif name=='cmp':lhs,rhs=value(args[1]),value(args[0])
        elif name in ('be','bne','ble','bnh','br'):
            take={'be':lhs==rhs,'bne':lhs!=rhs,'ble':signed(lhs,32)<=signed(rhs,32),
                  'bnh':(lhs & 0xffffffff)<=(rhs & 0xffffffff),'br':True}[name]
            if take:next_pc=int(args[0],0)
        else:raise AssertionError(f'unimplemented instruction {pc:#x}: {op}')
        pc=next_pc
    raise AssertionError('bounded V850 path did not finish')


class InputControlTests(unittest.TestCase):
    def test_trim_targeted_sentinel_and_muting_boundary(self):
        values=[[2] for _ in range(18)]
        r=source_adjustment_request(bytes([0x26,99,0x80]),values=values,composite=False)
        self.assertEqual(r['values'],values);self.assertEqual(r['changed_sources'],[])
        r=source_adjustment_request(bytes([0x26,99,243]),values=values,composite=False)
        self.assertEqual(r['changed_sources'],[5]);self.assertEqual(r['values'][5],[-13])
        self.assertEqual(normal_level_target(raw_level=100,source_trim=r['values'][5][0],active_flag=165,inhibit=0),-100)
        self.assertEqual(values[5],[2])

    def test_trim_broadcast_and_seven_record_mapping(self):
        v=[[0] for _ in range(18)]
        r=source_adjustment_request(bytes([255,0,13]),values=v,composite=False)
        self.assertEqual(r['changed_sources'],[0,1,2,3,5,9,17])
        self.assertEqual(r['values'][4],[0])
        r=source_adjustment_request(bytes([0,77,1,2,3,4,5,6,7]),values=v,composite=False)
        self.assertEqual([r['values'][i][0] for i in [0,1,2,3,5,9,17]],list(range(1,8)))

    def test_composite_trim_range_and_independent_sentinels(self):
        v=[[1,2,3] for _ in range(18)]
        r=source_adjustment_request(bytes([0x21,0,224,128,247]),values=v,composite=True)
        self.assertEqual(r['values'][0],[-32,2,-9])
        r=source_adjustment_request(bytes([255,0,32,9,128]),values=v,composite=True)
        self.assertEqual(r['values'][17],[32,9,3])

    def test_invalid_requests_are_atomic_and_unknown_selector_noop(self):
        v=[[0] for _ in range(18)]
        for payload in (bytes([0,0,1,2,3,4,5,6,14]),bytes([0x21,0,14]),bytes([0x21,0])):
            with self.assertRaises(ValueError):source_adjustment_request(payload,values=v,composite=False)
        self.assertEqual(v,[[0] for _ in range(18)])
        r=source_adjustment_request(bytes([0x25,0,14]),values=v,composite=False)
        self.assertFalse(r['selector_recognized']);self.assertFalse(r['recompute_requested'])
        with self.assertRaises(ValueError):
            source_adjustment_request(bytes([0x21,0,0,10,0]),values=[[0]*3 for _ in range(18)],composite=True)

    def test_override_enter_exit_and_deadline_wrap(self):
        r=override_mode_request(operation=3,length=1,mode=90,session_state=64,guard_flags=0,tick=7)
        self.assertEqual((r['mode'],r['session_state'],r['guard_flags']),(165,192,16))
        self.assertTrue(r['event_9_28'])
        r=override_mode_request(operation=0,length=0,mode=165,session_state=192,guard_flags=16,tick=0xffffffff)
        self.assertEqual((r['mode'],r['session_state'],r['guard_flags'],r['deadline']),(90,64,0,4999))
        self.assertTrue(r['event_9_28'])

    def test_override_errors_and_noop_do_not_claim_mode_change(self):
        base=dict(operation=3,length=1,mode=90,session_state=64,guard_flags=0,tick=0)
        for change,err in [({'guard_flags':1},36),({'length':0},19),({'operation':2},49)]:
            r=override_mode_request(**dict(base,**change));self.assertEqual(r['error'],err)
            self.assertEqual(r['mode'],90);self.assertFalse(r['event_9_28'])
        r=override_mode_request(**dict(base,operation=0));self.assertEqual(r['mode'],90)
        self.assertIsNone(r['deadline'])
        self.assertFalse(override_mode_request(**dict(base,mode=165))['event_9_28'])

    def test_b_level_uses_selected_target_not_smoothed_eq_word(self):
        for level,wanted in [(-128,0),(-100,0),(-30,70),(0,100),(26,126)]:
            self.assertEqual(b_level_parameter(level),wanted)
        self.assertEqual(self.step(level_word=-50*(1<<24))['candidate_index'],13)
        self.assertEqual(b_level_parameter(-30),70) # new target, old ramp still -50

    def test_b_level_shadow_effective_and_curve_coordinate(self):
        r=b_level_apply(70,previous_effective=50,baseline=60,defer_word=0)
        self.assertEqual((r['shadow_1151'],r['effective_1137'],r['helper_delta']),(70,70,10))
        self.assertTrue(r['curve_helper_called']);self.assertFalse(r['actual_audio_effect_proven'])
        r=b_level_apply(70,previous_effective=50,baseline=60,defer_word=1)
        self.assertEqual(r['effective_1137'],50);self.assertEqual(r['shadow_1151'],70)
        self.assertIsNone(r['helper_delta']);self.assertFalse(r['curve_helper_called'])
        for v in (-201,201,float('nan')):
            with self.assertRaises(ValueError):b_level_apply(v,previous_effective=0,baseline=0,defer_word=0)

    def test_normal_levels_and_upper_cap(self):
        for raw,wanted in [(0,-100),(1,-99),(50,-50),(70,-30),(100,0),(126,26)]:
            self.assertEqual(normal_level_target(raw_level=raw,source_trim=0,active_flag=165,inhibit=0),wanted)
        self.assertEqual(normal_level_target(raw_level=126,source_trim=12,active_flag=165,inhibit=0),26)
        self.assertEqual(normal_level_target(raw_level=1,source_trim=-12,active_flag=165,inhibit=0),-111)

    def test_decoded_firmware_normal_path_matches_valid_domain(self):
        for raw in (0,1,40,50,70,100,126):
            for trim in (-128,-13,-12,-1,0,12,127):
                for active,inhibit,override in [(165,0,90),(165,0,165),(90,0,90),(165,1,90)]:
                    wanted=normal_level_target(raw_level=raw,source_trim=trim,active_flag=active,inhibit=inhibit)
                    cached,selected=decoded_normal_target(raw,trim,active,inhibit,override)
                    self.assertEqual(cached,wanted)
                    self.assertEqual(selected,42 if override==165 else wanted)

    def test_all_mute_and_per_lane_conditions(self):
        args=dict(gate_a=90,gate_b=90,flags=0,mapped_active_flags=[165]*4)
        self.assertEqual(gated_level_targets([-111,-100,-30,26],**args),[-100,-100,-30,26])
        for key in ('gate_a','gate_b'):
            self.assertEqual(gated_level_targets([0]*4,**dict(args,**{key:165})),[-100]*4)
        for flags in (1,2,3):
            self.assertEqual(gated_level_targets([0]*4,**dict(args,flags=flags)),[-100]*4)
        for flags in (4,8,16,32):
            self.assertEqual(gated_level_targets([0]*4,**dict(args,flags=flags)),[-100,0,0,0])
        self.assertEqual(gated_level_targets([0]*4,**dict(args,mapped_active_flags=[165,90,165,165])),[0,-100,0,0])

    def test_override_selection_does_not_infer_actual_mode(self):
        self.assertEqual(select_level_targets([0,-10,-20,-30],override_mode=90,override_level=-30),[0,-10,-20,-30])
        self.assertEqual(select_level_targets([0]*4,override_mode=165,override_level=-30),[-30,-100,-100,-100])

    def step(self,**changes):
        args=dict(level_word=-30*(1<<24),thresholds=THRESHOLDS,override_mode=90,
                  normal_enable=165,override_enable=90,commit_gate=165,previous_enable=165,previous_index=7)
        args.update(changes)
        return eq_selector_step(**args)

    def test_commit_and_disabled_writer_target_are_distinct(self):
        r=self.step();self.assertEqual(r['writer_target_if_delivered'],10)
        self.assertTrue(r['event17_queued']);self.assertFalse(r['actual_sharc_bank_proven'])
        r=self.step(commit_gate=90);self.assertEqual(r['candidate_index'],10)
        self.assertEqual(r['writer_target_if_delivered'],7);self.assertFalse(r['event17_queued'])
        r=self.step(normal_enable=90)
        self.assertEqual(r['cached_index'],10);self.assertEqual(r['writer_target_if_delivered'],0)
        self.assertTrue(r['event17_queued'])

    def test_override_enable_and_unchanged_state(self):
        self.assertEqual(self.step(override_mode=165)['writer_target_if_delivered'],0)
        self.assertFalse(self.step(previous_index=10)['event17_queued'])
        self.assertTrue(self.step(previous_index=10,previous_enable=90)['event17_queued'])

    def test_protocol_level_examples_and_fractional_ramp(self):
        for raw,bank in [(40,14),(50,13),(70,10),(80,8),(90,5),(100,4),(112,0)]:
            target=normal_level_target(raw_level=raw,source_trim=0,active_flag=165,inhibit=0)
            self.assertEqual(self.step(level_word=target*(1<<24))['candidate_index'],bank)
        # -29.5 truncates to -29, still bank10; -27.5 truncates to -27 ->bank9.
        self.assertEqual(self.step(level_word=-59*(1<<23))['candidate_index'],10)
        self.assertEqual(self.step(level_word=-55*(1<<23))['candidate_index'],9)

    def test_validation_and_audit_identity(self):
        report=json.loads((OUT/'level_control.json').read_text())
        self.assertEqual(report['selector']['thresholds'],THRESHOLDS)
        self.assertEqual(report['normal_source']['request_source_indices'],[0,1,2,3,5,9,17])
        for raw in (-1,127,True):
            with self.assertRaises(ValueError):normal_level_target(raw_level=raw,source_trim=0,active_flag=165,inhibit=0)
        with self.assertRaises(ValueError):self.step(previous_index=16)
        with self.assertRaises(ValueError):self.step(level_word=1<<31)


if __name__=='__main__':unittest.main()
