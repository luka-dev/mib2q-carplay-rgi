#!/usr/bin/env python3
"""Review-37 matched fine/coarse role quality, source and entry gate."""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_dual_state as DualDiag
import mask_net as MN
import sep_net_fine_role as SN


def median(values):
    good = [value for value in values if value is not None and np.isfinite(value)]
    return {"n": len(good), "value": float(np.median(good)) if good else None}


def direct_role_fraction(torch, net, X, output):
    delta = net.role_map(net.role_logits.softmax(-1) - .25)
    diagonal = torch.complex(delta[..., 0:4:2], delta[..., 1:4:2])
    cross = torch.complex(delta[..., 4:8:2], delta[..., 5:8:2])
    left = diagonal[..., 0] * X[..., 0] + cross[..., 1] * X[..., 1]
    right = cross[..., 0] * X[..., 0] + diagonal[..., 1] * X[..., 1]
    sent = torch.stack((left, right), -1)
    return float((sent.abs() ** 2).sum().sqrt() /
                 (output.abs() ** 2).sum().sqrt().clamp_min(1e-12))


def quality(cache, checkpoints, out):
    split = MN.load_split(str(cache))
    rows = {}
    summaries = {}
    for mode, checkpoint in checkpoints.items():
        torch, net, dev = SN.load(str(checkpoint))
        mode_rows = []
        with torch.no_grad():
            for song in split["dev"][:12]:
                path = cache / (song + ".npy")
                N = MN.npy_header(str(path))[0][2]
                n = min(20 * SN.SR, N)
                start = MN.dev_excerpt(str(path), n) if N > n else 0
                w = torch.as_tensor(
                    MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768,
                    device=dev)
                source = (w[0] + w[1], w[2] + w[3] + w[4], w[5], w[6])
                X = SN.stft(torch, w.sum(0)[None])
                output = net.separate_stft(X)
                prediction = net.role_logits.softmax(-1)
                power = torch.stack([(SN.stft(torch, s[None]).abs() ** 2).sum(-1)
                                     for s in source], -1)
                total = power.sum(-1)
                target = power / total.clamp_min(1e-8)[..., None]
                audible = total > 1e-5 * total.mean()
                positive = (target[..., 3] >= .8) & audible
                negative = (target[..., :3].max(-1).values >= .8) & audible
                auc = None
                if bool(positive.any()) and bool(negative.any()):
                    pos = prediction[..., 3][positive].cpu().numpy()
                    neg = np.sort(prediction[..., 3][negative].cpu().numpy())
                    lo = np.searchsorted(neg, pos, side="left")
                    hi = np.searchsorted(neg, pos, side="right")
                    auc = float(np.mean((lo + hi) / (2 * len(neg))))
                mixed = ((target[..., 3] >= .2) & (target[..., 3] <= .8) &
                         (target[..., 0] >= .2) & (target[..., 0] <= .8) & audible)
                corr = None
                if int(mixed.sum()) >= 100:
                    truth = target[..., 3][mixed].cpu().numpy()
                    pred = prediction[..., 3][mixed].cpu().numpy()
                    if np.std(truth) > 1e-8 and np.std(pred) > 1e-8:
                        corr = float(np.corrcoef(truth, pred)[0, 1])
                role_fraction = direct_role_fraction(torch, net, X, output)
                row = {"song": song, "auc": auc, "mixed_correlation": corr,
                       "mixed_coverage": float(mixed.float().mean()),
                       "direct_role_output_rms_fraction": role_fraction,
                       "pure_eligible_probability": float(prediction[..., 3][positive].mean())
                       if bool(positive.any()) else None}
                mode_rows.append(row)
                print(f"{mode} {song[:8]} auc={auc} mixed={corr} route={role_fraction:.4f}", flush=True)
        rows[mode] = mode_rows
        summaries[mode] = {
            "parameters": sum(p.numel() for p in net.parameters()),
            "estimated_neural_macs_per_second": net.macs * SN.FPS,
            **{key: median([r[key] for r in mode_rows]) for key in
               ("auc", "mixed_correlation", "mixed_coverage",
                "direct_role_output_rms_fraction", "pure_eligible_probability")},
        }
        out.write_text(json.dumps({"rows": rows, "summary": summaries}, indent=2) + "\n",
                       encoding="utf-8")
    if (summaries["coarse"]["parameters"] != summaries["fine"]["parameters"] or
            summaries["coarse"]["estimated_neural_macs_per_second"] !=
            summaries["fine"]["estimated_neural_macs_per_second"]):
        raise ValueError("matched role arms differ in size/compute")
    return {"rows": rows, "summary": summaries}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("coarse", type=Path)
    p.add_argument("fine", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    checkpoints = {"coarse": a.coarse, "fine": a.fine}
    role = quality(a.cache, checkpoints, a.out / "quality.json")
    DualDiag.SN = SN
    source = DualDiag.source_attribution(a.cache, a.coarse, a.fine,
                                         a.out / "source.json")
    transitions = DualDiag.event_rows(a.cache,
                                      {"reset2": a.coarse, "persistent": a.fine},
                                      a.out / "transitions.json")
    transitions["arm_labels"] = {"reset2": "coarse", "persistent": "fine"}
    (a.out / "transitions.json").write_text(json.dumps(transitions, indent=2) + "\n",
                                               encoding="utf-8")
    coarse, fine = source["anchor"]["median"], source["candidate"]["median"]
    ec, ef = transitions["entry_summary"]["reset2"], transitions["entry_summary"]["persistent"]
    qc, qf = role["summary"]["coarse"], role["summary"]["fine"]
    get = lambda data, key: data[key]["value"]
    gate = {
        "mixed_correlation_gain": get(qf, "mixed_correlation") - get(qc, "mixed_correlation") >= .10,
        "auc_retained": get(qf, "auc") - get(qc, "auc") >= -.05,
        "direct_role_route": get(qf, "direct_role_output_rms_fraction") >= .05,
        "eligible": get(fine, "eligible") >= .432,
        "vocal": get(fine, "vocal_over_eligible") <= .324,
        "drums": get(fine, "drums_over_eligible") <= .0568,
        "bass": get(fine, "bass_over_eligible") <= .0339,
        "protected_vs_coarse": all(
            get(fine, key + "_over_eligible") - get(coarse, key + "_over_eligible") <= .01
            for key in ("vocal", "drums", "bass")),
        "entry_median": get(ef, "duck_median") >= .85,
        "entry_p10": get(ef, "duck_p10") >= .60,
        "entry_pre": get(ef, "ret_pre") >= .25,
        "entry_post": get(ef, "ret_post") >= .25,
        "entry_p10_gain": get(ef, "duck_p10") - get(ec, "duck_p10") >= .04,
        "entry_count": ef["n"] == ec["n"] == 43,
    }
    summary = {"role": role["summary"],
               "source": {"coarse": coarse, "fine": fine},
               "entry": {"coarse": ec, "fine": ef},
               "gate": gate, "promotion": all(gate.values())}
    (a.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n",
                                        encoding="utf-8")
    print("summary:", json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
