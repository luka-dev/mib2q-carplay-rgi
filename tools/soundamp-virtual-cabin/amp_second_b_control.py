"""Static V850 target writers for the second-B auxiliary path.

Targets only: no automatic conversion to delivered/effective coefficient state,
no current gain/history defaults, and no claim about actual graph8/9/10 audio.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from amp_second_b import ROOT, IMAGE, LOADER, BINDING, load_static_inventory

MASTER = ('0xc4ba7', '0xc4bd5', '0xc4c03')
FANOUT = ('0xc4bbe', '0xc4bec', '0xc4c31')


def load_control_inventory(image=IMAGE, loader=LOADER, binding=BINDING):
    inventory = load_static_inventory(image,loader,binding)
    sys.path.insert(0,str(ROOT/'tools'))
    from audit_amp_output_bindings import source_offset, filter_record, frequency_lookup, loader_dm
    from extract_amp_sharc_app import parse_blocks
    from amp_dsp_chain import design_to_sos
    data = Path(image).read_bytes(); ld = Path(loader).read_bytes()
    dm = loader_dm(ld,parse_blocks(ld))
    ranges = {'input_EQ_writer':(0xA9938,0xA9A20), 'input_gain_writer':(0xAC1C6,0xAC238),
              'aux_gain_targets':(0xA7EB0,0xA80A8), 'aux_gain_mode_writer':(0xA80B8,0xA822A),
              'fanout_gain_mode_writer':(0xAC248,0xAC454), 'delay_writer':(0xA9C94,0xA9D70),
              'side_scalar_writer':(0xAC59E,0xAC63C), 'side_filter_writer':(0xAB73C,0xAB7EA)}
    pins = {0xA9960:'088a0600',0xAC1DC:'3206b8040c00',0xAC1EC:'2d06c88b0600',
            0xA7ECC:'380654890600',0xA7F18:'137f0000',0xA7F20:'848fd3c1',
            0xA7F30:'0f7eaeff',0xA7FC8:'219669d6',0xA7FF2:'2f8f1200',
            0xA8010:'32571200',0xA803E:'205e18fc',0xA9CD6:'3306688a0600',
            0xA9D36:'2196d4cc',0xAC60E:'0197a2d7',0xAB7BC:'2f0656060e00',
            0xAB750:'8467a3c2',0xA80E8:'0192',0xAC25A:'3806a8890600'}
    for pc,raw in pins.items():
        if data[pc:pc+len(raw)//2].hex() != raw:
            raise ValueError(f'second-B control instruction changed at{pc:x}')
    def unpack(fmt,rel): return list(struct.unpack_from(fmt,data,source_offset(rel)))
    def packed(record):
        b0,b1,b2,_,a1,a2 = design_to_sos(record)
        return [-a1,-a2,b0,b1,b2]
    targets = list(struct.unpack_from('<24I',data,0x68A08))
    input_records = []
    for index,target in enumerate(targets):
        if target == 0xffffffff: continue
        record = filter_record(data,index,source_offset(-0x13590),0)
        input_records.append({'design_index':index, 'target':hex(target),
                              'design_record':record, 'coefficients':packed(record)})
    if [int(r['target'],16) for r in input_records] != list(range(0xE05C0,0xE0610,5)):
        raise ValueError('input EQ writers do not cover exact16 packed records')
    counts = [dm[0xE05B8+i] for i in range(3)]
    if counts != [8,4,4]: raise ValueError('loader input-EQ counts changed')
    table_targets = struct.unpack_from('<6I',data,0x68954)
    expected = [int(n['parameters']['target'],16) for d in MASTER+FANOUT
                for n in inventory['nodes'] if n['descriptor']==d]
    if list(table_targets) != expected: raise ValueError('auxiliary gain target mapping changed')
    master_control_table = struct.unpack_from('<15I',data,0x6896C)
    master_controls = {}
    fanout_controls = {}
    fanout_control_table = struct.unpack_from('<12I',data,0x689A8)
    for i,desc in enumerate(MASTER):
        address=int(desc,16)
        targets=list(master_control_table[3*(i+1):3*(i+2)])
        if targets != [address+10,dm[address+17],dm[address+14]]:
            raise ValueError('master mode/rate/time control binding changed')
        master_controls[desc]=[hex(v) for v in targets]
    for i,desc in enumerate(FANOUT):
        address=int(desc,16)
        targets=list(fanout_control_table[4*i:4*(i+1)])
        if targets != [address+10,dm[address+17],dm[address+17]+16,dm[address+14]]:
            raise ValueError('fanout mode/rate/time control binding changed')
        fanout_controls[desc]=[hex(v) for v in targets]
    delay_targets = struct.unpack_from('<2I',data,0x68A68)
    if delay_targets[1] != 0xE079C: raise ValueError('second delay target changed')
    scalar_templates = {}
    for selector,frequency,type_code in ((0,1000,0),(1,5512,3),(2,11025,3)):
        r = dict(frequency_selector=frequency,gain_tenths_db=0,gain_linear=1.,q_code=7,type_code=type_code)
        if type_code: r.update(frequency_lookup(data,frequency))
        scalar_templates[str(selector)] = {'record':r,'coefficients':packed(r)}
    gain_lookup = [struct.unpack_from('<f',data,0x69398-4*v)[0] for v in range(-1000,401)]
    fanout_mode = struct.unpack_from('<I',data,0x68BD4)[0]
    fanout_rates = list(struct.unpack_from('<32f',data,0x68BD8))
    if fanout_mode != 3: raise ValueError('fanout initializer mode changed')
    scalar_code = unpack('<b',-0x1285E)[0]
    stored = copy.deepcopy(inventory['stored_writer_targets'])
    stored.update({'0xc4b96':{'records':[r['coefficients'] for r in input_records]},
                   '0xc4b7f':{'target':list(struct.unpack_from('<3f',data,0x68BC8))},
                   '0xc4cf9':{'samples':unpack('<16H',-0x1332C)},
                   '0xc4c2b':{'gain':gain_lookup[scalar_code+1000]}})
    return {'source_sha256':inventory['source_sha256'],'loader_sha256':inventory['loader_sha256'],
            'scope':'V850 target plans plus explicit loader-count hypothesis, NOT effective state/readback',
            'writer_ranges':{name:{'start':hex(a),'end_exclusive':hex(b),
                                   'sha256':hashlib.sha256(data[a:b]).hexdigest()}
                             for name,(a,b) in ranges.items()},
            'stored_stage_targets':stored,
            'input_eq':{'writer':'0xa9938','target_table':'0x68a08','source_base':hex(source_offset(-0x13590)),
                        'records':input_records,'loader_counts':counts,
                        'counts_boundary':'Loader PM counts, not proof no indirect writer or live change exists.'},
            'input_gain':{'writer':'0xac1c6','table':'0x68bc8','target':'0xc04b8'},
            'gain_control':{'writer':'0xa7eb0..0xa80a8 (within A7D2C)',
                            'master_descriptors':list(MASTER),'fanout_descriptors':list(FANOUT),
                            'target_addresses':[hex(v) for v in table_targets],
                            'source_levels_gp':[-0x5E2E,-0x5E2D,-0x5E2C],
                            'common_addition_gp':-0x3E2E,'master_calibration_tenths_db':unpack('<3b',-0x134D0),
                            'table1_positions_gp':[-0x3E37,-0x3E36,-0x3E35],
                            'table2_positions_gp':[-0x3E33,-0x3E32,-0x3E31],
                            'map_source':hex(source_offset(-0x12997)), 'slot_to_table_row':unpack('<16B',-0x12997),
                            'table1':[unpack('<190h',-0x12E5C)[i*19:(i+1)*19] for i in range(10)],
                            'table2':[unpack('<190h',-0x12CE0)[i*19:(i+1)*19] for i in range(10)],
                            'sub_enable_gp':-0x3E30,'sub_position_gp':-0x3E2F,
                            'sub_table':unpack('<19h',-0x12AF2)},
            'gain_lookup':{'first_tenths_db':-1000,'last_tenths_db':400,
                           'address_formula':'0x69398 - 4*tenths_db','linear':gain_lookup},
            'scalar':{'writer':'0xac59e','parameter':'0xc4c30','source_gp':-0x1285E,
                      'stored_tenths_db':scalar_code},
            'scalar_filter':{'writer':'0xab73c','parameter':'0xe0656','selector_gp':-0x3D5E,
                             'selectors':scalar_templates,'other_selector':'same bypass template as0'},
            'delay':{'writer':'0xa9c94','target':'0xe079c','source_gp':-0x1332C,
                     'file_offset':hex(source_offset(-0x1332C)),
                     'mapping':'direct slot order, NOT output config permutation68B30'},
            'mode_boundary':'A80B8 requests mode1 for masters; AC248 initializes fanout mode/ramp fields. '
                            'These are not silently converted to effective state. Current gain, effective mode/rates and histories remain required.',
            'requested_gain_control_writes':{
                'master':{'writer':'0xa80b8','descriptors':list(MASTER),'mode':1,
                          'mode_rate_time_targets':master_controls,
                          'multiplicative_rates':[struct.unpack('<f',struct.pack('<I',v))[0]
                                                   for v in (0x3F83FD9E,0x3F78428B)],
                          'mode1_time_parameter':4.,'boundary':'Mode1 requires explicit previous-mode/previous-target/time/step state in SecondBAuxReference; no substituted mode or inferred current state.'},
                'fanout':{'writer':'0xac248','descriptors':list(FANOUT),'mode':fanout_mode,
                          'mode_rate_first_half_rate_second_half_time_targets':fanout_controls,
                          'multiplicative_rates':[fanout_rates[i:i+2] for i in range(0,32,2)],
                          'mode1_time_parameters':list(struct.unpack_from('<16f',data,0x68C58)),
                          'boundary':'Successful control writes only, no current state or delivery proof.'}}}


def _ints(values, length, low, high, name):
    if (not isinstance(values,(list,tuple)) or len(values)!=length
            or any(type(v) is not int or not low<=v<=high for v in values)):
        raise ValueError(f'{name}: {length} integers in[{low},{high}] required')
    return list(values)


def target_overlay(parameters, *, source_levels, common_addition, table1_positions,
                   table2_positions, sub_enabled, sub_position, scalar_filter_selector,
                   assume_loader_input_eq_counts):
    """Compute successful writer target values; do not instantiate live state.

    All controls are required raw decoded values, not guessed MMI percentages.
    Table positions are deliberately bounded to the calibrated19-entry arrays.
    Requesting loader counts is a visible hypothesis, never an effective readback.
    """
    levels = _ints(source_levels,3,-128,127,'source_levels')
    pos1 = _ints(table1_positions,3,-9,9,'table1_positions')
    pos2 = _ints(table2_positions,3,-9,9,'table2_positions')
    _ints([common_addition],1,0,255,'common_addition')
    _ints([sub_position],1,-9,9,'sub_position')
    _ints([scalar_filter_selector],1,0,255,'scalar_filter_selector')
    if type(sub_enabled) is not bool or type(assume_loader_input_eq_counts) is not bool:
        raise ValueError('explicit boolean sub-enable and loader-count acceptance required')
    g = parameters['gain_control']; lookup = parameters['gain_lookup']['linear']
    stages = copy.deepcopy(parameters['stored_stage_targets']); tenths = {}
    def put(desc,values):
        values = [max(-1000,min(400,int(v))) for v in values]
        stages[desc] = {'target':[lookup[v+1000] for v in values]}; tenths[desc] = values
    for i,desc in enumerate(MASTER):
        put(desc,[10*levels[i]+common_addition+g['master_calibration_tenths_db'][i]-82])
    for i,desc in enumerate(FANOUT):
        gains = []
        for row in g['slot_to_table_row']:
            value = g['table1'][row][9+pos1[i]] + g['table2'][row][9+pos2[i]]
            if row == 3:
                value = value+g['sub_table'][9+sub_position] if sub_enabled else -1000
            gains.append(value)
        put(desc,gains)
    selected = str(scalar_filter_selector) if scalar_filter_selector in (1,2) else '0'
    stages['0xc4c24'] = {'record':copy.deepcopy(parameters['scalar_filter']['selectors'][selected]['coefficients'])}
    if assume_loader_input_eq_counts:
        stages['0xc4b96']['counts'] = parameters['input_eq']['loader_counts'].copy()
    return {'source_sha256':parameters['source_sha256'],'stages':stages,'target_tenths_db':tenths,
            'control_inputs':{'source_levels':levels,'common_addition':common_addition,
                              'table1_positions':pos1,'table2_positions':pos2,'sub_enabled':sub_enabled,
                              'sub_position':sub_position,'scalar_filter_selector':scalar_filter_selector,
                              'assume_loader_input_eq_counts':assume_loader_input_eq_counts},
            'live_state_proven':False,'effective_state_populated':False,
            'remaining_required':'current gain, effective gain mode/rates, filter histories, delay histories/lifecycle; '
                                 'input EQ counts unless explicitly assuming loader values; actual auxiliary PCM',
            'boundary':'Target-only overlay after successful chosen writer delivery; no current-state or complete-preset inference.'}


def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--output',type=Path,required=True)
    p.add_argument('--image',type=Path,default=IMAGE); p.add_argument('--loader',type=Path,default=LOADER)
    p.add_argument('--binding',type=Path,default=BINDING)
    p.add_argument('--listings',action='store_true',help='Also regenerate bounded read-only V850 r2 listings beside inventory.')
    a=p.parse_args()
    report=load_control_inventory(a.image,a.loader,a.binding)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    if a.listings:
        for name,r in report['writer_ranges'].items():
            start,end=int(r['start'],16),int(r['end_exclusive'],16)
            command=['r2','-q','-e','scr.color=false','-a','v850','-b','32',
                     '-c',f'pD {end-start} @ {start}',str(a.image)]
            result=subprocess.run(command,check=True,text=True,capture_output=True)
            listing=a.output.parent/(name+'.asm'); listing.write_text(result.stdout)
            r['listing']={'path':str(listing),'sha256':hashlib.sha256(result.stdout.encode()).hexdigest(),
                          'command':command}
    a.output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')


if __name__=='__main__':main()
