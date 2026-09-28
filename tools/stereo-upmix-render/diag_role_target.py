#!/usr/bin/env python3
"""Check whether the four-role power target is a useful shared amplitude mask.

Uses true aligned stems on the same 12 deterministic DEV excerpts as diag_gap.
All masks are privileged noncausal bounds, not deployable estimators.
"""

import argparse
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    a = p.parse_args()
    torch = SN._torch()
    split = MN.load_split(str(a.cache))
    paths = [a.cache / (song + ".npy") for song in split["dev"][:12]]
    rows = {name: [] for name in ("power", "sqrt_power", "irm")}
    for path in paths:
        N = MN.npy_header(str(path))[0][2]
        n = min(20 * SN.SR, N)
        start = MN.dev_excerpt(str(path), n) if N > n else 0
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768)
        sources = (w[0] + w[1], w[2] + w[3] + w[4], w[5], w[6])
        with torch.no_grad():
            spectra = [SN.stft(torch, s[None]) for s in sources]
            X = SN.stft(torch, w.sum(0)[None])
            power = torch.stack([(S.abs() ** 2).sum(-1) for S in spectra], -1)
            eligible_power = power[..., 3] / power.sum(-1).clamp_min(1e-8)
            masks = {
                "power": eligible_power[..., None].expand_as(X),
                "sqrt_power": eligible_power.sqrt()[..., None].expand_as(X),
                "irm": (spectra[3].abs() / X.abs().clamp_min(1e-7)).clamp(0, 1),
            }
            clean = [SN.stft(torch, SN.istft(torch, S, n)) for S in spectra]
            for name, mask in masks.items():
                transmission = []
                for S, C in zip(spectra, clean):
                    sent = SN.stft(torch, SN.istft(torch, mask * S, n))
                    transmission.append(float((sent.abs() ** 2).sum() / (C.abs() ** 2).sum().clamp_min(1e-8)))
                rows[name].append(transmission)
    for name, values in rows.items():
        vals = np.array(values)
        ratio = vals[:, :3] / np.maximum(vals[:, 3:4], 1e-8)
        print(f"{name:10s} eligible {np.median(vals[:,3]):.4f} vocal {np.median(vals[:,0]):.4f} "
              f"drums {np.median(vals[:,1]):.4f} bass {np.median(vals[:,2]):.4f} | "
              f"v/e {np.median(ratio[:,0]):.4f} d/e {np.median(ratio[:,1]):.4f} "
              f"b/e {np.median(ratio[:,2]):.4f}", flush=True)


if __name__ == "__main__":
    main()
