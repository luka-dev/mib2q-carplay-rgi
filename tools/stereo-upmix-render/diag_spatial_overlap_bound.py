#!/usr/bin/env python3
"""Privileged diagonal/full spatial fits through exact source and entry metrics."""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_irm_overlap_bound as IRM
import diag_spatial_oracle as SO
import diag_transitions as DT
import mask_net as MN
import sep_net as SN


ROLES = ("vocal", "drums", "bass", "eligible")
OPERATORS = ("diagonal", "full", "irm")
FIT = 30 * SN.SR
GAP = SN.SR // 10


def spectra(torch, w):
    return {
        "vocal": SN.stft(torch, (w[0] + w[1])[None]),
        "drums": SN.stft(torch, (w[2] + w[3] + w[4])[None]),
        "bass": SN.stft(torch, w[5][None]),
        "eligible": SN.stft(torch, w[6][None]),
    }


def filter_spectrum(torch, source, operator, kind):
    if kind == "irm":
        return operator * source
    return SO.apply(torch, source[0], operator)[None]


def fit(torch, w):
    X = SN.stft(torch, w.sum(0)[None])[0]
    E = SN.stft(torch, w[6][None])[0]
    return SO.fit_operators(torch, X, E)


def energy_ratio(torch, source, operator, kind, n):
    clean = IRM.reanalysed(torch, source, n)
    sent = IRM.reanalysed(torch, filter_spectrum(torch, source, operator, kind), n)
    return float((sent.abs() ** 2).sum()), float((clean.abs() ** 2).sum())


def source_rows(torch, cache, songs, partial):
    rows = []
    for song in songs:
        path = cache / (song + ".npy")
        n = MN.npy_header(str(path))[0][2]
        if n < 60 * SN.SR:
            raise ValueError(f"source song shorter than 60 s: {song}")
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), 0, 60 * SN.SR)).astype(np.float32) / 32768)
        diagonal, full = fit(torch, w[:, :, :FIT - GAP])
        test = w[:, :, FIT + GAP:60 * SN.SR]
        test_n = test.shape[-1]
        S = spectra(torch, test)
        X = SN.stft(torch, test.sum(0)[None])
        operators = {"diagonal": diagonal, "full": full, "irm": IRM.irm(X, S["eligible"])}
        methods = {}
        for kind in OPERATORS:
            energy = {name: energy_ratio(torch, S[name], operators[kind], kind, test_n) for name in ROLES}
            mix_power = sum(den for _, den in energy.values())
            trans = {name: num / den if den > 1e-6 * mix_power else None
                     for name, (num, den) in energy.items()}
            eligible = trans["eligible"]
            ratio = {name: trans[name] / eligible if eligible and trans[name] is not None else None
                     for name in ROLES[:3]}
            methods[kind] = {"transmission": trans, "protected_over_eligible": ratio}
        # Positive scalar for an equal-eligible-output comparison; protected/E ratios are unchanged.
        full_e = methods["full"]["transmission"]["eligible"]
        irm_e = methods["irm"]["transmission"]["eligible"]
        matched_gain = float(np.sqrt(irm_e / full_e)) if full_e and irm_e else None
        rows.append({"song": song, "methods": methods, "full_gain_to_match_irm_eligible": matched_gain})
        partial.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
        print(f"source {len(rows):2d}/{len(songs)} {song[:8]} full E={full_e:.4f} "
              f"bass/E={methods['full']['protected_over_eligible']['bass']}", flush=True)
    summary = {}
    for kind in OPERATORS:
        summary[kind] = {name: IRM.median([r["methods"][kind]["transmission"][name] for r in rows])
                         for name in ROLES}
        summary[kind].update({name + "_over_eligible": IRM.median([
            r["methods"][kind]["protected_over_eligible"][name] for r in rows])
            for name in ROLES[:3]})
    summary["full_gain_to_match_irm_eligible"] = IRM.median([
        r["full_gain_to_match_irm_eligible"] for r in rows])
    summary["full_minus_diagonal_bass_over_eligible"] = IRM.median([
        r["methods"]["full"]["protected_over_eligible"]["bass"]
        - r["methods"]["diagonal"]["protected_over_eligible"]["bass"]
        if r["methods"]["full"]["protected_over_eligible"]["bass"] is not None
        and r["methods"]["diagonal"]["protected_over_eligible"]["bass"] is not None else None
        for r in rows])
    return {"rows": rows, "summary": summary}


