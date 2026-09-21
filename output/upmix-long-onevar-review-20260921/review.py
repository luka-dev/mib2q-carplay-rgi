"""Read-only review of the 60 s one-variable grid; writes review.json/rows.csv beside itself.

Pre-declared before the run (do not tune after seeing results):
  TAIL_WINDOW_S   = 0.25   analysis window inside the explicit zero tail
  DECAY_DB        = -80.0  electrical peak (re 1.0 full scale) below which a window counts as silent
  A row "decayed" only if every window from decay_at_s to the END of the tail is silent.
  Reaching the end of the tail without that is "not decayed within tail", never "decayed".
Level matching (documented, two anchors, applied post hoc to band powers only; peaks stay raw):
  anchor "front_mid": offset = dry front mid/treble 500-2000 Hz - variant same band
  anchor "broadband": offset = dry mean linear power over the 10 grouped configs - variant same
The amp model (aux 0, adaptation 0, limiter threshold 100) is linear, so a post-hoc offset equals
an input gain change for those stages; the C renderer guards are NOT linear and are not matched.
"""
import csv, hashlib, json, sys
from pathlib import Path
import numpy as np
import soundfile as sf

ROOT = Path('/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay')
for candidate in Path(__file__).resolve().parents:   # prefer the repo this script lives in
    if (candidate / 'tools/soundamp-virtual-cabin').is_dir():
        ROOT = candidate
        break
sys.path.insert(0, str(ROOT / 'tools/soundamp-virtual-cabin'))
from audit_upmix_end_to_end import verify, fingerprint, array_fingerprint  # noqa: E402
from amp_end_to_end_metrics import measure, paired_delta  # noqa: E402
from amp_music_report import GROUPS, BANDS  # noqa: E402

SR = 48000
TAIL_WINDOW_S = 0.25
DECAY_DB = -80.0
SCENARIO = 'full_static_nearest_zero_aux'
BASE = 'startup'
DRY = 'dry_bank1'
VARIANTS = [dict(name='startup', render_mode='s16', surround=.4, front_bass=-3., lfe_gain=0., retention=0., lfe='assist'),
            dict(name='bass0', render_mode='s16', surround=.4, front_bass=0., lfe_gain=0., retention=0., lfe='assist'),
            dict(name='surr30', render_mode='s16', surround=.3, front_bass=-3., lfe_gain=0., retention=0., lfe='assist'),
            dict(name='retain75', render_mode='s16', surround=.4, front_bass=-3., lfe_gain=0., retention=.75, lfe='assist'),
            dict(name='float', render_mode='float', surround=.4, front_bass=-3., lfe_gain=0., retention=0., lfe='assist')]
CHANGED = {'startup': 'baseline', 'bass0': 'front_bass -3 -> 0', 'surr30': 'surround 0.40 -> 0.30',
           'retain75': 'retention 0 -> 0.75', 'float': 's16 -> float render'}
GROUP_CONFIGS = sorted({i for v in GROUPS.values() for i in v})
MID = 4   # BANDS index of 500-2000
B60 = 1   # 60-120
B20 = 0   # 20-60
B2K = 5   # 2000-8000


def db(p):
    return 10 * np.log10(np.maximum(p, 1e-30))


def tail_decay(out16, programme_frames):
    tail = out16[programme_frames:]
    w = int(TAIL_WINDOW_S * SR)
    n = len(tail) // w
    peaks = [float(20 * np.log10(max(float(np.abs(tail[i * w:(i + 1) * w]).max()), 1e-15))) for i in range(n)]
    silent = [p <= DECAY_DB for p in peaks]
    decay_at = None
    for i in range(n):
        if all(silent[i:]):
            decay_at = i * TAIL_WINDOW_S
            break
    return {'window_s': TAIL_WINDOW_S, 'threshold_dbfs': DECAY_DB, 'windows': n, 'window_peak_dbfs': peaks,
            'decayed_within_tail': decay_at is not None, 'decay_at_s': decay_at,
            'last_window_peak_dbfs': peaks[-1] if peaks else None,
            'tail_end_peak_dbfs_last_10ms': float(20 * np.log10(max(float(np.abs(tail[-480:]).max()), 1e-15)))}


