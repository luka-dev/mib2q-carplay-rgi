#!/usr/bin/env python3
"""Static-only provenance for the V850 level word selecting common input EQ.

Raw LE V850/32 CodeFlash, base0; all addresses are byte/file offsets.
No vehicle I/O, firmware writes, guessed protocol names or actual-state claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from audit_amp_output_bindings import SOURCE_SHA256, SOURCE_SIZE
from audit_amp_b_outer_control import RANGES as OUTER_RANGES, outer_control


RANGES = {
    'level_init': (0xAEE1E, 0xAEF10),
    'level_periodic_order': (0xAEF20, 0xAEF48),
    'level_target_gates': (0xAEFC0, 0xAF07A),
    'level_ramp_setup': (0xAF134, 0xAF4AC),
    'level_ramp_step': (0xAF828, 0xAFB14),
    'level_post_offset': (0xAFB16, 0xAFB9C),
    'level_bank_commit': (0xB0066, 0xB00EC),
    'level_startup_controls': (0xB6638, 0xB6A9A),
    'level_normal_target': (0xC68E8, 0xC6A40),
    'level_source_mode': (0xC35F0, 0xC378C),
    'level_override_target': (0xC3790, 0xC37C2),
    'level_override_other_lanes': (0xC387A, 0xC38CC),
    'level_request_writer': (0xC5B64, 0xC5D3C),
    'level_override_request': (0xA21D4, 0xA22AC),
    'level_override_enable': (0xA254C, 0xA25FC),
    'level_normal_enable': (0xD455A, 0xD45A8),
    'level_trim_request': (0xC7870, 0xC7A8C),
    'level_composite_trim_request': (0xC53A0, 0xC571C),
    'level_override_mode_request': (0xA1FB4, 0xA21D0),
    'level_b25_writer': (0xAA568, 0xAA59A),
    'level_b_event_dispatch': (0xAA600, 0xAA60E),
    'level_b_control_transport': (0xAC648, 0xAC6CC),
    'level_b_all_event_writers': (0xAA39A,0xAA600),
    'level_b_generic_proxy': (0xA782E,0xA7850),
    'level_b_generic_request': (0xD40C8,0xD4180),
    **OUTER_RANGES,
}


def level_control_parameters(data):
    if len(data) != SOURCE_SIZE or hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError('wrong CodeFlash size or SHA-256')
    thresholds=list(struct.unpack_from('<16b',data,0x102EA0))
    bank_gains=struct.unpack_from('<48h',data,0x102E40)
    examples=[]
    for raw in (0,40,50,70,80,85,90,100,112,126):
        level=raw-100 if raw else -100
        bank=next((i for i,t in enumerate(thresholds) if level>=t),15)
        examples.append({'raw_protocol_level':raw,'settled_control_level':level,
                         'candidate_bank':bank,'bank_55hz_design_gain_db':bank_gains[bank*3]/10})
    return {
        'source_sha256': SOURCE_SHA256,
        'format': 'raw V850 LE32, base0; byte/file addresses',
        'scope': 'static control flow and valid request domain; not live values or a complete ramp emulator',
        'level_word': {'gp_relative': -0x77F8, 'count': 4, 'stride': 4,
            'encoding': 'signed control level * 2^24; first lane selects common EQ',
            'init': 'AEE20/AEE24/AEE5C initializes all4 to -100*2^24',
            'writers': ['AF9CE quantized ramp', 'AFA0C linear ramp',
                        'AFA92 ramp clamp', 'AFAB6 stationary target'],
            'source_target_gp_relative': -30720,
            'periodic_order': ['AEFBC target copy/gates', 'AF130 ramp setup',
                'AF824 ramp step', 'AFB14 post-level offset', 'AFB9C mute lifecycle',
                'AFD9C publish controls', 'B0064 EQ selector'],
            'independence': 'B0066 reads pre-offset level; AFB14 separately adds gp-24028 for gain publication'},
        'normal_source': {'writer': 'C68E8..C69EA',
            'lane_to_source_gp_relative': -15764, 'source_count': 18,
            'active_flags_gp_relative': -15465, 'inhibit_gp_relative': -15056,
            'raw_levels_gp_relative': -15425, 'signed_trims_gp_relative': -14992,
            'cached_target_gp_relative': -14967, 'selected_target_gp_relative': -15718,
            'formula': 'if active==A5 and inhibit==0 and raw>0 and trim>-13: min(26,raw-100+trim); else -100',
            'request_range': [0, 126],
            'lower_bound': 'no lower clamp here; AF03A later replaces target<-100 by -100',
            'source0_selection': 'active[0]==A5 selects0; else descending17..4 loop leaves lowest active4..17; if none, mapping remains unchanged',
            'request_writer': 'C5B64 requires2 payload bytes, validates selector and level<=126; C5CDA stores and C5D20 calls C68E4',
            'request_selectors': list(data[0x96E2C:0x96E33]),
            'request_source_indices': [v-33 for v in data[0x96E2C:0x96E33]],
            'request_zero_selector': 'sets all7 listed sources, not all18',
            'trim_writers': {'single_field':'C7870: signed -13..13, or raw80 unchanged',
                'composite':'C53A0: signed -32..32 for trim, two other fields -9..9; raw80 unchanged independently',
                'packet_selection':'selector0 has7 records; FF repeats one record for all7; listed source selector has1 record',
                'packet_lengths':{'single_field':[9,3,3],'composite':[23,5,5]},
                'header_byte1':'not interpreted by these two handlers',
                'invalid_selector':'returns0 without changing controls; do not treat as successful source update',
                'validation':'all requested fields validated before stores; invalid value/length takes error path',
                'post_write':'C7A70 or C56F8 calls C68E4; both schedule1/11; composite also queues49',
                'sentinel':'80 is leave unchanged, NOT trim -128; actual trim<=-13 mutes in C698C'},
            'boundary': 'raw protocol level is not established as MMI percent; outer service/source meaning and indirect writers remain unproven'},
        'override': {'mode_gp_relative': -17779, 'level_gp_relative': -17778,
            'level_request_range': [-100, 26], 'request_default': -30,
            'selected_target_writer': 'C37B2/C37B6 if mode==A5: lane0=override level; lanes1..3=-100',
            'normal_copy': 'C69C6/C69DC publishes normal targets only when mode!=A5; C368A clears mute bit0 on mode exit',
            'mode_writer': {'entry':'A1FB4','inner_function_id':375,
                'id_decode':'big-endian bytes19/20; operation byte21; caller length supplied in r7',
                'gate':'gp-18912 & EF must be0, else error36',
                'enter':'operation3 AND length1: gp-17867=C0, mode=A5, set gp-18912 bit4; schedule9/28 only on mode change',
                'exit':'operation0 AND gp-17867==C0: set gp-17867=40, deadline=tick+5000, mode=5A, clear bit4; schedule9/28 only on change',
                'related_inner_ids':{'379':'override level A21D0','1063':'override EQ enable A2548'},
                'boundary':'inner dispatcher IDs, not a ready-to-send MOST/UDS request; transport/auth/UI mode naming not established'},
            'boundary': 'override is a code-path label, not a proven user-visible mode name'},
        'target_gates': {'writer': 'AEFC0..AF078', 'flags_gp_relative': -17777,
            'global_gate_gp_relative': [-30750, -30751],
            'all_lanes': 'both global bytes must equal5A and flags&3 must equal0, otherwise target=-100',
            'lane0': 'flags&0x3C forces lane0 target=-100',
            'per_lane': 'target<-100 or mapped active flag!=A5 forces target=-100'},
        'selector': {'thresholds': thresholds,
            'normal_enable_gp_relative': -15406, 'override_enable_gp_relative': -17772,
            'enable_choice': 'gp-17779==A5 chooses gp-17772, else gp-15406',
            'startup': 'B675C r8=A5 -> B6946 normal enable=A5, B694A cached enable=A5; B6692 r13=5A -> B69FE override mode=5A and B6A1C override enable=5A; B6A96 cached index=0',
            'normal_enable_writer': 'D455A accepts payload byte A5 or5A; other bytes leave value unchanged',
            'override_enable_writer': 'A254C operation0 setsA5, operation3 uses payload byte23 bit0 (A5/5A)',
            'commit_gate_gp_relative': -30788,
            'commit_gate_writers': ['AEEEC setsA5', 'AF16A sets5A for specified gp-17777 changes',
                'AFAD8 setsA5 when lane0 stationary and gp-17777==0'],
            'commit': 'B00C0: gate==A5 and changed index/enable -> store gp-3E29/gp-3E2A and queue17; otherwise retain prior state',
            'delivery': 'A8B26 sends cached index if cached enable==A5, else0; queue is not delivery proof'},
        'settled_examples': {'conditions':'trim0, active source, no inhibit/mute, normal mode, EQ enabled, commit and event delivery assumed; raw0 is muted',
            'boundary':'EQ design gain only; NOT total signal gain including volume attenuation and NOT MMI percentages',
            'rows':examples},
        'ranges': {name: {'start': hex(a), 'end_exclusive': hex(b),
                          'sha256': hashlib.sha256(data[a:b]).hexdigest()} for name,(a,b) in RANGES.items()},
        'remaining': ['outer request dispatch/service binding, source meanings and indirect/other writers',
            'external input values and actual event ordering cannot be fixed by this image alone',
            'physical PCM graph binding, full B parameter loading and wrapper timing remain separate work'],
    }


def first_b_level_control(data, dm, mem):
    """Bounded API25 provenance, not the full parameter-curve implementation."""
    from inspect_amp_visa import decode
    from audit_amp_b_gain_controls import gain_controls
    level_control_parameters(data)  # image identity
    pins={0x131CE1:'I4=0xb913d',0x131D04:'I5=0xb911e',
          0x131F9E:'DM(I7,M7)=25',0x131FA2:'R12=0x43480000',
          0x131FA5:'R8=0xc3480000',0x132407:'I3=MODIFY(I4,+1149)',
          0x132425:'I3=MODIFY(I3,-2)',0x13262D:'R2=DM(+31,I3)',
          0x13262F:'R2=PASS R2 || DM(+4,I3)=R8',
          0x132635:'R2=DM(-57,I3)',0x132637:'F8=F8-F2 || DM(-10,I3)=R8',
          0x13263B:'CJUMP 0x12ce71 (DB); R2=I6, I6=I7',
          0x12CEB0:'R4=DM(+46,I5)',0x12CEE8:'R4=DM(+50,I5)',0x12CF20:'R4=DM(+51,I5)'}
    for pc,text in pins.items():
        if decode(mem,pc)[1]!=text:raise ValueError(f'B level instruction changed at {pc:#x}')
    validator,applier=dm[0xB913D+9],dm[0xB914B+9]
    if (validator,applier)!=(0x131F9A,0x13262D):raise ValueError('API25 jump table changed')
    event_target=0xAA60E+2*struct.unpack_from('<h',data,0xAA60E+2*(52-45))[0]
    if event_target!=0xAA568:raise ValueError('event52 dispatch changed')
    return {'source_sha256':SOURCE_SHA256,'api_parameter_id':25,'v850_event':52,
        'event_target':hex(event_target),'source_gp_relative':-15718,
        'formula':'float(max(0,s8(gp-15718)+100)); selected TARGET, NOT smoothed gp-77F8 and NOT gated gp-30720',
        'transport':'AC648 writes API id/value to C4AD9+8/+9 via descriptor C4AE1; callback124F35 calls131A65',
        'pending_address':hex(0xB911E+9),'validator':hex(validator),'applier':hex(applier),
        'accepted_finite_range':[-200,200],
        'apply':{'shadow_offset':1151,'effective_offset':1137,'defer_word_offset':1178,
            'baseline_offset':1090,'helper':'0x12ce71',
            'formula':'always shadow=value; if object1178==0: effective=value; helper argument=value-object1090; otherwise skip helper',
            'helper_descriptor_offsets':[46,50,51],
            'curve_record_offsets':[[773,782],[776,785],[779,788]],
            'curve_base':'DM(object+1180)',
            'helper_arguments':'12B00F: slot6, signed level delta, lower0, upper5, two curve records per descriptor',
            'success_followup':'12FBA5 with curve/target buffer and B909A; interpolation/application timing not modelled'},
        'boundary':'nine branch-gain curve slots bound below; other helper effects, later writers and actual state remain unbound; not a flat/zero-B claim',
        'instruction_pins':{hex(pc):text for pc,text in pins.items()},
        'curve_bindings':first_b_level_curve_bindings(data,dm,mem),
        'gain_controls':gain_controls(data,dm,mem)}


def first_b_level_curve_bindings(data, dm, mem):
    """Bind API25 to three linear branch gains, conditional on image tables."""
    from inspect_amp_visa import decode
    pins={
        0x130F2F:'R2=0xf5',0x130F31:'R6=R11+R2',
        0x130F33:'R2=0x12b',0x130F35:'R10=R13+R2 || R4=R6',
        0x130F88:'DM(+1179,I5)=R6',0x130F8B:'DM(+1180,I5)=R7',
        0x12DDE9:'R12=0xb89c0',
        0x132ABC:'LCNTR=414, DO 0x132ac2 UNTIL LCE',
        0x132ACA:'R2=DM(I5,M6)',0x132ACB:'DM(I3,M6)=R2',
        0x12BCC6:'R2=0x2e',0x12BCCA:'R2=0x7a',
        0x12BD7A:'R2=0x32',0x12BD7E:'R2=0x125',
        0x12BDA7:'R2=0x33',0x12BDAB:'R2=0x179',
        0x12AFC9:'DM(I5,M6)=R13',0x12AFCA:'DM(I5,M6)=R15',
        0x12AFCD:'DM(I5,M6)=R2',0x12AFE2:'DM(M5,I4)=R0',
        0x12B10D:'I4=MODIFY(I2,+3)',0x12B112:'DM(M4,I4)=R2',
        0x12B12D:'I5=DM(-11,I4)',0x12B12F:'F2=F2+F1 || R1=DM(-10,I4)',
        0x12B132:'F2+=F1',0x12B133:'DM(M5,I5)=R2',
        0x131487:'R1=0x250',0x1314D3:'R1=0x1a5',0x131565:'R2=0x2a4',
        0x132D23:'CJUMP 0x12faf2 (DB); R2=I6, I6=I7',
        0x132D34:'R12=PASS R8 || R4=DM(I5,M5)',0x12FD19:'F1=F4*F8',
        0x12FBD5:'LCNTR=829, DO 0x12fbdb UNTIL LCE',
        0x12FBCC:'R2=BTGL R8 BY 0',0x12FBD2:'IF NOT SZ JUMP 0x12fbef',
        0x12FC01:'LCNTR=829, DO 0x12fc1d UNTIL LCE',
    }
    for pc,text in pins.items():
        if decode(mem,pc)[1]!=text:raise ValueError(f'B curve binding changed at {pc:#x}')
    calls={0x130F3A:0x132A9B,0x130F6E:0x132A9B,0x131097:0x12BC52,
           0x12BCCE:0x12AF9A,0x12BD82:0x12AF9A,0x12BDAF:0x12AF9A,
           0x131490:0x132D0B,0x1314DA:0x132D0B,0x13156D:0x132D0B,
           0x132D37:0x12FCE1}
    for pc,target in calls.items():
        if decode(mem,pc)[2]!=target:raise ValueError(f'B curve call changed at {pc:#x}')
    source=dm[0xB95E5]
    if source!=0xC3C6A:raise ValueError('B configuration source pointer changed')
    f=lambda w:struct.unpack('<f',struct.pack('<I',w))[0]
    def value(offset):
        a=0x12000C+4*(245+offset);lw=dm[source+245+offset]
        bw=struct.unpack_from('<I',data,a)[0]
        return {'config_offset':offset,'loader_address':hex(source+245+offset),
                'b01_file_offset':hex(a),'loader_word':hex(lw),'b01_word':hex(bw),
                'loader_float':f(lw),'b01_float':f(bw)}
    bindings=[]
    for descriptor,offset,channels,records,call in (
            (46,122,4,[773,782],0x1314DA),(50,293,6,[776,785],0x131490),
            (51,377,2,[779,788],0x13156D)):
        bindings.append({'descriptor_object_offset':descriptor,'target_config_offset':offset,
            'effective_object_offset':299+offset,'channels':channels,'scalar_gain_call':hex(call),
            'base_value':value(offset),'curve_records':[[value(i) for i in range(r,r+3)] for r in records]})
    return {'scope':'normal init from loader or B01, not actual playback state or exhaustive later-writer proof',
        'copy':{'source_base':hex(source),'source_config_offset':245,'words':829,
                'effective_object_offset':299,'target_address':'0xb89c0',
                'validation':'132A9B checks checksum at config828 before copying'},
        'descriptor_layout':{'words':12,'target_pointer':1,'initial_base_value':2,
                             'contribution_offsets':list(range(3,12)),'level_slot':6},
        'bindings':bindings,'baseline':value(791),'baseline_effective_object_offset':1090,
        'curve_math':'12B00F signed-side bracket, x==0 contribution0; near-equal x uses near y; otherwise linear/squared/sqrt interpolation; write slot then clamp(base+sum9,0,5)',
        'zero_curve_words':all(dm[source+245+i]==0 and struct.unpack_from('<I',data,0x12000C+4*(245+i))[0]==0 for i in range(773,792)),
        'conclusion':'both image tables give zero API25 contribution for finite -200..200 at baseline0; does NOT mean B is zero, unity, or its other control slots are zero',
        'publication':'12FBA5 selector==1 copies829 target words; other selectors prepare per-field steps except mask==1 immediate fields;132D0B calls12FAF2 once per branch invocation before multiplying samples',
        'transition_count':value(2),
        'remaining':['non-branch-gain helper effects and parameter-block replacement writers',
                     'wrapper call scheduling and complete transition timing',
                     'actual input mapping and external request sequence'],
        'instruction_pins':{hex(pc):text for pc,text in pins.items()},
        'calls':{hex(pc):hex(target) for pc,target in calls.items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--loader', type=Path)
    args = parser.parse_args()
    report = level_control_parameters(args.image.read_bytes())
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name,(start,end) in RANGES.items():
        command = ['r2','-a','v850','-b','32','-q','-e','scr.color=0',
                   '-e','asm.lines=false','-e','asm.bytes=true','-c',
                   f'pD {end-start} @ {start}',str(args.image)]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        if any(s in result.stdout for s in ('invalid','truncated','UNKNOWN')):
            raise ValueError(f'undecoded or truncated instruction in {name}')
        (args.output_dir/f'{name}.asm').write_text(result.stdout)
    (args.output_dir/'level_control.json').write_text(json.dumps(report,indent=2)+'\n')
    (args.output_dir/'b_outer_control.json').write_text(json.dumps(outer_control(args.image.read_bytes()),indent=2)+'\n')
    if args.loader:
        from inspect_amp_visa import load_shortwords
        from extract_amp_sharc_app import parse_blocks
        from audit_amp_output_bindings import loader_dm
        raw=args.loader.read_bytes();mem=load_shortwords(raw)
        dm=loader_dm(raw,parse_blocks(raw))
        b=first_b_level_control(args.image.read_bytes(),dm,mem)
        (args.output_dir/'b_level_control.json').write_text(json.dumps(b,indent=2)+'\n')
    print(f'{len(RANGES)} bounded V850 listings; {args.output_dir}/level_control.json')


if __name__ == '__main__':
    main()
