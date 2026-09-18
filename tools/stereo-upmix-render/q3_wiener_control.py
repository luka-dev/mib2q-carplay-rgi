"""Static per-bin Wiener control for the Q2 DEV comparison.

Fits, on TRAIN cache songs, per identity head k and STFT bin f a time-invariant
2x2 complex filter W_k(f) = <s_k x^H><x x^H>^-1 (and a real diagonal per-channel
gain variant), using the SAME FFT1024/hop256 periodic-Hann analysis/synthesis
as the Q2 frontend (r1_refiner_audio). Applies it to the 19 sealed DEV songs and
scores whole-song NMSE in exactly the cells the sealed evaluator scored, reading
those cells (predicted/raw/source-relative NMSE, active flags) from the sealed
per-song score.json files. Zero learned parameters at runtime beyond 12x513x4
complex numbers; ~2-6 k MAC per frame. Research control, not a product.

  python q3_wiener_control.py --cache R/q3-cache-a --dev-scores R/q2-full-source-dev-predicted-a/attempts \
      --calibration R/q2-full-source-direct-predicted-a/calibration.json --out R/q3-wiener-control-a/report.json
"""
import argparse, json, math, time
from pathlib import Path

import numpy as np
import torch

from q2_source_data import CLASSES
from r1_refiner_audio import analyze, synthesize

K = len(CLASSES)
BINS = 513


def qicdf(values, q=0.5):
    """Empirical inverse-CDF quantile as in summarize_q2_full_source (no interpolation)."""
    v = sorted(values)
    return v[max(0, math.ceil(len(v) * q) - 1)] if v else None


from q3_cache import load_song  # expands compact/legacy cache layouts to [N,13,2]


def spectra(wave_f16):
    """float16 [N,2] -> complex64 [T,2,513] via the frozen analysis."""
    w = torch.from_numpy(np.ascontiguousarray(wave_f16))
    return analyze(w).frames[0]


def fit(cache, songs, heads_clean):
    rxx = np.zeros((BINS, 2, 2), 'c16')
    rsx = np.zeros((K, BINS, 2, 2), 'c16')
    frames_per_head = np.zeros(K, 'i8')
    scalar_num = np.zeros(K, 'f8')
    scalar_den = 0.0
    t0 = time.time()
    for i, song in enumerate(songs):
        a = load_song(cache, song)
        clean = heads_clean[song]
        X = spectra(a[:, 0])                             # [T,2,513]
        Xn = X.permute(2, 0, 1).numpy().astype('c16')    # [513,T,2]
        rxx += np.einsum('ftc,ftd->fcd', Xn, Xn.conj())
        xt = np.asarray(a[:, 0], 'f8')
        scalar_den += float((xt * xt).sum())
        for k in range(K):
            if not clean[k]:
                continue
            S = spectra(a[:, 1 + k]).permute(2, 0, 1).numpy().astype('c16')
            rsx[k] += np.einsum('ftc,ftd->fcd', S, Xn.conj())
            frames_per_head[k] += Xn.shape[1]
            st = np.asarray(a[:, 1 + k], 'f8')
            scalar_num[k] += float((xt * st).sum())
        print(f'  fit [{i+1}/{len(songs)}] {song[:8]} {time.time()-t0:.0f}s', flush=True)
    eps = 1e-6 * np.real(np.trace(rxx, axis1=1, axis2=2)).mean()
    inv = np.linalg.inv(rxx + eps * np.eye(2)[None])
    w22 = np.einsum('kfcd,fde->kfce', rsx, inv)                       # [K,513,2,2]
    diag = np.real(np.einsum('kfcc->kfc', rsx)) / np.maximum(np.real(np.einsum('fcc->fc', rxx)), 1e-12)
    raw_scalar = scalar_num / max(scalar_den, 1e-30)
    return dict(w22=w22, diag=diag, raw_scalar=raw_scalar, frames_per_head=frames_per_head)


def apply_score(cache, song_id, filt, sealed):
    a = load_song(cache, song_id)
    X = spectra(a[:, 0])                                   # [T,2,513] complex64
    n = a.shape[0]
    Xn = X.permute(2, 0, 1).numpy()                        # [513,T,2]
    out = {}
    for k, c in enumerate(CLASSES):
        cell = sealed[k]
        if not any(cell['active']):
            continue
        s = np.asarray(a[:, 1 + k], 'f8')
        ss = (s * s).sum(0)
        res = dict(identity=c, active=cell['active'], target_power=cell['target_power'],
                   nmse_predicted=cell['nmse'], nmse_raw=cell['controls_nmse']['raw-pcm'],
                   nmse_source_relative=cell['controls_nmse']['source-relative'])
        for name, Y in (('w22', np.einsum('fce,fte->ftc', filt['w22'][k], Xn)),
                        ('diag', filt['diag'][k][:, None, :] * Xn)):
            fr = torch.from_numpy(np.ascontiguousarray(Y.transpose(1, 2, 0), dtype='c8'))[None]
            y = synthesize(fr, n).numpy().astype('f8')
            res['nmse_' + name] = (((y - s) ** 2).sum(0) / np.maximum(ss, 1e-30)).tolist()
            res['out_minus_target_db_' + name] = (10 * np.log10(np.maximum((y * y).sum(0), 1e-30) / np.maximum(ss, 1e-30))).tolist()
        # raw scalar reproduced from our fit (validation against the sealed control)
        x = np.asarray(a[:, 0], 'f8')
        y = filt['raw_scalar'][k] * x
        res['nmse_raw_refit'] = (((y - s) ** 2).sum(0) / np.maximum(ss, 1e-30)).tolist()
        out[c] = res
    return out


