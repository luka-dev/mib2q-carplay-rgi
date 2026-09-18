"""Stage 0: azimuth-object second scene (offline prototype, no ML).

The stereo mix already carries each source's pan position. Per STFT bin we estimate
the level-pan azimuth p in [-1,1] (constant-power panning), cluster the energy over a
sliding window into up to K dominant azimuths (histogram peaks with hysteresis), and
assign bins to the nearest peak. Scene rule:
  |peak| <  p_center          -> FRONT (lead, bass, kick, snare live here; free protection)
  |peak| >= p_center          -> REAR object, kept at its own L/R image
  bins far from every peak, low inter-channel coherence -> diffuse layer, low level
  bins below lf_hz            -> FRONT (bass management)
Render: rear = sum(object masks) * X (per channel), HP, delay; front = X - alpha*rear.
Evaluation with true stems (MoisesDB / AlbumDB caches): per-stem share of energy sent
rear (exact, masks are linear), lead leakage, mask flicker. Same FFT1024/hop256 frontend
as everything else; the C port targets hssr_dsp's FFT512/hop128 loop.

  python q5_azimuth_scene.py --out R/q5-azimuth-scene-a --cache R/q3-cache-a 747d5c98 260c431b d028d7c2 --wav file.mp3 ...
"""
import argparse, json, subprocess, time
from pathlib import Path

import numpy as np
import soundfile as sf
import torch
from scipy.ndimage import uniform_filter1d
from scipy.signal import butter, sosfilt, resample_poly

from q2_source_data import CLASSES
from r1_refiner_audio import analyze, synthesize

SR, HOP, BINS = 48000, 256, 513


# ----------------------------------------------------------------------------- io

