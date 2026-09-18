"""Exact in-mix leak: per-hop complex 2x2 rear transfer matrices captured on the MIX (build/capture_scene_transfers),
then applied to each true stem -> that stem's contribution to the rear (masks fixed by the mix; STFT-domain linearity).
Reports leak dB = contribution energy / stem energy, and the rear composition (share of rear energy per stem)."""
import sys, os, json, subprocess, numpy as np, soundfile as sf
from pathlib import Path
sys.path.insert(0, 'tools/stereo-upmix-render')
from q2_source_data import CLASSES
from q3_cache import load_song
FFT, HOP, BINS = 512, 128, 257
def stft_frames(x):                       # x [N] float -> frames [hops, 512], frame k covers [128k-512, 128k)
    hops = len(x) // HOP
    xp = np.concatenate((np.zeros(FFT, 'f4'), x.astype('f4')))
    return np.lib.stride_tricks.sliding_window_view(xp, FFT)[HOP:HOP * hops + 1:HOP][:hops]
def apply_transfers(stereo, T, awin, swin):
    hops = min(T.shape[0], len(stereo) // HOP)
    FL = np.fft.rfft(stft_frames(stereo[:, 0])[:hops] * awin, axis=1); FR = np.fft.rfft(stft_frames(stereo[:, 1])[:hops] * awin, axis=1)
    M = T[:hops]
    LL = M[:, :, 0] + 1j * M[:, :, 1]; LR = M[:, :, 2] + 1j * M[:, :, 3]; RL = M[:, :, 4] + 1j * M[:, :, 5]; RR = M[:, :, 6] + 1j * M[:, :, 7]
    outL = np.fft.irfft(LL * FL + LR * FR, n=FFT, axis=1) * swin; outR = np.fft.irfft(RL * FL + RR * FR, n=FFT, axis=1) * swin
    y = np.zeros((hops * HOP + FFT, 2), 'f8')
    for j in range(4):                    # OLA: frame k occupies [128k-512, 128k) -> index base 128k - 512 + 512(pad) = 128k
        y[j * HOP:j * HOP + hops * HOP, 0] += outL[:, j * HOP:(j + 1) * HOP].reshape(-1)
        y[j * HOP:j * HOP + hops * HOP, 1] += outR[:, j * HOP:(j + 1) * HOP].reshape(-1)
    return y[:hops * HOP]                 # sample t of y aligns with input sample t - 512 + ... (lag fitted below)
def db(x): return 10 * np.log10(max(x, 1e-30))
def capture(model, guard, s16, tr, rear):
    subprocess.run(['build/capture_scene_transfers', model, guard or '-', str(s16), str(tr), str(rear)], check=True)
    raw = np.fromfile(tr, dtype='f4'); awin, swin = raw[:FFT], raw[FFT:2 * FFT]
    T = raw[2 * FFT:].reshape(-1, BINS, 8); R = np.fromfile(rear, dtype='f4').reshape(-1, 2)
    return T, awin, swin, R
def fit_lag_gain(est, ref):               # find integer lag + scalar gain: ref ~= gain * est shifted
    n = min(len(est), len(ref)); e = est[:n, 0]; r = ref[:n, 0]
    best = None
    for lag in range(0, 1025, 1):
        a = e[:n - lag]; b = r[lag:n]
        g = float(a @ b / (a @ a + 1e-20)); err = float(((b - g * a) ** 2).sum() / (b @ b + 1e-20))
        if best is None or err < best[2]: best = (lag, g, err)
    return best
CACHE, OUT = Path(sys.argv[1]), Path(sys.argv[2]); OUT.mkdir(exist_ok=True)
CONFIGS = [c.split(':') for c in sys.argv[3].split(',')]   # name:model[:guard]
STEMS = ('lead_vocal', 'backing_vocal', 'bass', 'drums', 'high_percussion', 'keys', 'guitar', 'other_unknown')
report = {}
for js in sorted(CACHE.glob('*.json')):
    meta = json.load(open(js)); a = load_song(CACHE, js.stem); n = 48000 * 45
    mix, stems = a[:n, 0].astype('f8'), a[:n, 1:].astype('f8'); title = meta.get('title', '')[:14]
    (np.clip(mix, -1, 1) * 32767).astype('int16').tofile(OUT / 'mix.s16')
    print(f'\n=== {title}')
    for cfg in CONFIGS:
        name, model = cfg[0], cfg[1]; guard = cfg[2] if len(cfg) > 2 else None
        T, awin, swin, R = capture(model, guard, OUT / 'mix.s16', OUT / 'tr.f32', OUT / 'rear.f32')
        est_mix = apply_transfers(mix, T, awin, swin)
        lag, g, err = fit_lag_gain(est_mix, R.astype('f8'))
        rear_total = float((est_mix ** 2).sum())
        row = {}; parts = []
        for c in STEMS:
            k = CLASSES.index(c); s = stems[:, k]
            if float((s ** 2).sum()) < 1e-4 * float((mix ** 2).sum()): continue
            contrib = apply_transfers(s, T, awin, swin); ce = float((contrib ** 2).sum())
            row[c] = dict(leak_db=db(ce / float((s ** 2).sum())), rear_share=ce / rear_total)
            parts.append(f"{c[:6]} {row[c]['leak_db']:+5.1f}dB/{100 * row[c]['rear_share']:4.1f}%")
        print(f"{name:10s} [fit lag {lag} gain {g:.3f} err {err:.4f}]  " + '  '.join(parts), flush=True)
        report.setdefault(title, {})[name] = row
    (OUT / 'report.json').write_text(json.dumps(report, indent=1))
