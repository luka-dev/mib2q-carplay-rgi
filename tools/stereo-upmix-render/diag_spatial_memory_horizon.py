#!/usr/bin/env python3
"""True-stem spatial matrix upper bound after 2, 8 or 30 s of past history.

All fits use only the song's beginning and all scores use the same 30.1–60 s
held-out half. Privileged labels are unavailable in the live CarPlay chain.
"""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_spatial_oracle as SO
import mask_net as MN
import sep_net as SN


HORIZONS = (2.0, 8.0, 29.9)
LABELS = ("2s", "8s", "30s")
PROTECTED = ("vocal", "drums", "bass")


def median(values):
    vals = [x for x in values if x is not None and np.isfinite(x)]
    return {"n": len(vals), "value": float(np.median(vals)) if vals else None}


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
            test = times >= 30.1
            test_sources = {name: S[test] for name, S in spectra.items()}
            row = {"song": song, "horizons": {}}
            for seconds, label in zip(HORIZONS, LABELS):
                fit = times < seconds
                _, full = SO.fit_operators(torch, X[fit], spectra["eligible"][fit])
                row["horizons"][label] = SO.measure(
                    torch, X[test], test_sources["eligible"], test_sources, full)
            rows.append(row)
            partial.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
            for label in LABELS[:2]:
                current = row["horizons"][label]
                reference = row["horizons"]["30s"]
                vr, base_vr = (current["protected_over_eligible"]["vocal"],
                               reference["protected_over_eligible"]["vocal"])
                vocal_factor = f"{vr / base_vr:.3f}" if vr is not None and base_vr else "n/a"
                print(f"{song[:8]} {label}/30s eligible "
                      f"{current['transmission']['eligible'] - reference['transmission']['eligible']:+.3f} "
                      f"error {current['normalized_complex_error_db'] - reference['normalized_complex_error_db']:+.3f} dB "
                      f"vocal/E x{vocal_factor}", flush=True)
    summary = {"30s": {}, "2s": {}, "8s": {}}
    for label in LABELS:
        summary[label]["eligible"] = median([r["horizons"][label]["transmission"]["eligible"] for r in rows])
        summary[label]["error_db"] = median([r["horizons"][label]["normalized_complex_error_db"] for r in rows])
        for source in PROTECTED:
            summary[label][source + "_over_eligible"] = median([
                r["horizons"][label]["protected_over_eligible"][source] for r in rows])
    for label in LABELS[:2]:
        item = summary[label]
        item["paired_eligible_minus_30s"] = median([
            r["horizons"][label]["transmission"]["eligible"]
            - r["horizons"]["30s"]["transmission"]["eligible"] for r in rows])
        item["paired_error_minus_30s_db"] = median([
            r["horizons"][label]["normalized_complex_error_db"]
            - r["horizons"]["30s"]["normalized_complex_error_db"] for r in rows])
        for source in PROTECTED:
            pairs = [(r["horizons"][label]["protected_over_eligible"][source],
                      r["horizons"]["30s"]["protected_over_eligible"][source]) for r in rows]
            item["paired_" + source + "_minus_30s"] = median([
                current - reference if current is not None and reference is not None else None
                for current, reference in pairs])
            if source == "vocal":
                item["paired_vocal_factor_of_30s"] = median([
                    current / reference if current is not None and reference else None
                    for current, reference in pairs])
        item["short_history_gate"] = (
            item["paired_eligible_minus_30s"]["value"] is not None
            and item["paired_eligible_minus_30s"]["value"] >= -0.03
            and item["paired_error_minus_30s_db"]["value"] <= 0.3
            and item["paired_vocal_factor_of_30s"]["n"] >= 8
            and item["paired_vocal_factor_of_30s"]["value"] <= 1.25
            and all(item["paired_" + source + "_minus_30s"]["n"] >= 8
                    and item["paired_" + source + "_minus_30s"]["value"] <= 0.01
                    for source in ("drums", "bass"))
        )
        print(label + " summary " + " ".join(
            f"{key}={value['value']:.4f} (n={value['n']})"
            for key, value in item.items() if isinstance(value, dict) and value["value"] is not None)
              + f" gate={item['short_history_gate']}", flush=True)
    print("30s summary " + " ".join(
        f"{key}={value['value']:.4f} (n={value['n']})"
        for key, value in summary["30s"].items()), flush=True)
    a.out.write_text(json.dumps({
        "scope": "first 12 inspected DEV songs, privileged true-stem first 2/8/30 s fit, identical 30.1-60 s test",
        "rows": rows, "summary": summary,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
