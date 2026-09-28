#!/usr/bin/env python3
"""How much source energy lives in pure versus mixed cells on fixed DEV excerpts?"""

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
    rows = []
    for path in paths:
        N = MN.npy_header(str(path))[0][2]
        n = min(20 * SN.SR, N)
        start = MN.dev_excerpt(str(path), n) if N > n else 0
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768)
        sources = (w[0] + w[1], w[2] + w[3] + w[4], w[5], w[6])
        with torch.no_grad():
            power = torch.stack([(SN.stft(torch, s[None]).abs() ** 2).sum(-1)
                                 for s in sources], -1)
            total = power.sum(-1)
            fraction = power / total.clamp_min(1e-8)[..., None]
            audible = total > 1e-5 * total.mean()
            pure_e = (fraction[..., 3] >= 0.8) & audible
            pure_p = (fraction[..., :3].max(-1).values >= 0.8) & audible
            mixed = audible & ~(pure_e | pure_p)
            row = {}
            for index, name in enumerate(("vocal", "drums", "bass", "eligible")):
                source = power[..., index]
                den = source[audible].sum().clamp_min(1e-8)
                row[name] = [float(source[mask].sum() / den) for mask in (pure_e, mixed, pure_p)]
            rows.append(row)
    for name in ("eligible", "vocal", "drums", "bass"):
        values = np.array([row[name] for row in rows])
        med = np.median(values, axis=0)
        print(f"{name:8s} energy fraction in pure-eligible/mixed/pure-protected cells "
              f"{med[0]:.3f}/{med[1]:.3f}/{med[2]:.3f} (medians by song)")


if __name__ == "__main__":
    main()
