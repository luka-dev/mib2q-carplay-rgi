#!/usr/bin/env python3
"""Reanalyzed source transmission and gain-invariant selectivity on fixed DEV excerpts.

Use the same first 12 deterministic DEV excerpts as diag_gap.py. Each frozen
mask is applied to the four aligned true sources, then iSTFT/STFT reanalysis
matches the renderer harness's input path. Report per-song protected/eligible
transmission ratios and song-median summaries; no independent-test claim.
"""

import argparse
import json
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("anchor", type=Path)
    p.add_argument("candidate", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch = SN._torch()
    split = MN.load_split(str(a.cache))
    paths = [a.cache / (song + ".npy") for song in split["dev"][:12]]
    results = {}
    for label, ckpt in (("anchor", a.anchor), ("candidate", a.candidate)):
        _, net, dev = SN.load(str(ckpt))
        rows = []
        for path in paths:
            N = MN.npy_header(str(path))[0][2]
            n = min(20 * SN.SR, N)
            start = MN.dev_excerpt(str(path), n) if N > n else 0
            w = torch.as_tensor(
                MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768,
                device=dev,
            )
            sources = (w[0] + w[1], w[2] + w[3] + w[4], w[5], w[6])
            mix = w.sum(0)[None]
            with torch.no_grad():
                X = SN.stft(torch, mix)
                net.separate_stft(X)
                mask = net.last_mask
                energy = {}
                for name, source in zip(("vocal", "drums", "bass", "eligible"), sources):
                    S = SN.stft(torch, source[None])
                    clean = SN.stft(torch, SN.istft(torch, S, n))
                    sent = SN.stft(torch, SN.istft(torch, mask * S, n))
                    energy[name] = (float((sent.abs() ** 2).sum()), float((clean.abs() ** 2).sum()))
            mix_energy = sum(den for _, den in energy.values())
            transmission = {name: num / den if den > 1e-6 * mix_energy else None
                            for name, (num, den) in energy.items()}
            eligible = transmission["eligible"]
            ratio = {name: transmission[name] / eligible
                     if eligible and transmission[name] is not None else None
                     for name in ("vocal", "drums", "bass")}
            rows.append({"song": path.stem, "transmission": transmission,
                         "protected_over_eligible": ratio})
        median = {}
        for key in ("eligible", "vocal", "drums", "bass"):
            vals = [row["transmission"][key] for row in rows if row["transmission"][key] is not None]
            median[key] = {"n": len(vals), "value": float(np.median(vals)) if vals else None}
        for key in ("vocal", "drums", "bass"):
            vals = [row["protected_over_eligible"][key] for row in rows
                    if row["protected_over_eligible"][key] is not None]
            median[key + "_over_eligible"] = {"n": len(vals), "value": float(np.median(vals)) if vals else None}
        results[label] = {"checkpoint": str(ckpt), "estimated_neural_macs_per_second": net.macs * SN.FPS,
                          "rows": rows, "median": median}
        print(f"{label}: " + " ".join(f"{key}={value['value']:.4f} (n={value['n']})"
                                    for key, value in median.items() if value["value"] is not None), flush=True)
    gates = {}
    anchor, candidate = results["anchor"]["median"], results["candidate"]["median"]
    gates["eligible_recall_at_least_anchor_minus_point03"] = (
        candidate["eligible"]["value"] >= anchor["eligible"]["value"] - 0.03)
    for key in ("vocal", "drums", "bass"):
        ratio_key = key + "_over_eligible"
        gates[key + "_ratio_at_most_point8_anchor"] = (
            candidate[ratio_key]["value"] <= 0.8 * anchor[ratio_key]["value"])
    out = {"scope": "same 12 inspected DEV excerpts as diag_gap; gain-invariant source selectivity; not independent validation",
           "anchor": results["anchor"], "candidate": results["candidate"], "gates": gates}
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print("gates: " + ", ".join(f"{key}={value}" for key, value in gates.items()), flush=True)


if __name__ == "__main__":
    main()
