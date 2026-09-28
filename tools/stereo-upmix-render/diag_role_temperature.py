#!/usr/bin/env python3
"""Exploratory role-logit temperature curve on the fixed 12 inspected DEV excerpts.

One network forward pass per song; reanalyzed source transmissions are reported
for a fixed temperature grid. This is an operating-point diagnostic, not an
independent validation or a product-level model selection.
"""

import argparse
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN

TEMPERATURES = (1.0, 0.75, 0.5, 0.33)


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
    rows = {t: [] for t in TEMPERATURES}
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
            logits = net.role_logits
            spectra = [SN.stft(torch, s[None]) for s in sources]
            clean_power = [(SN.stft(torch, SN.istft(torch, S, n)).abs() ** 2).sum().clamp_min(1e-8)
                           for S in spectra]
            for temp in TEMPERATURES:
                mask = torch.softmax(logits / temp, -1)[..., 3, None]
                transmission = []
                for S, denominator in zip(spectra, clean_power):
                    sent = SN.stft(torch, SN.istft(torch, mask * S, n))
                    transmission.append(float((sent.abs() ** 2).sum() / denominator))
                rows[temp].append(transmission)
    for temp, values in rows.items():
        vals = np.array(values)
        ratio = vals[:, :3] / np.maximum(vals[:, 3:4], 1e-8)
        print(f"temp={temp:.2f} eligible={np.median(vals[:,3]):.4f} vocal={np.median(vals[:,0]):.4f} "
              f"drums={np.median(vals[:,1]):.4f} bass={np.median(vals[:,2]):.4f} "
              f"v/e={np.median(ratio[:,0]):.4f} d/e={np.median(ratio[:,1]):.4f} "
              f"b/e={np.median(ratio[:,2]):.4f}", flush=True)


if __name__ == "__main__":
    main()
