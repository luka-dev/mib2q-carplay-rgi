#!/usr/bin/env python3
"""Locate source-loss matrix changes in pure and mixed time-frequency cells.

This is a diagnostic on the same 12 inspected DEV excerpts as
diag_role_selectivity.py, not an independent validation or a promotion gate.
The full operator is inferred once from the mixture and then applied to each
aligned true source. Cell classes use *input* source power and remain fixed
between checkpoints. Raw STFT energy preserves the cell partition; unlike the
renderer-facing source diagnostic, no iSTFT/STFT reanalysis is performed.
"""

import argparse
import json
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


NAMES = ("vocal", "drums", "bass", "eligible")
CLASSES = ("pure_eligible", "mixed", "pure_protected")
DETAIL_CLASSES = CLASSES + ("pure_vocal", "pure_drums", "pure_bass")


def finite_median(values):
    v = [x for x in values if x is not None and np.isfinite(x)]
    return {"n": len(v), "value": float(np.median(v)) if v else None}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("control", type=Path)
    p.add_argument("candidate", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch = SN._torch()
    nets = {}
    stats = {}
    for name, ckpt in (("control", a.control), ("candidate", a.candidate)):
        _, net, dev = SN.load(str(ckpt))
        if net.mask_mode != "matrix2" or dev != "cpu":
            raise ValueError("requires two matrix2 checkpoints and CPU inference")
        nets[name] = net
        stats[name] = dict(SN.STATS)
    split = MN.load_split(str(a.cache))
    rows = []
    for song in split["dev"][:12]:
        path = a.cache / (song + ".npy")
        total_samples = MN.npy_header(str(path))[0][2]
        n = min(20 * SN.SR, total_samples)
        start = MN.dev_excerpt(str(path), n) if total_samples > n else 0
        w = torch.as_tensor(
            MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768
        )
        with torch.no_grad():
            S = {
                "vocal": SN.stft(torch, (w[0] + w[1])[None]),
                "drums": SN.stft(torch, (w[2] + w[3] + w[4])[None]),
                "bass": SN.stft(torch, w[5][None]),
                "eligible": SN.stft(torch, w[6][None]),
            }
            X = SN.stft(torch, w.sum(0)[None])
            power = torch.stack([(S[name].abs() ** 2).sum(-1) for name in NAMES], -1)
            total = power.sum(-1)
            fraction = power / total.clamp_min(1e-8)[..., None]
            audible = total > 1e-5 * total.mean()
            pure_e = audible & (fraction[..., 3] >= 0.8)
            pure_p = audible & (fraction[..., :3].max(-1).values >= 0.8)
            masks = {"pure_eligible": pure_e, "mixed": audible & ~(pure_e | pure_p),
                     "pure_protected": pure_p,
                     "pure_vocal": audible & (fraction[..., 0] >= 0.8),
                     "pure_drums": audible & (fraction[..., 1] >= 0.8),
                     "pure_bass": audible & (fraction[..., 2] >= 0.8)}
            row = {"song": song, "input_energy_fraction": {}, "transmission": {},
                   "candidate_minus_control_output_per_input": {}, "positive_gain_share": {}}
            for source in NAMES:
                source_power = power[..., NAMES.index(source)]
                den = float(source_power[audible].sum())
                row["input_energy_fraction"][source] = {
                    cls: float(source_power[mask].sum()) / den if den > 0 else None
                    for cls, mask in masks.items()
                }
            outputs = {}
            for label, net in nets.items():
                SN.STATS.update(stats[label])
                net.separate_stft(X)
                outputs[label] = {}
                row["transmission"][label] = {}
                for source in NAMES:
                    sent_power = (net.apply_last_filter(S[source]).abs() ** 2).sum(-1)
                    input_power = power[..., NAMES.index(source)]
                    source_total = float(input_power[audible].sum())
                    outputs[label][source] = {}
                    row["transmission"][label][source] = {}
                    for cls, mask in masks.items():
                        inp = float(input_power[mask].sum())
                        out = float(sent_power[mask].sum())
                        outputs[label][source][cls] = out
                        row["transmission"][label][source][cls] = (
                            out / inp if inp > 1e-6 * source_total else None
                        )
            for source in NAMES:
                source_total = float(power[..., NAMES.index(source)][audible].sum())
                deltas = {
                    cls: ((outputs["candidate"][source][cls] - outputs["control"][source][cls])
                          / source_total if source_total > 0 else None)
                    for cls in DETAIL_CLASSES
                }
                positive = sum(max(deltas[cls], 0) for cls in CLASSES if deltas[cls] is not None)
                row["candidate_minus_control_output_per_input"][source] = deltas
                row["positive_gain_share"][source] = {
                    cls: max(deltas[cls], 0) / positive if positive > 0 else None
                    for cls in CLASSES
                }
        rows.append(row)
        print(song[:8] + " vocal positive-gain mixed share " +
              (f"{row['positive_gain_share']['vocal']['mixed']:.3f}"
               if row["positive_gain_share"]["vocal"]["mixed"] is not None else "n/a"), flush=True)
    summary = {"input_energy_fraction": {}, "transmission": {},
               "candidate_minus_control_output_per_input": {}, "positive_gain_share": {}}
    for source in NAMES:
        for key in ("input_energy_fraction", "candidate_minus_control_output_per_input", "positive_gain_share"):
            summary[key][source] = {
                cls: finite_median([row[key][source][cls] for row in rows])
                for cls in (CLASSES if key == "positive_gain_share" else DETAIL_CLASSES)
            }
        summary["transmission"][source] = {
            label: {cls: finite_median([row["transmission"][label][source][cls] for row in rows])
                    for cls in DETAIL_CLASSES}
            for label in nets
        }
    shares = [row["positive_gain_share"]["vocal"]["mixed"] for row in rows]
    summary["vocal_mixed_majority_songs"] = sum(v is not None and v > 0.5 for v in shares)
    summary["vocal_positive_gain_covered_songs"] = sum(v is not None for v in shares)
    summary["vocal_pure_protected_majority_songs"] = sum(
        (row["positive_gain_share"]["vocal"]["pure_protected"] or 0) > 0.5 for row in rows
    )
    print("vocal mixed-majority positive gain " +
          f"{summary['vocal_mixed_majority_songs']}/{summary['vocal_positive_gain_covered_songs']}", flush=True)
    for source, cls in (("vocal", "mixed"), ("vocal", "pure_protected"),
                        ("vocal", "pure_vocal"), ("vocal", "pure_drums"),
                        ("vocal", "pure_bass"),
                        ("eligible", "mixed"), ("eligible", "pure_eligible")):
        val = summary["candidate_minus_control_output_per_input"][source][cls]
        print(f"{source} {cls} output gain/input median {val['value']:.4f} (n={val['n']})", flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({
        "scope": "first 12 already-inspected DEV excerpts; fixed input cell classes; raw STFT operator energy",
        "checkpoints": {"control": str(a.control), "candidate": str(a.candidate)},
        "rows": rows, "summary": summary,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
