#!/usr/bin/env python3
"""Did paired training reduce eligible-output sensitivity to removing vocals?"""

import argparse
import json
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("control", type=Path)
    p.add_argument("candidate", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch = SN._torch()
    split = MN.load_split(str(a.cache))
    models = {}
    stats = {}
    for label, ckpt in (("control", a.control), ("candidate", a.candidate)):
        _, model, dev = SN.load(str(ckpt))
        if model.mask_mode != "matrix2" or dev != "cpu":
            raise ValueError("requires matrix2 checkpoints on CPU")
        models[label] = model
        stats[label] = dict(SN.STATS)
    rows = []
    for song in split["dev"][:12]:
        path = a.cache / (song + ".npy")
        N = MN.npy_header(str(path))[0][2]
        n = min(20 * SN.SR, N)
        start = MN.dev_excerpt(str(path), n) if N > n else 0
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768)
        mix = w.sum(0)[None]
        vocal = (w[0] + w[1])[None]
        eligible = w[6][None]
        with torch.no_grad():
            X = SN.stft(torch, mix)
            quiet = SN.stft(torch, mix - vocal)
            E = SN.stft(torch, eligible)
            den = float((E.abs() ** 2).sum())
            floor = max(1e-6, (3e-3) ** 2 * float((X.abs() ** 2).sum()))
            row = {"song": song, "sensitivity": {}}
            for label, net in models.items():
                SN.STATS.update(stats[label])
                full = net.separate_stft(X)
                no_vocal = net.separate_stft(quiet)
                row["sensitivity"][label] = float(
                    ((full - no_vocal).abs() ** 2).sum() / (den + floor))
        row["candidate_over_control"] = (
            row["sensitivity"]["candidate"] / row["sensitivity"]["control"]
            if row["sensitivity"]["control"] > 0 else None)
        rows.append(row)
    ratios = [r["candidate_over_control"] for r in rows if r["candidate_over_control"] is not None]
    summary = {"n": len(ratios), "median_candidate_over_control": float(np.median(ratios)),
               "candidate_lower_songs": sum(v < 1 for v in ratios)}
    print(f"counterfactual sensitivity candidate/control median "
          f"{summary['median_candidate_over_control']:.4f}; lower "
          f"{summary['candidate_lower_songs']}/{summary['n']}", flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({
        "scope": "same 12 inspected DEV excerpts, source-conditioned counterfactual diagnostic only",
        "checkpoints": {"control": str(a.control), "candidate": str(a.candidate)},
        "rows": rows, "summary": summary,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
