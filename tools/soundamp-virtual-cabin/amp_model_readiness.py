#!/usr/bin/env python3
"""Evidence/readiness audit, not another DSP model or a certification authority.

This tool distinguishes numerical coverage, declared measurements and proof of
the actual vehicle state. A JSON flag alone never establishes hardware fidelity.
It performs no vehicle I/O and does not change any model/preset.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
B_CASES = ('B0', 'B1_plus', 'B1_minus', 'B1_delay96',
           'B2_plus', 'B2_minus', 'B2_delay96')
MOST_CHANNELS = ['L', 'R', 'C', 'LFE', 'Ls', 'Rs']
CONFIG_CHANNELS = [f'cfg{i:02d}' for i in range(16)]
BANDS = [[20, 60], [60, 120], [120, 250], [250, 500], [500, 2000], [2000, 8000], [8000, 20000]]
GROUPS = {'front mid/treble', 'rear doors', 'centre mid', 'centre tweeter', 'front woofers', 'subwoofer'}


def finite_numbers(value):
    if isinstance(value, dict):
        return all(finite_numbers(v) for v in value.values())
    if isinstance(value, list):
        return all(finite_numbers(v) for v in value)
    return math.isfinite(value) if isinstance(value, (int, float)) else True


def numeric(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value, length):
    return isinstance(value, list) and len(value) == length and all(numeric(v) for v in value)


def valid_stress_cell(cell, frames):
    if not isinstance(cell, dict) or not vector(cell.get('peak_out_dbfs'), 16):
        return False
    bands = cell.get('bands_re_dry_L')
    if not isinstance(bands, list) or len(bands) != 16 or not all(vector(v, 7) for v in bands):
        return False
    groups = cell.get('groups')
    if not isinstance(groups, dict) or set(groups) != GROUPS:
        return False
    for group in groups.values():
        if (not isinstance(group, dict) or not vector(group.get('power_dbfs'), 7)
                or not vector(group.get('bands_re_dry_L'), 7) or not numeric(group.get('peak_out_dbfs'))):
            return False
    whole = cell.get('whole_clip')
    return (isinstance(whole, dict) and numeric(whole.get('peak_dbfs'))
            and type(whole.get('frames')) is int and whole['frames'] == frames
            and all(type(whole.get(k)) is int and 0 <= whole[k] <= frames for k in
                    ('frames_above_0dbfs', 'frames_above_minus1dbfs'))
            and whole['frames_above_0dbfs'] <= whole['frames_above_minus1dbfs']
            and finite_numbers(cell))


def fingerprint(path):
    path = Path(path).resolve()
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def check_reference(ref, base=ROOT):
    """Resolve explicit references relative to their owning manifest or repo."""
    if not isinstance(ref, dict) or not isinstance(ref.get('path'), str):
        return {'valid': False, 'reason': 'missing path/hash reference'}
    path = Path(ref['path'])
    if not path.is_absolute():
        path = Path(base) / path
    try:
        actual = fingerprint(path)
    except OSError as exc:
        return {'valid': False, 'reason': str(exc)}
    valid = actual['sha256'] == ref.get('sha256')
    return dict(actual, valid=valid,
                reason='hash matches (content truth not certified)' if valid else 'hash mismatch')


def model_gates(model):
    """Only an explicit scenario closes an omission; it does not prove live use."""
    aux = model.get('default_auxiliary_audio') or {}
    def scenario(key):
        return isinstance(model.get(key), dict) and bool(model[key])
    gates = {
        'model_identified': bool(model.get('version') and model.get('source_sha256')),
        'common_input_scenario_explicit': scenario('upstream_scenario'),
        'first_B_scenario_explicit': scenario('first_b_scenario'),
        'second_B_scenario_explicit': scenario('second_b_scenario'),
        'complex_scenario_explicit': scenario('complex_scenario'),
        'runtime_scenario_explicit': scenario('runtime_scenario'),
    }
    return {'gates': gates,
            'default_auxiliary_audio': aux,
            'omissions_or_hypotheses': [k for k, v in gates.items() if not v],
            'warnings': model.get('warnings', []),
            'live_state_verified': False,
            'physical_mapping_verified': False,
            'bit_exact_hardware_model_verified': False,
            'boundary': 'These presence checks are not kernel/schema validation. Explicit scenarios are caller hypotheses, '
                        'not vehicle readback or physical pinout evidence.'}


def coverage_audit(report, base=ROOT):
    """Check the full expected Cartesian grid, not just complete:true/counts.

    Scope is the existing tuning stress format: source tracks x declared EQ
    scenarios x dry/selected variants x seven synthetic B-boundary probes.
    No number of these probes proves actual B routing or calibrated bounds.
    """
    result = {'ready': False, 'declared_complete': report.get('complete') is True,
              'kind': 'synthetic_B_and_EQ_sensitivity_not_state_proof',
              'missing': [], 'unexpected': [], 'cells_present': 0}
    refs = {'grid': check_reference(report.get('grid'), base),
            'common_input_audit': check_reference(report.get('common_input_audit'), base)}
    scenario_files = report.get('scenario_files', [])
    for i, ref in enumerate(scenario_files):
        refs[f'scenario_{i}'] = check_reference(ref, base)
    if report.get('extra_report') is not None:
        refs['extra_report'] = check_reference(report['extra_report'], base)
    result['references'] = refs
    if not refs['grid']['valid']:
        result['missing'].append('valid source grid reference')
        return result
    grid = json.loads(Path(refs['grid']['path']).read_text())
    tracks = list(grid.get('tracks', {}))
    banks = list(report.get('scenarios', {}))
    names = report.get('names', [])
    if (not isinstance(names, list) or not all(isinstance(v, str) and v for v in names)
            or len(set(names)) != len(names) or 'dry_bank1' in names):
        result['missing'].append('unique nonempty candidate names excluding dry_bank1')
        return result
    variants = ['dry_bank1'] + names
    if len(scenario_files) != len(banks) or not scenario_files:
        result['missing'].append('one hash-pinned scenario file per EQ scenario')
    matched_scenarios = []
    for i, _ in enumerate(scenario_files):
        ref = refs[f'scenario_{i}']
        if not ref['valid']:
            continue
        scenario = json.loads(Path(ref['path']).read_text())
        audit_path = Path(scenario.get('audit_path', ''))
        scenario['audit_path'] = str((Path(ref['path']).parent / audit_path).resolve())
        matches = [k for k in banks if report['scenarios'][k] == scenario]
        if len(matches) != 1 or (refs['common_input_audit']['valid'] and
                scenario['audit_path'] != refs['common_input_audit']['path']):
            result['missing'].append(f'scenario_{i} exact scenario/common-audit binding')
        else:
            matched_scenarios.extend(matches)
    if sorted(matched_scenarios) != sorted(banks):
        result['missing'].append('unique exact scenario-file coverage of EQ axis')
    if set(report.get('scenario_models', {})) != set(banks):
        result['missing'].append('scenario model metadata for exact EQ axis')
    if report.get('bands') != BANDS or grid.get('bands') != BANDS:
        result['missing'].append('exact seven-band definition')
    frames = report.get('frames_rendered')
    if type(frames) is not int or frames <= 0:
        result['missing'].append('positive integer frames_rendered')
        frames = 0
    extra = None
    if refs.get('extra_report', {}).get('valid'):
        extra = json.loads(Path(refs['extra_report']['path']).read_text())
        if (extra.get('complete') is not True or extra.get('model') != grid.get('model')
                or extra.get('analysis') != grid.get('analysis')
                or set(extra.get('tracks', {})) != set(tracks)):
            result['missing'].append('complete compatible extra report')
    for track in tracks:
        grid_track = grid['tracks'][track]
        extra_track = (extra or {}).get('tracks', {}).get(track, {})
        if extra_track and extra_track.get('source') != grid_track.get('source'):
            result['missing'].append(f'{track}/extra source mismatch')
        for name in names:
            in_grid = name in grid_track.get('variants', {})
            in_extra = name in extra_track.get('variants', {})
            if not in_grid and not in_extra:
                result['missing'].append(f'{track}/{name}/hash-pinned source variant')
            if in_grid and in_extra:
                result['missing'].append(f'{track}/{name}/ambiguous duplicate source variant')
    result['expected_axes'] = {'tracks': tracks, 'eq_scenarios': banks,
                               'variants': variants, 'B_probes': list(B_CASES)}
    result['expected_cells'] = len(tracks) * len(banks) * len(variants) * len(B_CASES)
    if grid.get('complete') is not True:
        result['missing'].append('complete source grid')
    if not tracks or not banks or len(variants) < 2:
        result['missing'].append('nonempty declared axes')
    actual = report.get('tracks', {})
    for track in tracks:
        track_result = actual.get(track, {})
        result['unexpected'].extend('/'.join((track, key)) for key in set(track_result) - set(banks))
        for bank in banks:
            bank_result = track_result.get(bank, {})
            result['unexpected'].extend('/'.join((track, bank, key)) for key in set(bank_result) - set(variants))
            for variant in variants:
                cells = bank_result.get(variant, {})
                for probe in B_CASES:
                    cell = cells.get(probe)
                    if not valid_stress_cell(cell, frames):
                        result['missing'].append('/'.join((track, bank, variant, probe)))
                    else:
                        result['cells_present'] += 1
                result['unexpected'].extend('/'.join((track, bank, variant, key))
                                            for key in set(cells) - set(B_CASES))
    result['unexpected'].extend('track/' + key for key in set(actual) - set(tracks))
    result['ready'] = (result['declared_complete'] and not result['missing']
                       and not result['unexpected'] and all(r['valid'] for r in refs.values()))
    result['frames_rendered'] = report.get('frames_rendered')
    result['window_source_seconds'] = report.get('window_source_seconds')
    result['B_definition'] = report.get('B_definition')
    return result


def acoustic_audit(report, evidence=None, base=ROOT):
    """Inspect validated bridge report plus separately pinned evidence files.

    A valid report is an assertion of successful file validation, not an
    independently verified measurement. Supporting evidence is retained for
    human review; even matching hashes cannot establish its truth.
    """
    a = report.get('acoustics', {})
    domain = a.get('domain')
    expected_channels = MOST_CHANNELS if domain == 'most_inputs' else CONFIG_CHANNELS
    expected_stages = (['upmix_pcm', 'measured_brir'] if domain == 'most_inputs'
                       else ['upmix_pcm', 'amp_chain', 'measured_brir'])
    capture = a.get('capture') or {}
    provenance = a.get('provenance') or {}
    gates = {
        'domain_supported': domain in ('most_inputs', 'post_amp_config_outputs'),
        'channels_exact': a.get('channels') == expected_channels,
        'sample_rate_48000': a.get('sample_rate') == 48000,
        'response_hashes_checked': a.get('response_hashes_verified') is True,
        'response_audio_validated': a.get('responses_validated') is True,
        'no_duplicate_electronics': (a.get('stage_order') == expected_stages
                                    and (report.get('model') is None if domain == 'most_inputs'
                                         else isinstance(report.get('model'), dict))),
        'capture_fields_present': all(capture.get(k) for k in
                                     ('seat', 'state', 'microphone_calibration', 'level_reference')),
        'declared_measured_not_synthetic': provenance.get('kind') == 'measured',
    }
    manifest_ref = check_reference(a.get('manifest'), base)
    gates['manifest_hash_matches'] = manifest_ref['valid']
    # Re-open the actual manifest/IR files, not only historical report booleans.
    # This catches response replacement after the render and metadata spoofing.
    revalidation = {'valid': False, 'reason': 'manifest reference invalid'}
    if manifest_ref['valid']:
        try:
            from amp_cabin_render import validate_manifest
            current = validate_manifest(manifest_ref['path'])
            keys = ('domain', 'channels', 'sample_rate', 'provenance', 'capture', 'stage_order')
            matches = all(current.get(k) == a.get(k) for k in keys)
            revalidation = {'valid': matches,
                            'reason': 'IR hashes and metadata revalidated' if matches
                            else 'report metadata differs from current manifest'}
        except (OSError, ValueError, KeyError, TypeError) as exc:
            revalidation = {'valid': False, 'reason': str(exc)}
    gates['current_manifest_and_IRs_match_report'] = revalidation['valid']
    evidence = evidence or {}
    evidence_refs = {k: check_reference(evidence.get(k), base)
                     for k in ('capture_recording', 'state_record', 'microphone_calibration')}
    gates['provenance_files_hash_match'] = all(r['valid'] for r in evidence_refs.values())
    gates['capture_target_matches_evidence'] = (
        bool(capture.get('state')) and evidence.get('capture_state') == capture.get('state')
        and bool(capture.get('seat')) and evidence.get('seat') == capture.get('seat'))
    # post-amp response numbering must be measured/proven in config order, not
    # inherited from candidate physical speaker labels. This is review evidence.
    mapping = check_reference(evidence.get('config_order_mapping'), base)
    gates['config_order_mapping_evidence'] = domain == 'most_inputs' or mapping['valid']
    return {'gates': gates, 'domain': domain, 'provenance': provenance,
            'manifest_reference': manifest_ref, 'evidence_references': evidence_refs,
            'response_revalidation': revalidation,
            'config_order_mapping_reference': mapping,
            'evidence_package_complete': all(gates.values()),
            'hardware_measurement_independently_verified': False,
            'boundary': 'Hash-checked documentation enables review, not automatic proof of hardware truth. '
                        'Fixed-state IRs do not model arbitrary nonlinear/adaptive state changes or all seats.'}


def audit(model, coverage=None, acoustic=None, evidence=None, base=ROOT):
    electrical = model_gates(model)
    result = {'schema_version': 1, 'kind': 'upmix-model-readiness',
              'model': model, 'electrical': electrical,
              'coverage': coverage_audit(coverage, base) if coverage is not None else None,
              'acoustic': acoustic_audit(acoustic, evidence, base) if acoustic is not None else None}
    result['claims'] = {
        'conditional_electrical_comparison_available': electrical['gates']['model_identified'],
        'all_electrical_scenarios_explicit': all(electrical['gates'].values()),
        'declared_stress_grid_complete': bool(result['coverage'] and result['coverage']['ready']),
        'measured_cabin_evidence_package_ready_for_review': bool(
            result['acoustic'] and result['acoustic']['evidence_package_complete']),
        'actual_vehicle_state_verified': False,
        'full_cabin_fidelity_established': False,
        'subjective_optimality_established': False,
    }
    result['next_steps'] = ['Close or explicitly bound each missing electrical scenario.',
                            'Compare candidates across complete declared sensitivity grids.',
                            'Obtain independent state and acoustic evidence before claiming real-cabin fidelity.']
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, help='Existing report with model metadata; defaults to AmpChain.')
    parser.add_argument('--coverage', type=Path, help='Tuning stress report to audit against its source grid.')
    parser.add_argument('--acoustic-report', type=Path, help='amp_cabin_render report; not a raw BRIR manifest.')
    parser.add_argument('--evidence', type=Path, help='Hash-pinned measurement evidence; paths relative to this JSON.')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--require', choices=('stress-coverage', 'cabin-evidence', 'full-fidelity'))
    args = parser.parse_args()
    def read(path):
        return json.loads(path.read_text()) if path else None
    if args.report:
        report = read(args.report)
        model = report.get('model') or {}
    else:
        from amp_dsp_chain import AmpChain
        model = AmpChain().metadata()
    evidence = read(args.evidence)
    if evidence:
        # Evidence paths belong to evidence JSON; report refs remain repo-relative.
        for key in ('capture_recording', 'state_record', 'microphone_calibration', 'config_order_mapping'):
            if isinstance(evidence.get(key), dict) and evidence[key].get('path'):
                p = Path(evidence[key]['path'])
                if not p.is_absolute():
                    evidence[key]['path'] = str((args.evidence.parent / p).resolve())
    result = audit(model, read(args.coverage), read(args.acoustic_report), evidence)
    result['inputs'] = {key: fingerprint(path) for key, path in vars(args).items()
                        if key in ('report', 'coverage', 'acoustic_report', 'evidence') and path}
    text = json.dumps(result, indent=2, allow_nan=False) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    else:
        print(text, end='')
    requirements = {'stress-coverage': 'declared_stress_grid_complete',
                    'cabin-evidence': 'measured_cabin_evidence_package_ready_for_review',
                    'full-fidelity': 'full_cabin_fidelity_established'}
    return 2 if args.require and not result['claims'][requirements[args.require]] else 0


if __name__ == '__main__':
    raise SystemExit(main())
