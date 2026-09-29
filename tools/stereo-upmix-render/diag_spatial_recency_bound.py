#!/usr/bin/env python3
"""Privileged weighted 2x2 spatial fit with causal 2/8/30 s decay constants."""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_spatial_oracle as SO
import mask_net as MN
import sep_net as SN


TAUS = (2.0, 8.0, 30.0)
LABELS = ("tau2", "tau8", "tau30")
PROTECTED = ("vocal", "drums", "bass")


def median(values):
    vals = [x for x in values if x is not None and np.isfinite(x)]
    return {"n": len(vals), "value": float(np.median(vals)) if vals else None}


def summary_pair(rows, label):
    item = {}
    current = lambda r: r["fits"][label]
    reference = lambda r: r["fits"]["uniform"]
    item["eligible"] = median([current(r)["transmission"]["eligible"] for r in rows])
    item["paired_eligible_minus_uniform"] = median([
        current(r)["transmission"]["eligible"] - reference(r)["transmission"]["eligible"]
        for r in rows])
    item["paired_error_minus_uniform_db"] = median([
        current(r)["normalized_complex_error_db"] - reference(r)["normalized_complex_error_db"]
        for r in rows])
    for source in PROTECTED:
        ratios = [(current(r)["protected_over_eligible"][source],
                   reference(r)["protected_over_eligible"][source]) for r in rows]
        item[source + "_over_eligible"] = median([current for current, _ in ratios])
        item["paired_" + source + "_minus_uniform"] = median([
            current - base if current is not None and base is not None else None
            for current, base in ratios])
        if source == "vocal":
            item["paired_vocal_factor_of_uniform"] = median([
                current / base if current is not None and base else None
                for current, base in ratios])
    item["short_decay_gate"] = (
        item["paired_eligible_minus_uniform"]["value"] >= -0.03
        and item["paired_error_minus_uniform_db"]["value"] <= 0.3
        and item["paired_vocal_factor_of_uniform"]["n"] >= 8
        and item["paired_vocal_factor_of_uniform"]["value"] <= 1.25
        and all(item["paired_" + source + "_minus_uniform"]["n"] >= 8
                and item["paired_" + source + "_minus_uniform"]["value"] <= 0.01
                for source in ("drums", "bass"))
    )
    return item


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch = SN._torch()
    split = MN.load_split(str(a.cache))
    rows = []
    a.out.parent.mkdir(parents=True, exist_ok=True)
    partial = a.out.with_suffix(".partial.json")
    with torch.no_grad():
        for song in split["dev"][:12]:
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
            first8 = times < 8.0
            test = times >= 30.1
            test_sources = {name: S[test] for name, S in spectra.items()}
            row = {"song": song, "fits": {}, "first8_mean_power_vs_first30_db": {}}
            for source in ("eligible", "vocal"):
                P = (spectra[source].abs() ** 2).sum((1, 2))
                early, full = P[first8].mean(), P[train].mean()
                row["first8_mean_power_vs_first30_db"][source] = float(
                    10 * torch.log10((early + 1e-12) / (full + 1e-12)))
            _, uniform = SO.fit_operators(torch, X[train], spectra["eligible"][train])
            row["fits"]["uniform"] = SO.measure(
                torch, X[test], test_sources["eligible"], test_sources, uniform)
            for tau, label in zip(TAUS, LABELS):
                weight = torch.exp((times[train] - 29.9) / tau).sqrt()[:, None, None]
                _, full = SO.fit_operators(torch, X[train] * weight,
                                           spectra["eligible"][train] * weight)
                row["fits"][label] = SO.measure(
                    torch, X[test], test_sources["eligible"], test_sources, full)
            rows.append(row)
            partial.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
            print(f"{song[:8]} first8 eligible/vocal dB "
                  f"{row['first8_mean_power_vs_first30_db']['eligible']:+.1f}/"
                  f"{row['first8_mean_power_vs_first30_db']['vocal']:+.1f} "
                  + " ".join(
                      f"{label} error {row['fits'][label]['normalized_complex_error_db'] - row['fits']['uniform']['normalized_complex_error_db']:+.2f} dB"
                      for label in LABELS), flush=True)
    summary = {label: summary_pair(rows, label) for label in LABELS}
    summary["uniform"] = {
        "eligible": median([r["fits"]["uniform"]["transmission"]["eligible"] for r in rows]),
        **{source + "_over_eligible": median([
            r["fits"]["uniform"]["protected_over_eligible"][source] for r in rows])
           for source in PROTECTED},
    }
    summary["first8_low_eligible_songs"] = sum(
        r["first8_mean_power_vs_first30_db"]["eligible"] <= -20 for r in rows)
    summary["first8_low_vocal_songs"] = sum(
        r["first8_mean_power_vs_first30_db"]["vocal"] <= -20 for r in rows)
    for label in LABELS:
        item = summary[label]
        print(label + " summary " + " ".join(
            f"{key}={value['value']:.4f} (n={value['n']})"
            for key, value in item.items() if isinstance(value, dict) and value["value"] is not None)
              + f" gate={item['short_decay_gate']}", flush=True)
    print("first8 <=-20dB relative source level: eligible "
          f"{summary['first8_low_eligible_songs']}/12, vocal {summary['first8_low_vocal_songs']}/12", flush=True)
    a.out.write_text(json.dumps({
        "scope": "first 12 inspected DEV songs; privileged 0-29.9 s weighted true-stem fit; same 30.1-60 s test",
        "rows": rows, "summary": summary,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
