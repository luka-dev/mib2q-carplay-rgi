#!/usr/bin/env python3
"""Read-only verification of recovered AmpChain stages, J525 SW0420.

This checks the stored profile-0 calibration, not the live amplifier or its
complete nonlinear processing. It does not modify AmpChain or production code.
Run: python3 tools/soundamp-virtual-cabin/audit_bank1_model.py IMAGE --output OUT.json
"""
import argparse
import json
from pathlib import Path
import struct
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import audit_amp_output_bindings as binding
from amp_dsp_chain import AUDIT, AmpChain, MATRIX_DB, design_to_sos
from digital_stages import PackedOutputEQ, pair_mix, select


def inspect(image):
    data = image.read_bytes()
    recovered = binding.audit(data)  # Enforces exact source SHA and size.
    chain = AmpChain()
    stored_matrix = np.array(struct.unpack_from('<64b', data, 0x101BF4)).reshape(8, 8)[:, :6]
    assert np.array_equal(stored_matrix, MATRIX_DB)
    assert recovered == json.loads(AUDIT.read_text())

    # V850 0xA9FF4: gp-0x13608 (profile A), gp-0x13618 (A polarity),
    # gp-0x1333C (global B) -> alternating words at SHARC 0xE453A.
    # Instructions AA06A..AA07C use lookup address 0x69398 - (int8 << 3).
    # Unlike the first pair mixer, each stored unit is 0.2 dB; -128 mutes.
    codes_a = struct.unpack_from('<16b', data, binding.source_offset(-0x13608))
    codes_b = struct.unpack_from('<16b', data, binding.source_offset(-0x1333C))
    polarity = data[binding.source_offset(-0x13618):binding.source_offset(-0x13618) + 16]

    def gain(code):
        return 0.0 if code == -128 else struct.unpack_from('<f', data, 0x69398 - 8 * code)[0]

    gains_a = np.array([gain(v) * (-1 if polarity[i] == 0x5A else 1)
                        for i, v in enumerate(codes_a)])
    gains_b = np.array([gain(v) for v in codes_b])
    route = recovered['normal_route']
    perm = recovered['v850_config_to_sharc_slot']
    inverse = recovered['sharc_slot_to_v850_config']

    # Independent numeric composition: selectors -> split-half pair mixer ->
    # DSP-slot packed coefficient recurrence -> inverse output permutation/delay.
    # Zero B is explicit for this equivalence check, not a live-state claim.
    source = np.random.default_rng(4).normal(0, .01, (4096, 6))
    stages = chain.process(source)
    expanded = select(stages['ten'], recovered['split_modes'][0])
    mixed = pair_mix(np.concatenate([expanded, np.zeros_like(expanded)], axis=1),
                     np.column_stack([gains_a, gains_b]).ravel())
    dsp_order = select(mixed, route)[:, inverse]
    records = [r for c in sorted(recovered['channels'], key=lambda c: c['sharc_processing_slot'])
               for r in c['packed_eq_records']]
    sos = np.array([design_to_sos(r, chain.zeta) for r in records])
    packed = np.column_stack([-sos[:, 4], -sos[:, 5], sos[:, :3]])
    actual = PackedOutputEQ(recovered['output_eq_counts'], packed).process(dsp_order)[:, perm]
    for channel, delay in enumerate(chain.out_delay):
        if delay:
            actual[:, channel] = np.r_[np.zeros(delay), actual[:-delay, channel]]
    expected = stages['out']
    error = float(np.max(np.abs(actual - expected)))
    assert error < 1e-10

    # Additional design banks are included as verified unity identities.
    neutral_records = []
    for base, count in [(binding.source_offset(-0x1284C) + 0x80, 16),
                        (binding.source_offset(-0x1258C), 8),
                        (binding.source_offset(-0x1254C), 8)]:
        for index in range(count):
            record = binding.filter_record(data, index, base)
            assert record['type_code'] == 1 and record['gain_tenths_db'] == 0
            neutral_records.append(record['file_offset'])

    return {
        'source_sha256': recovered['source_sha256'], 'profile': 0,
        'model': chain.metadata(),
        'writer': 'V850 0xA9FF4; instruction check AA06A..AA07C, AA0D4..AA0E6',
        'mixer_descriptor': '0xC4DFB', 'parameter_address': '0xE453A',
        'matrix_db_matches': True, 'schedule_nodes': recovered['graph_schedule_count'],
        'mixer_a_codes': codes_a, 'mixer_b_codes': codes_b,
        'mixer_a_linear': gains_a.tolist(), 'mixer_b_linear': gains_b.tolist(),
        'per_output_a_gain_db': [float(20*np.log10(abs(g))) for g in gains_a[route]],
        'output_labels': chain.label, 'numeric_composition_max_error': error,
        'additional_unity_peak_records': neutral_records,
        'pre_eq_delay_vectors': [list(struct.unpack_from('<8H', data, binding.source_offset(v)))
                                 for v in (-0x13638, -0x13628)],
        'limitations': ['B input is zero only for numerical equivalence test',
                       'complex processor and other gain/dynamic stages not emulated',
                       'input matrix floor near -99 dB is included separately from pair-mixer mutes',
                       'speaker labels remain candidates; no acoustic power or SPL prediction'],
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = inspect(args.image)
    text = json.dumps(report, indent=2) + '\n'
    if args.output:
        if args.output.resolve() == args.image.resolve():
            raise SystemExit('refusing to overwrite firmware')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    print(text)
