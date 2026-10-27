#!/usr/bin/env python3
"""Offline attribution of cached front/centre loss, and diagnostic front blends.

Default v7 linear scenario only. No new upmix, vehicle I/O, or PCM writes.
Blends are post-hoc float counterfactuals, NOT a new upmixer implementation.
"""
import argparse
import json
from pathlib import Path
import re

import numpy as np
from scipy.signal import sosfilt

from amp_dsp_chain import AmpChain, IN, SR, delay_channels
from amp_music_report import BANDS, GROUPS, band_powers, db, fingerprint, measure, read_audio

TARGETS = {'front mid/treble': [0, 1], 'centre mid': [10]}


def output_contributions(chain, x, output):
    """Independent per-input fixed-linear recomposition, including exact delays.

    Each default output has one A bus; its six input contributions share the
    same downstream filters. Never apply this factorization to B/nonlinear or
    common-input scenarios. Tests compare to the unfactored AmpChain renderer.
    """
    if any(v is not None for v in (chain.runtime, chain.upstream_scenario,
                                  chain.complex_scenario, chain.first_b_scenario)):
        raise ValueError('only fixed default A path can be decomposed')
    x = np.asarray(x, dtype=float)
    if x.ndim != 2 or x.shape[1] != 6 or not np.isfinite(x).all():
        raise ValueError('finite PCM6 required')
    src = chain.out_src[output]
    if src >= 8:
        return np.zeros_like(x)
    y = x * chain.M[src]
    filters = [chain.bus_sos[src], chain.out_sos[output]]
    filters += [bank[output] for bank in chain.post_eq_sos]
    for sos in filters:
        if len(sos):
            y = sosfilt(sos, y, axis=0)
    y *= chain.pair_a[src] * chain.output_pair_a[chain.route[output]] * chain.out_gain[output]
    return delay_channels(y, [chain.total_a_delay[output]] * 6)


def power_terms(samples):
    """Welch band self-powers and signed L/R cross term, never sum dB."""
    self_power = band_powers(samples)
    lr_power = band_powers(samples[:, :2].sum(axis=1))
    total = band_powers(samples.sum(axis=1))
    lr_incoherent = self_power[:, :2].sum(axis=1)
    return {'input_self_power': self_power.tolist(), 'total_power': total.tolist(),
            'lr_power': lr_power.tolist(), 'lr_incoherent_power': lr_incoherent.tolist(),
            'lr_cross_power': (lr_power - lr_incoherent).tolist(),
            'lr_summation_db': (db(lr_power) - db(lr_incoherent)).tolist(),
            'non_lr_effect_db': (db(total) - db(lr_power)).tolist()}


def attribution(dry, authored):
    incoherent = db(authored['lr_incoherent_power']) - db(dry['lr_incoherent_power'])
    summation = np.array(authored['lr_summation_db']) - dry['lr_summation_db']
    non_lr = np.array(authored['non_lr_effect_db']) - dry['non_lr_effect_db']
    total = db(authored['total_power']) - db(dry['total_power'])
    return {'self_power_change_db': incoherent.tolist(),
            'lr_summation_change_db': summation.tolist(),
            'non_lr_change_db': non_lr.tolist(), 'total_change_db': total.tolist(),
            'closure_max_abs_db': float(np.max(abs(total - incoherent - summation - non_lr)))}


def delayed_front_reference(stereo, frames, latency, gain):
    if type(latency) is not int or latency < 0 or not 0 <= gain <= 1:
        raise ValueError('nonnegative integer delay and unit-range gain required')
    reference = np.zeros((frames, 2))
    count = min(len(stereo), max(0, frames - latency))
    reference[latency:latency + count] = stereo[:count] * gain
    return reference


