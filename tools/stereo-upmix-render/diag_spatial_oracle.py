#!/usr/bin/env python3
"""True-stem fitted diagonal versus 2x2 stereo complex matrix on separate song halves.

Fit one frequency-dependent linear operator on 0-30 s of each DEV song and
evaluate it unchanged on 30-60 s. This is a privileged spatial-cue upper
bound, not a live separator or an independent test set.
"""

import argparse
import json
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def fit_operators(torch, X, E, ridge_fraction=0.01):
    """Return frequency-wise complex diagonal and full 2x2 least-squares maps."""
    gram = torch.einsum("tfi,tfj->fij", X.conj(), X)
    rhs = torch.einsum("tfi,tfj->fij", X.conj(), E)
    diagonal = gram.diagonal(dim1=-2, dim2=-1).real
    trace = diagonal.sum(-1)
    ridge = ridge_fraction * trace / 2 + trace.mean().clamp_min(1e-8) * 1e-6
    regularized = gram + ridge[:, None, None] * torch.eye(2, device=X.device, dtype=X.dtype)
    full = torch.linalg.solve(regularized, rhs)
    diag = torch.zeros_like(full)
    diag[:, 0, 0] = rhs[:, 0, 0] / (diagonal[:, 0] + ridge)
    diag[:, 1, 1] = rhs[:, 1, 1] / (diagonal[:, 1] + ridge)
    return diag, full


def apply(torch, source, operator):
    return torch.einsum("tfi,fij->tfj", source, operator)


def measure(torch, X, E, sources, operator):
    power = lambda x: float((x.abs() ** 2).sum())
    estimated = apply(torch, X, operator)
    target_power = power(E)
    mix_power = power(X)
    values = {
        "normalized_complex_error_db": float(10 * np.log10(power(estimated - E) / max(target_power, 1e-20))),
        "transmission": {}, "protected_over_eligible": {},
    }
    for name, source in sources.items():
        den = power(source)
        values["transmission"][name] = (
            power(apply(torch, source, operator)) / den if den > 1e-6 * mix_power else None)
    eligible = values["transmission"]["eligible"]
    for name in ("vocal", "drums", "bass"):
        protected = values["transmission"][name]
        values["protected_over_eligible"][name] = (
            protected / eligible if protected is not None and eligible and eligible > 0 else None)
    return values


