#!/usr/bin/env python3
"""Hard-threshold upper bound for a frozen role score on fixed inspected DEV excerpts.

This deliberately discontinuous mask tests ranking, not listening quality or
streaming deployment. True stems only score the output; they do not set the
threshold for each song. The global thresholds below are exploratory.
"""

import argparse
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN

THRESHOLDS = (0.25, 0.35, 0.45, 0.55, 0.65)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoint", type=Path)
    a = p.parse_args()
    torch, net, dev = SN.load(str(a.checkpoint))
    if net.mask_mode != "role_softmax":
        p.error("checkpoint is not a four-role model")
    split = MN.load_split(str(a.cache))
    paths = [a.cache / (song + ".npy") for song in split["dev"][:12]]
    rows = {threshold: [] for threshold in THRESHOLDS}
    for path in paths:
        N = MN.npy_header(str(path))[0][2]
        n = min(20 * SN.SR, N)
        start = MN.dev_excerpt(str(path), n) if N > n else 0
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768,
                            device=dev)
        sources = (w[0] + w[1], w[2] + w[3] + w[4], w[5], w[6])
        with torch.no_grad():
            X = SN.stft(torch, w.sum(0)[None])
            net.separate_stft(X)
            score = torch.softmax(net.role_logits, -1)[..., 3]
            spectra = [SN.stft(torch, s[None]) for s in sources]
            clean = [(SN.stft(torch, SN.istft(torch, S, n)).abs() ** 2).sum().clamp_min(1e-8)
                     for S in spectra]
            for threshold in THRESHOLDS:
                mask = (score >= threshold)[..., None]
                transmission = []
                for S, den in zip(spectra, clean):
                    sent = SN.stft(torch, SN.istft(torch, mask * S, n))
                    transmission.append(float((sent.abs() ** 2).sum() / den))
                rows[threshold].append(transmission)
    for threshold, values in rows.items():
        vals = np.array(values)
        ratio = vals[:, :3] / np.maximum(vals[:, 3:4], 1e-8)
        print(f"threshold={threshold:.2f} eligible={np.median(vals[:,3]):.4f} "
              f"vocal={np.median(vals[:,0]):.4f} drums={np.median(vals[:,1]):.4f} "
              f"bass={np.median(vals[:,2]):.4f} v/e={np.median(ratio[:,0]):.4f} "
              f"d/e={np.median(ratio[:,1]):.4f} b/e={np.median(ratio[:,2]):.4f}", flush=True)


if __name__ == "__main__":
    main()
