#!/usr/bin/env python3
"""Fresh local C renders: corrected guards and independent front retention.

Unity programme gain, current model/guard blobs explicitly supplied, original
seven-track PCM hashes checked. No vehicle I/O or choice of deployed settings.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from amp_dsp_chain import AmpChain, ROOT, SR
from amp_music_report import BANDS, GROUPS, band_powers, fingerprint, measure, read_audio


VARIANTS = {'s16_r0': (False, 0.), 'float_r0': (True, 0.),
            'float_r50': (True, .5), 'float_r75': (True, .75), 'float_r100': (True, 1.)}


def peak_metrics(x, matrix):
    peaks = np.zeros(8)
    for start in range(0, len(x), 65536):
        peaks = np.maximum(peaks, np.max(abs(x[start:start+65536] @ matrix.T), axis=0))
    return {'pcm_peak': np.max(abs(x), axis=0).tolist(), 'matrix_bus_peak': peaks.tolist(),
            'pcm_rail_samples': int(np.count_nonzero((x >= 32767/32768) | (x <= -1)))}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('report', 'renderer', 'model', 'guard', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    base = json.loads(a.report.read_text()); chain = AmpChain()
    if base['model'] != chain.metadata():
        raise ValueError('matching default v7 baseline required')
    if a.out.exists():
        raise ValueError('use a new output directory; preserve previous reports')
    a.out.mkdir(parents=True)
    pcm_dir = Path(tempfile.mkdtemp(prefix='upmix-retention-20260920-'))
    report = {'scope': __doc__, 'complete': False, 'model': chain.metadata(),
              'baseline_report': fingerprint(a.report), 'pcm_directory': str(pcm_dir),
              'renderer': fingerprint(a.renderer), 'hssr_model': fingerprint(a.model),
              'hssr_guard': fingerprint(a.guard), 'variants': VARIANTS, 'bands': BANDS,
              'analysis': base['analysis'], 'tracks': {},
              'fixed_settings': {'surround': .4, 'front_bass_db': 0, 'lfe_gain_db': 0,
                                 'soundcheck_programme_gain': 1., 'float_guard_ceiling_dbfs': -1},
              'sources': [fingerprint(ROOT / path) for path in (
                  'hook/audio/stereo_upmix.c', 'hook/audio/stereo_upmix.h',
                  'hook/audio/hssr_dsp.c', 'hook/audio/hssr_neon.c',
                  'hook/audio/soundcheck.c', 'tools/stereo-upmix-render/stereo_upmix_render.c')]}
    start, end = [round(t*SR) for t in base['analysis']['window_source_seconds']]
    latency = base['analysis']['upmix_latency_frames']
    for key, source in base['files'].items():
        if not key.endswith(':dry'):
            continue
        if fingerprint(source['path'])['sha256'] != source['sha256']:
            raise ValueError('source PCM changed')
        song = key.rsplit(':', 1)[0]
        stereo = read_audio(source['path'], 2)
        ref = band_powers(stereo[start:end, 0])
        rows = {}
        for name, (floating, retention) in VARIANTS.items():
            target = pcm_dir / (song+'-'+name+'.wav')
            cmd = [str(a.renderer.resolve()), '--mode', 'hssr-universal',
                   '--model', str(a.model.resolve()), '--guard-model', str(a.guard.resolve()),
                   '--lfe', 'assist', '--lfe-gain', '0', '--surround', '.4',
                   '--front-bass', '0', '--front-retention', str(retention)]
            if floating:
                cmd += ['--float-render']
            cmd += [source['path'], str(target)]
            run = subprocess.run(cmd, check=True, capture_output=True, text=True)
            (a.out/(song+'-'+name+'.log')).write_text(run.stderr)
            x = read_audio(target, 6)
            metrics = peak_metrics(x, chain.M)
            # Post-S16 quantization can add a few LSB to a weighted sum.
            ceiling = 10**(-1/20) + 8/32768 if floating else 32275.01/32768
            if max(metrics['matrix_bus_peak']) > ceiling or metrics['pcm_rail_samples']:
                raise ValueError('corrected matrix/PCM bound failed: '+song+'/'+name)
            measured = measure(chain, x, ref, start, end, latency)
            measured.update(file=fingerprint(target), command=cmd, input_metrics=metrics)
            measured['delta_vs_dry_bank1'] = {g: (np.array(r['power_dbfs'])-base['results'][key]['groups'][g]['power_dbfs']).tolist()
                                             for g, r in measured['groups'].items()}
            measured['delta_vs_cached_s16'] = {g: (np.array(r['power_dbfs'])-base['results'][song+':gain4096_fb0']['groups'][g]['power_dbfs']).tolist()
                                             for g, r in measured['groups'].items()}
            if floating:
                fields = dict(re.findall(r'([a-z-]+)=([0-9.eE+-]+)', run.stderr))
                measured['float_diagnostics'] = {k: float(fields[k]) for k in
                    ('pre-gain-peak', 'post-gain-peak', 'minimum-guard-gain', 'attenuated-frames', 'total-frames', 'limiter-attacks')}
            rows[name] = measured
            report['tracks'][song] = {'source': source, 'variants': rows}
            (a.out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
            print(song, name, 'matrix peak', round(max(metrics['matrix_bus_peak']), 5), 'done', flush=True)
    report['summary'] = {}
    for name in VARIANTS:
        rows = [t['variants'][name] for t in report['tracks'].values()]
        report['summary'][name] = {
            'mean_delta_vs_dry_bank1': {g: np.mean([r['delta_vs_dry_bank1'][g] for r in rows], axis=0).tolist() for g in GROUPS},
            'max_output_peak_dbfs': max(max(r['peak_out_dbfs']) for r in rows),
            'max_matrix_peak': max(max(r['input_metrics']['matrix_bus_peak']) for r in rows),
            'pcm_rail_samples': sum(r['input_metrics']['pcm_rail_samples'] for r in rows)}
    report['complete'] = True
    (a.out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(a.out/'report.json', flush=True)


if __name__ == '__main__':
    main()