def median(rows, path):
    vals = []
    for row in rows:
        value = row
        for key in path:
            value = value[key]
        if value is not None and np.isfinite(value):
            vals.append(value)
    return {"n": len(vals), "value": float(np.median(vals)) if vals else None}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("--songs", type=int, default=12)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch = SN._torch()
    split = MN.load_split(str(a.cache))
    rows = []
    a.out.parent.mkdir(parents=True, exist_ok=True)
    partial = a.out.with_suffix(".partial.json")
    with torch.no_grad():
        for song in split["dev"][:a.songs]:
            path = a.cache / (song + ".npy")
            n = min(MN.npy_header(str(path))[0][2], 60 * SN.SR)
            if n < 59 * SN.SR:
                raise ValueError(f"short DEV song: {song}")
            w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768)
            spectra = {
                "vocal": SN.stft(torch, (w[0] + w[1])[None])[0],
                "drums": SN.stft(torch, (w[2] + w[3] + w[4])[None])[0],
                "bass": SN.stft(torch, w[5][None])[0],
                "eligible": SN.stft(torch, w[6][None])[0],
            }
            X = SN.stft(torch, w.sum(0)[None])[0]
            del w
            times = torch.arange(X.shape[0]) * (SN.HOP / SN.SR)
            train = times < 29.9
            test = times >= 30.1
            diagonal, full = fit_operators(torch, X[train], spectra["eligible"][train])
            sources = {name: S[test] for name, S in spectra.items()}
            control = measure(torch, X[test], sources["eligible"], sources, diagonal)
            candidate = measure(torch, X[test], sources["eligible"], sources, full)
            raw_eligible = candidate["transmission"]["eligible"]
            gain = np.sqrt(control["transmission"]["eligible"] / max(raw_eligible, 1e-20))
            matched = measure(torch, X[test], sources["eligible"], sources, full * gain)
            row = {"song": song, "diag": control, "full": candidate,
                   "full_gain_for_eligible_match": float(gain), "full_matched_eligible": matched}
            row["full_minus_diag_error_db"] = (
                candidate["normalized_complex_error_db"] - control["normalized_complex_error_db"])
            row["full_minus_diag_eligible"] = (
                candidate["transmission"]["eligible"] - control["transmission"]["eligible"])
            candidate_vocal = candidate["protected_over_eligible"]["vocal"]
            control_vocal = control["protected_over_eligible"]["vocal"]
            row["full_over_diag_vocal_ratio"] = (
                candidate_vocal / max(control_vocal, 1e-20)
                if candidate_vocal is not None and control_vocal is not None else None)
            rows.append(row)
            partial.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
            vocal_ratio = (f"x{row['full_over_diag_vocal_ratio']:.3f}"
                           if row["full_over_diag_vocal_ratio"] is not None else "n/a")
            print(f"{song[:8]} error {row['full_minus_diag_error_db']:+.3f} dB "
                  f"eligible {row['full_minus_diag_eligible']:+.3f} "
                  f"vocal/E {vocal_ratio}", flush=True)
    summary = {
        "full_minus_diag_error_db": median(rows, ("full_minus_diag_error_db",)),
        "full_minus_diag_eligible": median(rows, ("full_minus_diag_eligible",)),
        "full_over_diag_vocal_ratio": median(rows, ("full_over_diag_vocal_ratio",)),
        "diag_vocal_ratio": median(rows, ("diag", "protected_over_eligible", "vocal")),
        "full_vocal_ratio": median(rows, ("full", "protected_over_eligible", "vocal")),
        "diag_drums_ratio": median(rows, ("diag", "protected_over_eligible", "drums")),
        "full_drums_ratio": median(rows, ("full", "protected_over_eligible", "drums")),
        "diag_bass_ratio": median(rows, ("diag", "protected_over_eligible", "bass")),
        "full_bass_ratio": median(rows, ("full", "protected_over_eligible", "bass")),
        "diag_eligible": median(rows, ("diag", "transmission", "eligible")),
        "full_eligible": median(rows, ("full", "transmission", "eligible")),
    }
    summary["full_lower_error_songs"] = sum(row["full_minus_diag_error_db"] < 0 for row in rows)
    coverage = all(summary[key]["n"] >= 8 for key in (
        "full_over_diag_vocal_ratio", "diag_drums_ratio", "full_drums_ratio",
        "diag_bass_ratio", "full_bass_ratio"))
    summary["adequate_protected_coverage"] = coverage
    summary["spatial_promotion_gate"] = (
        coverage
        and summary["full_over_diag_vocal_ratio"]["value"] is not None
        and summary["full_drums_ratio"]["value"] is not None
        and summary["diag_drums_ratio"]["value"] is not None
        and summary["full_bass_ratio"]["value"] is not None
        and summary["diag_bass_ratio"]["value"] is not None
        and summary["full_over_diag_vocal_ratio"]["value"] <= 0.8
        and summary["full_minus_diag_error_db"]["value"] <= -0.3
        and summary["full_lower_error_songs"] >= 8
        and summary["full_minus_diag_eligible"]["value"] >= -0.03
        and summary["full_drums_ratio"]["value"] <= summary["diag_drums_ratio"]["value"] + 0.01
        and summary["full_bass_ratio"]["value"] <= summary["diag_bass_ratio"]["value"] + 0.01)
    print("summary: " + " ".join(f"{key}={value['value']:.4f} (n={value['n']})"
                                 for key, value in summary.items() if isinstance(value, dict)), flush=True)
    print(f"full lower error {summary['full_lower_error_songs']}/{len(rows)}; "
          f"spatial promotion gate {summary['spatial_promotion_gate']}", flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({"scope": "12 inspected DEV songs, privileged first-half stem fit, second-half test; "
                                       "not a deployable or independent result", "rows": rows, "summary": summary},
                                indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
