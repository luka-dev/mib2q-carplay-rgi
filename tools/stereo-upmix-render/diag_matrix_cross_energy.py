#!/usr/bin/env python3
"""Measure the frozen matrix student's cross-channel output on fixed DEV excerpts."""

import argparse
import json
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoint", type=Path)
    p.add_argument("--songs", type=int, default=12)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch, net, dev = SN.load(str(a.checkpoint))
    if net.mask_mode != "matrix2":
        p.error("checkpoint must use matrix2 output")
    split = MN.load_split(str(a.cache))
    power = lambda Z: float((Z.abs() ** 2).sum())
    rows = []
    with torch.no_grad():
        for song in split["dev"][:a.songs]:
            path = a.cache / (song + ".npy")
            N = MN.npy_header(str(path))[0][2]
            n = min(20 * SN.SR, N)
            start = MN.dev_excerpt(str(path), n) if N > n else 0
            w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768,
                                device=dev)
            X = SN.stft(torch, w.sum(0)[None])
            E = SN.stft(torch, w[6][None])
            full = net.separate_stft(X)
            diagonal, cross = net.last_matrix
            diagonal_only = diagonal * X
            cross_output = full - diagonal_only
            target_power = power(E)
            full_error = 10 * np.log10(power(full - E) / max(target_power, 1e-20))
            diagonal_error = 10 * np.log10(power(diagonal_only - E) / max(target_power, 1e-20))
            rows.append({"song": song,
                         "cross_output_energy_fraction": power(cross_output) / max(power(full), 1e-20),
                         "full_error_db": float(full_error), "zero_cross_error_db": float(diagonal_error),
                         "full_minus_zero_cross_error_db": float(full_error - diagonal_error)})
            print(f"{song[:8]} cross-energy/full {rows[-1]['cross_output_energy_fraction']:.3f} "
                  f"error full-zero {rows[-1]['full_minus_zero_cross_error_db']:+.3f} dB", flush=True)
    median = {key: float(np.median([row[key] for row in rows])) for key in (
        "cross_output_energy_fraction", "full_minus_zero_cross_error_db")}
    out = {"scope": "same 12 inspected 20-s DEV excerpts; frozen matrix checkpoint; no product promotion",
           "checkpoint": str(a.checkpoint), "rows": rows, "median": median,
           "cross_terms_material_at_five_percent": median["cross_output_energy_fraction"] >= .05,
           "full_lower_error_songs": sum(row["full_minus_zero_cross_error_db"] < 0 for row in rows)}
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(f"median cross-energy/full {median['cross_output_energy_fraction']:.3f}; "
          f"median full-zero error {median['full_minus_zero_cross_error_db']:+.3f} dB; "
          f"full lower error {out['full_lower_error_songs']}/{len(rows)}; "
          f"material={out['cross_terms_material_at_five_percent']}", flush=True)


if __name__ == "__main__":
    main()
