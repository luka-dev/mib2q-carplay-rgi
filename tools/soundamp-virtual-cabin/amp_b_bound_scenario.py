"""Explicit B parameter-to-audio bindings; never an inferred vehicle preset.

The effective snapshot and replay target slice are deliberately separate types
of result. A partial replay must not masquerade as a whole settled configuration.
Fixed successful mode2/5 output finish only; zero-reset host filter state.
"""
import hashlib
import math
import struct
import numpy as np

from amp_b_control import (api18_19_parameter_targets, replay_branch_gain_targets,
                           zero_gain_eq_coefficients, update_parameter_slot)
from amp_b_mode_control import resolve_mode_parameters
from amp_b_output import BOutputFinishReference
from amp_b_eq_design import design_b_eq


def _config(values):
    if len(values) != 829 or not all(math.isfinite(v) for v in values):
        raise ValueError('explicit finite effective829-word float configuration required')
    return list(values)


def retained_config(report, *, source, bank):
    """Decode a retained image bank as a SCENARIO, not active/effective state."""
    if source not in ('loader', 'b01') or type(bank) is not int or bank not in range(1, 5):
        raise ValueError('explicit loader/b01 and B calibration bank1..4 required')
    record = report['sources'][source]['banks'][bank-1]
    if record['bank'] != bank:
        raise ValueError('bank record mismatch')
    if len(record['words']) != 829 or hashlib.sha256(struct.pack('<829I', *record['words'])).hexdigest() != record['sha256']:
        raise ValueError('retained bank word fingerprint mismatch')
    return _config([struct.unpack('<f', struct.pack('<I', w))[0] for w in record['words']])


def _sections(config, count_offset, capacity):
    # 12DBDE: round positive count by trunc(value + .5). Keep the bounded
    # retained-bank capacity; arbitrary indexed writes may exceed this scope.
    value = config[count_offset]
    if not 0 <= value < capacity + .5:
        raise ValueError('filter count outside bounded inline bank')
    count = math.trunc(float(np.float32(np.float32(value) + np.float32(.5))))
    if count > capacity:
        raise ValueError('rounded filter count outside bounded inline bank')
    return [config[count_offset+1+5*i:count_offset+6+5*i] for i in range(count)]


def bind_effective_output(*, effective_config, library_mode, successful_filter_path):
    """Bind recovered INLINE banks, not pointer-selected tail gains.

Caller explicitly selects settled coefficients and successful filter behavior.
No ready/transition/reset/mute/error emulation. A mode6 early return is NOT the
same finish. Zero section banks are excluded: firmware inactive lifecycle may
clear audio/state rather than the host cascade's identity convention.
"""
    config = _config(effective_config)
    if type(library_mode) is not int or library_mode not in (2, 5):
        raise ValueError('successful library mode2/5 required')
    if successful_filter_path is not True:
        raise ValueError('explicit successful active filter path required')
    args = {name: config[offset] for name, offset in
            [('four_gain', 122), ('six_gain', 293), ('two_gain', 377), ('final_gain', 420)]}
    for name, offset, capacity in [('six_eq', 294, 2), ('two_eq', 378, 2), ('final_eq', 399, 4)]:
        args[name] = _sections(config, offset, capacity)
        if not args[name]:
            raise ValueError('zero-section lifecycle is outside fixed active reference')
    # Construction also checks stability; no silently replaced unstable filters.
    BOutputFinishReference(**args)
    return {'kind': 'explicit_effective_output_slice', 'library_mode': library_mode,
            'finish_parameters': args, 'actual_vehicle_state_proven': False,
            'boundary': 'tail boundary audio -> eight outputs; no spectral generator, tail, graph map, lifecycle or live state'}


def build_effective_finish(*, effective_config, library_mode, successful_filter_path):
    """Fresh zero-history BOutputFinishReference from explicit effective values."""
    result = bind_effective_output(effective_config=effective_config, library_mode=library_mode,
                                  successful_filter_path=successful_filter_path)
    return BOutputFinishReference(**result['finish_parameters'])


def bind_effective_mode(mode_binding, *, effective_config, pointers):
    """Expose only recovered pointer consumers; preserve all18 explicit offsets."""
    return {'kind': 'explicit_effective_mode_slice',
            'parameters': resolve_mode_parameters(mode_binding, effective_config=_config(effective_config),
                                                   pointers=pointers),
            'unresolved_pointer_words': [1162, 1163, 1164, 1165],
            'actual_vehicle_state_proven': False}


