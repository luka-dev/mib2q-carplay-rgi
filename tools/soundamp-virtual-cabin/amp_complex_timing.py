"""Explicit C4D17 calibration excitation and timing reference.

Closes generator shaping (126D55..126D95) and stage4 output gating
(127172..1271AF), not calibration detector/LMS composition or live state.
All generator, sequencer, scaling and filter histories are explicit. Float64
math over the modular PRNG reference is NOT instruction-rounding emulation.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys

import numpy as np
from scipy.signal import sosfilt

from amp_complex_modes import CalibrationNoiseCore, CalibrationSequence, select_stage_mask
from amp_complex_path import biquad_sos

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'


def load_builtin_records(loader=LOADER):
    """Hash-pinned loader constants, never guessed coefficient defaults."""
    sys.path.insert(0, str(ROOT/'tools'))
    from extract_amp_sharc_app import BITREV_SHA256, parse_blocks
    from audit_amp_output_bindings import loader_dm
    data = Path(loader).read_bytes()
    if hashlib.sha256(data).hexdigest() != BITREV_SHA256:
        raise ValueError('audited bit-reversed SHARC loader required')
    dm = loader_dm(data, parse_blocks(data))
    return np.array([[struct.unpack('<f', struct.pack('<I', dm[a+i]))[0]
                      for i in range(5)] for a in (0xB94F4, 0xB94F9)])


class CalibrationShapedNoise:
    """126CEC: raw core, then TWO cascaded scalar biquads for types1/9.

Type1 uses supplied hash-verified builtins, type9 supplied descriptor+14/+15
records. Every other type is raw. Initial SOS state is always supplied, even
when zero; no inference that current firmware filter history is empty.
"""
    def __init__(self, *, noise_type, core_state, builtin_records,
                 custom_records, initial_filter_state):
        if type(noise_type) is not int or not 0 <= noise_type <= 0xffffffff:
            raise ValueError('explicit uint32 generator type required')
        self.noise_type = noise_type
        selected = builtin_records if noise_type == 1 else custom_records if noise_type == 9 else None
        if noise_type in (1, 9):
            selected = np.asarray(selected, dtype=float)
            if selected.shape != (2, 5) or not np.isfinite(selected).all():
                raise ValueError('selected shaping type requires two finite five-word records')
            self.sos = np.array([biquad_sos(r) for r in selected])
        else:
            self.sos = None
        self.state = np.array(initial_filter_state, dtype=float, copy=True)
        if self.state.shape != (2, 2) or not np.isfinite(self.state).all():
            raise ValueError('explicit finite two-section two-state history required')
        self.core = CalibrationNoiseCore(**copy.deepcopy(core_state))

    def process(self, frames):
        raw = self.core.process(frames)
        if not frames or self.sos is None:
            return raw
        out, self.state = sosfilt(self.sos, raw, zi=self.state)
        return out


class CalibrationExcitationReference:
    """Mode2 stages1/3/4 only, 64-frame scheduler calls.

