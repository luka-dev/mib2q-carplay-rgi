"""Static extraction invariants; not claims about selected playback state."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_amp_output_bindings import audit, loader_dm
from audit_amp_remaining_blocks import complex_parameters, first_b_output_routes, first_b_spectral_bindings, first_b_activation_audit, first_b_mode5_bindings, first_b_input_config, RANGES
from extract_amp_sharc_app import parse_blocks
from inspect_amp_visa import load_shortwords, decode
from audit_amp_input_path import input_path_parameters

import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
IMAGE=Path(_WS + '/Firmwares/SoundAmp/'
           'uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin')
LOADER=ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'


@unittest.skipUnless(IMAGE.exists() and LOADER.exists(),'local fixed images required')
class RemainingExtractionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=IMAGE.read_bytes()
        cls.report=audit(cls.data)  # reject wrong image identity
        loader=LOADER.read_bytes()
        cls.mem=load_shortwords(loader)  # reject wrong loader identity
        cls.dm=loader_dm(loader,parse_blocks(loader))
        cls.profiles=[complex_parameters(cls.data,cls.dm,p) for p in (0,1)]

    def test_startup_signature_selects_calibrated_copy_in_dump(self):
        self.assertTrue(self.profiles[0]['startup_calibration_flag_B64E0']['matches_dump'])
        self.assertEqual(self.profiles[0]['startup_calibration_flag_B64E0']['signature_hex'],
                         b'AD301A07'.hex())

    def test_first_b_boot_gate_is_not_live_silence_proof(self):
        r=first_b_activation_audit(self.data,self.dm,self.mem)
        self.assertEqual(r['callback_gate']['loader_value'],0)
        self.assertEqual(r['loader_callback_state'],1)
        self.assertFalse(r['live_B_zero_proven'])
        self.assertEqual(decode(self.mem,0x124F70)[1],'IF NE JUMP 0x12503c')

    def test_first_b_bypass_is_copy_not_mute(self):
        r=first_b_activation_audit(self.data,self.dm,self.mem)
        self.assertEqual(r['loader_graph_class'],{'process':'0x124df3','bypass':'0x124d64'})
        self.assertEqual(r['graph_bypass']['output_input_indices'],[0,1,0,1,0,1,0,1])

    def test_first_b_gate_one_clears_zero_reaches_library(self):
        # Follow the decoded short branch, not the audit's prose condition.
        # This is a bounded branch check, NOT full SHARC emulation.
        for gate, expected_call in [(0,0x12DE3B),(1,0x1257D0),
                                    (2,0x12DE3B),(3,0x12DE3B),
                                    (0xffffffff,0x12DE3B)]:
            pc=0x124F73
            sz=False
            for _ in range(16):
                size,insn,target=decode(self.mem,pc)
                if insn=='R2=DM(0xbc9ec)':
                    r2=gate
                elif insn=='R2=BTGL R2 BY 0':
                    r2 ^= 1
                    sz = r2 == 0
                elif insn.startswith('IF SZ JUMP '):
                    if sz:
                        pc=target
                        continue
                elif insn.startswith('CJUMP '):
                    self.assertEqual(target,expected_call,hex(gate))
                    break
                elif insn not in ('R4=DM(0xbc9eb)','R8=R4',
                                  'R4=DM(0xbc9ea)','R12=0x1000','R8=M5'):
                    self.fail(f'unexpected instruction {pc:#x}: {insn}')
                pc += size
            else:
                self.fail('no call reached')
        r=first_b_activation_audit(self.data,self.dm,self.mem)
        self.assertEqual(r['callback_gate']['inhibit_value'],1)

    def test_first_b_graph_command_class_offsets(self):
        r=first_b_activation_audit(self.data,self.dm,self.mem)['graph_commands']
        self.assertEqual(r['0x20001'],{'selector':1,'class_offset':1,'handler':'0x124df3'})
        self.assertEqual(r['0x20003'],{'selector':3,'class_offset':2,'handler':'0x124d64'})
        self.assertEqual(self.dm[0xC53E1],0)  # no class-flags override
        self.assertEqual(self.data[0xAC846:0xAC84C].hex(),'2f0603000200')
        self.assertEqual(self.data[0xAC866:0xAC86C].hex(),'320601000200')
        self.assertEqual(self.data[0xAC880:0xAC886].hex(),'2d06d94a0c00')

    def test_first_b_graph_trace_slot_indirection(self):
        for pc,insn in [(0x1292E4,'I4=DM(+16,I4)'),
                        (0x1292E6,'I4=MODIFY(I4,+31)'),
                        (0x1292E9,'R1=DM(I4,M6)'),
                        (0x1292EA,'COMP(R4,R1) || R0=I4'),
                        (0x1292ED,'IF NE R0=R4+R2')]:
            self.assertEqual(decode(self.mem,pc)[1],insn)

    def test_first_b_bulk_queue_includes_bypass_then_process(self):
        # Bounds of three ascending loops, read from V850 movea immediates.
        import struct
        bounds=[struct.unpack_from('<H',self.data,pc+2)[0]
                for pc in (0xC28D6,0xC2902,0xC2906,0xC292E,0xC2932)]
        self.assertEqual(bounds,[30,30,33,33,54])
        first_end,second_start,second_end,third_start,third_end=bounds
        events=list(range(first_end))+list(range(second_start,second_end))+list(range(third_start,third_end))
        self.assertEqual(events,list(range(54)))
        self.assertLess(events.index(5),events.index(44))
        self.assertEqual(self.data[0xC264A:0xC264E].hex(),'61eaaa1d')

    def test_first_b_mode_parameter_writer_table_is_two_or_five(self):
        r=first_b_activation_audit(self.data,self.dm,self.mem)['mode_control']
        self.assertEqual(r['v850_float_table'],[2,5,5])
        self.assertEqual(self.data[0xB6B86:0xB6B8A].hex(),'440770c2')
        self.assertEqual(self.data[0xAA4BA:0xAA4BE].hex(),'20361000')
        self.assertEqual(self.data[0xAC68C:0xAC692].hex(),'3106e14a0c00')
        self.assertEqual(self.dm[0xB914B],0x132434)
        self.assertEqual(1149-2-19, r['object_offset'])

    def test_first_b_mode5_preparation_pins_calls_and_parallel_old_pointer(self):
        r=first_b_mode5_bindings(self.data,self.mem)
        self.assertEqual(r['gain_object_offset'],97)
        self.assertEqual(r['input_graph_ports'],[24,23,22])
        self.assertEqual(r['formula'],['S0 + gain*S2','S1 + gain*S2'])
        for pc,insn in [(0x13123F,'R1=R3+R14 || DM(-21,I6)=R12'),
                        (0x131245,'R2=R3+R15 || R8=DM(M5,I3)'),
                        (0x131272,'R12=R7+R15'),
                        (0x131274,'R8=R7+R14 || DM(-24,I6)=R12'),
                        (0x131277,'R4=R7+R5 || DM(-30,I6)=R8'),
                        (0x13127A,'R1=R7+R13 || DM(-23,I6)=R4')]:
            self.assertEqual(decode(self.mem,pc)[1],insn)
        # At1315D3, R12 still=P+512. The parallel store saves OLD R12,
        # not the new R2;1315D6 then replaces R12 with frame count.
        self.assertEqual(decode(self.mem,0x1315D6)[1],'R8=PASS R2 || R12=R10')

    def test_first_b_matrix_bank_one_selects_library_five(self):
        r=first_b_mode5_bindings(self.data,self.mem)
        self.assertEqual(r['sink26_raw_bank'],1)
        self.assertEqual(r['bank_to_library_mode'][r['sink26_raw_bank']],5)
        self.assertEqual(r['source_to_bank_table'][0],{'sink':'0x21','raw_bank':0})
        self.assertEqual(r['source_to_bank_table'][-1],{'sink':'0x32','raw_bank':2})
        # SAME gp byte as AA4A4's API16 writer;<<6 selects a64-byte bank.
        self.assertEqual(self.data[0xC2852:0xC2858].hex(),'849f71c2c69a')
        self.assertEqual(self.data[0xAA4A4:0xAA4AC].hex(),'849771c2c2923306')
        self.assertEqual(self.data[0xC2886:0xC288A].hex(),'20363000')

    def test_first_b_sink_index_uses_sink_minus_0x21(self):
        self.assertEqual(self.data[0xD0036:0xD0040].hex(),'118edfff5489448f84c2')

    def test_first_b_gain_copy_offset_and_both_source_values(self):
        import math
        r=first_b_input_config(self.data,self.dm,self.mem,self.report)['gain']
        self.assertEqual(r['copy'],{'source_start':38,'object_start':92,'words':207})
        self.assertEqual(r['source_base'],'0xc3c6a')
        self.assertEqual(r['source_word_offset'],43)
        self.assertEqual(r['loader'],{'address':'0xc3c95','word':'0x3f000000','linear':.5})
        self.assertEqual(r['b01']['header'],'AD301B01')
        self.assertEqual(r['b01']['count'],3562)
        self.assertEqual(r['b01']['address'],'0x1200b8')
        self.assertEqual(r['b01']['word'],'0x3f004dce')
        self.assertAlmostEqual(20*math.log10(r['b01']['linear']),-6,places=6)
        self.assertFalse(r['live_value_proven'])

    def test_first_b_gain_is_below_registered_interpolation_base(self):
        r=first_b_input_config(self.data,self.dm,self.mem,self.report)['gain_interpolation']
        self.assertEqual(r['base_object_offset'],299)
        self.assertEqual(r['gain_object_offset'],97)
        # Follow unsigned comparison to the early return before target lookup
        # or a gain write, for representative valid object base addresses.
        for base in (0,0x80000,0xC0000):
            pc=0x12FAF9
            size,text,_=decode(self.mem,pc)
            self.assertEqual(text,'R2=DM(0xb903a)')
            r2=base+r['base_object_offset']; r4=base+r['gain_object_offset']
            pc+=size
            size,text,_=decode(self.mem,pc)
            self.assertEqual(text,'COMPU(R2,R4)')
            gt=(r2 & 0xffffffff) > (r4 & 0xffffffff)
            pc+=size
            _,text,target=decode(self.mem,pc)
            self.assertEqual(text,'IF GT JUMP 0x12fb36')
            self.assertTrue(gt)
            self.assertEqual(target,0x12FB36)

    def test_first_b_input_graph_is_before_not_after_a07_matrix(self):
        r=first_b_input_config(self.data,self.dm,self.mem,self.report)
        stages=r['upstream_nodes']
        self.assertEqual(len(stages),11)
        self.assertEqual(stages[0]['descriptor'],'0xc4a54')
        self.assertEqual(stages[-2]['port_ids'],[42,24,23])
        self.assertEqual(stages[-1]['port_ids'],[43,22,21])
        descriptors=[n['descriptor'] for n in self.report['graph_nodes']]
        self.assertLess(descriptors.index('0xc4ad9'),descriptors.index('0xc4aed'))

    def test_first_b_input_eq_distinct_record_orders_and_sizes(self):
        r=first_b_input_config(self.data,self.dm,self.mem,self.report)
        self.assertEqual(r['shared_eq']['sections'],5)
        self.assertEqual(r['shared_eq']['record_order'],['-a1','-a2','b0','b1','b2'])
        self.assertEqual(r['shared_eq']['current'],'0xe004e')
        self.assertEqual(r['shared_eq']['target'],'0xe0068')
        self.assertEqual(r['bank_eq']['sections'],3)
        self.assertEqual(r['bank_eq']['banks'],16)
        self.assertEqual(r['bank_eq']['record_order'],['-a2','-a1','b2','b1'])
        self.assertFalse(r['bank_eq']['channel_mixing'])
        self.assertEqual(decode(self.mem,0x1261A0)[1],'M4=0x2')
        self.assertEqual(decode(self.mem,0x1261A8)[1],'M12=M4')

    def test_first_b_input_audit_rejects_changed_copy_and_upload(self):
        mem=dict(self.mem); mem[0x12B2CA]^=1
        with self.assertRaises(ValueError):
            first_b_input_config(self.data,self.dm,mem,self.report)
        data=bytearray(self.data); data[0xAD038]^=1
        with self.assertRaises(ValueError):
            first_b_input_config(data,self.dm,self.mem,self.report)

    def test_input_path_calibrated_banks_and_selector_thresholds(self):
        p=input_path_parameters(self.data,self.dm,self.mem,0)
        self.assertTrue(p['calibration']['signature_matches_dump'])
        bank=p['bank_eq']
        self.assertEqual([r['frequency_selector'] for r in bank['templates']],[55,10000,4500])
        self.assertEqual([r['q_code'] for r in bank['templates']],[8,7,8])
        self.assertEqual(bank['gains_tenths_db'][0],[0,-35,-30])
        self.assertEqual(bank['gains_tenths_db'][7],[0,0,0])
        self.assertEqual(bank['gains_tenths_db'][15],[205,0,80])
        self.assertEqual(bank['level_selector']['thresholds'],[12,9,6,3,-3,-10,-12,-16,-22,-28,-33,-38,-44,-50,-60,-60])
        self.assertEqual(bank['level_selector']['input_gp_relative'],-0x77F8)
        self.assertEqual(self.data[0xB6A96:0xB6A9A].hex(),'4407d7c1')

    def test_input_path_prefilter_unity_gain_and_two_shared_eq_families(self):
        p=input_path_parameters(self.data,self.dm,self.mem,0)
        self.assertEqual(p['input_gain']['target_linear'],[1]*8)
        self.assertEqual(p['prefilter']['targets'],[0xE0038,0xE05AC])
        self.assertEqual(p['prefilter']['records'][0]['type_code'],0)
        eq=p['shared_eq']
        self.assertEqual([r['type_code'] for r in eq['tone']['templates']],[26,1,27])
        self.assertEqual([row[9] for row in eq['tone']['position_tables_tenths_db']],[0]*3)
        self.assertEqual([r['frequency_selector'] for r in eq['graphic']['records']],[63,250,1000,4000,16000])
        self.assertEqual(eq['tone']['fourth_template']['frequency_selector'],90)

    def test_input_delay_writer_compensation_and_capacity(self):
        for profile in (0,1):
            p=input_path_parameters(self.data,self.dm,self.mem,profile)['delay']
            self.assertEqual(p['minimum_candidate_samples'],1719)
            self.assertEqual(p['fixed_compensation_samples'],1248)
            self.assertEqual(p['maximum_samples'],9600)
            self.assertEqual(self.dm[0xC4A54+13],8*9600)
            self.assertEqual(self.dm[0xC4A54+12],8*9600+64)
            self.assertEqual(self.dm[0xC4A54+14],0xE0004)
            self.assertEqual(self.dm[0xC4A54+15],0xE000C)

    def test_input_delay_indirect_dispatch_wait_is_clear_not_process(self):
        p=input_path_parameters(self.data,self.dm,self.mem,0)['delay']
        self.assertEqual(p['state_dispatch'],{'0':'0x12465a','1':'0x12467c',
                                              '2':'0x1245b2','3':'0x12470d'})
        self.assertEqual(decode(self.mem,0x1245AB)[1],'R2=0x3')
        self.assertEqual(decode(self.mem,0x1245C2)[1],'R1=0x3')
        self.assertEqual(decode(self.mem,0x1245C5)[1],'IF NE JUMP 0x1245e8')
        self.assertEqual(decode(self.mem,0x1245DB)[2],0x1257D0)
        self.assertEqual(decode(self.mem,0x1244E3)[1],'R2=BTGL R2 BY 1')
        self.assertEqual(decode(self.mem,0x1244FF)[1],'R2=BTGL R2 BY 2')

    def test_input_delay_zero_branch_is_copy_and_not_cross_channel_sum(self):
        self.assertEqual(decode(self.mem,0x12479E)[1],'IF LE JUMP 0x12486c')
        self.assertEqual(decode(self.mem,0x12486C)[1],'R2=R2+R10 || I4=DM(M5,I3)')
        self.assertEqual(decode(self.mem,0x12486F)[1],'R12=PASS R3 || R8=I4')
        self.assertEqual(decode(self.mem,0x12487A)[2],0x1257B2)
        self.assertEqual(decode(self.mem,0x1247F5)[1],'R13-=R2')
        self.assertEqual(decode(self.mem,0x1247F8)[1],'IF LT R13=R13+R15')

    def test_input_writer_pins_reject_changed_control_or_delay_constants(self):
        for pc in (0xA76C2,0xA9E28,0xB0080):
            data=bytearray(self.data); data[pc]^=1
            with self.assertRaises(ValueError):input_path_parameters(data,self.dm,self.mem,0)

    def test_input_eq_feeds_b_and_a_matrix_not_b_only(self):
        nodes={n['descriptor']:n for n in self.report['graph_nodes']}
        self.assertEqual(nodes['0xc4ad9']['port_ids'][:3],[24,23,22])
        self.assertEqual(nodes['0xc4aed']['port_ids'][:8],[24,23,22,21,50,51,36,37])
        self.assertEqual(nodes['0xc4ae3']['port_ids'],[44,50,51])
        self.assertEqual(nodes['0xc4ae8']['port_ids'],[45,36,37])

    def test_explicit_profile_cap_codes_and_unity_other_gain(self):
        self.assertEqual([p['cap_code_AAFC0'] for p in self.profiles],[15,10])
        for p in self.profiles:
            self.assertEqual(p['other_base_AAD70'],[1]*5)
            self.assertTrue(p['five_band_writer_equals_loader'])
            self.assertEqual(p['stage8_loader_delays'],[{'requested':1023,'maximum':1023}]*5)
            self.assertEqual(p['profile_selector']['startup_value'],0)

    def test_common_gain_is_skipped_by_calibrated_field34(self):
        for p in self.profiles:
            fields={c['descriptor_offset']:c['calibrated'] for c in p['controls_AAC88']}
            self.assertEqual(fields[34],1)
            self.assertEqual(fields[17],24)
            self.assertEqual(fields[38],0)
        self.assertEqual(decode(self.mem,0x127310)[1],'IF NOT SZ JUMP 0x127346')

    def test_link_branch_direction_and_ramp_constant(self):
        self.assertEqual(decode(self.mem,0x1276A1)[1],'IF NOT SZ JUMP 0x1276fd')
        self.assertEqual(decode(self.mem,0x1277FB)[1],'R4=0x3a800000') # 1/1024

    def test_db_conversion_is_point_zero_five_not_point_one(self):
        import struct
        factor=struct.unpack('<f',struct.pack('<I',0x3d4ccccd))[0]
        self.assertAlmostEqual(factor,.05,places=8)
        for pc in (0x127569,0x1275CF,0x12761D,0x127648):
            self.assertEqual(decode(self.mem,pc)[1],'R6=0x3d4ccccd')

    def test_calibration_loader_is_not_live_enable_state(self):
        p=self.profiles[0]['calibration_loader']
        self.assertEqual(p['master_enable'],0)
        self.assertEqual(p['initialized'],0)
        self.assertEqual(p['phase'],4)
        self.assertEqual(p['channel_mask'],1023)
        self.assertEqual(p['wait_samples'],144000)
        self.assertEqual(p['measure_samples'],5760000)
        self.assertEqual(p['noise_seed'],1)
        self.assertAlmostEqual(p['auxiliary_scalar'],1.9952623,places=6)

    def test_all_remaining_ranges_decode_and_branch_targets_align(self):
        for name,(start,end) in RANGES.items():
            pc=start;instructions={};targets=[]
            while pc<end:
                n,s,t=decode(self.mem,pc)
                self.assertNotIn('UNKNOWN',s,(name,hex(pc)))
                instructions[pc]=s
                if t is not None:targets.append(t)
                pc+=n
            self.assertEqual(pc,end,name)
            for t in targets:
                if start<=t<end:self.assertIn(t,instructions,name)

    def test_real_fft_shared_kernel_and_window_call_bindings(self):
        for pc in (0x12B5BF, 0x133186):
            self.assertEqual(decode(self.mem, pc)[2], 0x12EF65)
        for pc in (0x12E8C8, 0x12E8F0, 0x12E7C4, 0x12E7E9):
            self.assertEqual(decode(self.mem, pc)[2], 0x130AEB)
        self.assertEqual(decode(self.mem, 0x130A9C)[2], 0x125C2B)
        self.assertEqual(decode(self.mem, 0x13313E)[1], 'F2=SCALB F14 BY R0')

    def test_loaded_fft_twiddles_negative_angle_and_unit_scale(self):
        import math
        import struct
        values = [struct.unpack('<f', struct.pack('<I', self.dm[0xE4754+i]))[0]
                  for i in range(1024)]
        errors = [abs(complex(values[2*k], values[2*k+1])-
                      complex(math.cos(2*math.pi*k/1024), -math.sin(2*math.pi*k/1024)))
                  for k in range(512)]
        self.assertEqual(values[:2], [1, 0])
        self.assertLess(max(errors), 1e-7)

    def test_spatial_control_and_branch_bindings(self):
        for pc, target in ((0x13036C, 0x12A530), (0x130331, 0x12D791),
                           (0x130267, 0x131899), (0x130862, 0x13027B),
                           (0x1308F7, 0x12FFBA), (0x130956, 0x1304E4)):
            self.assertEqual(decode(self.mem, pc)[2], target)
        for pc, expected in ((0x13023E, 'R2=0x3f7d70a4'),
                (0x130324, 'R2=0x9'), (0x13032D, 'R8=0x33'),
                (0x130341, 'R4=MANT F1 || R8=DM(M6,I6)'),
                (0x1302B5, 'R4=0x25800000'), (0x130730, 'R4=0x3089705f'),
                (0x130924, 'R2=BTGL R3 BY R9'), (0x130929, 'IF SZ JUMP 0x1309e0')):
            self.assertEqual(decode(self.mem, pc)[1], expected)
        self.assertEqual(decode(self.mem, 0x130210)[2], 0x1244A6)
        self.assertEqual(decode(self.mem, 0x130204)[1], 'R2=DM(I5,M5)')
        self.assertEqual(decode(self.mem, 0x13020C)[1], 'F2*=F0')
        self.assertEqual(decode(self.mem, 0x13005D)[1], 'R2=BTGL R2 BY R8 || R1=DM(-5,I6)')

    def test_normalize_pointer_is_computed_power_workspace_not_supplied_input(self):
        # Pointer PASS sets flags, but count PASS overwrites them before EQ jump.
        self.assertEqual(decode(self.mem,0x1317BD)[1], 'R2=PASS R2 || R0=DM(+2,I6)')
        self.assertEqual(decode(self.mem,0x1317C0)[1], 'IF EQ R2=R8')
        self.assertEqual(decode(self.mem,0x1317C2)[1], 'R1=PASS R0 || I5=R4')
        self.assertEqual(decode(self.mem,0x1317C7)[1], 'IF EQ JUMP 0x1317f9 (DB)')

    def test_spectral_wrappers_scratch_and_state_bindings(self):
        r=first_b_spectral_bindings(self.mem)
        self.assertEqual(r['size']['complex_bins'],513)
        self.assertEqual(r['post_bindings']['magnitude_smoother_count'],256)
        self.assertEqual([r['mode2_tail_routes'][k]['channels'] for k in ('pre','main','post')],[4,2,6])
        for pc,expected in ((0x1315B3,'R12=0x201'),(0x12E334,'R10=0xd3'),
                (0x12E588,'R14=0x2d'),(0x12E6F4,'R12=0x100'),(0x12E706,'R12=0x100'),
                (0x12E780,'R2=0x7b'),(0x12F3A0,'R2=0x4a'),(0x12F3A2,'R1=0xde')):
            self.assertEqual(decode(self.mem,pc)[1],expected)

    def test_fir_profiles_must_not_be_silently_interchanged(self):
        a,b=[p['fir_bank_AAA60'] for p in self.profiles]
        self.assertEqual(a['nonzero_words'],5120)
        self.assertEqual(b['nonzero_words'],10)
        self.assertNotEqual(a['sha256'],b['sha256'])
        self.assertGreater(a['max_abs'],.2)
        self.assertLess(b['max_abs'],1e-40)

    def test_reject_unaudited_profile(self):
        with self.assertRaises(ValueError): complex_parameters(self.data,self.dm,2)

    def test_b_tail_routes_to_attenuated_row1_slots_only(self):
        routes=first_b_output_routes(self.report)
        self.assertEqual([r['graph_port'] for r in routes],[20,19,18,17,34,35,40,41])
        self.assertEqual([r['mixer_b_slot'] for r in routes],[0,1,4,5,6,7,8,9])
        self.assertEqual([r['stored_row1_db'] for r in routes],[0]*6+[-99]*2)
        self.assertEqual([r['mixer_output_port'] for r in routes[-2:]],[58,59])

    def test_nested_allpass_parallel_feedback_equations(self):
        self.assertEqual(decode(self.mem,0x12D187)[1],'F8=F2*F4, F12=F8-F12')
        self.assertEqual(decode(self.mem,0x12D189)[1],'F8=F12-F8')
        self.assertEqual(decode(self.mem,0x12D18B)[1],'F8=F8*F4 || DM(M3,I2)=R8')
        self.assertEqual(decode(self.mem,0x12D197)[1],'F12=F12+F11')

    def test_spectral_seed_clamps_and_half_scaling_operands(self):
        for pc,expected in {
            0x13178A:'F2=RSQRTS F4',
            0x1317D0:'R8=0x7149f2ca',0x1317DF:'R12=0x2edbe6ff',
            0x1317FD:'R8=0x1e3ce508',
            0x12E40C:'R4=0x3f000000',0x12E432:'R4=0x3f000000',
            0x12B7FF:'F1=F4-F8',0x12EC9F:'F1=F4+F8',
        }.items():self.assertEqual(decode(self.mem,pc)[1],expected)

    def test_six_channel_fanout_and_finish_call_order(self):
        def calls(start,end):
            pc=start;result=[]
            while pc<end:
                n,s,t=decode(self.mem,pc)
                if s.startswith('CJUMP'):result.append(t)
                pc+=n
            return result
        # Filter out only explicit error reporting, not arbitrary callees.
        self.assertEqual([c for c in calls(0x12A7F9,0x12AB01) if c!=0x12B6A7],
            [0x132E08]*2+[0x132D0B,0x12F411]+[0x12D10E]*4+[0x132E08]*4+
            [0x12EB9B,0x12B90B,0x12B90B,0x132D0B]*2+
            [0x12EB9B,0x132D0B,0x132D0B])
        self.assertEqual([c for c in calls(0x12EDE6,0x12EF05) if c!=0x12B6A7],
            [0x132E08]*2+[0x12F411]+[0x132E08]*2+[0x12EB9B]*2+[0x132D0B])
        self.assertEqual([c for c in calls(0x12B87B,0x12B90B) if c!=0x12B6A7],
            [0x12DBDE,0x132D0B])

    def test_dispatch_compares_modes_two_and_five_not_a_generic_bit_enable(self):
        for pc,expected in {
            0x131315:'R1=BTGL R2 BY 1',
            0x131318:'IF SZ JUMP 0x131444',
            0x13131B:'R1=0x5',0x13131D:'COMP(R2,R1)',
            0x13131E:'IF EQ JUMP 0x131444',
            0x1313A4:'R0=0x6',0x1313A9:'IF NE JUMP 0x1313d6',
        }.items():self.assertEqual(decode(self.mem,pc)[1],expected)

    def test_level_mask_db_factor_and_power_log_offset(self):
        for pc,expected in {
            0x12F8B4:'R13=0x3dcccccd',0x12F8C9:'F8=F2*F13 || DM(I1,M5)=R2',
            0x12F9C1:'R4=0x3f800000',0x12F9E1:'R4=0x41200000',
            0x13195C:'R1=0xda24260',0x13195F:'F2=ABS(F7-F2) || I3=R4',
        }.items():self.assertEqual(decode(self.mem,pc)[1],expected)

    def test_transient_mask_is_74_bands_with_square_after_division(self):
        for pc,expected in {
            0x12F166:'LCNTR=74, DO 0x12f183 UNTIL LCE',
            0x12F23F:'R8=DM(M7,I2)',0x12F241:'F0=F8-F1 || R2=DM(I1,M5)',
            0x12F247:'F2*=F0',0x12F248:'F4=F2+F1',
            0x12F24D:'F2=F0*F0',
            0x12D3D9:'JUMP 0x12d38b (DB)',
        }.items():self.assertEqual(decode(self.mem,pc)[1],expected)

    def test_scheduler_and_detector_block_layout(self):
        self.assertEqual(decode(self.mem,0x1267AA)[1],'R11=DM(+9,I5)')
        self.assertEqual(decode(self.mem,0x1267E7)[1],'R8=PASS R15 || R12=R11')
        self.assertEqual(decode(self.mem,0x127A96)[1],'M4=0x2')
        for p in self.profiles:
            d=p['detector_layout']
            self.assertEqual([d[k] for k in ('audio_frames','detector_frames','history_blocks')],[64,4,16])
            self.assertEqual(d['decimation_stride'],16)
            self.assertEqual(d['sample_indices'],[0,16,32,48])
            self.assertEqual(d['reference_gains'],[1]*10)
            self.assertEqual(d['history_sum_scale'],1/64)
            self.assertEqual(d['history_max_scale'],1/4)
            self.assertEqual(d['scratch_word_offsets'],dict(auxiliary=0,references=list(range(4,44,4)),residual=44,music_sum=48))
            self.assertEqual(len(d['prefilter_coefficients']),8)


if __name__=='__main__':unittest.main()
