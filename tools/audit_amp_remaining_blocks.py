#!/usr/bin/env python3
"""Reproducible read-only listings and stage inventory for the remaining DSP.

No vehicle connection or firmware writes. Both source hashes are validated by
the existing image auditor and VISA loader reader before generating artifacts.
"""
import argparse
import json
import re
import subprocess
import struct
import hashlib
from pathlib import Path
from audit_amp_output_bindings import audit, loader_dm, words
from extract_amp_sharc_app import parse_blocks
from inspect_amp_visa import load_shortwords, listing, decode
from audit_amp_input_path import input_path_parameters
from audit_amp_level_control import first_b_level_control
from audit_amp_b_gain_controls import RANGES as GAIN_CONTROL_RANGES
from audit_amp_b_parameter_controls import RANGES as PARAMETER_CONTROL_RANGES
from audit_amp_b_config_control import RANGES as CONFIG_CONTROL_RANGES,config_control
from audit_amp_b_mode_control import RANGES as MODE_CONTROL_RANGES,mode_control
from audit_amp_b_indexed_control import RANGES as INDEXED_CONTROL_RANGES,indexed_control
from audit_amp_matrix_control import RANGES as MATRIX_CONTROL_RANGES,matrix_control

RANGES = {
    **GAIN_CONTROL_RANGES,
    **PARAMETER_CONTROL_RANGES,
    **CONFIG_CONTROL_RANGES,
    **MODE_CONTROL_RANGES,
    **INDEXED_CONTROL_RANGES,
    **MATRIX_CONTROL_RANGES,
    'first_b_control_descriptor_init': (0x12AF9A,0x12B00F),
    'first_b_control_descriptor_bindings': (0x12BC52,0x12BF21),
    'first_b_control_config_copy': (0x132A9B,0x132AEF),
    'first_b_control_config_publish': (0x12FBA5,0x12FC3C),
    'first_b_api_entry': (0x131A65,0x131ACC),
    'first_b_api_pending_dispatch': (0x131CCA,0x131D22),
    'first_b_api_range_check': (0x131A3E,0x131A65),
    'first_b_api25_validation': (0x131F9A,0x131FB6),
    'first_b_level_curve_setter': (0x12CE71,0x12CF4D),
    'first_b_control_curve_kernel': (0x12B00F,0x12B1E7),
    'gain': (0x12879C, 0x128910),
    'limiter': (0x12823C, 0x1283F6),
    'post_eq': (0x126A95, 0x126BAE),
    'first_b_wrapper_callback': (0x124DF3, 0x12504D),
    'complex': (0x126DB2, 0x127852),
    'complex_target_helper': (0x12796D, 0x1279ED),
    'natural_log': (0x124393, 0x12440B),
    'natural_exp': (0x12512E, 0x1251A9),
    'complex_stereo_delay': (0x1279ED, 0x127A4D),
    'complex_adaptive_residual': (0x125F36, 0x126092),
    'first_b_library_init': (0x12DD7D, 0x12DE3B),
    'first_b_library_process': (0x12DE3B, 0x12DFA8),
    'first_b_clear': (0x12DD16, 0x12DD67),
    'first_b_algorithm_dispatch': (0x131208, 0x13163D),
    'graph_scheduler': (0x12677D, 0x12682B),
    'detector_prefilter': (0x12621D, 0x1262DA),
    'detector_pair_biquad': (0x12610B, 0x12618C),
    'detector_unpack': (0x127A80, 0x127ABA),
    'detector_weighted_sum': (0x1283F6, 0x128453),
    'complex_calibration_noise': (0x126CEC, 0x126DB2),
    'complex_auxiliary_gain': (0x127A4D, 0x127A80),
    'first_b_filter_wrapper': (0x12DBDE, 0x12DD16),
    'first_b_filter_cascade': (0x12DAA5, 0x12DBDE),
    'first_b_filter_pair_wrapper': (0x12D9A9, 0x12DA2F),
    'first_b_filter_odd_wrapper': (0x12DA2F, 0x12DAA5),
    'first_b_filter_scalar_kernel': (0x12B529, 0x12B57F),
    'first_b_filter_first_order_kernel': (0x12BB91, 0x12BBDD),
    'first_b_filter_stability': (0x13163D, 0x13165D),
    'first_b_coefficient_smoothing': (0x12FAF2, 0x12FB40),
    'first_b_copy_kernel': (0x132E08, 0x132E3D),
    'first_b_object_init': (0x130EF3, 0x131208),
    'first_b_two_channel_tail': (0x132F86, 0x1330E6),
    'first_b_nested_allpass': (0x12D10E, 0x12D21B),
    'first_b_single_allpass': (0x12B90B, 0x12BA83),
    'first_b_delay_gain_eq': (0x12EB9B, 0x12EC7A),
    'first_b_variable_delay': (0x12F411, 0x12F639),
    'first_b_scalar_gain': (0x132D0B, 0x132D56),
    'first_b_spectral_wrapper': (0x12E80E, 0x12EA98),
    'first_b_spectral_pre_wrapper': (0x12E1BB, 0x12E30A),
    'first_b_spectral_main_wrapper': (0x12E30A, 0x12E3E6),
    'first_b_half_sum_difference': (0x12E3E6, 0x12E44B),
    'first_b_sum_difference': (0x12E44B, 0x12E487),
    'first_b_spectral_crossmix': (0x12E487, 0x12E521),
    'first_b_spectral_post_wrapper': (0x12E521, 0x12E80E),
    'first_b_reciprocal': (0x131773, 0x131787),
    'first_b_reciprocal_sqrt': (0x131787, 0x1317A2),
    'first_b_complex_normalize': (0x1317A2, 0x131835),
    'first_b_complex_magnitude': (0x131835, 0x131899),
    'first_b_sqrt_vector': (0x12EF05, 0x12EF65),
    'first_b_vector_multiply': (0x12AF36, 0x12AF83),
    'first_b_vector_add': (0x12EC7A, 0x12ECC7),
    'first_b_vector_subtract': (0x12B7DA, 0x12B827),
    'first_b_strided_add': (0x12D73B, 0x12D791),
    'first_b_vector_clamp': (0x12BB49, 0x12BB91),
    'first_b_six_channel_tail': (0x12A7F9, 0x12AB01),
    'first_b_four_channel_tail': (0x12EDE6, 0x12EF05),
    'first_b_eight_channel_finish': (0x12B87B, 0x12B90B),
    'first_b_spectral_ramp': (0x12AD70, 0x12ADFF),
    'first_b_spectral_subtraction': (0x12ADFF, 0x12AF36),
    'first_b_transient_mask': (0x12F103, 0x12F250),
    'first_b_transient_mask_wrapper': (0x12F250, 0x12F411),
    'first_b_level_mask': (0x12F90A, 0x12FAF2),
    'first_b_level_gain': (0x12F87B, 0x12F90A),
    'first_b_frame_vector_smoother': (0x12D355, 0x12D3E0),
    'first_b_frame_vector_smoother_reset': (0x12D3E0, 0x12D3F6),
    'first_b_band_expand': (0x13165D, 0x1316C1),
    'first_b_band_sum': (0x1316C1, 0x131761),
    'first_b_band_layout_getters': (0x131761, 0x131773),
    'first_b_asymmetric_history': (0x131899, 0x1318F2),
    'first_b_linear_history': (0x1318F2, 0x131948),
    'first_b_frequency_smoother': (0x131948, 0x1319FE),
    'first_b_scalar_add': (0x12BC0F, 0x12BC52),
    'first_b_log10_vector': (0x12D80B, 0x12D844),
    'first_b_strided_multiply': (0x12D6A3, 0x12D6F9),
    'first_b_strided_divide': (0x12D6F9, 0x12D73B),
    'first_b_window_init': (0x130A1C, 0x130AEB),
    'first_b_window_apply': (0x130AEB, 0x130B6D),
    'first_b_real_fft': (0x12B588, 0x12B633),
    'first_b_real_inverse_fft': (0x1330E6, 0x1331AF),
    'first_b_complex_fft_kernel': (0x12EF65, 0x12F0B8),
    'first_b_analysis_history': (0x12D49F, 0x12D546),
    'first_b_analysis_history_reset': (0x12D546, 0x12D577),
    'first_b_overlap_add': (0x132B2B, 0x132BB4),
    'first_b_overlap_add_reset': (0x132BB4, 0x132BE4),
    'first_b_spatial_dispatch': (0x130771, 0x130A1C),
    'first_b_spatial_statistics': (0x130658, 0x130771),
    'first_b_spatial_bin_power': (0x130609, 0x130658),
    'first_b_spatial_cross_wrapper': (0x13039C, 0x1303B5),
    'first_b_complex_conjugate_product': (0x12ECC7, 0x12ED19),
    'first_b_spatial_coherence': (0x13027B, 0x130321),
    'first_b_spatial_minimum': (0x130359, 0x13039C),
    'first_b_vector_minimum': (0x12A530, 0x12A574),
    'first_b_spatial_fixed_mean': (0x130321, 0x130359),
    'first_b_vector_sum': (0x12D791, 0x12D7DC),
    'first_b_spatial_gate': (0x130237, 0x13027B),
    'first_b_spatial_ramp': (0x1303B5, 0x13045E),
    'first_b_spatial_frequency_blend': (0x13045E, 0x1304E4),
    'first_b_spatial_diffuse_mask': (0x1304E4, 0x130609),
    'first_b_spatial_separator': (0x12FFBA, 0x130237),
    'scalar_divide': (0x124354, 0x12438A),
    'scalar_square_root': (0x1244A6, 0x1244DF),
    'first_b_complex_remodulate': (0x130B6D, 0x130BBA),
    'first_b_graph_bypass': (0x124D64, 0x124DF3),
    'first_b_parameter_defaults': (0x12B26D, 0x12B2BF),
    'first_b_queued_controls': (0x1323DE, 0x1326C7),
    'graph_handler_slot': (0x1292D8, 0x1292FD),
    'graph_handler_select': (0x1292FD, 0x129376),
    'host_command_records': (0x129533, 0x1296EE),
    'word_fill': (0x1257D0, 0x1257E6),
    'first_b_scalar_multiply': (0x12FCE1, 0x12FD24),
    'first_b_config_copy': (0x12B2BF, 0x12B2E9),
    'first_b_config_init': (0x12B4D0, 0x12B529),
    'first_b_interpolation_init': (0x12FC3C, 0x12FC78),
    'upstream_planar_biquads': (0x126559, 0x126685),
    'upstream_packed_biquads': (0x126685, 0x126724),
    'upstream_shared_eq': (0x126724, 0x12677D),
    'upstream_bank_eq': (0x12682B, 0x12694C),
    'upstream_bank_eq_kernel': (0x12618C, 0x12621D),
    'upstream_shared_eq_smoothing': (0x1262DA, 0x1263AB),
    'upstream_bank_eq_smoothing': (0x1263AB, 0x126499),
    'upstream_stereo_pack': (0x127B3B, 0x127B75),
    'input_delay_callbacks': (0x1244DF, 0x124517),
    'input_delay_state_machine': (0x124517, 0x12488B),
    'input_delay_dma_descriptors': (0x12488B, 0x1249DD),
    'input_delay_dma_queue': (0x1249DD, 0x124A96),
    'input_delay_dma_execution': (0x124A96, 0x124CA0),
    'input_delay_copy': (0x1257B2, 0x1257D0),
}