def read_any(path):
    try:
        a, sr = sf.read(path, dtype='float32', always_2d=True)
    except Exception:
        raw = subprocess.run(['ffmpeg', '-v', 'quiet', '-i', str(path), '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'], capture_output=True, check=True).stdout
        return np.frombuffer(raw, 'f4').reshape(-1, 2).copy()
    if a.shape[1] == 1:
        a = np.repeat(a, 2, 1)
    if sr != SR:
        from math import gcd
        g = gcd(SR, sr); a = resample_poly(a.astype('f8'), SR // g, sr // g, axis=0, padtype='constant').astype('f4')
    return a


def stft(x):
    return analyze(torch.from_numpy(np.ascontiguousarray(x, dtype='f4'))).frames[0].numpy()   # [T,2,513]


def istft(S, n):
    return synthesize(torch.from_numpy(np.ascontiguousarray(S, dtype='c8'))[None], n).numpy()


def smooth_t(x, frames):
    return uniform_filter1d(x, size=max(1, int(frames)), axis=0, mode='nearest')


# ----------------------------------------------------------------------------- azimuth objects

def azimuth_features(X, t_smooth=6):
    L, R = X[:, 0], X[:, 1]
    PL, PR = np.maximum(smooth_t(np.abs(L) ** 2, t_smooth), 0), np.maximum(smooth_t(np.abs(R) ** 2, t_smooth), 0)   # float32 box filter can go slightly negative
    cross = L * np.conj(R)
    sC = smooth_t(cross.real, t_smooth) + 1j * smooth_t(cross.imag, t_smooth)
    coh = np.abs(sC) / np.sqrt(PL * PR + 1e-12)                       # 1 = one panned source, low = diffuse
    pan = (np.sqrt(PR) - np.sqrt(PL)) / (np.sqrt(PR) + np.sqrt(PL) + 1e-9)   # amplitude pan in [-1,1]
    return pan, coh, PL + PR


def smoothstep(x, a, b):
    t = np.clip((x - a) / max(b - a, 1e-6), 0, 1)
    return t * t * (3 - 2 * t)


def azimuth_objects(X, p0=0.12, p1=0.35, coh_diffuse=0.35, coh_full=0.6, lf_hz=120.0, attack=4, release=28):
    """Continuous pan->rear mapping. rear_mask [T,513]: off-centre coherent bins, keeping their own L/R image;
    diffuse_mask: low-coherence bins (ambience) as a separate low-level layer. Returns (rear, diffuse, info, flicker)."""
    pan, coh, P = azimuth_features(X)
    T = X.shape[0]
    freqs = np.arange(BINS) * SR / 1024
    hf = (freqs >= lf_hz)[None, :]
    side = smoothstep(np.abs(pan), p0, p1)                       # 0 at centre .. 1 fully off-centre
    coherent = smoothstep(coh, coh_diffuse, coh_full)            # 1 = one clean panned source
    rear_raw = (side * coherent * hf).astype('f4')
    diffuse_raw = ((1 - coherent) * hf).astype('f4')
    def ar_smooth(m):
        out = np.zeros_like(m); st = np.zeros(m.shape[1], 'f4')
        ca, cr = 1 - np.exp(-1 / attack), 1 - np.exp(-1 / release)
        for t in range(m.shape[0]):
            c = np.where(m[t] > st, ca, cr); st = st + c * (m[t] - st); out[t] = st
        return uniform_filter1d(out, 3, axis=1, mode='nearest')
    rear = np.clip(ar_smooth(rear_raw), 0, 1); diffuse = np.clip(ar_smooth(diffuse_raw), 0, 1)
    flicker = float(np.abs(np.diff(rear, axis=0)).mean())
    info = dict(p0=p0, p1=p1, coh_diffuse=coh_diffuse, coh_full=coh_full, side_energy_share=float((P * side * hf).sum() / (P * hf).sum()))
    return rear, diffuse, info, flicker


# ----------------------------------------------------------------------------- render + eval

def db(x):
    return float(10 * np.log10(max(x, 1e-30)))


def render(mix, rear_t, diffuse_t, alpha=0.7, rear_gain=0.6, diffuse_gain=0.25, delay_ms=12.0, hp_hz=120.0):
    sos = butter(2, hp_hz, 'highpass', fs=SR, output='sos')
    r = sosfilt(sos, rear_t, axis=0).astype('f4'); dfz = sosfilt(sos, diffuse_t, axis=0).astype('f4')
    front = (mix - alpha * r).astype('f4')                       # relocate objects: remove their share from the front
    d = int(SR * delay_ms / 1000)
    rear = np.concatenate((np.zeros((d, 2), 'f4'), (rear_gain * r + diffuse_gain * dfz)[:-d]), 0)
    quad = np.concatenate((front, rear), 1)
    return front, rear, quad, front + 0.8 * rear


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--cache', type=Path, nargs='*', default=[])
    ap.add_argument('--wav', type=Path, nargs='*', default=[])
    ap.add_argument('--seconds', type=float, default=None)
    ap.add_argument('--p0', type=float, default=0.12)
    ap.add_argument('--p1', type=float, default=0.35)
    ap.add_argument('--alpha', type=float, default=0.7)
    ap.add_argument('--songs', nargs='*', default=[])
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    report = dict(schema='q5-azimuth-scene-v2', params=dict(p0=args.p0, p1=args.p1, alpha=args.alpha), songs={})
    jobs = []
    from q3_cache import load_song
    for cache in args.cache:
        metas = {p.stem: json.load(open(p)) for p in cache.glob('*.json') if p.name != 'manifest.json'}
        for pre in args.songs:
            for sid, m in metas.items():
                if sid.startswith(pre):
                    jobs.append((f"{sid[:8]}-{m.get('artist','')}-{m.get('title','')}".replace(' ', '_').replace('/', '_'), ('cache', cache, sid, m)))
    for w in args.wav:
        jobs.append((w.stem.replace(' ', '_')[:60], ('wav', w, None, None)))
    for name, (kind, src, sid, meta) in jobs:
        t0 = time.time(); d = args.out / name; d.mkdir(exist_ok=True)
        if kind == 'cache':
            a = load_song(src, sid); mix, stems = a[:, 0], a[:, 1:]
        else:
            mix, stems = read_any(src), None
        if args.seconds:
            mix = mix[:int(args.seconds * SR)]; stems = None if stems is None else stems[:len(mix)]
        n = len(mix); X = stft(mix)
        rear_m, diff_m, info, flicker = azimuth_objects(X, p0=args.p0, p1=args.p1)
        rear_t = istft(rear_m[:, None, :] * X, n); diff_t = istft(diff_m[:, None, :] * X, n)
        front, rear, quad, preview = render(mix, rear_t, diff_t, alpha=args.alpha)
        g = float(min(1.0, 0.98 / max(np.abs(quad).max(), np.abs(preview).max(), 1e-9)))
        for fn, arr in (('front', front), ('rear', rear), ('quad', quad), ('preview', preview), ('original', mix)):
            sf.write(d / f'{fn}.wav', arr * g, SR, subtype='PCM_24')
        res = dict(rear_re_mix_db=db(float((rear_t ** 2).mean()) / float((mix ** 2).mean())), diffuse_re_mix_db=db(float((diff_t ** 2).mean()) / float((mix ** 2).mean())),
                   rear_mask_mean=float(rear_m.mean()), flicker=flicker, seconds=time.time() - t0)
        if stems is not None:
            per = {}
            for k, c in enumerate(CLASSES):
                s = stems[:, k]
                ps = float((s ** 2).sum())
                if ps < 1e-4 * float((mix ** 2).sum()):
                    continue
                Sk = stft(s); Pk = np.abs(Sk) ** 2
                per[c] = dict(to_rear_db=db(float((rear_m[:, None, :] ** 2 * Pk).sum()) / float(Pk.sum())),
                              to_diffuse_db=db(float((diff_m[:, None, :] ** 2 * Pk).sum()) / float(Pk.sum())))
            res['per_stem'] = per
        report['songs'][name] = res
        line = f"{name[:36]:36s} rear {res['rear_re_mix_db']:+.1f} dB diffuse {res['diffuse_re_mix_db']:+.1f} flicker {flicker:.3f}"
        if stems is not None:
            line += ' | ' + ' '.join(f"{c[:6]}→{v['to_rear_db']:+.0f}" for c, v in res['per_stem'].items())
        print(line, flush=True)
        (args.out / 'report.json').write_text(json.dumps(report, indent=1))
    print('done', args.out)


if __name__ == '__main__':
    main()