Produces ten stage4 music lanes before detector/LMS work. The calibration
mask excludes stage8: do NOT add full-mode's1023-sample delay here. Sequence
configure/wait/complete clears outputs without advancing the noise state.
The one completion fall-through block uses previous gates, per1270FD.
"""
    def __init__(self, *, sequence, generator, excitation_gain):
        if not isinstance(sequence, CalibrationSequence) or not isinstance(generator, CalibrationShapedNoise):
            raise ValueError('explicit calibration sequencer and generator required')
        if not np.isscalar(excitation_gain) or not np.isfinite(excitation_gain):
            raise ValueError('finite explicit descriptor+30 linear gain required')
        self.sequence = copy.deepcopy(sequence)
        self.generator = copy.deepcopy(generator)
        self.excitation_gain = float(excitation_gain)

    def process_block(self):
        action = self.sequence.begin_block(64)
        audio = np.zeros((64, 10))
        if action['action'] == 'generate_and_learn':
            wave = self.generator.process(64)*self.excitation_gain
            gates = [(action['gate_bits'] >> i) & 1 for i in range(10)]
            audio = wave[:, None]*np.array(gates)[None, :]
        return {'audio':audio, 'sequence':action,
                'scope':'stage4 excitation only; detector/LMS and hardware rounding excluded'}


def timing_inventory(audit):
    """Distinguish audio delay, update cadence, and buffer-write lifecycle."""
    profiles = []
    for p in audit['complex_control_profiles']:
        d = p['detector_layout']
        delays = [max(0, min(v['requested'], v['maximum'])) for v in p['stage8_loader_delays']]
        profiles.append({'profile':p['profile_index'], 'full_mode_pair_delay_samples':delays,
                         'audio_call_frames':d['audio_frames'],
                         'target_update_interval_audio_samples':d['audio_frames']*d['history_blocks'],
                         'detector_prefix_frames_per_call':d['detector_frames']})
    modes = []
    for word, sideband in ((0,0),(1,0),(2,0),(4,0),(4,1),(7,0)):
        mask, inhibit = select_stage_mask(word, sideband)
        modes.append({'mode_word':word, 'sideband':sideband, 'stage_mask':hex(mask),
                      'stage8_delay_active':bool(mask & (1<<8)), 'inhibit':inhibit})
    return {'profiles':profiles, 'modes_after_enable_and_binding':modes,
            'master_disabled_or_first_binding':'no output write, NOT inferred zero/copy',
            'c4a54':'settled uniform0..9600 samples; transition/DMA scheduling NOT emulated',
            'timing_is_not_acoustic_group_delay':True, 'live_state_proven':False}


def second_b_binding(binding):
    """Schedule-ordered structural dependencies, NOT numeric B audio.

