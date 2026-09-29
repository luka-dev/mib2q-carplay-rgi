#!/usr/bin/env python3
"""Frozen role-head quality and direct matrix-decoder-input intervention."""

import argparse
import json
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net_state as SN


ROLES = ("vocal", "drums", "bass", "eligible")


def median(values):
    good = [value for value in values if value is not None and np.isfinite(value)]
    return {"n": len(good), "value": float(np.median(good)) if good else None}


def apply(torch, source, operator):
    diagonal, cross = operator
    left = diagonal[..., 0] * source[..., 0] + cross[..., 1] * source[..., 1]
    right = cross[..., 0] * source[..., 0] + diagonal[..., 1] * source[..., 1]
    return torch.stack((left, right), -1)


def neutral_operator(torch, net):
    """Remove only direct role-probability contribution relative to 0.25 each."""
    B, T = net.role_logits.shape[:2]
    role_delta = net.role_logits.softmax(-1)
    blocks = []
    for index, (a, b) in enumerate(net.bands):
        role = role_delta[:, :, a] - .25
        weights = net.dec[index].weight[:, -4:]
        delta = torch.einsum("btq,oq->bto", role, weights).reshape(B, T, b - a, 8)
        blocks.append(delta)
    m = torch.cat(blocks, dim=2)
    delta_diagonal = torch.complex(m[..., 0:4:2], m[..., 1:4:2])
    delta_cross = torch.complex(m[..., 4:8:2], m[..., 5:8:2])
    diagonal, cross = net.last_matrix
    return (diagonal - delta_diagonal, cross - delta_cross)


def transmission(torch, source, n, operator):
    S = SN.stft(torch, source[None])
    clean = SN.stft(torch, SN.istft(torch, S, n))
    sent = SN.stft(torch, SN.istft(torch, apply(torch, S, operator), n))
    return float((sent.abs() ** 2).sum()), float((clean.abs() ** 2).sum())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoint", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch, net, dev = SN.load(str(a.checkpoint))
    if net.state_mode != "persistent":
        p.error("expected persistent checkpoint")
    split = MN.load_split(str(a.cache))
    rows = []
    a.out.parent.mkdir(parents=True, exist_ok=True)
    partial = a.out.with_suffix(".partial.json")
    with torch.no_grad():
        for song in split["dev"][:12]:
            path = a.cache / (song + ".npy")
            N = MN.npy_header(str(path))[0][2]
            n = min(20 * SN.SR, N)
            start = MN.dev_excerpt(str(path), n) if N > n else 0
            w = torch.as_tensor(
                MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768,
                device=dev)
            source = (w[0] + w[1], w[2] + w[3] + w[4], w[5], w[6])
            X = SN.stft(torch, w.sum(0)[None])
            output = net.separate_stft(X)
            current = net.last_matrix
            neutral = neutral_operator(torch, net)
            neutral_output = apply(torch, X, neutral)
            delta_fraction = float(
                ((output - neutral_output).abs() ** 2).sum().sqrt() /
                (output.abs() ** 2).sum().sqrt().clamp_min(1e-12))

            prediction = net.role_logits.softmax(-1)
            power = torch.stack([(SN.stft(torch, s[None]).abs() ** 2).sum(-1)
                                 for s in source], -1)
            total = power.sum(-1)
            target = power / total.clamp_min(1e-8)[..., None]
            audible = total > 1e-5 * total.mean()
            own_predicted = {}
            eligible_predicted = {}
            coverage = {}
            for index, role in enumerate(ROLES):
                pure = (target[..., index] >= .8) & audible
                own_predicted[role] = float(prediction[..., index][pure].mean()) if bool(pure.any()) else None
                eligible_predicted[role] = float(prediction[..., 3][pure].mean()) if bool(pure.any()) else None
                coverage[role] = float(pure.float().mean())
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
            mixed_corr = None
            if int(mixed.sum()) >= 100:
                actual = target[..., 3][mixed].cpu().numpy()
                predicted = prediction[..., 3][mixed].cpu().numpy()
                if np.std(actual) > 1e-8 and np.std(predicted) > 1e-8:
                    mixed_corr = float(np.corrcoef(actual, predicted)[0, 1])
            energy = {label: {} for label in ("current", "neutral")}
            for label, operator in (("current", current), ("neutral", neutral)):
                for name, stem in zip(ROLES, source):
                    energy[label][name] = transmission(torch, stem, n, operator)
            trans = {}
            ratios = {}
            for label in energy:
                mix_energy = sum(den for _, den in energy[label].values())
                trans[label] = {name: num / den if den > 1e-6 * mix_energy else None
                                for name, (num, den) in energy[label].items()}
                eligible = trans[label]["eligible"]
                ratios[label] = {name: trans[label][name] / eligible
                                 if eligible and trans[label][name] is not None else None
                                 for name in ROLES[:3]}
            row = {"song": song, "pure_own_probability": own_predicted,
                   "pure_eligible_probability": eligible_predicted,
                   "pure_cell_coverage": coverage,
                   "eligible_vs_protected_auc": auc,
                   "mixed_eligible_vocal_correlation": mixed_corr,
                   "mixed_cell_coverage": float(mixed.float().mean()),
                   "direct_role_output_rms_fraction": delta_fraction,
                   "transmission": trans, "protected_over_eligible": ratios}
            rows.append(row)
            partial.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
            print(f"{song[:8]} AUC={auc} mixed_corr={mixed_corr} role_delta={delta_fraction:.4f}", flush=True)
    summary = {
        "auc": median([r["eligible_vs_protected_auc"] for r in rows]),
        "mixed_correlation": median([r["mixed_eligible_vocal_correlation"] for r in rows]),
        "mixed_coverage": median([r["mixed_cell_coverage"] for r in rows]),
        "direct_role_output_rms_fraction": median([r["direct_role_output_rms_fraction"] for r in rows]),
    }
    for role in ROLES:
        summary["pure_" + role + "_own_probability"] = median([
            r["pure_own_probability"][role] for r in rows])
        summary["pure_" + role + "_eligible_probability"] = median([
            r["pure_eligible_probability"][role] for r in rows])
    for label in ("current", "neutral"):
        for role in ROLES:
            summary[label + "_" + role + "_transmission"] = median([
                r["transmission"][label][role] for r in rows])
        for role in ROLES[:3]:
            summary[label + "_" + role + "_over_eligible"] = median([
                r["protected_over_eligible"][label][role] for r in rows])
    interpretation = (
        "weak_fast_cue" if summary["auc"]["value"] is None or summary["auc"]["value"] < .75 or
        summary["mixed_correlation"]["value"] is None or summary["mixed_correlation"]["value"] < .30 else
        "weak_direct_decoder_use" if summary["direct_role_output_rms_fraction"]["value"] < .05 else
        "cue_and_direct_use_present_but_protection_still_fails")
    result = {"scope": "fixed 12 inspected DEV 20 s excerpts; frozen persistent review-35 checkpoint",
              "checkpoint": str(a.checkpoint), "rows": rows, "summary": summary,
              "predeclared_interpretation": interpretation}
    a.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("summary:", json.dumps(summary), flush=True)
    print("interpretation:", interpretation, flush=True)


if __name__ == "__main__":
    main()
