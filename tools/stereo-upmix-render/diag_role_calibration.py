#!/usr/bin/env python3
"""Compare four-role head probabilities with true stem-power roles on fixed DEV excerpts."""

import argparse
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoint", type=Path)
    p.add_argument("--split", choices=("train", "dev", "pseudo"), default="dev")
    p.add_argument("--songs", type=int, default=12)
    p.add_argument("--teacher-cache", type=Path, help="four-stem teacher cache, required for --split pseudo")
    a = p.parse_args()
    torch, net, dev = SN.load(str(a.checkpoint))
    if net.mask_mode != "role_softmax":
        p.error("checkpoint is not a four-role model")
    if a.split == "pseudo":
        if a.teacher_cache is None:
            p.error("--split pseudo requires --teacher-cache")
        paths = sorted(a.teacher_cache.glob("*.npy"))[:a.songs]
    else:
        split = MN.load_split(str(a.cache))
        paths = [a.cache / (song + ".npy") for song in split[a.split][:a.songs]]
    print(f"split={a.split} songs={len(paths)}")
    rows = {role: [] for role in ("vocal", "drums", "bass", "eligible")}
    emitted = []
    aucs = []
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
            prediction = torch.softmax(net.role_logits, -1)
            power = torch.stack([(SN.stft(torch, s[None]).abs() ** 2).sum(-1)
                                 for s in sources], -1)
            total = power.sum(-1)
            target = power / total.clamp_min(1e-8)[..., None]
            audible = total > 1e-5 * total.mean()
            for index, role in enumerate(rows):
                confident = (target[..., index] >= 0.8) & audible
                if confident.any():
                    rows[role].append((float(prediction[..., index][confident].mean()),
                                       float(prediction[..., 3][confident].mean()),
                                       float(target[..., 3][confident].mean()),
                                       float(confident.float().mean())))
            positive = (target[..., 3] >= 0.8) & audible
            negative = (target[..., :3].max(-1).values >= 0.8) & audible
            if positive.any() and negative.any():
                pos = prediction[..., 3][positive].cpu().numpy()
                neg = np.sort(prediction[..., 3][negative].cpu().numpy())
                lo = np.searchsorted(neg, pos, side="left")
                hi = np.searchsorted(neg, pos, side="right")
                aucs.append(float(np.mean((lo + hi) / (2 * len(neg)))))
            emitted.append((float(((prediction[..., 3] > 0.8) & audible).float().mean()),
                            float(((prediction[..., 3] < 0.2) & audible).float().mean()),
                            float(((target[..., 3] > 0.8) & audible).float().mean()),
                            float(((target[..., 3] < 0.2) & audible).float().mean())))
    for role, values in rows.items():
        if values:
            med = np.median(values, axis=0)
            print(f"{role:8s} n_songs={len(values)} predicted-own={med[0]:.3f} "
                  f"predicted-eligible={med[1]:.3f} target-eligible={med[2]:.3f} "
                  f"cell-coverage={med[3]:.3f}")
    med = np.median(emitted, axis=0)
    print(f"eligible confident/suppressed cell coverage predicted={med[0]:.3f}/{med[1]:.3f} "
          f"target={med[2]:.3f}/{med[3]:.3f}")
    if aucs:
        print(f"eligible-vs-protected confident-cell AUC median={np.median(aucs):.3f} "
              f"range={min(aucs):.3f}..{max(aucs):.3f} songs={len(aucs)}")


if __name__ == "__main__":
    main()
