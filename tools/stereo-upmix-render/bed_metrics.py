#!/usr/bin/env python3
"""PCM6-only metrics for the scene-bed experiments (no amplifier model, seconds per track).

    python3 bed_metrics.py SOURCE.wav BASE.pcm6.wav VARIANT.pcm6.wav [VARIANT2 ...]

Per file, over the analysis bands of amp_music_report:
  rear_msc        magnitude-squared coherence Ls vs Rs (Welch, 4096-point); lower = more decorrelated rear
  attack_smear_db rear energy 10..40 ms after a dry onset (aligned to the rear's 12 ms scene delay)
                  relative to 0..10 ms, mean over onsets; lower = a hit arrives from the rear once
  rear_share_db   rear (Ls+Rs) / front (L+R) power, 300..3000 Hz: lead-leak proxy, must not rise
  front_mid_db    front L+R power 500..2000 Hz re BASE
  peak_dbfs       sample peak of the 6-channel file
Onsets: spectral-flux peaks of the dry L+R, 512/128 STFT, threshold = median + 3 MAD, 50 ms refractory.
Not a listening result.  ponytail: single-process numpy, ~2 s per 60 s track.
"""
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import coherence, stft, welch

SR = 48000
BANDS = [(60, 120), (120, 250), (250, 500), (500, 2000), (2000, 8000), (8000, 20000)]
LATENCY = 512 + 576  # dry-front convention of the renderer + the 12 ms scene rear delay (rear windows only)


def band_power(x, lo, hi):
    f, p = welch(x, SR, nperseg=8192, noverlap=4096, axis=0)
    sel = (f >= lo) & (f < hi)
    return float(np.sum(p[sel], axis=0).sum() * (f[1] - f[0]))


def onsets(mono):
    f, t, z = stft(mono, SR, nperseg=512, noverlap=384)
    mag = np.abs(z)
    flux = np.maximum(mag[:, 1:] - mag[:, :-1], 0).sum(axis=0)
    med = np.median(flux)
    mad = np.median(np.abs(flux - med)) + 1e-12
    hop = 128
    picks, last = [], -10 ** 9
    for i in range(1, len(flux) - 1):
        if flux[i] > med + 3 * mad and flux[i] >= flux[i - 1] and flux[i] >= flux[i + 1]:
            s = i * hop
            if s - last > int(0.05 * SR):
                picks.append(s)
                last = s
    return np.array(picks)


def attack_smear_db(rear, marks):
    early, late = int(0.010 * SR), int(0.040 * SR)
    vals = []
    for m in marks:
        m += LATENCY
        if m + late >= len(rear):
            continue
        e0 = np.sum(rear[m:m + early] ** 2) + 1e-12
        e1 = np.sum(rear[m + early:m + late] ** 2) / 3.0 + 1e-12  # per-10 ms window, comparable to e0
        vals.append(10 * np.log10(e1 / e0))
    return float(np.mean(vals)) if vals else None, len(vals)


def analyse(path, marks, base_front_mid=None):
    x, rate = sf.read(path, dtype="float64", always_2d=True)
    assert rate == SR and x.shape[1] == 6, path
    front = x[:, 0] + x[:, 1]
    rear = x[:, 4] + x[:, 5]
    f, msc = coherence(x[:, 4], x[:, 5], SR, nperseg=4096, noverlap=2048)
    out = {
        "file": str(path),
        "peak_dbfs": float(20 * np.log10(np.abs(x).max() + 1e-12)),
        "rear_msc": {f"{lo}-{hi}": float(np.mean(msc[(f >= lo) & (f < hi)])) for lo, hi in BANDS},
        "rear_share_db": float(10 * np.log10((band_power(rear, 300, 3000) + 1e-18) / (band_power(front, 300, 3000) + 1e-18))),
        "front_mid_power": band_power(front, 500, 2000),
        "rear_power_2k_8k": band_power(rear, 2000, 8000),
    }
    smear, n = attack_smear_db(rear, marks)
    out["attack_smear_db"] = smear
    out["onsets_used"] = n
    if base_front_mid is not None:
        out["front_mid_db_re_base"] = float(10 * np.log10((out["front_mid_power"] + 1e-18) / (base_front_mid + 1e-18)))
    return out


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    src, rate = sf.read(sys.argv[1], dtype="float64", always_2d=True)
    assert rate == SR and src.shape[1] == 2
    marks = onsets(src[:, 0] + src[:, 1])
    base = analyse(Path(sys.argv[2]), marks)
    base["front_mid_db_re_base"] = 0.0
    rows = [base] + [analyse(Path(p), marks, base["front_mid_power"]) for p in sys.argv[3:]]
    print(json.dumps({"onsets": int(len(marks)), "rows": rows}, indent=1))


if __name__ == "__main__":
    main()
