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
    a = p.parse_args()
    torch, net, dev = SN.load(str(a.checkpoint))
    if net.mask_mode != "role_softmax":
        p.error("checkpoint is not a four-role model")
    split = MN.load_split(str(a.cache))
    paths = [a.cache / (song + ".npy") for song in split["dev"][:12]]
    rows = {role: [] for role in ("vocal", "drums", "bass", "eligible")}
    emitted = []
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


if __name__ == "__main__":
    main()
