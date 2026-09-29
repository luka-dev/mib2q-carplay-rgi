#!/usr/bin/env python3
"""Calibrate anchored vocal-entry reconstruction using training batches only."""

import argparse
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


SOURCE_WEIGHT = 0.0734755788
TARGET_GRADIENT_FRACTION = 0.30


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
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(1)
    rng = np.random.default_rng(1)
    split = MN.load_split(str(a.cache))
    truth = [str(a.cache / (song + ".npy")) for song in split["train"]]
    pseudo = [str(f) for f in sorted(a.teacher_cache.glob("*.npy"))
              if f.with_suffix(".json").exists()]
    net = SN.BandSepNet(torch, feat=36, h1=24, h2=24, hg=24,
                        dec_skip=True, norm="stream", mask="matrix2").to(dev)
    batches = [SN.windows(torch, (truth, pseudo), 8.0, 4, rng, with_sources=True)
               for _ in range(4)]
    with torch.no_grad():
        lm = torch.cat([torch.log(SN.stft(torch, b[0].to(dev)).abs() + 1e-5).flatten()
                        for b in batches])
        SN.STATS["mean"], SN.STATS["std"] = float(lm.mean()), float(lm.std() + 1e-3)
    ratios = []
    for index, batch in enumerate(batches, 1):
        mix, elig, voc, drums, bass = (part.to(dev) for part in batch)
        X, E = SN.stft(torch, mix), SN.stft(torch, elig)
        V, D, B = (SN.stft(torch, source) for source in (voc, drums, bass))

        def base_loss():
            est = net.separate_stft(X)
            source = SN.loss_source_attr_operator(torch, net, E, V, D, B)
            return SN.loss_audio(torch, est, E) + SOURCE_WEIGHT * source + SN.multires_loss(
                torch, SN.istft(torch, est, mix.shape[-1]), elig)

        def entry_loss():
            entry_mix = SN.vocal_entry_counterfactual(torch, mix, voc)
            entry_est = net.separate_stft(SN.stft(torch, entry_mix))
            return SN.loss_vocal_entry_target(torch, entry_est, entry_mix, elig)

        base = grad_norm(torch, net, base_loss)
        entry = grad_norm(torch, net, entry_loss)
        if not np.isfinite(base) or not np.isfinite(entry) or entry <= 0:
            raise ValueError(f"invalid gradients on batch {index}: {base}, {entry}")
        ratios.append(base / entry)
        print(f"batch {index}: base+source {base:.6g}, entry@1 {entry:.6g}, "
              f"base/entry {ratios[-1]:.6g}", flush=True)
    print(f"vocal_entry_target weight for {TARGET_GRADIENT_FRACTION:.0%} median "
          f"parameter-gradient L2: {TARGET_GRADIENT_FRACTION * np.median(ratios):.9g}", flush=True)


if __name__ == "__main__":
    main()
