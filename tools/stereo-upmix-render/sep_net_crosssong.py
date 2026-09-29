#!/usr/bin/env python3
"""Review 43: train the review-37 fine-role net with cross-song true-stem mixtures.

Only the MoisesDB TRAIN pool is remixed. The teacher pseudo-label pool retains
the original same-song sampling, and validation still uses unmodified songs.
"""

from functools import lru_cache

import numpy as np

import mask_net as MN
import sep_net as SN
import sep_net_fine_role as FR


FAMILIES = ((0, 1), (2, 3, 4), (5,), (6,))
_original_windows = SN.windows


@lru_cache(maxsize=None)
def _length(path):
    return MN.npy_header(path)[0][2]


def crosssong_truth_window(pool, n, rng):
    """One seven-slot sample from four distinct TRAIN songs and offsets."""
    available = [path for path in pool if _length(path) > n]
    if len(available) < len(FAMILIES):
        raise ValueError("cross-song training needs at least four truth songs")
    w = np.zeros((7, 2, n), dtype=np.float32)
    choices = rng.choice(len(available), size=len(FAMILIES), replace=False)
    for indices, chosen in zip(FAMILIES, choices):
        path = available[chosen]
        length = _length(path)
        start = int(rng.integers(0, length - n))
        piece = MN.read_window(path, start, n, slots=indices).astype(np.float32) / 32768
        w[list(indices)] = piece
    gains = (10 ** (rng.uniform(-6, 6, size=(7, 1, 1)) / 20)).astype(np.float32)
    return w * gains


def windows(torch, tracks, seconds, batch, rng, merge_none=None,
            with_vocal=False, with_sources=False):
    if not isinstance(tracks, tuple) or len(tracks) != 2:
        raise ValueError("review-43 cross-song sampling requires truth/pseudo pools")
    truth, pseudo = tracks
    n = int(seconds * SN.SR)
    mixes, eligs, vocs, drums, basses = [], [], [], [], []
    for _ in range(batch):
        if rng.random() < SN.TRUTH_FRAC:
            w = crosssong_truth_window(truth, n, rng)
            mixes.append(w.sum(0)); eligs.append(w[6])
            if with_vocal or with_sources:
                vocs.append(w[0] + w[1])
            if with_sources:
                drums.append(w[2] + w[3] + w[4]); basses.append(w[5])
        else:
            original = _original_windows(torch, pseudo, seconds, 1, rng,
                                         with_vocal=with_vocal, with_sources=with_sources)
            mixes.append(original[0][0].numpy()); eligs.append(original[1][0].numpy())
            if with_vocal or with_sources:
                vocs.append(original[2][0].numpy())
            if with_sources:
                drums.append(original[3][0].numpy()); basses.append(original[4][0].numpy())
    out = (torch.as_tensor(np.stack(mixes)), torch.as_tensor(np.stack(eligs)))
    if with_sources:
        return out + tuple(torch.as_tensor(np.stack(part)) for part in (vocs, drums, basses))
    return out + (torch.as_tensor(np.stack(vocs)),) if with_vocal else out


def main():
    SN.windows = windows
    try:
        FR.main()
    finally:
        SN.windows = _original_windows


if __name__ == "__main__":
    main()
