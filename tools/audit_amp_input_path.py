"""Fixed-image parameter provenance for the eight channels before first B.

Values are calibrated writer inputs/targets, never a live RAM snapshot. Called
by the hash-validating remaining-block auditor; no vehicle communication.
"""
import struct
from audit_amp_output_bindings import source_offset, filter_record
from inspect_amp_visa import decode
from audit_amp_level_control import level_control_parameters


def input_path_parameters(data, dm, mem, profile):
    if profile not in (0, 1):
        raise ValueError('only audited profiles 0 and 1')
    def unpack(fmt, rel):
        return list(struct.unpack_from(fmt, data, source_offset(rel, profile)))
    def records(rel, count):
        return [filter_record(data, i, source_offset(rel, profile), 0) for i in range(count)]
    pins = {0x12459A:'R1=DM(-5,I1)', 0x1245AB:'R2=0x3',
            0x124726:'COMP(R2,R1) || DM(M4,I4)=R1',
            0x124745:'COMP(R1,R8)', 0x124797:'I4=DM(+14,I0)',
            0x12477A:'IF GE JUMP 0x12478e (DB)',
            0x12477F:'R2=MAX(R1,R2)',
            0x12479E:'IF LE JUMP 0x12486c',
            0x1249E4:'R2=BTGL R2 BY R1 || R0=M5',
            0x1249E7:'IF SZ R0=M6'}
    for pc, text in pins.items():
        if decode(mem,pc)[1] != text:
            raise ValueError(f'input-delay instruction changed at {pc:#x}')
    v850_pins = {
        0xA8942:'21066a010c00', 0xA8952:'440effff219ee4d6',
        0xA8974:'440effff2196fcd6', 0xA8AD0:'32065a010c00',
        0xA8B3C:'2f0682000e00', 0xA8B4C:'8487d7c1', 0xA8B68:'a457d7c1',
        0xAC108:'2d06908b0600', 0xA9E7A:'2f060c000e00',
        0xA9DA4:'f08f460a', 0xA9E28:'077ee004', 0xA9E3A:'205e8025',
        0xB007C:'24670988', 0xB0080:'400e0001e167c002',
        0xB009E:'21765cd7', 0xB00DC:'445fd7c1',
        0xA76C2:'8457bfc1200ea500e151da05',
    }
    for pc, raw in v850_pins.items():
        if data[pc:pc+len(raw)//2].hex()!=raw:
            raise ValueError(f'input writer changed at {pc:#x}')
    # Source RAM -> fixed calibration copy validated by B64E0 signature checks.
    bank_gains=unpack('<48h',-0x12904)
    bank_templates=records(-0x1291C,3)
    signature=data[0x94C54:0x94C5C]
    delay_maps=unpack('<16B',-0x12977)
    delay_groups=unpack('<16B',-0x12997)
    delay_output=unpack('<16h',-0x12E7C)
    # The V850 loop really allows indices8/9 too, outside the nominal8-lane
    # arrays. Preserve those source bytes and uint16 wrap; do not sanitize.
    delay_a=unpack('<16h',-0x13628)
    delay_b=unpack('<16h',-0x13638)
    candidates=[]
    for i, slot in enumerate(delay_maps):
        if slot<=15:
            group=delay_groups[slot]
            if group>=16: raise ValueError('unexpected delay group outside audited data')
            candidates.append((delay_output[i]+delay_a[group]+delay_b[group]) & 0xffff)
    return {
        'profile_index':profile,
        'scope':'calibrated fixed/global source-copy scenario, successful writers and settled coefficients; NOT live state',
        'consumers':'processed lanes0..7 feed A07 matrix C4AED via24,23,22,21,50,51,36,37; first3 also feed B C4AD9. This EQ is not confined to B.',
        'calibration':{'signature_matches_dump':signature==data[0x100000:0x100008]==data[0x10FFF0:0x10FFF8],
            'fixed_copy':'B64E0:102438 -> gp-1330C,3584 bytes; invalid-signature fallback8E53C is a different scenario',
            'profile_copy':'C23C0:100034+profile*370 -> gp-13900; profile is NOT matrix bank'},
        'input_gain':{'descriptor':'0xc4a7b','writer':'0xac0e2..0xac154',
            'target':'0xc0090','source':'0x68b90','target_linear':list(struct.unpack_from('<8f',data,0x68B90)),
            'loader_mode':dm[0xC4A7B+10],
            'boundary':'current C0088 and ramp state remain runtime; unity is a settled writer target'},
        'prefilter':{'descriptor':'0xc4a6c','writer':'0xab6a6..0xab72e','event':18,
            'target_table':'0x68a70','targets':list(struct.unpack_from('<2I',data,0x68A70)),
            'records':records(-0x1285C,2),'first_b_record_index':0,
            'source_gp_relative':-0x1285C},
        'shared_eq':{'descriptor':'0xc4aa6','target':'0xe0068','events':[0,1,2,3,4],
            'mode_condition':'A76C2: gp-3E42==A5 selects tone A8F36; otherwise graphic A9BDE',
            'tone':{'templates':records(-0x129B0,3),
                'position_tables_tenths_db':[unpack('<57h',-0x12B64)[19*i:19*(i+1)] for i in range(3)],
                'positions_gp_relative':-0x3E28,'additions_gp_relative':-0x3E25,
                'fourth_template':records(-0x12A34,2)[profile],
                'fourth_gain_gp_relative':-0x3E22,
                'fifth':'A8FF2 sets type byte0 (bypass); uninitialized unused stack fields are irrelevant'},
            'graphic':{'records':records(-0x12944,5),
                'gain_clear_condition':'gp-3DDF==5A overwrites gain with zero before design'},
            'boundary':'current E004E is smoothed; multiple writers share targets, delivered order matters'},
        'bank_eq':{'descriptor':'0xc4ab7','writer':'0xa8904..0xa8b18',
            'templates':bank_templates,
            'gains_tenths_db':[bank_gains[3*i:3*i+3] for i in range(16)],
            'coefficient_target':'0xc016a','gain_target':'0xc015a',
            'packing':'per section[-a2/a0,-a1/a0,b2/b0,b1/b0], bank gain=product(b0/a0); b0 is implicit1 in kernel',
            'selector_writer':'0xa8b26..0xa8baa','selector_event':17,'selector_target':'0xe0082',
            'selector_condition':'all8 receive u8(gp-3E29) if gp-3E2A==A5, otherwise0; SHARC clamps0..15',
            'startup_selector':0,'startup_writer':'0xb6a96',
            'level_selector':{'writer':'0xb0066..0xb00e8','input_gp_relative':-30712,
                'conversion':'signed32 / 0x01000000, integer truncation toward zero',
                'thresholds':unpack('<16b',-0x128A4),
                'algorithm':'first threshold<=level, clamped15; ties select that index, not next',
                'commit_condition':'gp-7844==A5; then update flag/index if changed and queue17',
                'enable_source':'B006A reads gp-4573; if A5 use gp-456C, else gp-3C2E (not the B01-valid flag)',
                'provenance':level_control_parameters(data),
                'boundary':'gp-77F8 is the first smoothed source-level control word, not PCM measurement; protocol level is not MMI percentage; external values/event ordering remain conditional'},
            'boundary':'initial loader gain/coefficients zero is not the settled playback bank; this table is independent of tone positions'},
        'delay':{'descriptor':'0xc4a54','handler':'0x124517','writer':'0xa9d80..0xa9eba','event':19,
            'target':'0xe000c','current':'0xe0004','channels':8,
            'request':'gp-3DDC +4*u8(gp-3D94): byte0 enable, uint16+2 milliseconds; disabled request=0',
            'request_startup':'B69C2 loop zeros enable and milliseconds; runtime message writers C9F28/CA0A6 can override',
            'candidate_samples':candidates,'minimum_candidate_samples':min([2880]+candidates),
            'fixed_compensation_samples':1248,'maximum_samples':9600,
            'target_formula':'same8 channels: min(9600,max(0,48*enabled_request_ms - (min(2880,candidates)+1248)))',
            'state_dispatch':{str(i):hex(dm[0xB93D4+i]) for i in range(4)},
            'states':{'0':'allocate channels*frames scratch; successful allocation sets1, failure-1',
                '1':'clear scratch/external ring, queue clear DMA, set2; completion callback1244DF expects2 and sets3',
                '2':'clear eight output blocks while waiting for clear completion',
                '3':'copy targets to current, validate nonnegative delays and sum<=descriptor13; changed valid delays reinitialize via1; unchanged valid delays process',
                '4':'audio DMA pending; callback1244FB expects4 and sets3; scheduling without completion falls into invalid-state copy path',
                'invalid':'state outside0..3 sets-1 and copies each input to corresponding output, not mute'},
            'global_ready':'B93DD==2 enables normal state dispatch; otherwise descriptor state0 becomes-1 and copies once, other nonzero states leave outputs unchanged',
            'steady_audio':'zero delay:1257B2 direct input->output;0<d<frames:old scratch tail d + current input prefix;d>=frames:previously fetched block; no multiply or cross-channel sum',
            'dma_tap':'-max(d,frames):12477A delayed branch ALWAYS executes12477F MAX(d,0); d>=frames negates d at12478E, short d uses -frames at12478A',
            'dma_read':'13-word delay-line TCB: write N samples, then read N at updated external index +tap into same scratch for NEXT callback; ADI hardware reference4-138..141',
            'ring':'descriptor10/11 external base/current,12 ring length,13 capacity; current advances frames with wrap, channel segments subtract each delay; shared allocation is not audio summation',
            'boundary':'steady integer-delay reference only; DMA timing/interrupts/errors and startup clears are not emulated'},
        'remaining':'static-only continuation: external-value alternatives, further control writers, physical graph0..7 mapping and DMA lifecycle; explicit whole-amp scenarios exist, no actual-state inference',
    }
