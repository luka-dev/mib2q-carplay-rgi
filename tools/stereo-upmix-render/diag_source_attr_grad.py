#!/usr/bin/env python3
"""Calibrate source-attribution weight at init for the causal shared-mask pair.

Uses four fixed batches from the existing truth/pseudo-label pools on CPU.
Prints the weight whose parameter-gradient L2 is 15% of the audio-loss bundle.
This only sets the controlled experiment's starting weight; it is not a
predictor of final C-render performance.
"""

import argparse
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def grad_norm(torch, net, objective):
    for p in net.parameters():
        p.grad = None
    objective().backward()
    return float(torch.sqrt(sum((p.grad ** 2).sum() for p in net.parameters() if p.grad is not None)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("teacher_cache", type=Path)
    a = p.parse_args()
    torch = SN._torch()
    torch.manual_seed(1)
    rng = np.random.default_rng(1)
    split = MN.load_split(str(a.cache))
    truth = [str(a.cache / (t + ".npy")) for t in split["train"]]
    pseudo = [str(f) for f in sorted(a.teacher_cache.glob("*.npy"))
              if f.with_suffix(".json").exists()]
    net = SN.BandSepNet(torch, norm="stream", mask="shared_sigmoid")
    batches = [SN.windows(torch, (truth, pseudo), 8.0, 4, rng, with_sources=True)
               for _ in range(4)]
    with torch.no_grad():
        lm = torch.cat([torch.log(SN.stft(torch, b[0]).abs() + 1e-5).flatten()
                        for b in batches])
        SN.STATS["mean"], SN.STATS["std"] = float(lm.mean()), float(lm.std() + 1e-3)
    ratios = []
    for index, (mix, elig, voc, drums, bass) in enumerate(batches, 1):
        X, E = SN.stft(torch, mix), SN.stft(torch, elig)
        V, D, B = [SN.stft(torch, s) for s in (voc, drums, bass)]

        def base_loss():
            est = net.separate_stft(X)
            return SN.loss_audio(torch, est, E) + SN.multires_loss(
                torch, SN.istft(torch, est, mix.shape[-1]), elig)

        def source_loss():
            net.separate_stft(X)
            return SN.loss_source_attr(torch, net.last_mask, E, V, D, B)

        base = grad_norm(torch, net, base_loss)
        source = grad_norm(torch, net, source_loss)
        if not np.isfinite(base) or not np.isfinite(source) or source <= 0:
            raise ValueError(f"invalid gradients on batch {index}: {base}, {source}")
        ratios.append(base / source)
        print(f"batch {index}: base {base:.6g}, source@1 {source:.6g}, base/source {ratios[-1]:.6g}", flush=True)
    print(f"source_attr weight for 15% median parameter-gradient L2: {0.15 * np.median(ratios):.6g}", flush=True)


if __name__ == "__main__":
    main()
