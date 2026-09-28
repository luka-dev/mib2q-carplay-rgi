#!/usr/bin/env python3
"""Calibrate confident-cell equal-role CE against the frozen role training objective."""

import argparse
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN

ROLE_CE_WEIGHT = 0.279254


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
    net = SN.BandSepNet(torch, norm="stream", mask="role_softmax")
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

        def baseline():
            est = net.separate_stft(X)
            return (SN.loss_audio(torch, est, E) + SN.multires_loss(
                torch, SN.istft(torch, est, mix.shape[-1]), elig)
                + ROLE_CE_WEIGHT * SN.loss_role_ce(torch, net.role_logits, E, V, D, B))

        def focus():
            net.separate_stft(X)
            return SN.loss_role_focus(torch, net.role_logits, E, V, D, B)

        base = grad_norm(torch, net, baseline)
        extra = grad_norm(torch, net, focus)
        if not np.isfinite(base) or not np.isfinite(extra) or extra <= 0:
            raise ValueError(f"invalid gradients on batch {index}: {base}, {extra}")
        ratios.append(base / extra)
        print(f"batch {index}: baseline {base:.6g}, focus@1 {extra:.6g}, baseline/focus {ratios[-1]:.6g}", flush=True)
    print(f"role_focus weight for 30% median parameter-gradient L2: {0.30 * np.median(ratios):.6g}", flush=True)


if __name__ == "__main__":
    main()