Repeated graph IDs are storage reuse. Read all node inputs before publishing
outputs and track only schedule21..50; unrelated earlier music users of24/23/22
must not leak into this dependency result. Matrix edges conservatively include
every declared input regardless of current coefficient values.
"""
    nodes=binding['graph_nodes']
    if (nodes[21]['descriptor'],nodes[49]['descriptor'],nodes[50]['descriptor'],nodes[55]['descriptor']) != (
            '0xc4b70','0xc4ce8','0xc4cf9','0xc4dfb'):
        raise ValueError('audited second-B schedule boundary required')
    dependencies={i:{i} for i in (8,9,10)}
    producers={i:'external_graph_input' for i in (8,9,10)}
    for index in range(21,51):
        node=nodes[index];ports=node['port_ids'];handler=node['process_visa_address_candidate']
        if handler=='0x1269bc':
            if len(ports)!=4:raise ValueError('expected three-input one-output auxiliary matrix')
            incoming=ports[:3];outgoing=ports[3:]
            roots=[set().union(*(dependencies[p] for p in incoming))]
        else:
            if handler not in ('0x126559','0x12879c','0x126a95','0x126092','0x127a4d','0x124517'):
                raise ValueError('unexamined auxiliary handler')
            n=(len(ports)-1)//2 if handler=='0x126559' else len(ports)//2
            if len(ports)!=(2*n+1 if handler=='0x126559' else 2*n):
                raise ValueError('unexpected auxiliary port layout')
            incoming=ports[:n];outgoing=ports[n:2*n]
            roots=[dependencies[p].copy() for p in incoming]
        for port, root in zip(outgoing,roots):
            dependencies[port]=root;producers[port]=node['descriptor']
    tail=nodes[50]['port_ids'][16:]
    mixer=nodes[55]['port_ids'][16:32]
    if tail!=mixer or len(tail)!=16:
        raise ValueError('delay outputs do not exactly match second-B mixer half')
    intervening_outputs=(nodes[51]['port_ids'][1:2]+nodes[52]['port_ids'][12:22]
                         +nodes[53]['port_ids'][10:20]+nodes[54]['port_ids'][10:26])
    if set(tail)&set(intervening_outputs):
        raise ValueError('intervening audio writer overwrites second-B boundary')
    gains=binding['known_electronic_stages']['output_pair_mixer']['b_linear']
    return {'producer_descriptor':'0xc4cf9','producer_schedule_index':50,
            'producer_handler':'0x124517','upstream_post_eq_descriptor':'0xc4ce8',
            'mixer_descriptor':'0xc4dfb','mixer_schedule_index':55,
            'ordered_b_ports':tail,'slot_possible_input_roots':[sorted(dependencies[p]) for p in mixer],
            'source_roots':sorted(set().union(*(dependencies[p] for p in mixer))),
            'stored_nonzero_b_slots':[i for i,gain in enumerate(gains) if gain!=0],
            'warning':'Structural auxiliary8/9/10 ancestry, not PCM identity, current coefficients or silence proof; no music donor binding.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--loader', type=Path, default=LOADER)
    parser.add_argument('--audit', type=Path, default=ROOT/'output/r2/soundamp/remaining-blocks-20260919/audit.json')
    parser.add_argument('--binding', type=Path, default=ROOT/'output/r2/soundamp/output_binding_audit.json')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    records = load_builtin_records(args.loader)
    audit = json.loads(args.audit.read_text())
    from extract_amp_sharc_app import parse_blocks
    from inspect_amp_visa import load_shortwords, listing
    loader_data = args.loader.read_bytes()
    blocks = parse_blocks(loader_data)
    memory = load_shortwords(loader_data)
    evidence_ranges = {}
    for name, start, end in (('noise',0x126CEC,0x126DB2),
                             ('scalar_biquad',0x126092,0x12610B),
                             ('stage4_excitation',0x12716C,0x1271B1)):
        lines = list(listing(memory,start,end))
        if any('UNKNOWN' in line for line in lines):
            raise ValueError('unresolved instruction in calibration evidence')
        evidence_ranges[name] = {'start_shortword':hex(start),'end_exclusive_shortword':hex(end),
                                 'instruction_count':len(lines),
                                 'listing_sha256':hashlib.sha256(('\n'.join(lines)+'\n').encode()).hexdigest()}
    record_offsets = []
    for address in (0xB94F4,0xB94F9):
        owners = [b for b in blocks if b.tag==4 and b.address<=address and address+5<=b.address+b.count]
        if len(owners)!=1:
            raise ValueError('expected one explicit loader record owner')
        block=owners[0]
        offset=block.offset+12+4*(address-block.address)
        record_offsets.append({'word_address':hex(address),'loader_file_offset':hex(offset),
                               'record_sha256':hashlib.sha256(loader_data[offset:offset+20]).hexdigest()})
    report = {'scope':'static calibration-excitation closure and conditional timing inventory',
              'loader_sha256':hashlib.sha256(args.loader.read_bytes()).hexdigest(),
              'audit_sha256':hashlib.sha256(args.audit.read_bytes()).hexdigest(),
              'source_sha256':audit['source_sha256'],
              'builtin_addresses':['0xB94F4','0xB94F9'],
              'builtin_record_sources':record_offsets,
              'decoded_instruction_evidence':evidence_ranges,
              'builtin_records_minus_a1_minus_a2_b0_b1_b2':records.tolist(),
              'generator_type1_selector_pc':'0x126D5A',
              'generator_type9_selector_pc':'0x126D66',
              'cascade_call_pcs':['0x126D7E','0x126D8F'],
              'stage4_gate_gain_pc':'0x12719D',
              'binding_audit_sha256':hashlib.sha256(args.binding.read_bytes()).hexdigest(),
              'second_b_binding':second_b_binding(json.loads(args.binding.read_text())),
              'timing':timing_inventory(audit)}
    encoded = json.dumps(report, indent=2, allow_nan=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded)
        print(args.output)
    else:
        print(encoded, end='')


if __name__ == '__main__':
    main()