def replay_output_target_slice(binding, *, source, effective_values, contributions, shadow_selectors,
                               allow_nonzero_eq=False):
    """132168 replay slice: three branch gains, four EQs and two delays.

Explicit all15 prior slot banks, all9 effective controls, both shadow selectors.
Uses RAW API25 as this replay does, not the single-update baseline subtraction.
This is NOT a complete replay: API21/22 update descriptor54 (final gain) and
other parameters not exported by the existing branch-gain helper. Accordingly
NO effective829-word config and NO complete output-finish object are returned.
    Targets are not instantaneous audio coefficients. Nonzero EQ requires an
    explicit opt-in to the mathematical (not bit-exact) designer.
"""
    if set(effective_values) != {row['api'] for row in binding['slots']}:
        raise ValueError('all nine effective controls required')
    if type(allow_nonzero_eq) is not bool:
        raise ValueError('explicit boolean nonzero-EQ opt-in required')
    state = contributions
    results = {}
    for api in (19, 18):
        result = api18_19_parameter_targets(binding['parameter_controls'], source=source, api=api,
            coordinate=effective_values[api], contributions=state, shadow_selectors=shadow_selectors)
        state = result['contributions']; results[api] = result
    gain = replay_branch_gain_targets(binding, source=source, effective_values=effective_values,
                                      contributions={d: state[d] for d in (46, 50, 51)})
    for d, slots in gain['contributions'].items():
        state[d] = slots
    updates = {binding['bases'][str(d)]['offset']: v for d, v in gain['targets'].items()}
    # API18 does not overwrite descriptors52/53; use API19's selected result.
    updates.update({offset: results[19]['config_updates'][offset] for offset in (316, 317)})
    eq = {}
    for offset, design in results[18]['eq_design_records'].items():
        eq[offset] = (design_b_eq if allow_nonzero_eq else zero_gain_eq_coefficients)(design)
        updates.update({offset+i: value for i, value in enumerate(eq[offset])})
    return {'kind': 'partial_replay_target_slice', 'config_updates': updates,
            'branch_gain_targets': gain['targets'], 'eq_records': eq,
            'two_channel_delays_samples': [math.trunc(updates[o]+.5) for o in (316, 317)],
            'contributions_for_covered_helpers_only': state,
            'excluded_replay_descriptors': [47, 48, 54],
            'excluded_final_gain_offset': 420, 'actual_vehicle_state_proven': False,
            'eq_design': _eq_design_metadata(allow_nonzero_eq),
            'boundary': 'partial targets only; no full replay, publication, smoothing, current config or B input/output map'}


# All calls in the two bounded successful helper paths. Independent decoded
# caller execution in the tests checks descriptor, slot, clamps and both pointers.
API21_22_RECORDS = {
    22: {46: (671, 689), 47: (674, 692), 48: (677, 695),
         50: (680, 698), 51: (683, 701), 54: (686, 704)},
    21: {46: (707, 722), 48: (710, 725), 50: (713, 728),
         51: (716, 731), 54: (719, 734)},
}


def api21_22_bindings(report, *, source, bank):
    """Extract the eleven scalar-call records from one retained calibration.

Later source/target-RAM mutation is not inferred. Base descriptor initialization
uses the same calibration here; this is not an arbitrary active-state reader.
"""
    config = retained_config(report, source=source, bank=bank)
    original = report['control_bindings_by_bank'][bank-1]
    if original['config_bank'] != bank:
        raise ValueError('control binding bank mismatch')
    bases = {p['descriptor']: p for p in original['parameter_controls']['apis'][0]['parameters']}
    result = {}
    for api, descriptors in API21_22_RECORDS.items():
        row = next(r for r in original['slots'] if r['api'] == api)
        if row['all_helper_descriptors'] != list(descriptors):
            raise ValueError('extracted descriptor census mismatch')
        result[api] = {'slot': row['slot'], 'accepted_range': row['accepted_range'],
            'helper': row['helper'], 'range_sha256': row['range_sha256'], 'parameters': [
                {'descriptor': d, 'base': bases[d]['base'][source],
                 'target_config_offset': bases[d]['target_config_offset'],
                 'record_offsets': list(offsets), 'records': [config[o:o+3] for o in offsets],
                 'clamp': [0, 5]} for d, offsets in descriptors.items()]}
    return result