def first_b_activation_audit(data, dm, mem):
    """Loader/callsite evidence, never a claim that a live B lane is silent."""
    expected = {
        0x124F73: 'R2=DM(0xbc9ec)',
        0x124F76: 'R2=BTGL R2 BY 0',
        0x124F7C: 'IF SZ JUMP 0x124fbb',
        0x124F84: 'CJUMP 0x12de3b (DB); R2=I6, I6=I7',
        0x124FBB: 'R12=0x1000',
        0x124FBD: 'R8=M5',
        0x124FBF: 'CJUMP 0x1257d0 (DB); R2=I6, I6=I7',
        0x124D73: 'I5=MODIFY(I4,+5)',
        0x124D7A: 'LCNTR=4, DO 0x124dd6 UNTIL LCE',
        0x124D80: 'I2=DM(-5,I5)',
        0x124DAC: 'I2=DM(-4,I5)',
        0x124DD0: 'I4=MODIFY(I4,+2)',
        0x12B26F: 'I13=0x40000000',
        0x12B274: 'I4=MODIFY(I4,+1084)',
        0x12B297: 'DM(M7,I4)=I13',
        0x132407: 'I3=MODIFY(I4,+1149)',
        0x132425: 'I3=MODIFY(I3,-2)',
        0x132468: 'DM(-19,I3)=R8',
        0x1295F4: 'R2=FEXT R5 BY 16:15',
        0x12969F: 'R2=BTGL R2 BY 1',
        0x1296A5: 'R8=FEXT R5 BY 0:16',
        0x1296AA: 'CJUMP 0x1292fd (DB); R2=I6, I6=I7',
        0x129323: 'R2=BSET R2 BY R1 || R8=M5',
        0x12934C: 'R1=DM(+2,I5)',
        0x129351: 'DM(M5,I4)=R2',
        0x129365: 'R2=BTGL R8 BY 0',
        0x129368: 'IF NOT SZ JUMP 0x12933b',
        0x129372: 'R2=DM(M6,I5)',
        0x129374: 'DM(M5,I4)=R2',
        0x1257DD: 'DM(I4,+1)=R8',
    }
    for pc, text in expected.items():
        if decode(mem, pc)[1] != text:
            raise ValueError(f'first-B activation instruction mismatch at {pc:#x}')
    return {
        'live_B_zero_proven': False,
        'graph_descriptor': '0xc4ad9',
        'loader_graph_class': {'process': hex(dm[0xC53E2]), 'bypass': hex(dm[0xC53E3])},
        'callback_gate': {'address': '0xbc9ec', 'loader_value': dm[0xBC9EC],
            'inhibit_value': 1,
            'condition': 'callback state B95E2==3 AND gate!=1 calls library; gate==1 clears 4096 output words',
            'evidence': ['0x124f6d..0x124f84', '0x124fbb..0x124fc6'],
            'warning': 'CORRECTION: previous gate polarity was reversed; boot zero permits the library call in state3, but is not a playback readback; indirect/host writes remain possible'},
        'graph_commands': {
            '0x20001': {'selector':1, 'class_offset':1, 'handler':hex(dm[0xC53E2])},
            '0x20003': {'selector':3, 'class_offset':2, 'handler':hex(dm[0xC53E3])},
            'dispatch':'1295F4 record kind2 ->1296A5 low16 selector ->1292FD',
            'slot':'normally descriptor+4;1292D8 instead returns parent graph object+32 when graph object+31 matches traced node',
            'v850':'AC818 event5 emits20003, other events emit20001; A76B0 events5/44 call AC818',
            'bulk_queue':'C28A8 arg1 queues0..53: event5 bypass precedes event44 process; arg8 queues0..29 only',
            'bulk_caller':'C2658 calls C28A8 only after C264A tests r29==1; no live completion/order claim',
            'warning':'event IDs and bulk argument are not MOST bank or MMI3D enums; later commands/failures may change the handler'},
        'loader_callback_state': dm[0xB95E2],
        'graph_bypass': {'handler': '0x124d64', 'output_input_indices': [0,1]*4,
            'warning': 'bypass copies boundary inputs, NOT a B-output mute; active handler not established'},
        'mode_control': {'object_offset':1128, 'initial_value':2.0,
            'initial_evidence':'130F90 passes object+59;12B26D stores 2.0 at relative+1069 = object+1128',
            'api_parameter_id':16, 'pending_word':'0xb911e',
            'pending_apply':'132468: object+1149-2-19 = object+1128, when object+1178==0',
            'v850_writer':'0xaa388, cases45/48 -> 0xac648 -> descriptor+8/+9 ->131A65',
            'v850_index_gp_relative':-15760,
            'v850_float_table_address':'0x6eaa0',
            'v850_float_table':list(struct.unpack_from('<3f', data, 0x6EAA0)),
            'v850_index_startup':'0xb6b86 writes zero',
            'v850_index_other_writers':['0xc2802','0xc2822','0xc283c','0xd4c2e'],
            'matrix_bank_binding':'C2852 reads SAME gp-15760, shifts6 and indexes the64-byte input matrix; bank0/1/2 maps to library2/5/5, not equal numeric enums',
            'sink26_binding':'source index5 ->95314+20 raw bank1, unchanged by ASAD ->library5 after control delivery; command D4C2E can override bank byte',
            'warning':'matrix bank and library selector are distinct numbers but coupled by the writer; neither is MMI3D; no live selector claim'},
        'readback_required':['C4ADD active graph handler','B95E2 callback state',
            'BC9EC callback gate','B89BA ready','B89B9 object pointer',
            'object+1128 selector','B909B transition','B89BF transition counter',
            'B audio during nonzero input and actual first-mixer gain row'],
    }


