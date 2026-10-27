"""Explicit auxiliary graph8/9/10 -> C4DFB B slots, schedule21..50.

Fixed coefficients/targets and settled delay lifecycle only. No inferred MOST
mapping, no musical donor, no live-state claim, no SHARC bit-exact arithmetic.
All histories and gain current/target/rates are mandatory caller inputs.
"""
from __future__ import annotations

import copy
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

import numpy as np

from amp_dynamic import gain_step
from amp_b_input_eq import _finite
from digital_stages import PackedOutputEQ

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT / 'assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
BINDING = ROOT / 'assets/soundamp/output_binding_audit.json'
IMAGE = ROOT.parents[1] / 'Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin'
GAINS = ('0xc4b7f', '0xc4ba7', '0xc4bbe', '0xc4bd5', '0xc4bec', '0xc4c03', '0xc4c31')


def _json(value):
    if isinstance(value, np.ndarray): return value.tolist()
    if isinstance(value, dict): return {k: _json(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)): return [_json(v) for v in value]
    if isinstance(value, np.generic): return value.item()
    return value


def _shape(value, shape, label):
    a = _finite(value)
    if a.shape != shape: raise ValueError(f'{label}: expected {shape}')
    return a


def mode1_gain_update(current, target, *, previous_mode, previous_target,
                      time_parameters, scale, steps):
    """One 12879C mode1 update, including mode-entry and retained step state.

    +9 current, +12 target, +13 previous target, +14 time parameters,
    +15 interleaved additive steps, +11 scalar, +22 previous mode. A changed
    mode copies current into previous-target before checking target magnitude.
    12880D delayed equality branch still loads the old positive step at128813.
    Only changed target MAGNITUDE recalculates abs(|target|-|current|)/(time*scale).
    Nonpositive denominator is replaced by1, not max(denominator,1).
    Division is float64 equivalent, not SHARC RECIPS/refinement rounding.
    """
    current = _finite(current)
    if current.ndim != 1 or not len(current): raise ValueError('nonempty current gain vector required')
    n = len(current); target = _shape(target,(n,),'target')
    previous = _shape(previous_target,(n,),'previous target')
    time = _shape(time_parameters,(n,),'time parameters')
    old_steps = _shape(steps,(n,2),'mode1 steps')
    scale = float(_shape(scale,(),'mode1 scale'))
    if type(previous_mode) is not int or not 0 <= previous_mode <= 0xffffffff:
        raise ValueError('explicit uint32 previous mode required')
    if previous_mode != 1: previous = current.copy()
    changed = np.abs(target) != np.abs(previous)
    denominator = time*scale
    if not np.isfinite(denominator).all(): raise ValueError('nonfinite mode1 denominator')
    denominator = np.where(denominator <= 0,1.,denominator)
    positive = old_steps[:,0].copy()
    positive[changed] = np.abs(np.abs(target[changed])-np.abs(current[changed]))/denominator[changed]
    new_steps = np.column_stack((positive,-positive))
    updated = gain_step(current,target,mode=2,rates=new_steps)
    if not np.isfinite(updated).all() or not np.isfinite(new_steps).all():
        raise ValueError('nonfinite mode1 gain state')
    return {'current':updated,'previous_target':target.copy(),'previous_mode':1,
            'steps':new_steps,'recomputed':changed}