def summarize(per_song):
    """Song-balanced medians of gain dB vs raw scalar, per family and overall, summarizer rule."""
    def gain(ctrl, cand):
        return 10 * (math.log10(ctrl) - math.log10(cand)) if ctrl > 0 and cand > 0 else None
    cands = ('predicted', 'w22', 'diag', 'raw_refit', 'source_relative')
    table = {}
    for cand in cands:
        fam = {}
        overall = []
        for song, heads in per_song.items():
            cells = []
            for c, r in heads.items():
                g = [gain(r['nmse_raw'][ch], r['nmse_' + cand][ch]) for ch in (0, 1) if r['active'][ch]]
                g = [v for v in g if v is not None]
                if g:
                    fam.setdefault(c, []).append(qicdf(g))
                    cells += g
            if cells:
                overall.append(qicdf(cells))
        table[cand] = dict(all=qicdf(overall), songs=len(overall),
                           per_family={c: dict(median=qicdf(v), songs=len(v), min=min(v), max=max(v)) for c, v in fam.items()})
    return table


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cache', type=Path, required=True)
    ap.add_argument('--dev-scores', type=Path, required=True)
    ap.add_argument('--calibration', type=Path, default=None)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--fit', default='strict', choices=('strict', 'all', 'self'),
                    help='strict=81 admitted TRAIN (model exposure); all=181 TRAIN with clean-head masks; self=fit on the DEV songs themselves (smoke only)')
    ap.add_argument('--limit-dev', type=int, default=None)
    args = ap.parse_args()
    # sidecars are authoritative (manifest.json may predate the last builder)
    manifest = {p.stem: json.load(open(p)) for p in sorted(args.cache.glob('*.json')) if p.name != 'manifest.json'}
    dev_cells = {}
    for p in sorted(args.dev_scores.glob('*/score.json')):
        s = json.load(open(p))
        dev_cells[s['song_id']] = s['whole_song']['families']
    dev = [s for s in sorted(dev_cells) if s in manifest][:args.limit_dev]
    assert dev, 'no sealed DEV songs present in cache yet'
    if args.fit == 'self':
        fit_songs = dev
        heads_clean = {s: manifest[s]['heads_clean'] for s in dev}
    else:
        train = [s for s, m in manifest.items() if m['partition'] == 'TRAIN' and (args.fit == 'all' or m['strict_admitted'])]
        fit_songs = sorted(train)
        heads_clean = {s: (manifest[s]['heads_clean'] if args.fit == 'all' else [True] * K) for s in train}
    print(f'fit on {len(fit_songs)} songs ({args.fit}); score {len(dev)} DEV songs', flush=True)
    filt = fit(args.cache, fit_songs, heads_clean)
    check = None
    if args.calibration and args.calibration.exists():
        cal = json.load(open(args.calibration))
        sealed = cal['scalar_controls']['raw-pcm']
        sealed = [sealed[c] if isinstance(sealed, dict) else sealed[i] for i, c in enumerate(CLASSES)]
        check = dict(sealed_raw_scalar=[float(v) for v in sealed], refit_raw_scalar=filt['raw_scalar'].tolist(),
                     max_abs_db_diff=float(max(abs(20 * math.log10(max(abs(a), 1e-12) / max(abs(b), 1e-12)))
                                               for a, b in zip(sealed, filt['raw_scalar']))))
        print('raw scalar reproduction max |dB diff| =', round(check['max_abs_db_diff'], 3), flush=True)
    per_song = {}
    for i, song in enumerate(dev):
        per_song[song] = apply_score(args.cache, song, filt, dev_cells[song])
        print(f'  dev [{i+1}/{len(dev)}] {song[:8]}', flush=True)
    table = summarize(per_song)
    report = dict(schema='q3-static-wiener-control-v1', fit=args.fit, fit_songs=fit_songs,
                  frames_per_head=filt['frames_per_head'].tolist(), scalar_check=check,
                  gain_db_vs_raw_scalar=table, per_song=per_song,
                  filter_shapes=dict(w22=list(filt['w22'].shape), diag=list(filt['diag'].shape)),
                  note='static, time-invariant, no lookahead beyond the shared 768-sample WOLA; research control only',
                  car_approved=False)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=1))
    np.savez(args.out.with_suffix('.filters.npz'), w22=filt['w22'], diag=filt['diag'], raw_scalar=filt['raw_scalar'])
    print('\nsong-balanced median gain dB vs raw scalar:')
    for cand, t in table.items():
        print(f"  {cand:16s} all={t['all']:+.3f}  " + '  '.join(f"{c[:8]}={v['median']:+.2f}" for c, v in t['per_family'].items()))


if __name__ == '__main__':
    main()