def blend_front(pcm6, reference, fraction):
    if not np.isfinite(fraction) or not 0 <= fraction <= 1 or reference.shape != pcm6[:, :2].shape:
        raise ValueError('matching reference and blend fraction0..1 required')
    result = pcm6.copy()
    result[:, :2] += fraction * (reference - pcm6[:, :2])
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    report = json.loads(a.report.read_text())
    chain = AmpChain()
    if report['model'] != chain.metadata():
        raise ValueError('current default v7 metadata required')
    if report['bands'] != [list(b) for b in BANDS] or report['group_channels'] != GROUPS:
        raise ValueError('band/group mismatch')
    variant = 'gain4096_fb0'
    if [variant, .4, 0., 0.] not in report['variants']:
        raise ValueError('baseline variant missing')
    start, end = [round(t * SR) for t in report['analysis']['window_source_seconds']]
    latency = report['analysis']['upmix_latency_frames']
    result = {'scope': __doc__, 'baseline_report': fingerprint(a.report),
              'model': chain.metadata(), 'bands': BANDS, 'input_channels': IN,
              'analysis': report['analysis'], 'tracks': {},
              'candidate_policy': 'front=(1-alpha)*cached_front+alpha*gain*delayed_dry; C/LFE/Ls/Rs unchanged; no requantization or new guard'}
    for key in report['results']:
        if not key.endswith(':' + variant):
            continue
        song = key.rsplit(':', 1)[0]
        drykey = song + ':dry'
        sources = [report['files'][k] for k in (drykey, key)]
        for f in sources:
            if fingerprint(f['path'])['sha256'] != f['sha256']:
                raise ValueError('PCM changed: ' + f['path'])
        log = Path(sources[1]['path']).with_suffix('.log')
        match = re.search(r'front=\S+ \(q15=(\d+)\)', log.read_text())
        if match is None:
            raise ValueError('front gain missing in cached render log')
        gain = int(match[1]) / 32768
        stereo, x = read_audio(sources[0]['path'], 2), read_audio(sources[1]['path'], 6)
        if end + latency + max(chain.total_a_delay) > len(x) or end + max(chain.total_a_delay) > len(stereo):
            raise ValueError('analysis exceeds available samples')
        dry = np.zeros((len(stereo), 6)); dry[:, :2] = stereo
        reference = delayed_front_reference(stereo, len(x), latency, gain)
        raw_dry, raw_up = band_powers(stereo[start:end]), band_powers(x[start+latency:end+latency])
        raw_delta = db(raw_up[:, :2].mean(axis=1)) - db(raw_dry.mean(axis=1))
        row = {'sources': sources, 'render_log': fingerprint(log), 'front_gain': gain,
               'gain_only_db': float(20*np.log10(gain)),
               'centre_nonzero_samples': int(np.count_nonzero(x[:, 2])),
               'input_front_change_db': raw_delta.tolist(),
               'input_front_change_beyond_gain_db': (raw_delta-20*np.log10(gain)).tolist(),
               'outputs': {}, 'candidates': {}, 'baseline': report['results'][key]}
        for output in [0, 1, 10]:
            delay = int(chain.total_a_delay[output])
            terms = []
            for pcm, offset in ((dry, 0), (x, latency)):
                y = output_contributions(chain, pcm, output)
                terms.append(power_terms(y[start+offset+delay:end+offset+delay]))
                del y
            row['outputs'][str(output)] = {'dry': terms[0], 'authored': terms[1],
                                          'attribution': attribution(*terms)}
        row['groups'] = {}
        for group, outputs in TARGETS.items():
            means = []
            for which in ('dry', 'authored'):
                rows = [row['outputs'][str(o)][which] for o in outputs]
                averaged = {k: np.mean([r[k] for r in rows], axis=0).tolist()
                            for k in ('total_power', 'lr_power', 'lr_incoherent_power')}
                averaged['lr_summation_db'] = (db(averaged['lr_power'])-db(averaged['lr_incoherent_power'])).tolist()
                averaged['non_lr_effect_db'] = (db(averaged['total_power'])-db(averaged['lr_power'])).tolist()
                means.append(averaged)
            attr = attribution(*means)
            expected = np.array(report['results'][key]['groups'][group]['power_dbfs']) - report['results'][drykey]['groups'][group]['power_dbfs']
            error = float(np.max(abs(np.array(attr['total_change_db']) - expected)))
            if error > 1e-7:
                raise ValueError(f'recomposition differs from baseline: {song}/{group}: {error}')
            row['groups'][group] = dict(attr, baseline_max_abs_error_db=error)
        ref_power = band_powers(stereo[start:end, 0])
        for fraction in (.5, 1.):
            candidate = blend_front(x, reference, fraction)
            measured = measure(chain, candidate, ref_power, start, end, latency)
            measured['input_peak_dbfs'] = float(20*np.log10(max(np.max(abs(candidate)), 1e-15)))
            measured['delta_vs_dry_bank1'] = {g: (np.array(r['power_dbfs']) - report['results'][drykey]['groups'][g]['power_dbfs']).tolist()
                                                    for g, r in measured['groups'].items()}
            row['candidates'][str(fraction)] = measured
            del candidate
        result['tracks'][song] = row
        a.out.parent.mkdir(parents=True, exist_ok=True)
        # Partial output is explicitly labelled until all tracks finish.
        result['complete'] = False
        a.out.write_text(json.dumps(result, indent=2) + '\n')
        print(song, 'mid dB', round(row['groups']['front mid/treble']['total_change_db'][4], 3), 'done', flush=True)
    result['complete'] = True
    a.out.write_text(json.dumps(result, indent=2) + '\n')
    print(a.out, flush=True)


if __name__ == '__main__':
    main()
