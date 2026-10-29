#!/usr/bin/env python3
"""Fresh native C upmix -> explicit conditional electronics, offline only.

Not an acoustic cabin simulation, live preset, stock recording or sound-quality
acceptance test. No resampling, normalization, path-specific time alignment or
hidden auxiliary music donors. Every output directory must be new.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys

import numpy as np
import soundfile as sf

from amp_dsp_chain import AmpChain, ROOT, SR
from amp_end_to_end_metrics import measure, paired_delta
from amp_test_scenarios import make_test_scenario, scenario_manifest

STATIC_SCENARIOS = {
    f'full_static_{rounding}_{suffix}': (rounding, probe, adaptive)
    for rounding in ('nearest','floor')
    for suffix,probe,adaptive in (('zero_aux',False,0),('aux_fixed',True,0),
                                  ('zero_aux_adaptive',False,1),('aux_probe',True,1))}
SCENARIOS = ('baseline_v7', 'full_zero_aux', 'full_aux_probe',
             'full_retained_zero_aux', 'full_retained_aux_probe', *STATIC_SCENARIOS)
LATENCY = 512
DRY = 'dry_bank1'
DIAGNOSTICS = ('minimum-guard-gain', 'attenuated-frames', 'total-frames',
               'limiter-attacks', 'longest-observed-run', 'reduction-db-sum',
               'over3-frames', 'over6-frames', 'over10-frames')


def fingerprint(path):
    path = Path(path).resolve()
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return {'path': str(path), 'sha256': digest.hexdigest(), 'bytes': path.stat().st_size}


def verify(record):
    if fingerprint(record['path'])['sha256'] != record['sha256']:
        raise ValueError('file changed: ' + record['path'])


def variant(value):
    """Parse bounded renderer parameters; names also form safe artifact paths."""
    fields = value.split(':')
    if len(fields) != 6:
        raise ValueError('variant needs name:float_or_s16:surround:front_bass:lfe_gain:retention')
    name, mode = fields[:2]
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]*', name) or name == DRY:
        raise ValueError('invalid or reserved variant name')
    if mode not in ('float', 's16'):
        raise ValueError('variant render mode must be float or s16')
    values = [float(v) for v in fields[2:]]
    for val, (low, high) in zip(values, ((0, .5), (-12, 0), (-24, 12), (0, 1))):
        if not math.isfinite(val) or not low <= val <= high:
            raise ValueError('nonfinite or out-of-range renderer parameter')
    return dict(name=name, render_mode=mode, surround=values[0], front_bass=values[1],
                lfe_gain=values[2], retention=values[3], lfe='assist')


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('renderer', 'model', 'guard', 'out'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--input', type=Path, action='append', required=True)
    p.add_argument('--variant', action='append', required=True)
    p.add_argument('--scenario', choices=SCENARIOS, action='append', required=True)
    p.add_argument('--seconds', type=float, required=True, help='source prefix duration; no implicit truncation')
    p.add_argument('--tail-seconds', type=float, default=.25, help='explicit zeros before C render; at least 512 frames')
    p.add_argument('--input-eq-bank', type=int, default=7, choices=range(16), help='hypothesis, NOT MOST bank selector')
    p.add_argument('--build-provenance', type=Path)
    p.add_argument('--save-output-wavs', action='store_true', help='retain16ch FLOAT32 WAVs; default hash and measure arrays only')
    return p


def validate(a):
    if a.out.exists():
        raise ValueError('output directory already exists; refusing overwrite')
    if not math.isfinite(a.seconds) or a.seconds <= 0:
        raise ValueError('seconds must be finite and positive')
    if not math.isfinite(a.tail_seconds) or a.tail_seconds < 0:
        raise ValueError('tail-seconds must be finite and nonnegative')
    frames, tail = round(a.seconds * SR), round(a.tail_seconds * SR)
    if frames < 2 or tail < LATENCY:
        raise ValueError('at least two programme frames and 512 tail frames required')
    variants = [variant(v) for v in a.variant]
    if not variants or len({v['name'] for v in variants}) != len(variants):
        raise ValueError('explicit, uniquely named variants required')
    if not a.scenario or len(set(a.scenario)) != len(a.scenario) or any(s not in SCENARIOS for s in a.scenario):
        raise ValueError('explicit, unique known scenarios required')
    if type(a.input_eq_bank) is not int or a.input_eq_bank not in range(16):
        raise ValueError('input EQ bank hypothesis must be integer0..15')
    for path in (a.renderer, a.model, a.guard):
        if not path.is_file():
            raise ValueError('missing file: ' + str(path))
    if not os.access(a.renderer, os.X_OK):
        raise ValueError('renderer must be executable')
    paths = [p.resolve() for p in a.input]
    if not paths or len(set(paths)) != len(paths) or len({p.stem for p in paths}) != len(paths):
        raise ValueError('inputs must have unique paths and track names')
    for path in paths:
        info = sf.info(path)
        if info.samplerate != SR or info.channels != 2 or info.subtype != 'PCM_16' or info.format not in ('WAV', 'WAVEX'):
            raise ValueError(f'{path}: input must be stereo48k PCM16 WAV, no implicit conversion')
        if info.frames < frames:
            raise ValueError(f'{path}: source shorter than requested prefix')
    return variants, frames, tail


def source_provenance():
    paths = [ROOT / 'tools/stereo-upmix-render/stereo_upmix_render.c']
    paths += [ROOT / 'hook/audio' / (name + suffix)
              for name in ('stereo_upmix', 'hssr_dsp', 'hssr_neon', 'soundcheck') for suffix in ('.c', '.h')]
    paths += sorted(Path(__file__).parent.glob('amp_*.py'))
    paths += [Path(__file__).with_name('digital_stages.py'), Path(__file__)]
    return [fingerprint(p) for p in paths]


def build_provenance(path, renderer, model, guard):
    if path is None:
        return {'provided': False, 'binary_source_build_identity_attested': False}
    record = json.loads(path.read_text())
    required = [record['renderer'], record['models']['main_model'], record['models']['guard_model']]
    if [v['sha256'] for v in required] != [fingerprint(p)['sha256'] for p in (renderer, model, guard)]:
        raise ValueError('build provenance binary/model hashes differ from requested files')
    sources = record['sources']
    if not isinstance(sources, list) or not sources:
        raise ValueError('build provenance source list required')
    for item in required + sources:
        verify(item)
    return {'provided': True, 'file': fingerprint(path), 'record': record,
            'listed_file_hashes_verified': True, 'binary_source_build_identity_attested': False,
            'note': 'Local supplied build record, hash-checked; not independent compiler/build attestation.'}


def checkpoint(directory, report):
    temporary = directory / 'report.json.partial'
    temporary.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    temporary.replace(directory / 'report.json')


def array_fingerprint(value, dtype):
    data = np.ascontiguousarray(value, dtype=dtype)
    return {'sha256': hashlib.sha256(data.tobytes()).hexdigest(), 'shape': list(data.shape),
            'dtype': data.dtype.str, 'order': 'C frames then channels', 'bytes': data.nbytes}


def read_pcm6(path, frames):
    info = sf.info(path)
    if info.samplerate != SR or info.channels != 6 or info.subtype != 'PCM_16' or info.frames != frames:
        raise ValueError('renderer output must be exactly requested frames x6 PCM16 at48k')
    data, _ = sf.read(path, dtype='float64', always_2d=True)
    if not np.isfinite(data).all():
        raise ValueError('nonfinite renderer output')
    return data


def c_render(a, parameters, source, target, frames):
    cmd = [str(a.renderer.resolve()), '--mode', 'hssr-universal', '--model', str(a.model.resolve()),
           '--guard-model', str(a.guard.resolve()), '--front-retention', str(parameters['retention']),
           '--surround', str(parameters['surround']), '--front-bass', str(parameters['front_bass']),
           '--lfe', parameters['lfe'], '--lfe-gain', str(parameters['lfe_gain'])]
    if parameters['render_mode'] == 'float':
        cmd.append('--float-render')
    cmd += [str(source), str(target)]
    run = subprocess.run(cmd, capture_output=True, text=True, check=False)
    log = target.with_suffix('.log')
    log.write_text('COMMAND ' + json.dumps(cmd) + '\nSTDOUT\n' + run.stdout + '\nSTDERR\n' + run.stderr)
    if run.returncode:
        raise RuntimeError(f'renderer failed ({run.returncode}); see {log}')
    pcm = read_pcm6(target, frames)
    fields = dict(re.findall(r'([a-z0-9-]+)=([0-9.eE+-]+)', run.stderr))
    diagnostics = {k: float(fields[k]) for k in DIAGNOSTICS if k in fields}
    if not all(math.isfinite(v) for v in diagnostics.values()):
        raise ValueError('nonfinite renderer diagnostics')
    return pcm, {'parameters': parameters, 'command': cmd, 'log': fingerprint(log),
                 'file': fingerprint(target), 'guard': diagnostics,
                 'guard_telemetry_complete': set(diagnostics) == set(DIAGNOSTICS),
                 'pcm_contract': '6-channel PCM16; float means internal float synthesis/guard followed by S16 quantization'}


def auxiliary(scenario, programme_frames, tail_frames):
    """Independent post-boundary probes; never derive auxiliary PCM from music."""
    if scenario not in SCENARIOS:raise ValueError('known explicit scenario required')
    if scenario == 'baseline_v7':
        return {}, {'kind': 'omitted by explicit v7 identity/B-zero hypotheses'}
    count = programme_frames + tail_frames
    c4 = np.zeros(count)
    second = np.zeros((count, 3))
    probe = STATIC_SCENARIOS[scenario][1] if scenario in STATIC_SCENARIOS else scenario.endswith('_aux_probe')
    if probe:
        time = np.arange(programme_frames) / SR
        c4[:programme_frames] = .01 * np.sin(2 * np.pi * 63 * time)
        second[:programme_frames] = .003 * np.sin(2 * np.pi * time[:, None] * np.array([71., 113., 173.]))
    return {'complex_auxiliary': c4, 'second_b_auxiliary': second}, {
        'kind': 'synthetic_independent_probe' if probe else 'explicit_zero',
        'music_donors': False, 'active_window_frames': [0, programme_frames],
        'tail': 'explicit zeros', 'phase_radians': 0,
        'complex_graph11_after_C4D11': {'frequency_hz': 63, 'amplitude': .01 if probe else 0},
        'second_B_graph8_9_10': {'frequencies_hz': [71, 113, 173], 'amplitude_each': .003 if probe else 0},
        'physical_meaning_and_live_levels_proven': False}


def make_chain(scenario, bank):
    if scenario not in SCENARIOS:raise ValueError('known explicit scenario required')
    if scenario == 'baseline_v7':
        return AmpChain(), {'kind': 'baseline_v7', 'actual_vehicle_state_proven': False,
                            'note': 'existing v7 identity complex block and both B-zero hypotheses'}
    static = STATIC_SCENARIOS.get(scenario)
    parameters = make_test_scenario(input_eq_bank=bank, auxiliary_gain_mode=1,
                                   first_b_pointer_variant='default',
                                   first_b_spectral=('retained_calibration' if static or scenario.startswith('full_retained_')
                                                     else 'neutral_diagnostic'),
                                   first_b_numerics='firmware_library_'+static[0] if static else 'host_math',
                                   adaptive_enable=static[2] if static else int(scenario.endswith('_aux_probe')))
    return AmpChain(**parameters), scenario_manifest(parameters)


def summarize(tracks, scenarios, variants):
    result = {}
    for scenario in scenarios:
        result[scenario] = {}
        for name in [v['name'] for v in variants]:
            rows = [track['scenarios'][scenario]['rows'][name] for track in tracks.values()]
            tilts = [r['delta_vs_dry_bank1']['front woofers'][1] - r['delta_vs_dry_bank1']['front mid/treble'][4]
                     for r in rows]
            result[scenario][name] = {'tracks': len(rows),
                'bass60_120_minus_mid500_2000_delta_db_mean_min_max': [float(np.mean(tilts)), min(tilts), max(tilts)],
                'maximum_whole_clip_sample_peak_dbfs': max(r['metrics']['outputs']['whole_clip']['peak_dbfs'] for r in rows),
                'ranking_or_production_recommendation': False}
    return result


def run(a):
    variants, frames, tail = validate(a)
    provenance = {'renderer': fingerprint(a.renderer), 'main_model': fingerprint(a.model),
                  'guard_model': fingerprint(a.guard), 'source_code': source_provenance(),
                  'build': build_provenance(a.build_provenance, a.renderer, a.model, a.guard)}
    input_records = {p.stem: fingerprint(p) for p in a.input}
    report = {'schema': 1, 'complete': False, 'scope': __doc__, 'provenance': provenance,
        'axes': {'tracks': list(input_records), 'variants': [v['name'] for v in variants], 'scenarios': a.scenario},
        'parameters': {'variants': variants, 'source_prefix_seconds_requested': a.seconds,
                       'tail_seconds_requested': a.tail_seconds, 'programme_frames': frames, 'tail_frames': tail,
                       'frame_rounding': 'nearest integer Python round', 'sample_rate': SR,
                       'input_eq_bank_hypothesis': a.input_eq_bank, 'front_upmix_latency_frames': LATENCY,
                       'retain_output_wavs': a.save_output_wavs},
        'analysis_contract': {'windows': 'simultaneous [0,N) programme and [N,N+T) finite tail, including startup latency',
            'alignment': 'NO per-path delay realignment', 'dry_reference': 'L/R only, 512-frame delay, same BANK1/scenario; NOT stock',
            'tail': 'source padded before fresh C render; finite padding does NOT prove settled DSP',
            'normalization': 'none; unity programme gain; no SoundCheck metadata supplied',
            'reset': 'fresh C process per track/variant; fresh AmpChain per track/scenario/row',
            'output': '16 config-order electrical channels, float64 model then FLOAT32 measurement; overs preserved; not19-speaker acoustics',
            'host_boundary': 'native host synchronous inference/control_provider NULL; not QNX ARMv7 asynchronous worker/QSA or vehicle bit-exact validation',
            'delay_boundary': '512 is frontal dry-control convention; scene rear can differ (e.g.576); no universal upmix path-delay claim',
            'measured_full_cabin_or_vehicle_acceptance': False},
        'expected_rows': len(input_records) * len(a.scenario) * (len(variants) + 1),
        'completed_rows': 0, 'tracks': {}}
    a.out.mkdir(parents=True, exist_ok=False)
    checkpoint(a.out, report)
    try:
        for index, (name, source_record) in enumerate(input_records.items()):
            verify(source_record)
            directory = a.out / f'{index:02d}_{re.sub(r"[^A-Za-z0-9_-]", "_", name)}'
            directory.mkdir()
            source, rate = sf.read(source_record['path'], frames=frames, dtype='int16', always_2d=True)
            if source.shape != (frames, 2) or rate != SR:
                raise ValueError('source changed after validation')
            prefix = directory / 'source_prefix.wav'
            padded = directory / 'source_padded.wav'
            sf.write(prefix, source, SR, subtype='PCM_16')
            sf.write(padded, np.pad(source, ((0, tail), (0, 0))), SR, subtype='PCM_16')
            dry = np.zeros((frames + tail, 6), dtype=np.int16)
            dry[LATENCY:LATENCY + frames, :2] = source
            dry_path = directory / (DRY + '_6ch.wav')
            sf.write(dry_path, dry, SR, subtype='PCM_16')
            track = {'source': source_record, 'source_prefix': fingerprint(prefix), 'source_padded': fingerprint(padded),
                     'pcm6': {DRY: {'file': fingerprint(dry_path), 'reference_not_stock': True,
                                   'upmix_latency_frames': LATENCY}}, 'scenarios': {}}
            report['tracks'][name] = track
            for parameters in variants:
                _, record = c_render(a, parameters, padded, directory / (parameters['name'] + '_6ch.wav'), frames + tail)
                track['pcm6'][parameters['name']] = record
                checkpoint(a.out, report)
            for scenario in a.scenario:
                stage_inputs, aux_metadata = auxiliary(scenario, frames, tail)
                # DOUBLE preserves the actual float64 generator values supplied to the model.
                aux_metadata['files'] = {}
                for key, value in stage_inputs.items():
                    target = directory / (scenario + '_' + key + '.wav')
                    sf.write(target, value, SR, subtype='DOUBLE')
                    aux_metadata['files'][key] = fingerprint(target)
                group = {'auxiliary': aux_metadata, 'rows': {}}
                track['scenarios'][scenario] = group
                for candidate, pcm_record in track['pcm6'].items():
                    pcm = read_pcm6(pcm_record['file']['path'], frames + tail)
                    chain, scenario_record = make_chain(scenario, a.input_eq_bank)
                    output = chain.process(pcm, capture_stages=False, **stage_inputs)['out']
                    if output.shape != (frames + tail, 16) or not np.isfinite(output).all():
                        raise ValueError('electronics output must be finite frames x16')
                    hashes = {'model_float64': array_fingerprint(output, '<f8'),
                              'measurement_float32': array_fingerprint(output, '<f4')}
                    saved = np.asarray(output, dtype=np.float32).astype(np.float64)
                    file_record = None
                    if a.save_output_wavs:
                        target = directory / (scenario + '_' + candidate + '_16ch.wav')
                        sf.write(target, output, SR, subtype='FLOAT')
                        saved, saved_rate = sf.read(target, dtype='float64', always_2d=True)
                        if saved_rate != SR or array_fingerprint(saved, '<f4') != hashes['measurement_float32']:
                            raise ValueError('saved FLOAT32 output differs from rounded model output')
                        file_record = fingerprint(target)
                    if not np.isfinite(saved).all():
                        raise ValueError('nonfinite FLOAT32 measurement output')
                    metrics = measure(pcm, saved, frames, tail)
                    row = {'file': file_record, 'output_arrays': hashes,
                           'output_wav_retained': a.save_output_wavs, 'model': chain.metadata(),
                           'scenario': scenario_record, 'metrics': metrics,
                           'metrics_audio': 'FLOAT32-rounded model output without normalization; reread from WAV when retained',
                           'delta_vs_dry_bank1': None}
                    group['rows'][candidate] = row
                    row['delta_vs_dry_bank1'] = paired_delta(metrics, group['rows'][DRY]['metrics'])
                    report['completed_rows'] += 1
                    checkpoint(a.out, report)
                    print(name, scenario, candidate, 'done', flush=True)
            verify(source_record)
            report['summary'] = summarize(report['tracks'], a.scenario, variants)
            checkpoint(a.out, report)
        for item in [provenance['renderer'], provenance['main_model'], provenance['guard_model']] + provenance['source_code']:
            verify(item)
        if provenance['build']['provided']:
            verify(provenance['build']['file'])
        if report['completed_rows'] != report['expected_rows']:
            raise ValueError('incomplete row grid')
        report['summary'] = summarize(report['tracks'], a.scenario, variants)
        report['complete'] = True
        checkpoint(a.out, report)
    except Exception as exc:
        report['complete'] = False
        report['error'] = {'type': type(exc).__name__, 'message': str(exc)}
        checkpoint(a.out, report)
        raise
    return report


def main():
    try:
        run(parser().parse_args())
    except (ValueError, OSError, RuntimeError, KeyError) as exc:
        print('error:', exc, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