def api21_22_parameter_targets(binding, *, api, coordinate, contributions):
    """All successful scalar writes for one API21/22, not delivery or smoothing."""
    if api not in (21, 22) or not math.isfinite(coordinate) or not -200 <= coordinate <= 200:
        raise ValueError('API21/22 and finite coordinate in -200..200 required')
    if set(contributions) != set(range(44, 59)) or any(
            len(v) != 9 or not all(math.isfinite(x) for x in v) for v in contributions.values()):
        raise ValueError('all fifteen finite nine-slot descriptor banks required')
    state = {d: list(v) for d, v in contributions.items()}; targets = {}; updates = {}
    row = binding[api]
    for p in row['parameters']:
        d = p['descriptor']
        result = update_parameter_slot(base=p['base'], contributions=state[d], slot=row['slot'],
            coordinate=coordinate, records=p['records'], lower=p['clamp'][0], upper=p['clamp'][1])
        state[d] = result['contributions']; targets[d] = result['target']
        updates[p['target_config_offset']] = result['target']
    return {'targets': targets, 'config_updates': updates, 'contributions': state,
            'actual_vehicle_state_proven': False}


def replay_bound_scalar_targets(report, *, source, bank, effective_values, contributions, shadow_selectors,
                                allow_nonzero_eq=False):
    """All15 scalar descriptor targets for the nine-effective-control replay.

    Conditional retained calibration and successful helper paths only. Four EQs
    default to verified zero-gain design, optionally use mathematical nonzero
    design. This closes the scalar target gap, NOT the full
config-switch lifecycle, effective smoothing, nonzero EQ or audio input graph.
"""
    extra = api21_22_bindings(report, source=source, bank=bank)
    binding = report['control_bindings_by_bank'][bank-1]
    if set(effective_values) != {r['api'] for r in binding['slots']}:
        raise ValueError('all nine effective controls required')
    if type(allow_nonzero_eq) is not bool:
        raise ValueError('explicit boolean nonzero-EQ opt-in required')
    state = contributions; targets = {}; updates = {}; designs = {}
    for api in (19, 18, 22, 21):
        if api in (19, 18):
            result = api18_19_parameter_targets(binding['parameter_controls'], source=source, api=api,
                coordinate=effective_values[api], contributions=state, shadow_selectors=shadow_selectors)
            designs = result['eq_design_records']
        else:
            result = api21_22_parameter_targets(extra, api=api, coordinate=effective_values[api], contributions=state)
        state = result['contributions']; targets.update(result['targets']); updates.update(result['config_updates'])
    # The remaining five helpers touch only46/50/51. Repeating earlier gain-slot
    # replacements is idempotent; slots are replaced, never accumulated twice.
    gain = replay_branch_gain_targets(binding, source=source, effective_values=effective_values,
                                      contributions={d: state[d] for d in (46, 50, 51)})
    targets.update(gain['targets'])
    for d, slots in gain['contributions'].items():
        state[d] = slots; updates[binding['bases'][str(d)]['offset']] = gain['targets'][d]
    for offset, record in designs.items():
        designer = design_b_eq if allow_nonzero_eq else zero_gain_eq_coefficients
        updates.update({offset+i: v for i, v in enumerate(designer(record))})
    return {'kind': 'retained_calibration_all15_scalar_replay_targets', 'targets': targets,
            'config_updates': updates, 'contributions': state, 'final_gain_target': targets[54],
            'actual_vehicle_state_proven': False,
            'eq_design': _eq_design_metadata(allow_nonzero_eq),
            'boundary': 'all15 scalar targets plus four EQ rebuilds only; not full effective config, smoothing, activation, graph or live state'}


def _eq_design_metadata(nonzero_enabled):
    return {'nonzero_enabled': nonzero_enabled,
            'designer_math_reference_not_bitexact': nonzero_enabled,
            'coefficient_order': ['b0', 'b1', 'b2', '-a1', '-a2'],
            'nonzero_domain': 'finite0<f<fs/2, width>0, gain[-100,20]dB, rounded type0/1/2; real shelf radicand and stable float32-rounded coefficients required'}