def first_b_mode5_bindings(data, mem):
    """Mode5 input formula and bank/control linkage, not a live selection."""
    expected = {
        0x131591:'IF EQ JUMP 0x1315d3',
        0x1315D3:'R2=R7+R15 || DM(-2,I6)=R12',
        0x1315D9:'R4=R3+R13',
        0x1315DB:'CJUMP 0x132e08 (DB); R2=I6, I6=I7',
        0x1315E5:'R12=0x61',
        0x1315E7:'R4=R11+R12 || I7=MODIFY(I7,M7)',
        0x1315F1:'R12=M6',
        0x1315F3:'CJUMP 0x132d0b (DB); R2=I6, I6=I7',
        0x131602:'R12=PASS R1 || R4=DM(-24,I6)',
        0x131606:'CJUMP 0x12ec7a (DB); R2=I6, I6=I7',
        0x131614:'R12=PASS R0 || R2=DM(-22,I6)',
        0x131617:'R8=PASS R2 || R4=DM(-24,I6)',
        0x13161B:'CJUMP 0x12ec7a (DB); R2=I6, I6=I7',
        0x131622:'R3=DM(-26,I6)',
        0x131624:'JUMP 0x131594 (DB)',
        0x131627:'R1=DM(-24,I6)',
        0x132D23:'CJUMP 0x12faf2 (DB); R2=I6, I6=I7',
        0x132D37:'CJUMP 0x12fce1 (DB); R2=I6, I6=I7',
    }
    for pc,insn in expected.items():
        if decode(mem,pc)[1] != insn: raise ValueError(f'mode5 instruction mismatch at {pc:#x}')
    return {
        'input_graph_ports':[24,23,22],
        'gain_object_offset':97,
        'formula':['S0 + gain*S2','S1 + gain*S2'],
        'scratch_steps':['copy S+1024 to P+512','update object+97 once, multiply P+512 in-place',
                         'P+512 + S -> S+2048','P+512 + S+512 -> P+512',
                         'spectral wrapper receives S+2048 and P+512'],
        'scope':'512-frame finite fixed effective gain; graph22 follows upstream transfer/delay, gain and EQ stages, NOT the downstream A07 matrix or raw MOST centre',
        'source_to_bank_table':[{'sink':hex(0x21+i),'raw_bank':data[0x95314+4*i]} for i in range(18)],
        'bank_to_library_mode':list(struct.unpack_from('<3f',data,0x6EAA0)),
        'sink26_raw_bank':data[0x95314+4*(0x26-0x21)],
        'bank_normalization':'C2806..C283C: raw>=3->0; raw2+ASAD C0->0; other raw2 with ASAD&3==0->1; raw0/1 unchanged',
        'matrix_index_evidence':'C2852..C2862 uses gp-15760 <<6; C2886 queues48 and C2896 queues15',
        'delivery_boundary':'event48 writes API16 viaAA388/AC648; queued update132468 applies only when object+1178==0; graph/callback/ready also required',
    }


