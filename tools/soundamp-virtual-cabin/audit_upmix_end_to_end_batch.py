#!/usr/bin/env python3
"""Parallel single-track subprocesses for the offline end-to-end audit.

Same CLI as audit_upmix_end_to_end.py plus --jobs. No DSP changes. Each shard
keeps its original report; the aggregate is complete only after strict grid,
input, parameter and provenance checks. Existing directories are never reused.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
import copy
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import audit_upmix_end_to_end as base

THREAD_ENV = {key: '1' for key in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS',
              'MKL_NUM_THREADS', 'VECLIB_MAXIMUM_THREADS', 'NUMEXPR_NUM_THREADS')}
BASE_RUNNER = Path(__file__).with_name('audit_upmix_end_to_end.py')


def parser():
    result = base.parser()
    result.description = __doc__
    result.add_argument('--jobs', type=int, required=True, help='parallel single-track C/DSP subprocesses')
    return result


def command(a, source, out):
    args = [sys.executable, str(BASE_RUNNER.resolve())]
    for key in ('renderer', 'model', 'guard'):
        args += ['--' + key, str(getattr(a, key).resolve())]
    args += ['--out', str(out.resolve()), '--input', str(source.resolve()),
             '--seconds', str(a.seconds), '--tail-seconds', str(a.tail_seconds),
             '--input-eq-bank', str(a.input_eq_bank)]
    for value in a.variant:
        args += ['--variant', value]
    for value in a.scenario:
        args += ['--scenario', value]
    if a.build_provenance is not None:
        args += ['--build-provenance', str(a.build_provenance.resolve())]
    if a.save_output_wavs:
        args += ['--save-output-wavs']
    return args


def launch(a, index, source):
    name = f'{index:02d}_{re.sub(r"[^A-Za-z0-9_-]", "_", source.stem)}'
    directory = a.out / name
    cmd = command(a, source, directory)
    result = subprocess.run(cmd, capture_output=True, text=True, check=False,
                            env={**os.environ, **THREAD_ENV})
    log = a.out / (name + '.log')
    log.write_text('COMMAND ' + json.dumps(cmd) + '\nSTDOUT\n' + result.stdout + '\nSTDERR\n' + result.stderr)
    if result.returncode != 0:
        raise RuntimeError(f'{source.stem}: shard exit {result.returncode}; see {log}')
    return directory / 'report.json', {'command': cmd, 'log': base.fingerprint(log)}


def expected_parameters(a, variants, frames, tail):
    return {'variants': variants, 'source_prefix_seconds_requested': a.seconds,
            'tail_seconds_requested': a.tail_seconds, 'programme_frames': frames, 'tail_frames': tail,
            'frame_rounding': 'nearest integer Python round', 'sample_rate': base.SR,
            'input_eq_bank_hypothesis': a.input_eq_bank, 'front_upmix_latency_frames': base.LATENCY,
            'retain_output_wavs': a.save_output_wavs}


def check_shard(report, name, source_record, parameters, scenarios, provenance):
    """Reject incomplete/misaligned shards, even if their complete flag is set."""
    variants = [v['name'] for v in parameters['variants']]
    expected = len(scenarios) * (len(variants) + 1)
    if report.get('schema') != 1 or report.get('complete') is not True:
        raise ValueError(f'{name}: shard not complete schema1')
    if (type(report.get('expected_rows')) is not int or type(report.get('completed_rows')) is not int
            or report['expected_rows'] != expected or report['completed_rows'] != expected):
        raise ValueError(f'{name}: shard row count differs')
    if report.get('axes') != {'tracks': [name], 'variants': variants, 'scenarios': scenarios}:
        raise ValueError(f'{name}: shard axes differ')
    if report.get('parameters') != parameters or report.get('provenance') != provenance:
        raise ValueError(f'{name}: shard parameters/provenance differ')
    if set(report.get('tracks', {})) != {name}:
        raise ValueError(f'{name}: extra/missing track')
    track = report['tracks'][name]
    if track.get('source') != source_record:
        raise ValueError(f'{name}: source identity differs')
    if set(track.get('pcm6', {})) != {base.DRY, *variants}:
        raise ValueError(f'{name}: PCM variant axis differs')
    if set(track.get('scenarios', {})) != set(scenarios):
        raise ValueError(f'{name}: extra/missing scenario')
    for scenario in scenarios:
        rows = track['scenarios'][scenario].get('rows', {})
        if set(rows) != {base.DRY, *variants}:
            raise ValueError(f'{name}/{scenario}: extra/missing row')
        for row in rows.values():
            if row.get('output_wav_retained') is not parameters['retain_output_wavs']:
                raise ValueError(f'{name}: output retention differs')
            if not isinstance(row.get('metrics'), dict) or not isinstance(row.get('model'), dict):
                raise ValueError(f'{name}: missing metrics/model payload')
            if not isinstance(row.get('delta_vs_dry_bank1'), dict):
                raise ValueError(f'{name}: missing paired delta')
    if not isinstance(report.get('analysis_contract'), dict):
        raise ValueError(f'{name}: missing analysis contract')
    return track


def run(a):
    if type(a.jobs) is not int or a.jobs < 1:
        raise ValueError('--jobs must be a positive integer')
    variants, frames, tail = base.validate(a)
    parameters = expected_parameters(a, variants, frames, tail)
    provenance = {'renderer': base.fingerprint(a.renderer), 'main_model': base.fingerprint(a.model),
                  'guard_model': base.fingerprint(a.guard), 'source_code': base.source_provenance(),
                  'build': base.build_provenance(a.build_provenance, a.renderer, a.model, a.guard)}
    sources = {p.stem: base.fingerprint(p) for p in a.input}
    report = {'schema': 1, 'complete': False, 'scope': base.__doc__, 'provenance': provenance,
              'parameters': parameters,
              'axes': {'tracks': list(sources), 'variants': [v['name'] for v in variants], 'scenarios': a.scenario},
              'expected_rows': len(sources) * len(a.scenario) * (len(variants) + 1),
              'completed_rows': 0, 'tracks': {},
              'batch': {'wrapper': base.fingerprint(__file__), 'requested_jobs': a.jobs,
                        'effective_jobs': min(a.jobs, len(sources)), 'child_thread_environment': THREAD_ENV,
                        'shards': {}, 'errors': {}, 'independent_build_or_vehicle_certification': False}}
    a.out.mkdir(parents=True, exist_ok=False)
    base.checkpoint(a.out, report)
    with ThreadPoolExecutor(max_workers=min(a.jobs, len(sources))) as pool:
        futures = {pool.submit(launch, a, index, source): source.stem for index, source in enumerate(a.input)}
        for future in as_completed(futures):
            name = futures[future]
            try:
                path, record = future.result()
                before = base.fingerprint(path)
                shard = json.loads(path.read_text())
                base.verify(before)
                track = check_shard(shard, name, sources[name], parameters, a.scenario, provenance)
                if 'analysis_contract' in report and report['analysis_contract'] != shard['analysis_contract']:
                    raise ValueError(f'{name}: analysis contract differs between shards')
                report['analysis_contract'] = copy.deepcopy(shard['analysis_contract'])
                report['tracks'][name] = track
                report['completed_rows'] += shard['completed_rows']
                report['batch']['shards'][name] = {**record, 'report': before}
                report['summary'] = base.summarize(report['tracks'], a.scenario, variants)
                print(name, 'shard verified', flush=True)
            except Exception as exc:
                report['batch']['errors'][name] = {'type': type(exc).__name__, 'message': str(exc)}
            base.checkpoint(a.out, report)
    try:
        if report['batch']['errors']:
            raise RuntimeError('one or more shards failed; see aggregate batch.errors and shard logs')
        if report['completed_rows'] != report['expected_rows'] or set(report['tracks']) != set(sources):
            raise ValueError('incomplete aggregate grid')
        for item in list(sources.values()) + [provenance['renderer'], provenance['main_model'], provenance['guard_model']] + provenance['source_code']:
            base.verify(item)
        base.verify(report['batch']['wrapper'])
        if provenance['build']['provided']:
            base.verify(provenance['build']['file'])
        for item in report['batch']['shards'].values():
            base.verify(item['report'])
            base.verify(item['log'])
        report['tracks'] = {name: report['tracks'][name] for name in sources}
        report['summary'] = base.summarize(report['tracks'], a.scenario, variants)
        report['complete'] = True
        base.checkpoint(a.out, report)
    except Exception as exc:
        report['complete'] = False
        report['error'] = {'type': type(exc).__name__, 'message': str(exc)}
        base.checkpoint(a.out, report)
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