def pcm6_tail(pcm6, programme_frames):
    nz = np.flatnonzero(np.any(pcm6 != 0, axis=1))
    last = int(nz[-1]) if len(nz) else -1
    return {'last_nonzero_frame': last, 'nonzero_after_programme_s': max(0.0, (last + 1 - programme_frames) / SR),
            'tail_is_exact_zero_at_end': bool(last < len(pcm6) - 1)}


def main():
    grid = Path(sys.argv[1]).resolve()
    out = Path(__file__).parent
    for name in ('review.json', 'rows.csv'):
        if (out / name).exists():
            raise ValueError('refusing overwrite of review artifacts')
    r = json.loads((grid / 'report.json').read_text())
    P = r['parameters']
    assert r['complete'] and r['completed_rows'] == r['expected_rows'] == 7 * (len(VARIANTS) + 1), r['completed_rows']
    assert P['variants'] == VARIANTS, P['variants']
    assert P['programme_frames'] == 60 * SR and P['tail_frames'] == 4 * SR
    assert r['axes']['scenarios'] == [SCENARIO] and P['input_eq_bank_hypothesis'] == 7
    assert not r['batch']['errors']
    fps = {}

    def collect(v):
        if isinstance(v, dict):
            if 'path' in v and 'sha256' in v:
                fps[(v['path'], v['sha256'])] = v
            for x in v.values():
                collect(x)
        elif isinstance(v, list):
            for x in v:
                collect(x)
    collect(r)
    prev = json.loads((ROOT / 'output/upmix-full-model-check-eq7-20260921/report.json').read_text())
    N, T = P['programme_frames'], P['tail_frames']
    rows = []
    for name, track in r['tracks'].items():
        shard = json.loads(Path(r['batch']['shards'][name]['report']['path']).read_text())
        assert shard['tracks'][name] == track and shard['complete']
        # same 60 s source cut as the 6 s study (prefix equality is guaranteed by identical file hash)
        assert prev['tracks'][name]['source']['sha256'] == track['source']['sha256']
        g = track['scenarios'][SCENARIO]
        assert set(g['rows']) == {DRY} | {v['name'] for v in VARIANTS}
        assert g['auxiliary']['kind'] == 'explicit_zero'
        for a in g['auxiliary']['files'].values():
            d, rate = sf.read(a['path'], always_2d=True)
            assert rate == SR and not np.any(d)
        metrics = {}
        for variant in [DRY] + [v['name'] for v in VARIANTS]:
            row = g['rows'][variant]
            pcmr = track['pcm6'][variant]
            assert row['scenario']['first_b_numerical_reference'] == 'firmware_library_nearest'
            assert row['scenario']['adaptive_enable'] == 0 and row['model']['bank'] == 1
            pcm, rate = sf.read(pcmr['file']['path'], dtype='float64', always_2d=True)
            assert rate == SR and pcm.shape == (N + T, 6)
            o, rate = sf.read(row['file']['path'], dtype='float64', always_2d=True)
            assert rate == SR and o.shape == (N + T, 16) and np.isfinite(o).all()
            assert array_fingerprint(o, '<f4') == row['output_arrays']['measurement_float32']
            m = measure(pcm, o, N, T)
            assert m == row['metrics']
            assert m['pcm6']['digital_c_nonzero_samples'] == 0
            metrics[variant] = m
            rail = np.abs(pcm) >= 32767 / 32768
            rec = {'track': name, 'variant': variant, 'changed': CHANGED.get(variant, 'dry L/R through BANK1'),
                   'peak_dbfs': m['outputs']['whole_clip']['peak_dbfs'],
                   'peak_config_channel': int(np.argmax(m['outputs']['whole_clip']['per_channel_peak_dbfs'])),
                   'output_frames_above_unity': m['outputs']['whole_clip']['frames_above_0dbfs'],
                   'pcm6_peak_dbfs': m['pcm6']['peak_dbfs'], 'pcm6_near_rail_samples': int(rail.sum()),
                   'guard_minimum_gain': pcmr.get('guard', {}).get('minimum-guard-gain'),
                   'guard_attenuated_frames': pcmr.get('guard', {}).get('attenuated-frames'),
                   'guard_telemetry_complete': pcmr.get('guard_telemetry_complete', False)}
            rec.update({'tail_' + k: v for k, v in tail_decay(o, N).items() if k != 'window_peak_dbfs'})
            rec['tail_window_peaks'] = tail_decay(o, N)['window_peak_dbfs']
            rec.update({'pcm6_' + k: v for k, v in pcm6_tail(pcm, N).items()})
            if variant != DRY:
                assert paired_delta(m, metrics[DRY]) == row['delta_vs_dry_bank1']
                d = row['delta_vs_dry_bank1']
                fm, fw, sub, rear, cm = (d['front mid/treble'], d['front woofers'], d['subwoofer'], d['rear doors'], d['centre mid'])
                # anchors
                off_mid = -fm[MID]
                lin = lambda mm: np.mean([(10 ** (np.asarray(mm['outputs']['per_config_band_power_dbfs'])[i, :] / 10)).sum() for i in GROUP_CONFIGS])  # rows=16 configs, cols=7 bands
                off_bb = db(lin(metrics[DRY])) - db(lin(m))
                rec.update({'raw_woofer60_120_vs_dry_db': fw[B60], 'raw_mid500_2000_vs_dry_db': fm[MID],
                            'tilt_bass_mid_vs_dry_db': fw[B60] - fm[MID],
                            'raw_sub20_60_vs_dry_db': sub[B20], 'raw_sub60_120_vs_dry_db': sub[B60],
                            'raw_rear500_2000_vs_dry_db': rear[MID], 'raw_rear2000_8000_vs_dry_db': rear[B2K],
                            'raw_centre500_2000_vs_dry_db': cm[MID],
                            'match_front_mid_offset_db': off_mid, 'match_broadband_offset_db': float(off_bb),
                            'matched_frontmid_sub20_60_db': sub[B20] + off_mid, 'matched_frontmid_sub60_120_db': sub[B60] + off_mid,
                            'matched_frontmid_woofer60_120_db': fw[B60] + off_mid,
                            'matched_frontmid_rear500_2000_db': rear[MID] + off_mid, 'matched_frontmid_rear2000_8000_db': rear[B2K] + off_mid,
                            'matched_frontmid_centre500_2000_db': cm[MID] + off_mid,
                            'matched_broadband_woofer60_120_db': fw[B60] + float(off_bb), 'matched_broadband_mid500_2000_db': fm[MID] + float(off_bb),
                            'matched_broadband_sub20_60_db': sub[B20] + float(off_bb), 'matched_broadband_rear500_2000_db': rear[MID] + float(off_bb)})
            rows.append(rec)
            print('verified', name, variant, flush=True)
    for v in fps.values():
        verify(v)
    # one variable at a time: variant minus startup, per track
    keys = [k for k in rows[-1] if k.startswith(('raw_', 'tilt_', 'matched_')) or k in ('peak_dbfs', 'pcm6_peak_dbfs')]
    by = {(x['track'], x['variant']): x for x in rows}
    onevar = {}
    for v in VARIANTS:
        if v['name'] == BASE:
            continue
        onevar[v['name']] = {'changed': CHANGED[v['name']]}
        for k in keys:
            deltas = [by[(t, v['name'])][k] - by[(t, BASE)][k] for t in r['tracks']]
            onevar[v['name']][k + '_minus_startup'] = {'mean': float(np.mean(deltas)), 'min': float(min(deltas)),
                                                       'max': float(max(deltas)), 'per_track': dict(zip(r['tracks'], map(float, deltas)))}
    summary = {}
    for variant in [DRY] + [v['name'] for v in VARIANTS]:
        sel = [x for x in rows if x['variant'] == variant]
        worst = max(sel, key=lambda x: x['peak_dbfs'])
        s = {'changed': sel[0]['changed'], 'max_peak_dbfs': worst['peak_dbfs'], 'max_peak_track': worst['track'],
             'rows_exceeding_unity': sum(x['peak_dbfs'] > 0 for x in sel),
             'pcm6_near_rail_samples': sum(x['pcm6_near_rail_samples'] for x in sel),
             'tail_decayed_rows': sum(x['tail_decayed_within_tail'] for x in sel),
             'tail_decay_at_s_max': max((x['tail_decay_at_s'] for x in sel if x['tail_decay_at_s'] is not None), default=None),
             'tail_last_window_peak_dbfs_max': max(x['tail_last_window_peak_dbfs'] for x in sel),
             'pcm6_nonzero_after_programme_s_max': max(x['pcm6_nonzero_after_programme_s'] for x in sel)}
        if variant != DRY:
            for k in ('tilt_bass_mid_vs_dry_db', 'raw_sub20_60_vs_dry_db', 'raw_sub60_120_vs_dry_db', 'raw_rear500_2000_vs_dry_db',
                      'raw_centre500_2000_vs_dry_db', 'matched_frontmid_sub20_60_db', 'matched_frontmid_rear500_2000_db',
                      'matched_broadband_woofer60_120_db', 'matched_broadband_mid500_2000_db', 'match_front_mid_offset_db',
                      'match_broadband_offset_db'):
                vals = [x[k] for x in sel]
                s[k + '_mean_min_max'] = [float(np.mean(vals)), float(min(vals)), float(max(vals))]
        summary[variant] = s
    # 6 s vs 60 s: same variants exist in the earlier study under other names
    prev_map = {'startup': 'startup', 'bass0': 'zero_trim'}
    six_vs_sixty = {}
    for new, old in prev_map.items():
        six_vs_sixty[new] = {}
        for t in r['tracks']:
            d6 = prev['tracks'][t]['scenarios'][SCENARIO]['rows'][old]['delta_vs_dry_bank1']
            six_vs_sixty[new][t] = {'tilt_6s': d6['front woofers'][B60] - d6['front mid/treble'][MID],
                                    'tilt_60s': by[(t, new)]['tilt_bass_mid_vs_dry_db'],
                                    'peak_6s': prev['tracks'][t]['scenarios'][SCENARIO]['rows'][old]['metrics']['outputs']['whole_clip']['peak_dbfs'],
                                    'peak_60s': by[(t, new)]['peak_dbfs']}
    result = {'complete': True, 'verified_rows': len(rows), 'unique_referenced_file_hashes_checked': len(fps),
              'grid_report': fingerprint(grid / 'report.json'), 'review_script': fingerprint(__file__),
              'predeclared': {'tail_window_s': TAIL_WINDOW_S, 'decay_threshold_dbfs': DECAY_DB, 'tail_s': T / SR,
                              'level_match_anchors': ['front_mid 500-2000 Hz of configs 0/1', 'broadband mean linear power over grouped configs'],
                              'baseline': BASE, 'one_variable_per_variant': CHANGED},
              'scope': 'seven 60 s cuts + 4 s zero tail, EQ7 hypothesis only, full conditional electronics; not complete songs, not vehicle acceptance',
              'limitations': ['EQ10 not rerun; the 6 s study showed EQ7/EQ10 deltas within 0.03 dB but higher EQ10 peaks',
                              'level matching is post hoc on band powers; renderer guards are nonlinear and unmatched; peaks reported raw',
                              'tail decay is measured on the model output at -80 dBFS electrical; not a settled-state proof beyond the 4 s tail',
                              'S16 rows have no float guard telemetry',
                              'runtime gains 1, limiter threshold 100; >0 dBFS is a model headroom flag, not hardware clipping',
                              'dry is L/R through BANK1, not stock stereo; group names are unverified physical candidates',
                              'no acoustics, no dynamic transitions, no SHARC bit-exactness'],
              'summary': summary, 'one_variable_minus_startup': onevar, 'six_vs_sixty_seconds': six_vs_sixty,
              'rows': rows, 'hardware_or_subjective_acceptance': False}
    with (out / 'rows.csv').open('x', newline='') as f:
        flat = [{k: (json.dumps(v) if isinstance(v, (list, dict)) else v) for k, v in x.items()} for x in rows]
        keys_all = list(dict.fromkeys(k for x in flat for k in x))
        w = csv.DictWriter(f, fieldnames=keys_all)
        w.writeheader()
        w.writerows(flat)
    with (out / 'review.json').open('x') as f:
        json.dump(result, f, indent=2, allow_nan=False)
        f.write('\n')
    print(json.dumps({'summary': summary, 'one_variable_minus_startup': {k: {kk: vv for kk, vv in v.items() if kk == 'changed' or kk.startswith(('tilt', 'raw_sub20', 'raw_rear500', 'peak'))} for k, v in onevar.items()}}, indent=1))


if __name__ == '__main__':
    main()
