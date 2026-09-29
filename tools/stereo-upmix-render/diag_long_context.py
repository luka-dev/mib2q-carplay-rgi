#!/usr/bin/env python3
"""Paired continuous-vs-8s-reset inference on fixed DEV songs, without retraining.

The same mixture STFT and true sources are used in both paths. A reset path
restarts the recurrent and running-level state every 750 frames (8 seconds);
the continuous path holds them for the whole 60-second excerpt. The first
second after each reset is omitted from paired metrics, and only chunks after
16 seconds count. This is a training-context diagnostic, not a renderer score.
"""

import argparse
import json
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def evaluate_song(torch, net, X, E, V):
    chunk = int(8 * SN.FPS)
    warm = round(SN.FPS)
    totals = {key: 0.0 for key in (
        "eligible_den", "vocal_den", "continuous_error", "reset_error",
        "continuous_eligible", "reset_eligible", "continuous_vocal", "reset_vocal")}
    with torch.no_grad():
        whole = net.separate_stft(X)
        whole_e = net.apply_last_filter(E)
        whole_v = net.apply_last_filter(V)
        for index, start in enumerate(range(0, X.shape[1], chunk)):
            end = min(start + chunk, X.shape[1])
            if index < 2 or end - start <= warm:
                continue
            estimate = net.separate_stft(X[:, start:end])
            reset_e = net.apply_last_filter(E[:, start:end])
            reset_v = net.apply_last_filter(V[:, start:end])
            a, b = start + warm, end
            sl = slice(warm, end - start)
            power = lambda z: float((z.abs() ** 2).sum())
            totals["eligible_den"] += power(E[:, a:b])
            totals["vocal_den"] += power(V[:, a:b])
            totals["continuous_error"] += power(whole[:, a:b] - E[:, a:b])
            totals["reset_error"] += power(estimate[:, sl] - E[:, a:b])
            totals["continuous_eligible"] += power(whole_e[:, a:b])
            totals["reset_eligible"] += power(reset_e[:, sl])
            totals["continuous_vocal"] += power(whole_v[:, a:b])
            totals["reset_vocal"] += power(reset_v[:, sl])
    e, v = totals["eligible_den"], totals["vocal_den"]
    result = {"tail_seconds": round(sum(max(0, min(start + chunk, X.shape[1]) - start - warm)
                                        for index, start in enumerate(range(0, X.shape[1], chunk)) if index >= 2)
                                    / SN.FPS, 3)}
    for mode in ("continuous", "reset"):
        result[mode] = {
            "normalized_complex_error_db": float(10 * np.log10(totals[mode + "_error"] / max(e, 1e-20))),
            "eligible_transmission": totals[mode + "_eligible"] / max(e, 1e-20),
            "vocal_transmission": totals[mode + "_vocal"] / max(v, 1e-20),
        }
        result[mode]["vocal_over_eligible"] = (
            result[mode]["vocal_transmission"] / max(result[mode]["eligible_transmission"], 1e-20))
    result["continuous_minus_reset_error_db"] = (
        result["continuous"]["normalized_complex_error_db"] - result["reset"]["normalized_complex_error_db"])
    result["continuous_minus_reset_eligible"] = (
        result["continuous"]["eligible_transmission"] - result["reset"]["eligible_transmission"])
    result["continuous_minus_reset_vocal_ratio"] = (
        result["continuous"]["vocal_over_eligible"] - result["reset"]["vocal_over_eligible"])
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoints", type=Path, nargs="+")
    p.add_argument("--songs", type=int, default=6)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch = SN._torch()
    split = MN.load_split(str(a.cache))
    paths = [a.cache / (name + ".npy") for name in split["dev"][:a.songs]]
    results = {}
    for path in paths:
        n = min(MN.npy_header(str(path))[0][2], 60 * SN.SR)
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768)
        mix = w.sum(0)[None]
        E = SN.stft(torch, w[list(SN.ELIGIBLE_SLOTS)].sum(0)[None])
        V = SN.stft(torch, (w[0] + w[1])[None])
        X = SN.stft(torch, mix)
        del w, mix
        for ckpt in a.checkpoints:
            _, net, dev = SN.load(str(ckpt))
            if dev != "cpu":
                raise RuntimeError("Set CUDA_VISIBLE_DEVICES=-1; this diagnostic must not take the GPU tenant")
            label = ckpt.parent.name
            row = evaluate_song(torch, net, X, E, V)
            row["song"] = path.stem
            results.setdefault(label, {"checkpoint": str(ckpt), "rows": []})["rows"].append(row)
            print(f"{label} {path.stem[:8]} continuous-reset error {row['continuous_minus_reset_error_db']:+.3f} dB "
                  f"eligible {row['continuous_minus_reset_eligible']:+.3f} "
                  f"vocal/E {row['continuous_minus_reset_vocal_ratio']:+.3f}", flush=True)
            del net
        del X, E, V
    for label, result in results.items():
        rows = result["rows"]
        result["median"] = {key: float(np.median([row[key] for row in rows])) for key in (
            "continuous_minus_reset_error_db", "continuous_minus_reset_eligible",
            "continuous_minus_reset_vocal_ratio")}
        result["positive_error_songs"] = sum(row["continuous_minus_reset_error_db"] > 0 for row in rows)
        result["context_mismatch_gate"] = (
            result["median"]["continuous_minus_reset_error_db"] >= 0.5
            and result["positive_error_songs"] >= 4)
        print(f"{label}: median continuous-reset error "
              f"{result['median']['continuous_minus_reset_error_db']:+.3f} dB; "
              f"positive {result['positive_error_songs']}/{len(rows)}; "
              f"context-mismatch gate {result['context_mismatch_gate']}", flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({"scope": "first six inspected DEV songs; tail after 16 s; 1 s reset warmup omitted; "
                                       "paired STFT error, not renderer or independent validation",
                                  "results": results}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
