#!/usr/bin/env python3
"""Ground-truth vocal/drum/bass cue upper bound for the mask-driven C renderer.

This uses MoisesDB stems, so it is non-deployable. The rear still carries the
original stereo mixture; separate protected-source powers determine a fast
per-cell gain. It tests whether better cheap protection cues would suffice
before building estimators for them. No GPU or neural training is involved.
"""
import argparse
import json
from pathlib import Path
import re

import numpy as np
from scipy.signal import stft

import upmix_stem_tests as harness
from persistent_rear_membership import write_mask

SR, N_FFT, HOP, CONTROL = 48000, 512, 128, 4
WIN = np.sin(np.pi * (np.arange(N_FFT) + 0.5) / N_FFT)
# scipy.signal.stft divides by sum(window); mask_net's torch.stft does not.
# The energy ratio cancels that scale, but the absolute denominator floor must
# be scaled too or quiet/high-frequency cells become spuriously suppressed.
POWER_FLOOR = 1e-9 / float(WIN.sum() ** 2)


def stem_power(x):
    """The same center=False half-sine STFT geometry as mask_net.stft."""
    _, _, z = stft(x, fs=SR, window=WIN, nperseg=N_FFT,
                   noverlap=N_FFT - HOP, boundary=None, padded=False, axis=-1)
    return np.sum(np.abs(z) ** 2, axis=0).T.astype(np.float32)


def one_song(track, out, args):
    meta, stems = harness.load_song(track, 60.0)
    if len(stems) < 2:
        return None
    n = min(x.shape[1] for x in stems.values())
    mix = sum(x[:, :n] for x in stems.values())
    k = 0.7 / (np.abs(mix).max() + 1e-9)
    grouped = {}
    for name, audio in stems.items():
        group = name if name in ("vocals", "drums", "bass") else "eligible"
        p = stem_power(audio[:, :n] * k)
        grouped[group] = grouped.get(group, 0) + p
    shape = next(iter(grouped.values())).shape
    zero = np.zeros(shape, np.float32)
    e = grouped.get("eligible", zero)
    v = grouped.get("vocals", zero)
    d = grouped.get("drums", zero)
    b = grouped.get("bass", zero)
    ratio = e / (e + args.vocal_weight * v + args.drum_weight * d + args.bass_weight * b + POWER_FLOOR)
    length = (len(ratio) // CONTROL) * CONTROL
    ratio = ratio[:length].reshape(-1, CONTROL, N_FFT // 2 + 1).mean(axis=1)
    amplitude = np.clip(ratio, 0, 1) ** args.gamma
    tag = re.sub(r'[\\/:*?"<>|]', "_", f"{meta.get('artist', '?')} - {meta.get('song', '?')}")[:60]
    write_mask(out / (tag + ".msk"), 1 - amplitude)
    return dict(song=tag, controls=len(amplitude), mean_amplitude=float(amplitude.mean()),
                mean_eligible_ratio=float(ratio.mean()))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--split-file", type=Path, required=True)
    p.add_argument("--split", default="dev")
    p.add_argument("--songs", type=int, default=20)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--vocal-weight", type=float, default=1.0)
    p.add_argument("--drum-weight", type=float, default=1.0)
    p.add_argument("--bass-weight", type=float, default=1.0)
    p.add_argument("--gamma", type=float, default=0.5)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    if min(a.vocal_weight, a.drum_weight, a.bass_weight, a.gamma) <= 0:
        p.error("weights and gamma must be positive")
    a.out.mkdir(parents=True, exist_ok=True)
    split = json.loads(a.split_file.read_text())["split"]
    tracks = sorted(t for t in a.moisesdb.iterdir() if t.is_dir() and split.get(t.name) == a.split)
    rng = np.random.default_rng(a.seed)
    picked = [tracks[i] for i in rng.permutation(len(tracks))[:a.songs]]
    rows = []
    for track in picked:
        row = one_song(track, a.out, a)
        if row:
            rows.append(row)
            print(f"{len(rows)}/{len(picked)} {row['song']}", flush=True)
    if len(rows) != len(picked):
        raise ValueError(f"only {len(rows)} of {len(picked)} songs produced a mask")
    (a.out / "report.json").write_text(json.dumps(dict(config=vars(a), songs=rows), indent=2, default=str))


if __name__ == "__main__":
    main()