def first_b_input_config(data, dm, mem, report):
    """Pin init-copy provenance and upstream graph; no live configuration claim."""
    expected = {
        0x124FD9: 'R2=DM(0xb95e5)',
        0x12DDEF: 'R8=DM(+5,I6)',
        0x130F10: 'R9=R8',
        0x130F8E: 'R2=0x3b',
        0x130F90: 'R4=R13+R2 || R8=R9',
        0x12B4DD: 'R0=0x26',
        0x12B4E6: 'R8=R1+R0',
        0x12B2CA: 'I4=MODIFY(I4,+33)',
        0x12B2D1: 'LCNTR=103, DO 0x12b2d7 UNTIL LCE',
        0x12B2D4: 'R2=DM(I5,+2)',
        0x12B2D7: 'DM(I4,+2)=R2',
        0x12B2DF: 'R2=DM(I5,M6)',
        0x12B2E0: 'DM(I4,M6)=R2',
        0x130F33: 'R2=0x12b',
        0x130F35: 'R10=R13+R2 || R4=R6',
        0x1310B7: 'F0=F2-F1 || R4=R10',
        0x12FC60: 'DM(0xb903a)=R4',
        0x12FC63: 'DM(0xb903b)=R4',
        0x12FAF9: 'R2=DM(0xb903a)',
        0x12FAFC: 'COMPU(R2,R4)',
        0x12FAFE: 'IF GT JUMP 0x12fb36',
        0x1261CD: 'R2=PM(I9,M12)',
        0x1261CF: 'R1=PM(I9,M12)',
        0x1261D1: 'R6=PM(I9,M12)',
        0x1261D3: 'R5=PM(I9,M12)',
    }
    for pc, text in expected.items():
        if decode(mem, pc)[1] != text:
            raise ValueError(f'first-B input config mismatch at {pc:#x}')
    calls = {0x124FEE:0x12DD7D, 0x12DDF1:0x130EF3, 0x130F93:0x12B4D0,
             0x12B4E8:0x12B2BF, 0x1310BF:0x12FC3C,
             0x1265C8:0x12610B, 0x126748:0x1262DA,
             0x126760:0x126685, 0x1268CF:0x1263AB, 0x126912:0x12618C}
    for pc, target in calls.items():
        if decode(mem, pc)[2] != target:
            raise ValueError(f'first-B input config call mismatch at {pc:#x}')
    for pc, raw in {0xACFB4:'8457a5ba200ea500e151b205',
                    0xAD038:'2f066a3c0c00', 0xAD04A:'2b060c001200',
                    0xAD056:'1a88c28acb893197010070970100'}.items():
        if data[pc:pc+len(raw)//2].hex() != raw:
            raise ValueError(f'B01 upload mismatch at {pc:#x}')
    source = dm[0xB95E5]
    source_offset = 38 + (97 - (59+33))
    f = lambda w: struct.unpack('<f', struct.pack('<I', w))[0]
    address = 0x12000C+4*source_offset
    stages = [dict(n) for n in report['graph_nodes'][:11]]
    roles = ['transfer/delay state machine', 'shared planar biquad cascade',
             'per-channel gain', *(['stereo pack']*4),
             'shared five-section biquad cascade', 'per-channel banked three-section EQ',
             'stereo unpack', 'stereo unpack']
    for stage, role in zip(stages, roles): stage['role'] = role
    return {
        'scope':'fixed source images and decoded normal initialization; NOT a playback snapshot',
        'calls':{hex(pc):hex(target) for pc,target in calls.items()},
        'gain':{'object_offset':97, 'source_pointer_word':'0xb95e5',
                'source_base':hex(source), 'source_word_offset':source_offset,
                'copy':{'source_start':38,'object_start':59+33,'words':103*2+1},
                'loader':{'address':hex(source+source_offset),
                          'word':hex(dm[source+source_offset]),'linear':f(dm[source+source_offset])},
                'b01':{'header':data[0x120000:0x120008].decode('ascii'),
                       'count':words(data,0x120008,1)[0], 'address':hex(address),
                       'word':hex(words(data,address,1)[0]),
                       'linear':struct.unpack_from('<f',data,address)[0]},
                'upload_condition':'ACFB4: gp-455C==A5 and 0<count<=8000; AD038 target C3C6A+word index, AD04A source12000C+4*index; delivery and init order still required',
                'live_value_proven':False},
        'gain_interpolation':{'base_object_offset':299, 'gain_object_offset':97,
                'normal_init_skips_gain':True,
                'proof':'1310BF registers object+299 in B903A;12FAFC unsigned base>gain jumps12FB36 before any gain write; both delay-slot stores at12FC60/63 run even for zero duration',
                'boundary':'does not exclude later object/source writes or a different initialization'},
        'upstream_nodes':stages,
        'shared_eq':{'descriptor':'0xc4aa6','sections':dm[0xC4AA6+9],
                'current':hex(dm[0xC4AA6+10]),'target':hex(dm[0xC4AA6+15]),
                'record_order':['-a1','-a2','b0','b1','b2'],
                'smoothing':'1262DA target*s+current*(1-s), feedback discriminant/stability cap and epsilon snap; not just linear interpolation'},
        'bank_eq':{'descriptor':'0xc4ab7','sections':dm[0xC4AB7+9],
                'banks':dm[0xC4AB7+14], 'selectors':hex(dm[0xC4AB7+15]),
                'bank_gains':hex(dm[0xC4AB7+16]),'bank_coefficients':hex(dm[0xC4AB7+17]),
                'current_packed_coefficients':hex(dm[0xC4AB7+23]),
                'record_order':['-a2','-a1','b2','b1'], 'b0':1,
                'channel_mixing':False,
                'proof':'12618C SIMD lanes use separate gains/coefficient/state words, stride2, no cross-lane sum or swap in audio kernel',
                'boundary':'fixed effective gain/coefficients reference only; selector writers and runtime smoothing state not bound'},
        'boundary':'A07 matrix C4AED is downstream of B. Transfer/delay state machine, active upstream coefficients/gains/selectors and graph0/1/2 physical source binding still need verification; do not rename graph22 raw MOST C',
    }


def first_b_spectral_bindings(mem):
    """Recovered successful-path scratch roles; pins key callsites to image."""
    sites = {0x1315B5:0x12E80E, 0x12E998:0x12E1BB, 0x12E9F9:0x12E30A,
             0x12EA3F:0x12E521, 0x12E33F:0x12F250, 0x12E787:0x12F250,
             0x12E6AA:0x130771, 0x12E6F8:0x12D355, 0x12E70A:0x12D355,
             0x131542:0x132F86, 0x131461:0x12A7F9, 0x1314AE:0x12EDE6}
    for pc,target in sites.items():
        if decode(mem,pc)[2] != target: raise ValueError(f'changed spectral binding at {pc:#x}')
    return {
        'scope':'12E80E finite successful path; BMode2FixedReference binds prepared stereo to eight outputs; not runtime/lifecycle or full amplifier',
        'calls':{hex(pc):hex(target) for pc,target in sites.items()},
        'size':{'fft_samples':1024,'hop_samples':512,'complex_bins':513,
                'workspace_A_words':4096,'workspace_B_words':4617},
        'scratch_initial':{'A+0':'original left complex','A+2N':'original right complex',
            'A+4N':'sum spectrum then left magnitude','A+5N':'right magnitude',
            'A+6N':'.5*magnitude(left+right)', 'B+0':'left normalized complex',
            'B+2N':'right normalized complex'},
        'saved_before_main':{'A+4N':'copy ORIGINAL left complex','B+0':'copy ORIGINAL right complex',
            'A+6N':'copy left magnitude','B+8N':'copy right magnitude'},
        'pre':'subtract mono reference from scalar magnitudes with bank object+98; remodulate original phase; synth history object+8',
        'main':'12F250 bank object+211, history object+12; synth history object+10',
        'post':'ORIGINAL saved L/R -> unscaled sum/difference -> independent level masks shared bank object+104 -> half sum/difference -> crossfeed ->130771 -> normalize -> prefix256 temporal magnitudes -> remodulate -> unscaled sum/difference ->12F250 ->half sum/difference ->synth history object+9',
        'post_bindings':{'level_objects':[13,14],'crossfeed_pointer':1161,
            'spatial_object':15,'spatial_params_pointer':1166,
            'magnitude_smoother_objects':[16,17],'magnitude_smoother_bank':120,'magnitude_smoother_count':256,
            'transient_object':18,'transient_bank':123},
        'transient_history_offsets':{'left_level':0,'right_level':74,'left_gain':148,'right_gain':222},
        'transient_shared':'converted selector==1 uses average scalar input and LEFT histories only; other selectors independent; scalar and complex spectra each multiplied once',
        'mode2_tail_routes':{'pre':{'tail':'0x12ede6','channels':4},
                             'post':{'tail':'0x12a7f9','channels':6},
                             'main':{'tail':'0x132f86','channels':2}},
        'dispatch_buffers_mode2':{'S':'five input lanes,512 words each','P':'object+1174 workspace pointer',
            'pre':['P+0','S+1536'],'post':['S+2048','P+512'],'main':['P+1024','P+1536']},
        'normalize_correction':'1317A2 stack1 selects writable computed-power scratch, NOT supplied power;1317C2 PASS R0 resets flags from bin count before EQ branch; old supplied_power bypass API withdrawn'}


def first_b_output_routes(report):
    """Join extracted graph ports with stored row1 gains, not live selection."""
    nodes = {n['descriptor']: n for n in report['graph_nodes']}
    output_ports = nodes['0xc4ad9']['port_ids'][5:]
    mixer_ports = nodes['0xc4b4f']['port_ids']
    row = report['ten_bus_pair_gain_rows_db'][1]
    result = []
    for lane, port in enumerate(output_ports):
        slot = mixer_ports[10:20].index(port)
        result.append({'b_output_lane': lane, 'graph_port': port,
                       'mixer_b_slot': slot, 'mixer_output_port': mixer_ports[20+slot],
                       'stored_row1_db': row[2*slot+1]})
    return result


def complex_parameters(data, dm, profile):
    """Static writer operands, NOT a fabricated live descriptor snapshot.

    Caller validates CodeFlash/loader identity before calling this helper.
    Only the two explicitly examined calibration profiles are supported.
    """
    if profile not in (0,1): raise ValueError('only audited profiles 0 and 1')
    base = 0x103238 + profile*20644
    control_fields = words(data, 0x68A94, 20)
    integer_fields = {17,18,19,20,21,26,33,34,35,36,37,38}
    controls = []
    for i, address in enumerate(control_fields):
        field = address - 0xC4D17
        fmt = '<i' if field in integer_fields else '<f'
        controls.append({'descriptor_offset': field, 'target_word': hex(address),
                         'fallback': struct.unpack_from(fmt,data,0x8F3FC+4*i)[0],
                         'calibrated': struct.unpack_from(fmt,data,base+4*i)[0]})
    coeff = list(struct.unpack_from('<24f',data,0x8F4D8))
    fir = data[base+164:base+20644]
    fir_values = struct.unpack('<5120f',fir)
    f = lambda w: struct.unpack('<f',struct.pack('<I',w))[0]
    sig = data[0x94C54:0x94C5C]
    return {'profile_index':profile, 'profile_is_not_input_bank_number':True,
        'calibration_file_offset':hex(base), 'controls_AAC88':controls,
        'startup_calibration_flag_B64E0':{
            'condition':'signature94C54 == bytes100000 == bytes10FFF0 (8 bytes)',
            'signature_hex':sig.hex(),
            'matches_dump':sig == data[0x100000:0x100008] == data[0x10FFF0:0x10FFF8],
            'on_match':'gp-1250C=A5; fixed calibration copied from 102438',
            'on_mismatch':'gp-1250C=5A; fallback copied from 8E53C',
            'caveat':'A342A/A63A6/A6490 can later clear this flag; not live proof'},
        'startup_control_flag':{'gp_relative':'-0x3d62','value_hex':'0xa5',
            'evidence':'B675C sets r8=165; B69E6 stores it, no intervening r8 write or call',
            'later_writers':['0xab1ee','0xc35d4','0xd4002'],
            'caveat':'later command/state transitions and SHARC sideband still matter'},
        'profile_selector':{'gp_relative':'-0x4592','startup_value':0,
            'startup_writer':'0xb6810','later_writer':'0xd6eea',
            'caveat':'do not equate this selector with MOST input bank or MMI 3D level'},
        'stage8_loader_delays':[{'requested':dm[0xC4D17+139+4*i],
                                'maximum':dm[0xC4D17+140+4*i]} for i in range(5)],
        'db_conversion':{'factor_word':'0x3d4ccccd','factor':f(0x3d4ccccd),
            'log_ten_word':'0x40135d8e','mathematical_law':'10^(code/20), even for power vector scaling',
            'evidence_pcs':['0x127569','0x1275cf','0x12761d','0x127648'],
            'supersedes':'v3/v4 reference incorrectly used /10; synthetic reports must be recomputed'},
        'calibration_loader':{
            'master_enable':dm[0xC4D17+40], 'initialized':dm[0xC4D17+5],
            'mode_word':dm[0xC4D17+39], 'phase':dm[0xC4D17+11],
            'selected_bit':dm[0xC4D17+41], 'channel_mask':dm[0xC4D17+31],
            'wait_samples':dm[0xC4D17+42], 'measure_samples':dm[0xC4D17+32],
            'noise_type':dm[dm[0xC4D17+176]+6],
            'noise_seed':dm[dm[0xC4D17+176]+8],
            'auxiliary_scalar_word':hex(dm[0xC4D11+5]),
            'auxiliary_scalar':f(dm[0xC4D11+5]),
            'warning':'loader values, not live state; calibration field30 later written by AAC88'},
        'detector_layout':{
            'audio_frames':dm[0xC4F92+9], 'detector_frames':dm[0xC4D17+13],
            'history_blocks':dm[0xC4D17+127],
            'history_sum_scale':f(dm[0xC4D17+129]),
            'history_max_scale':f(dm[0xC4D17+130]),
            'reference_gains':[f(dm[dm[0xC4D17+131]+i]) for i in range(10)],
            'auxiliary_graph_id':11, 'scratch_graph_id':96,
            'scratch_word_offsets':{'auxiliary':0,'references':list(range(4,44,4)),
                                    'residual':44,'music_sum':48},
            'decimation_stride':dm[0xC4D17+12],
            'sample_indices':[i*dm[0xC4D17+12] for i in range(dm[0xC4D17+13])],
            'selection':'1273A8..1273C5 first compacts stride16 pair samples in place; 127A80 then unpacks the compacted prefix; effective samples0,16,32,48',
            'prefilter_scale':f(dm[0xC4D17+97]),
            'prefilter_coefficients':[[f(dm[dm[0xC4D17+98]+4*i+j]) for j in range(4)] for i in range(8)],
            'prefilter_format':'[-a2,-a1,b2,b1], b0=1, one global input scale',
            'power_pair_order':['residual (+136)','music sum (+135)']},
        'cap_code_AAFC0':data[0x102D58+profile*19]//10,
        'cap_word_AAFC0_source_byte':data[0x102D58+profile*19],
        'power_x_base_db':list(struct.unpack_from('<5i',data,base+80)),
        'power_y_base_db':list(struct.unpack_from('<5i',data,base+100)),
        'power_y_floor_base_db':list(struct.unpack_from('<5i',data,base+120)),
        'runtime_index_signed_offsets': [list(struct.unpack_from('<8b',data,base+140+8*i)) for i in range(3)],
        'other_base_AAD70':list(struct.unpack_from('<5f',data,0x8F538)),
        'helper_parameters_loader':[[f(dm[dm[0xC4D17+i]+j]) for j in range(2,6)] for i in range(54,59)],
        'helper_cap_initializer_field46':f(dm[0xC4D17+46]),
        'five_band_coefficients_AAB44':coeff,
        'five_band_writer_equals_loader':words(data,0x8F4D8,24)==[dm[0xE07EA+i] for i in range(24)],
        'detector_biquad_banks_AAB44':[
            {'target_word':hex(address),'coefficients':list(struct.unpack_from('<5f',data,0x8F44C+20*i))}
            for i,address in enumerate(words(data,0x68A78,7))],
        'fir_bank_AAA60':{'target_word':'0xc0a7e','shape':[10,512],
                         'file_offset':hex(base+164),'sha256':hashlib.sha256(fir).hexdigest(),
                         'nonzero_words':sum(v!=0 for v in fir_values),
                         'max_abs':max(abs(v) for v in fir_values)},
        'live_readback_needed':{'v850_gp_relative':['-0x1250c','-0x3d62','-0x4592','-0x3d61','-0x41a6','-0x458e'],
            'sharc_words':['0xc4d17..0xc4dd9 (descriptor, 195 words)',
                           '0xc09b8..0xc0a1c (tables and gain states)',
                           'graph97 sideband value, not merely its pointer']}}


def complex_stages(mem):
    """Inventory actual top-level bit tests/calls; not invented DSP meanings."""
    pc, stages, current = 0x127052, [], None
    while pc < 0x127826:
        n, text, target = decode(mem, pc)
        match = re.fullmatch(r'BTST R13 BY (\d+)', text)
        if match:
            if current: current['end'] = hex(pc)
            current = {'bit': int(match[1]), 'start': hex(pc), 'calls': []}
            stages.append(current)
        if current and ('CJUMP ' in text or 'CALL ' in text):
            current['calls'].append({'pc': hex(pc), 'target': hex(target)})
        pc += n
    if current: current['end'] = hex(pc)
    for s in stages:
        s['mask_full_sideband0'] = bool(0xFFF7FE5 & (1 << s['bit']))
        s['mask_full_sideband1'] = bool(0xDFF75E5 & (1 << s['bit']))
        s['mask_calibration_mode2'] = bool(0xF4BA & (1 << s['bit']))
    return stages


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', type=Path)
    p.add_argument('loader', type=Path)
    p.add_argument('--output-dir', type=Path, required=True)
    a = p.parse_args()
    data, loader = a.image.read_bytes(), a.loader.read_bytes()
    report = audit(data)
    mem = load_shortwords(loader)
    dm = loader_dm(loader, parse_blocks(loader))
    a.output_dir.mkdir(parents=True, exist_ok=True)
    result = {'source_sha256': report['source_sha256'],
              'dynamic_stages': report['dynamic_stage_research'],
              'complex_stages': complex_stages(mem), 'decoded_ranges': {},
              'complex_control_profiles':[complex_parameters(data,dm,p) for p in (0,1)],
              'first_b_output_routes':first_b_output_routes(report),
              'first_b_activation':first_b_activation_audit(data,dm,mem),
              'first_b_mode5':first_b_mode5_bindings(data,mem),
              'first_b_level_control':first_b_level_control(data,dm,mem),
              'first_b_input_config':first_b_input_config(data,dm,mem,report),
              'input_path_profiles':[input_path_parameters(data,dm,mem,p) for p in (0,1)],
              'first_b_spectral_bindings':first_b_spectral_bindings(mem),
              'first_b_runtime_boundary':{
                  'ready_word':'0xb89ba', 'object_pointer_word':'0xb89b9',
                  'frames_word':'0xb89bc','transition_word':'0xb909b',
                  'transition_counter':'0xb89bf',
                  'algorithm_selector':'object+1128, float converted to unsigned integer',
                  'ready_check':'12DE48..12DE63: ready must equal 1, else return 0x4000',
                  'transition':'state==1 clears five input and eight output lanes but STILL calls 131208; after count==10 clear state and counter',
                  'state_setters':['0x13246f','0x1324ae','0x132540'],
                  'algorithm_dispatch':'0x131208..0x13163d',
                  'filter_subgraph':'12DBDE -> 12DAA5 -> paired/odd wrappers -> direct-form-II kernels; isolated reference only',
                  'two_channel_tail':'132F86: common delay -> per-channel delays -> gain/EQ -> nested allpass -> single allpass -> final gain; fixed-parameter reference only',
                  'tail_parameter_offsets':{'common_delay_pointer':1160,'delay_gain_eq_bank':615,
                      'left_nested_bank':659,'left_single_bank':664,
                      'right_nested_bank':667,'right_single_bank':672,'final_gain_pointer':1158},
                  'tail_output_lanes':[6,7],
                  'six_channel_tail':{'handler':'0x12a7f9','pre_gain_bank':422,
                      'common_delay_pointer':1159,'nested_pointers':[[1167,1168],[1169,1170]],
                      'pair_delay_gain_eq_banks':[444,495,546],
                      'pair_single_allpass_banks':[[488,491],[539,542],[]],
                      'pair_gain_pointers':[1171,1172,1173],'final_gain_pointer':1157},
                  'four_channel_tail':{'handler':'0x12ede6','common_delay_bank':331,
                      'pair_delay_gain_eq_banks':[332,376],'final_gain_pointer':1156},
                  'output_finish':{'six_gain_bank':592,'four_gain_bank':421,'two_gain_bank':676,
                      'six_eq_bank':593,'two_eq_bank':677,'final_eight_eq_bank':698,'final_gain_bank':719,
                      'merge':'six lanes scaled; add scaled four lanes into first four; two lanes separate; six/two EQ then eight EQ/gain'},
                  'mode_branch':'selector 2 or 5 generates six-lane branch and adds four-lane branch; other selectors clear six lanes BEFORE stateful EQ (not permanent silence); selector6 bypasses tail processing and returns before final eight EQ/gain',
                  'spectral_boundary':'12F250 shared/independent wrapper and12E80E/pre/main/post spectra composed; BMode2FixedReference joins pre->four/post->six/main->two and finish; explicit parameters/state only, hardware and lifecycle validation remain open',
                  'spatial_reference':{'handler':'0x130771','params_words':28,
                      'statistics':'130658: bin power floor then band sum, p11/p12 history; cross=L*conj(R); output power floor1e-9 AFTER history commit',
                      'coherence':'13027B: magnitude(cross/(sqrt(PL*PR)+2^-52)), power floor1e-10 then clamp magnitude0..1',
                      'controls':'130359 minimum and130321 mean band entries9..59, each p13/p14; minimum>.99 makes gate target0, otherwise1; p23..26 asymmetric smoothing',
                      'ramp':'1303B5: p4/p5 endpoints, converted p6/p7 band indices, supplied step p8; endpoint<.001 adds .001',
                      'separator':'12FFBA complex 2x2 band matrix; converted p1==1 uses lambda estimate with Re(cross)^2 only; p9 regularization, p10 fallback; converted p2!=0 enables balance with object+10/+11 and fixed mean',
                      'diffuse':'1304E4 sqrt(1-coherence), p19/20 history, expand, low p17/18 high p15/16 smoothing blend, clamp p21/p22',
                      'finish':'input-separator; gate only when converted p3==1; (1-p27)*residual+p27*diffuse, no final clamp',
                      'scope':'finite successful path; explicit boundary spectra and state, not complete B render or hardware execution'},
                  'transform_reference':{'frame_size':1024,'hop_size':512,
                      'window_init':'130A1C: first half sin((i+.5)*pi/N), 130AEB mirrors it',
                      'forward':'12B588: N/2 complex forward FFT then real postpack; N/2+1 complex bins',
                      'inverse':'1330E6: prepack 1/N (quarter bin 2/N), SAME complex forward FFT',
                      'complex_kernel':'12EF65; decoded, ideal numpy FFT in reference, not bit-exact',
                      'neutral_reference':'two sine windows, 50% overlap: unity with 512-sample latency; not live B processing',
                      'twiddle_base':'0xe4754',
                      'twiddle_1024_words_sha256':hashlib.sha256(struct.pack('<1024I',*[dm[0xE4754+i] for i in range(1024)])).hexdigest()},
                  'boundary':'fixed mode2/5 references composed, mode5 shared third-input addition included; upstream graph, runtime parameters/interpolation, other modes, lifecycle and hardware equivalence remain open; no permanent B=0 proof'}}
    for name, (start, end) in RANGES.items():
        lines = list(listing(mem, start, end))
        result['decoded_ranges'][name] = {'start': hex(start), 'end': hex(end),
            'instructions': len(lines), 'unknown': sum('UNKNOWN' in s for s in lines)}
        (a.output_dir / f'{name}.asm').write_text('\n'.join(lines)+'\n')
    command = 'pD 0x184 @ 0xa7d2c; pD 0x1e0 @ 0xacaa0; pD 0x134 @ 0xaa144; pD 0x1c8 @ 0xa8548; pD 0x330 @ 0xacc84'
    r = subprocess.run(['r2', '-a', 'v850', '-b', '32', '-q', '-e', 'scr.color=0',
                        '-e', 'asm.lines=false', '-e', 'asm.bytes=false', '-c', command,
                        str(a.image)], check=True, capture_output=True, text=True)
    (a.output_dir/'v850_writers.asm').write_text(r.stdout)
    command = ('pD 0x924 @ 0xaa89a; pD 0xdc @ 0xb64e0; pD 0x3da @ 0xb6610; '
               'pD 0x10a @ 0xa6334; pD 0x7c @ 0xab1be; pD 0x36 @ 0xd3fee; '
               'pD 0x26 @ 0xd6ed2')
    r = subprocess.run(['r2','-a','v850','-b','32','-q','-e','scr.color=0',
                        '-e','asm.lines=false','-e','asm.bytes=false','-c',command,
                        str(a.image)],check=True,capture_output=True,text=True)
    (a.output_dir/'complex_control_writers.asm').write_text(r.stdout)
    command = ('pD 0x2a4 @ 0xaa388; pD 0x84 @ 0xac648; pD 0x90 @ 0xac818; '
               'pD 0x2a @ 0xb6b76; pD 0x50 @ 0xc27f0; pD 0x54 @ 0xd4c0e; '
               'pD 0x20 @ 0xa775e; pD 0x8 @ 0xa79c2; pD 0xc @ 0xa7c0e; '
               'pD 0xc2 @ 0xc28a8; pD 0x36 @ 0xc264a; pD 0x68 @ 0xa74ec; '
               'pD 0x130 @ 0xc2770; pD 0x12 @ 0xd002e; pD 0x10 @ 0xd953a; '
               'pD 0xf2 @ 0xacfb4')
    r = subprocess.run(['r2','-a','v850','-b','32','-q','-e','scr.color=0',
                        '-e','asm.lines=false','-e','asm.bytes=false','-c',command,
                        str(a.image)],check=True,capture_output=True,text=True)
    (a.output_dir/'first_b_activation_writers.asm').write_text(r.stdout)
    command = ('pD 0x2b0 @ 0xa8904; pD 0x76 @ 0xac0e2; '
               'pD 0xf0 @ 0xa8f34; pD 0xb0 @ 0xa9bdc; '
               'pD 0x90 @ 0xab6a4; pD 0x140 @ 0xa9d80; '
               'pD 0xa6 @ 0xb0066; pD 0xf0 @ 0xab850; '
               'pD 0x35c @ 0xa76b0; pD 0x34 @ 0xb69c0')
    r = subprocess.run(['r2','-a','v850','-b','32','-q','-e','scr.color=0',
                        '-e','asm.lines=false','-e','asm.bytes=false','-c',command,
                        str(a.image)],check=True,capture_output=True,text=True)
    (a.output_dir/'input_path_writers.asm').write_text(r.stdout)
    (a.output_dir/'b_config_control.json').write_text(json.dumps(config_control(data,dm,mem),indent=2)+'\n')
    (a.output_dir/'b_mode_control.json').write_text(json.dumps(mode_control(data,dm,mem),indent=2)+'\n')
    (a.output_dir/'b_indexed_control.json').write_text(json.dumps(indexed_control(data,dm,mem),indent=2)+'\n')
    (a.output_dir/'matrix_control.json').write_text(json.dumps(matrix_control(data,dm,mem),indent=2)+'\n')
    (a.output_dir/'audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result['decoded_ranges'], indent=2))


if __name__ == '__main__': main()