def load_static_inventory(image=IMAGE, loader=LOADER, binding=BINDING):
    """Decode exact descriptors and proven writer targets, never current RAM.

    Stored-target extraction here is deliberately PARTIAL. The companion
    amp_second_b_control adds recovered target writers; this helper does not
    fill gain controls, input packed EQ, scalar coefficients or delay requests
    with zeros or identity.
    """
    sys.path.insert(0, str(ROOT / 'tools'))
    from extract_amp_sharc_app import SOURCE_SHA256, BITREV_SHA256, parse_blocks
    from audit_amp_output_bindings import loader_dm, source_offset, filter_record
    from inspect_amp_visa import load_shortwords, decode
    from amp_dsp_chain import design_to_sos
    from amp_complex_timing import second_b_binding
    raw, ld = Path(image).read_bytes(), Path(loader).read_bytes()
    if hashlib.sha256(raw).hexdigest() != SOURCE_SHA256 or hashlib.sha256(ld).hexdigest() != BITREV_SHA256:
        raise ValueError('exact audited V850/SHARC images required')
    binding_data = json.loads(Path(binding).read_text())
    if binding_data['source_sha256'] != SOURCE_SHA256:
        raise ValueError('binding source does not match firmware')
    boundary = second_b_binding(binding_data)
    dm = loader_dm(ld, parse_blocks(ld)); mem = load_shortwords(ld)
    # Disassembly pins: input shared sections, packed counts, scalar coeff/state,
    # matrix dimensions, and delay capacity reads. Full image hashes pin the rest.
    pins = {0x126578: 'M3=DM(+5,I0)', 0x12657a: 'R14=DM(+9,I0)',
            0x126597: 'I10=DM(+10,I0)', 0x126ac1: 'R14=DM(+9,I0)',
            0x126ac3: 'I12=DM(+10,I0)', 0x126b1b: 'R13=PASS R13',
            0x1260b1: 'I9=DM(+5,I2)', 0x1260b3: 'I4=DM(+6,I2)',
            0x1287bb:'R1=DM(+22,I0)',0x1287bd:'DM(+22,I0)=R0',
            0x1287f3:'R5=DM(+11,I0)',0x1287fb:'I4=DM(+14,I0)',
            0x12880d:'IF EQ JUMP 0x12882d (DB)',
            0x128810:'F10=F2-F3 || R1=DM(I4,M6)',
            0x128813:'F1=F1*F5, F2=ABS F10 || R7=DM(M5,I5)',
            0x128818:'IF LE R13=R0',0x12882d:'F7=-F7 || DM(I5,M6)=R7'}
    for pc, expected in pins.items():
        if decode(mem, pc)[1] != expected: raise ValueError(f'instruction changed at {pc:x}')
    nodes = []
    for index in range(21, 51):
        n = copy.deepcopy(binding_data['graph_nodes'][index]); address = int(n['descriptor'], 16)
        # Parse ports directly from loader as a cross-check against derived JSON.
        if (dm[int(n['schedule_word'],16)] != address or dm[address] != int(n['class_descriptor'],16)
                or dm[dm[address]+1] != int(n['process_visa_address_candidate'],16)
                or n['port_ids'] != [dm[dm[address+1]+j] for j in range(dm[address+2])]):
            raise ValueError('schedule ports disagree with loader')
        n['schedule_index'] = index
        handler = n['process_visa_address_candidate']
        fields = {'channel_count': dm[address+5]}
        if handler == '0x12879c':
            fields.update(current=hex(dm[address+9]), target=hex(dm[address+12]),
                          loader_mode=dm[address+10], rates=hex(dm[address+17]),
                          loader_time_scale=struct.unpack('<f',struct.pack('<I',dm[address+11]))[0])
        elif handler == '0x126a95':
            fields.update(capacity=dm[address+9], counts=hex(dm[address+10]),
                          records=hex(dm[address+12]), history=hex(dm[address+13]))
        elif handler == '0x126559':
            fields.update(sections=dm[address+9], records=hex(dm[address+10]), history=hex(dm[address+11]))
        elif handler == '0x1269bc':
            fields.update(outputs=dm[address+7], coefficients=hex(dm[address+9]))
        elif handler == '0x126092':
            fields = {'records': hex(dm[address+5]), 'history': hex(dm[address+6])}
        elif handler == '0x127a4d':
            fields = {'gain_address': hex(address+5)}
        elif handler == '0x124517':
            fields.update(capacity_samples=dm[address+13], ring_samples=dm[address+12],
                          current=hex(dm[address+14]), target=hex(dm[address+15]))
        else: raise ValueError('unmodeled second-B handler')
        n['parameters'] = fields; nodes.append(n)
    # A928A: three unity coefficients, sixteen targets C0840+4*i.
    # AC59E: three unity coefficients at C073A (side branch).
    for pc, expected in {0xA92C8:'210640080c00', 0xA92D6:'40ee803f',
                         0xAC5B6:'2f063a070c00', 0xA9568:'33067c060e00',
                         0xA9A8C:'21069c060e00', 0xAB6BA:'3d06708a0600'}.items():
        if raw[pc:pc+len(expected)//2].hex() != expected:
            raise ValueError(f'writer pin changed at {pc:x}')
    def packed(rec):
        b0,b1,b2,_,a1,a2 = design_to_sos(rec)
        return [-a1,-a2,b0,b1,b2]
    pre_targets = struct.unpack_from('<2I', raw, 0x68A70)
    if pre_targets[1] != 0xE05AC: raise ValueError('second shared prefilter target mismatch')
    pre = filter_record(raw, 1, source_offset(-0x1285C), 0xE05A7)
    counts = list(raw[source_offset(-0x134CD):source_offset(-0x134CD)+16])
    records = [filter_record(raw, i, source_offset(-0x134BC), 0xE069C) for i in range(48)]
    targets = {'0xc4b70': {'records': [packed(pre)]},
               '0xc4c1a': {'coefficients': [1.,1.,1.]},
               '0xc4ce8': {'counts': counts, 'records': [packed(r) for r in records[:sum(counts)]]}}
    for n in nodes:
        if 33 <= n['schedule_index'] <= 48:
            targets[n['descriptor']] = {'coefficients': [1.,1.,1.]}
    return {'source_sha256': SOURCE_SHA256, 'loader_sha256': BITREV_SHA256,
            'binding_sha256': hashlib.sha256(Path(binding).read_bytes()).hexdigest(),
            'nodes': nodes, 'boundary': boundary, 'stored_writer_targets': targets,
            'stored_target_evidence': {
                'prefilter': {'writer':'0xab6a6','table':'0x68a70','record':pre},
                'three_input_matrices': {'writers':['0xac59e','0xa928a'], 'target':'C073A and C0840+4*i'},
                'post_eq': {'count_writer':'0xa952e', 'count_source':hex(source_offset(-0x134CD)),
                            'record_writer':'0xa9a2c', 'records':records}},
            'scope': 'Stored writer targets after delivery, not live state; partial calibration only.'}


class _EQ:
    def __init__(self, counts, records, history, capacity):
        if not sum(counts):
            records = _shape(records, (0,), 'empty EQ records').reshape(0,5)
            history = _shape(history, (0,), 'empty EQ history').reshape(0,2)
        self.eq = PackedOutputEQ(counts, records, capacity)
        self.eq.state[:] = _shape(history, (sum(counts), 2), 'EQ history')
    def process(self, x): return self.eq.process(x)


class SecondBAuxReference:
    """Streaming reference, gain updates once each 64-frame scheduler block.

    scenario={'label':str, 'stages':{descriptor: effective parameters...}}.
    Every stage must be present; the helper's stored_writer_targets is only a
    partial overlay. Histories use the reference's DF-II-transposed state and
    oldest-to-newest delay queues. Start phase is zero, with explicit histories.
    Partial process calls preserve phase: no invented zero padding/flush.
    """
    def __init__(self, *, scenario, inventory=None):
        self.inventory = copy.deepcopy(load_static_inventory() if inventory is None else inventory)
        self.scenario = copy.deepcopy(_json(scenario))
        if not isinstance(scenario.get('label'), str) or not scenario['label'].strip():
            raise ValueError('explicit scenario label required')
        self.nodes = self.inventory['nodes']
        stages = scenario.get('stages', {})
        if set(stages) != {n['descriptor'] for n in self.nodes}:
            raise ValueError('exact parameters for every second-B descriptor required')
        self.processors, self.gain_state, self.matrices = {}, {}, {}
        self.phase = 0
        for node in self.nodes:
            desc = node['descriptor']; p = copy.deepcopy(stages[desc]); fields = node['parameters']
            handler = node['process_visa_address_candidate']
            required = {'0x12879c':{'current','target','mode','rates'},
                        '0x126559':{'records','history'},
                        '0x126a95':{'counts','records','history'},
                        '0x126092':{'record','history'},
                        '0x1269bc':{'coefficients'}, '0x127a4d':{'gain'},
                        '0x124517':{'samples','history','lifecycle'}}
            if handler == '0x12879c' and p.get('mode') == 1:
                required[handler] = {'current','target','mode','previous_mode','previous_target',
                                     'time_parameters','scale','steps'}
            if set(p) != required.get(handler):
                raise ValueError(f'{desc}: exact effective parameter/history fields required')
            n = fields.get('channel_count')
            if handler == '0x12879c':
                current, target = _shape(p['current'], (n,), desc), _shape(p['target'], (n,), desc)
                mode = p['mode']
                if type(mode) is not int or mode not in (0,1,2,3):
                    raise ValueError('only modes0..3 supported')
                if mode == 1:
                    state = dict(current=current,target=target,mode=mode,
                                 previous_mode=p['previous_mode'],
                                 previous_target=_shape(p['previous_target'],(n,),desc),
                                 time_parameters=_shape(p['time_parameters'],(n,),desc),
                                 scale=float(_shape(p['scale'],(),desc)),steps=_shape(p['steps'],(n,2),desc))
                    # Validate without advancing or substituting supplied state.
                    mode1_gain_update(**{k:v for k,v in state.items() if k!='mode'})
                    self.gain_state[desc] = state
                    continue
                rates = _shape(p['rates'], (n,2), desc)
                if (mode == 3 and (rates < 0).any()) or (mode == 2 and ((rates[:,0] < 0).any() or (rates[:,1] > 0).any())):
                    raise ValueError('invalid gain rates')
                self.gain_state[desc] = dict(current=current,target=target,mode=mode,rates=rates)
            elif handler == '0x126559':
                sections = fields['sections']; records = _shape(p['records'], (sections,5), desc)
                history = _shape(p['history'], (n,sections,2), desc)
                self.processors[desc] = _EQ([sections]*n, np.tile(records,(n,1)), history.reshape(-1,2), n*sections)
            elif handler == '0x126a95':
                counts = p['counts']
                if len(counts) != n: raise ValueError('packed counts must match node channels')
                self.processors[desc] = _EQ(counts, p['records'], p['history'], fields['capacity'])
            elif handler == '0x126092':
                self.processors[desc] = _EQ([1], [_shape(p['record'], (5,), desc)],
                                             [_shape(p['history'], (2,), desc)], 1)
            elif handler == '0x1269bc':
                self.matrices[desc] = _shape(p['coefficients'], (3,), desc)
            elif handler == '0x127a4d':
                self.scalar_gain = float(_shape(p['gain'], (), desc))
            elif handler == '0x124517':
                delays = np.asarray(p['samples'])
                if (delays.shape != (16,) or delays.dtype.kind not in 'iu' or (delays < 0).any()
                        or int(delays.sum()) > fields['capacity_samples'] or p['lifecycle'] != 'settled_dma'):
                    raise ValueError('16 settled delays within shared capacity required; lifecycle not emulated')
                if len(p['history']) != 16: raise ValueError('16 explicit delay histories required')
                self.delay_history = [_shape(h, (int(d),), desc) for h,d in zip(p['history'], delays)]
            else: raise ValueError('unknown handler')

    def metadata(self):
        return {'label':self.scenario['label'], 'kind':'explicit_second_B_aux_reference',
                'source_sha256':self.inventory['source_sha256'],
                'loader_sha256':self.inventory['loader_sha256'],
                'profile_scope':'global_fixed_calibration_not_profile_specific',
                'scenario_sha256':hashlib.sha256(json.dumps(self.scenario,sort_keys=True,allow_nan=False).encode()).hexdigest(),
                'input_graph_ids':[8,9,10], 'output_B_slot_ports':self.inventory['boundary']['ordered_b_ports'],
                'block_frames':64, 'initial_history':'explicit caller state, not implicitly zero',
                'gain_update':'once per64 frames; modes0/2/3 or explicit mode1 previous-target/time/steps; phase preserved across chunks',
                'delay_lifecycle':'settled integer delays only; no DMA startup/transitions/errors',
                'side_branch':'nodes29..31 execute but graph60 has no later consumer in21..50',
                'live_state_proven':False, 'physical_input_mapping_proven':False,
                'warning':'No ten/split16 donor. Fixed effective coefficients, no bit-exact SHARC or control delivery.'}

    def _segment(self, x, trace):
        values = {port:x[:,i:i+1].copy() for i,port in enumerate((8,9,10))}
        for node in self.nodes:
            desc = node['descriptor']; ports = node['port_ids']; h = node['process_visa_address_candidate']
            count = 3 if h == '0x1269bc' else (len(ports)-1)//2 if h == '0x126559' else len(ports)//2
            inputs, outputs = ports[:count], ports[count:2*count]
            if h == '0x1269bc': outputs = ports[3:]
            block = np.concatenate([values[p] for p in inputs],axis=1)
            if h == '0x12879c':
                state = self.gain_state[desc]
                if self.phase == 0:
                    if state['mode'] == 1:
                        update = mode1_gain_update(**{k:v for k,v in state.items() if k!='mode'})
                        state.update({k:v for k,v in update.items() if k!='recomputed'})
                    else:
                        state['current'] = gain_step(state['current'],state['target'],mode=state['mode'],rates=state['rates'])
                y = block*state['current']
            elif h in ('0x126559','0x126a95','0x126092'):
                y = self.processors[desc].process(block)
            elif h == '0x1269bc': y = (block@self.matrices[desc])[:,None]
            elif h == '0x127a4d': y = block*self.scalar_gain
            elif h == '0x124517':
                y = np.empty_like(block)
                for i, hist in enumerate(self.delay_history):
                    buffered = np.concatenate((hist,block[:,i]))
                    y[:,i] = buffered[:len(block)]
                    self.delay_history[i] = buffered[len(block):].copy()
            else: raise ValueError('unknown handler')
            if not np.isfinite(y).all(): raise ValueError(f'nonfinite stage output at {desc}')
            for i, port in enumerate(outputs): values[port] = y[:,i:i+1].copy()
            if trace is not None: trace.setdefault(desc,[]).append(y.copy())
        return np.concatenate([values[p] for p in self.inventory['boundary']['ordered_b_ports']],axis=1)

    def process(self, auxiliary, *, capture_stages=False):
        x = _finite(auxiliary)
        if x.ndim != 2 or x.shape[1] != 3: raise ValueError('explicit graph8/9/10 frames×3 required')
        out = np.empty((len(x),16)); trace = {} if capture_stages else None
        offset = 0
        while offset < len(x):
            count = min(64-self.phase, len(x)-offset)
            out[offset:offset+count] = self._segment(x[offset:offset+count],trace)
            self.phase = (self.phase+count)%64; offset += count
        if trace is not None:
            return {'out':out, 'stages':{k:np.concatenate(v) for k,v in trace.items()}}
        return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,default=IMAGE)
    parser.add_argument('--loader',type=Path,default=LOADER)
    parser.add_argument('--binding',type=Path,default=BINDING)
    parser.add_argument('--output',type=Path)
    args = parser.parse_args()
    inventory = load_static_inventory(args.image,args.loader,args.binding)
    encoded = json.dumps(inventory,indent=2,allow_nan=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(encoded)
    else: print(encoded,end='')


if __name__ == '__main__': main()