def event_rows(torch, cache, songs, partial):
    axis = np.arange((DT.PRE + DT.POST) // DT.FR) * .5 - 4.0
    before, after = axis < 0, axis >= 0
    rows = []
    for index, song in enumerate(songs, 1):
        path = cache / (song + ".npy")
        n = MN.npy_header(str(path))[0][2]
        w = MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768
        entries = [onset for kind, onset in DT.events(w[0] + w[1]) if kind == "entry"
                   and onset - DT.PRE - DT.WARM - FIT >= 0 and onset + DT.POST <= n]
        for onset in entries:
            test_start = onset - DT.PRE - DT.WARM
            train_start = test_start - FIT
            train = torch.as_tensor(w[:, :, train_start:test_start])
            test = torch.as_tensor(w[:, :, test_start:onset + DT.POST])
            diagonal, full = fit(torch, train)
            S = spectra(torch, test)
            X = SN.stft(torch, test.sum(0)[None])
            operators = {"diagonal": diagonal, "full": full, "irm": IRM.irm(X, S["eligible"])}
            first = round(DT.WARM / SN.HOP)
            frames = round((DT.PRE + DT.POST) / SN.HOP)
            section = lambda z: z[:, first:first + frames]
            metrics = {}
            for kind in OPERATORS:
                ret = DT.per_frame(torch, section(IRM.reanalysed(torch,
                    filter_spectrum(torch, S["eligible"], operators[kind], kind), test.shape[-1])),
                    section(IRM.reanalysed(torch, S["eligible"], test.shape[-1])), len(axis), DT.FLOOR_DB)
                leak = DT.per_frame(torch, section(IRM.reanalysed(torch,
                    filter_spectrum(torch, S["vocal"], operators[kind], kind), test.shape[-1])),
                    section(IRM.reanalysed(torch, S["vocal"], test.shape[-1])), len(axis), DT.FLOOR_DB)
                pre, post = float(np.nanmean(ret[before])), float(np.nanmean(ret[after]))
                metrics[kind] = {"ret_pre": pre, "ret_post": post,
                                 "duck": post / pre if pre > 0 else None,
                                 "leak_0_2": float(np.nanmean(leak[(axis >= 0) & (axis < 2)]))}
            rows.append({"song": song, "time_seconds": onset / SN.SR, "methods": metrics})
        partial.write_text(json.dumps({"rows": rows}, indent=2, allow_nan=True) + "\n", encoding="utf-8")
        print(f"events {index:2d}/{len(songs)} {song[:8]} entries={len(entries)}", flush=True)
    summary = {}
    for kind in OPERATORS:
        summary[kind] = {
            "duck_median": IRM.median([r["methods"][kind]["duck"] for r in rows]),
            "duck_p10": IRM.percentile([r["methods"][kind]["duck"] for r in rows], 10),
            "ret_pre": IRM.median([r["methods"][kind]["ret_pre"] for r in rows]),
            "ret_post": IRM.median([r["methods"][kind]["ret_post"] for r in rows]),
            "leak_0_2": IRM.median([r["methods"][kind]["leak_0_2"] for r in rows]),
        }
    return {"rows": rows, "summary": summary}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    split = MN.load_split(str(a.cache))
    torch = SN._torch()
    a.out.parent.mkdir(parents=True, exist_ok=True)
    with torch.no_grad():
        source = source_rows(torch, a.cache, split["dev"][:12], a.out.with_suffix(".sources.partial.json"))
        events = event_rows(torch, a.cache, split["dev"][:20], a.out.with_suffix(".events.partial.json"))
    s, e = source["summary"]["full"], events["summary"]["full"]
    get = lambda d, k: d[k]["value"]
    gate = {
        "source_coverage": get(s, "eligible") is not None and s["eligible"]["n"] == 12
                           and all(s[name + "_over_eligible"]["n"] >= 8 for name in ROLES[:3]),
        "entry_coverage": len(events["rows"]) >= 25 and e["duck_median"]["n"] >= 25,
        "eligible": get(s, "eligible") is not None and get(s, "eligible") >= .432,
        "vocal": get(s, "vocal_over_eligible") is not None and get(s, "vocal_over_eligible") <= .324,
        "drums": get(s, "drums_over_eligible") is not None and get(s, "drums_over_eligible") <= .0568,
        "bass": get(s, "bass_over_eligible") is not None and get(s, "bass_over_eligible") <= .0339,
        "bass_better_than_diagonal": source["summary"]["full_minus_diagonal_bass_over_eligible"]["value"] is not None
                                     and source["summary"]["full_minus_diagonal_bass_over_eligible"]["value"] < 0,
        "entry_median": get(e, "duck_median") is not None and get(e, "duck_median") >= .85,
        "entry_p10": get(e, "duck_p10") is not None and get(e, "duck_p10") >= .60,
        "absolute_pre": get(e, "ret_pre") is not None and get(e, "ret_pre") >= .25,
        "absolute_post": get(e, "ret_post") is not None and get(e, "ret_post") >= .25,
    }
    result = {"scope": "privileged true-stem spatial fits; inspected DEV12 30.1-60s source tests and first DEV20 rolling 30s fit before vocal entries",
              "source": source, "events": events, "gate": gate, "joint_pass": all(gate.values())}
    a.out.write_text(json.dumps(result, indent=2, allow_nan=True) + "\n", encoding="utf-8")
    print("source summary:", json.dumps(source["summary"]), flush=True)
    print("entry summary:", json.dumps(events["summary"]), flush=True)
    print("gate:", json.dumps(gate), "joint_pass:", result["joint_pass"], flush=True)


if __name__ == "__main__":
    main()
