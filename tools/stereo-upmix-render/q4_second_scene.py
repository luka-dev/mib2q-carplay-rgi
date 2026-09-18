"""Second-scene prototype (offline, listening only): genre-agnostic rear mask.

Rear = what is NOT protected (lead / bass / drum core) AND is spatially wide or
decorrelated or texture-like. Three variants per song:
  dsp       : mix only. Spatial (pan, inter-channel coherence) + texture cues, protection
              = "center and dry" heuristic. Works on ANY stereo file (mp3/flac/wav).
  roles     : MoisesDB true stems by a role rule (oracle "second scene by roles").
  dsp+prot  : dsp rear mask vetoed by a true-stem protection mask (lead+bass+drums IRM);
              this is the product shape: DSP rear + learned protection.
Render: rear = mask*X (same FFT1024/hop256 frontend), HP 120 Hz, delayed 15 ms, gain;
front = X - alpha*rear (undelayed). Outputs per song: front.wav, rear.wav, quad.wav
(FL FR RL RR), preview.wav (headphones). No cabin matrix, no limiter: not the car chain.

  python q4_second_scene.py --out R/q4-second-scene-a --cache R/q3-cache-a 747d5c98 260c431b d028d7c2
  python q4_second_scene.py --out R/q4-second-scene-a --wav ~/Music/x.mp3
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

HOP, SR = 256, 48000
FRONT_CLASSES = ('lead_vocal', 'bass', 'drums')
REAR_CLASSES = ('backing_vocal', 'other_vocal', 'high_percussion', 'percussion', 'strings_winds', 'other_plucked', 'other_unknown')
DOMINANCE_CLASSES = ('guitar', 'keys')


# ----------------------------------------------------------------------------- io

def read_any(path):
    try:
        a, sr = sf.read(path, dtype='float32', always_2d=True)
    except Exception:
        raw = subprocess.run(['ffmpeg', '-v', 'quiet', '-i', str(path), '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'],
                             capture_output=True, check=True).stdout
        return np.frombuffer(raw, 'f4').reshape(-1, 2).copy()
    if a.shape[1] == 1:
        a = np.repeat(a, 2, 1)
    if sr != SR:
        from math import gcd
        g = gcd(SR, sr)
        a = resample_poly(a.astype('f8'), SR // g, sr // g, axis=0, padtype='constant').astype('f4')
    return a


def stft(x):
    return analyze(torch.from_numpy(np.ascontiguousarray(x, dtype='f4'))).frames[0].numpy()   # [T,2,513] c8


def istft(S, n):
    return synthesize(torch.from_numpy(np.ascontiguousarray(S, dtype='c8'))[None], n).numpy()


def ema(x, frames, axis=0):
    """Causal-ish smoothing over time (uniform box is fine offline)."""
    return uniform_filter1d(x, size=max(1, int(frames)), axis=axis, mode='nearest')


# ----------------------------------------------------------------------------- masks

def spatial_features(X):
    """X [T,2,513] -> coherence c[T,513] in [0,1], pan p[T,513] in [-1,1], texture tex[T,513] in [0,1]."""
    L, R = X[:, 0], X[:, 1]
    PL, PR = np.abs(L) ** 2, np.abs(R) ** 2
    cross = L * np.conj(R)
    n = 10                                   # ~53 ms
    sPL, sPR = ema(PL, n), ema(PR, n)
    sC = ema(cross.real, n) + 1j * ema(cross.imag, n)
    coh = np.abs(sC) / np.sqrt(sPL * sPR + 1e-12)
    pan = (sPR - sPL) / (sPR + sPL + 1e-12)
    mag = np.sqrt(0.5 * (PL + PR))
    fast, slow = ema(mag, 3), ema(mag, 40)   # 16 ms vs 213 ms
    transient = np.clip((fast - slow) / (fast + 1e-9), 0, 1)
    tex = 1 - ema(transient, 20)
    # light frequency smoothing to avoid musical noise
    coh = uniform_filter1d(coh, 3, axis=1, mode='nearest'); pan = uniform_filter1d(pan, 3, axis=1, mode='nearest')
    return coh, pan, tex


def dsp_rear_mask(X, p0=0.35, c0=0.75, lf_hz=120.0, prot=None):
    """Genre-agnostic rear mask in [0,1] per (t,f); applied to both channels."""
    coh, pan, tex = spatial_features(X)
    m_side = np.clip(np.abs(pan) / p0, 0, 1)
    m_amb = np.clip((c0 - coh) / c0, 0, 1)
    m = np.maximum(m_side, m_amb) * (0.5 + 0.5 * tex)
    center_dry = coh * (1 - np.abs(pan)) ** 2          # vocal, bass, kick live here
    if prot is None:
        prot = np.clip((center_dry - 0.5) / 0.4, 0, 1)   # heuristic protection
    m = m * (1 - prot)
    bins = np.arange(X.shape[-1]) * SR / 1024
    m[:, bins < lf_hz] = 0
    m = ema(m, 6)                                        # ~30 ms
    return np.clip(m, 0, 1), dict(coh=coh, pan=pan, tex=tex, prot=prot)


def role_targets(mix, stems, present):
    """MoisesDB oracle: front/rear stem sums by role rule. stems [N,12,2]."""
    rear = np.zeros_like(mix)
    front = np.zeros_like(mix)
    for c in REAR_CLASSES:
        rear += stems[:, CLASSES.index(c)]
    for c in FRONT_CLASSES:
        front += stems[:, CLASSES.index(c)]
    # guitar/keys: if a lead vocal is present in the window, both support; else the louder is foreground
    n = len(mix); win = SR * 2
    lv = stems[:, CLASSES.index('lead_vocal')]
    for s in range(0, n, win):
        e = min(n, s + win)
        seg = {c: float((stems[s:e, CLASSES.index(c)] ** 2).mean()) for c in DOMINANCE_CLASSES}
        vocal = float((lv[s:e] ** 2).mean()) > 1e-4 * float((mix[s:e] ** 2).mean() + 1e-12)
        loud = max(seg, key=seg.get)
        for c in DOMINANCE_CLASSES:
            k = CLASSES.index(c)
            if vocal or c != loud:
                rear[s:e] += stems[s:e, k]
            else:
                front[s:e] += stems[s:e, k]
    return front, rear


def irm(S_target, S_mix):
    return np.clip(np.abs(S_target) / (np.abs(S_mix) + 1e-9), 0, 1)


# ----------------------------------------------------------------------------- render

def render(mix, rear_time, alpha=0.5, rear_gain=0.5, delay_ms=15.0, hp_hz=120.0):
    sos = butter(2, hp_hz, 'highpass', fs=SR, output='sos')
    rear_f = sosfilt(sos, rear_time, axis=0).astype('f4')
    front = (mix - alpha * rear_f).astype('f4')
    d = int(SR * delay_ms / 1000)
    rear_d = np.concatenate((np.zeros((d, 2), 'f4'), rear_f[:-d] if d else rear_f), 0) * rear_gain
    quad = np.concatenate((front, rear_d), 1)
    preview = front + 0.8 * rear_d
    return front, rear_d, quad, preview


def db(x):
    return float(10 * np.log10(max(x, 1e-30)))


def write_set(d, tag, mix, rear_time, extra=None, **kw):
    front, rear_d, quad, preview = render(mix, rear_time, **kw)
    peak = max(np.abs(quad).max(), np.abs(preview).max(), 1e-9)
    g = float(min(1.0, 0.98 / peak))
    for name, a in (('front', front), ('rear', rear_d), ('quad', quad), ('preview', preview)):
        sf.write(d / f'{tag}-{name}.wav', a * g, SR, subtype='PCM_24')
    pm, pr = float((mix ** 2).mean()), float((rear_time ** 2).mean())
    stats = dict(rear_re_mix_db=db(pr / pm), moved_fraction=pr / pm, alpha=kw.get('alpha', 0.5),
                 rear_gain=kw.get('rear_gain', 0.5), delay_ms=kw.get('delay_ms', 15.0), norm_gain=g)
    if extra:
        stats.update(extra)
    return stats


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--cache', type=Path, default=None)
    ap.add_argument('--wav', type=Path, nargs='*', default=[])
    ap.add_argument('--alpha', type=float, default=0.5, help='fraction of rear removed from front')
    ap.add_argument('--rear-gain', type=float, default=0.5)
    ap.add_argument('--delay-ms', type=float, default=15.0)
    ap.add_argument('--seconds', type=float, default=None)
    ap.add_argument('songs', nargs='*')
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    kw = dict(alpha=args.alpha, rear_gain=args.rear_gain, delay_ms=args.delay_ms)
    report = dict(schema='q4-second-scene-listen-v1', params=kw, songs={})

    jobs = []
    if args.cache:
        from q3_cache import load_song
        manifest = {p.stem: json.load(open(p)) for p in args.cache.glob('*.json') if p.name != 'manifest.json'}
        for pre in args.songs:
            sid = next(s for s in manifest if s.startswith(pre))
            m = manifest[sid]
            jobs.append((f"{sid[:8]}-{m['artist']}-{m['title']}".replace(' ', '_').replace('/', '_'), ('cache', sid, m)))
    for w in args.wav:
        jobs.append((w.stem.replace(' ', '_')[:60], ('wav', w, None)))

    for name, (kind, src, meta) in jobs:
        t0 = time.time()
        d = args.out / name; d.mkdir(exist_ok=True)
        stems = None
        if kind == 'cache':
            a = load_song(args.cache, src)
            mix, stems = a[:, 0], a[:, 1:]
        else:
            mix = read_any(src)
        if args.seconds:
            mix = mix[:int(args.seconds * SR)]
            stems = stems[:int(args.seconds * SR)] if stems is not None else None
        n = len(mix)
        X = stft(mix)
        res = {}
        # --- variant 1: pure DSP on the mix
        m, feats = dsp_rear_mask(X)
        rear = istft(m[:, None, :] * X, n)
        res['dsp'] = write_set(d, 'dsp', mix, rear, dict(mask_mean=float(m.mean())), **kw)
        if stems is not None:
            present = np.array(meta['heads_present'])
            # --- variant 2: oracle roles from true stems
            front_t, rear_t = role_targets(mix, stems, present)
            res['roles'] = write_set(d, 'roles', mix, rear_t, **kw)
            # --- variant 3: DSP rear vetoed by true protection (lead+bass+drums IRM)
            prot_time = sum(stems[:, CLASSES.index(c)] for c in FRONT_CLASSES)
            Sp_, Sr_ = stft(prot_time), stft(mix - prot_time)
            prot_mask = ((np.abs(Sp_) ** 2) / (np.abs(Sp_) ** 2 + np.abs(Sr_) ** 2 + 1e-12)).mean(1)   # Wiener energy share [T,513]
            m2, _ = dsp_rear_mask(X, prot=prot_mask)
            rear2 = istft(m2[:, None, :] * X, n)
            # exact leakage for mask variants: rear = m*X = m*(S_prot + S_rest) -> leaked = m*S_prot
            Sp = stft(prot_time); pp = float((np.abs(Sp) ** 2).sum()) + 1e-30
            def leak(mask):
                return db(float((np.abs(mask[:, None, :] * Sp) ** 2).sum()) / pp)
            res['dsp+prot'] = write_set(d, 'dsp+prot', mix, rear2, dict(mask_mean=float(m2.mean()), protected_leak_db=leak(m2)), **kw)
            # --- variant 4: everything not protected is a rear candidate; width/texture only shape the level
            coh, pan, tex = spatial_features(X)
            m3 = (1 - prot_mask) * (0.4 + 0.3 * tex + 0.3 * np.clip(np.abs(pan) / 0.35, 0, 1))
            m3[:, np.arange(X.shape[-1]) * SR / 1024 < 120.0] = 0
            m3 = np.clip(ema(m3, 6), 0, 1)
            rear3 = istft(m3[:, None, :] * X, n)
            res['prot-only'] = write_set(d, 'prot-only', mix, rear3, dict(mask_mean=float(m3.mean()), protected_leak_db=leak(m3)), **kw)
            res['dsp']['protected_leak_db'] = leak(m)
            res['roles']['protected_leak_db'] = None   # zero by construction (true stems)
        sf.write(d / 'original.wav', mix, SR, subtype='PCM_24')
        report['songs'][name] = res
        print(f'{name} ({n/SR/60:.1f} min, {time.time()-t0:.0f}s): ' +
              '  '.join(f"{k}: rear {v['rear_re_mix_db']:+.1f} dB" + (f", leak {v['protected_leak_db']:+.1f} dB" if v.get('protected_leak_db') is not None else '') for k, v in res.items()), flush=True)
        (args.out / 'report.json').write_text(json.dumps(report, indent=1))
    print('done', args.out)


if __name__ == '__main__':
    main()
