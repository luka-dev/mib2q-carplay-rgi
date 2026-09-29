#!/usr/bin/env python3
"""Privileged bounded real IRM through fixed source and vocal-entry metrics."""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_transitions as DT
import mask_net as MN
import sep_net as SN


ROLES = ("vocal", "drums", "bass", "eligible")


def median(values):
    good = [value for value in values if value is not None and np.isfinite(value)]
    return {"n": len(good), "value": float(np.median(good)) if good else None}


def percentile(values, p):
    good = [value for value in values if value is not None and np.isfinite(value)]
    return {"n": len(good), "value": float(np.percentile(good, p)) if good else None}


def irm(X, E):
    return (E.abs() / (X.abs() + 1e-7)).clamp(0, 1)


def reanalysed(torch, spectra, n):
    return SN.stft(torch, SN.istft(torch, spectra, n))


def source_rows(torch, cache, songs):
    rows = []
    for song in songs:
        path = cache / (song + ".npy")
        N = MN.npy_header(str(path))[0][2]
        n = min(20 * SN.SR, N)
        start = MN.dev_excerpt(str(path), n) if N > n else 0
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768)
        sources = (w[0] + w[1], w[2] + w[3] + w[4], w[5], w[6])
        X = SN.stft(torch, w.sum(0)[None])
        E = SN.stft(torch, sources[3][None])
        mask = irm(X, E)
        energy = {}
        for name, source in zip(ROLES, sources):
            spectrum = SN.stft(torch, source[None])
            clean = reanalysed(torch, spectrum, n)
            sent = reanalysed(torch, mask * spectrum, n)
            energy[name] = (float((sent.abs() ** 2).sum()), float((clean.abs() ** 2).sum()))
        mix_energy = sum(den for _, den in energy.values())
        trans = {name: num / den if den > 1e-6 * mix_energy else None
                 for name, (num, den) in energy.items()}
        eligible = trans["eligible"]
        ratio = {name: trans[name] / eligible if eligible and trans[name] is not None else None
                 for name in ROLES[:3]}
        rows.append({"song": song, "transmission": trans, "protected_over_eligible": ratio})
        print(f"source {song[:8]} eligible={eligible} vocal/E={ratio['vocal']}", flush=True)
    summary = {name: median([r["transmission"][name] for r in rows]) for name in ROLES}
    summary.update({name + "_over_eligible": median([
        r["protected_over_eligible"][name] for r in rows]) for name in ROLES[:3]})
    return {"rows": rows, "summary": summary}


def event_rows(torch, cache, songs, partial):
    t_axis = np.arange((DT.PRE + DT.POST) // DT.FR) * .5 - 4.0
    pre, post = t_axis < 0, t_axis >= 0
    rows = []
    for index, song in enumerate(songs, 1):
        path = cache / (song + ".npy")
        n = MN.npy_header(str(path))[0][2]
        w = MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768
        events = [(kind, onset) for kind, onset in DT.events(w[0] + w[1])
                  if onset - DT.PRE - DT.WARM >= 0 and onset + DT.POST <= n]
        audio = torch.as_tensor(w)
        mix = audio.sum(0)[None]
        E = SN.stft(torch, audio[6][None])
        V = SN.stft(torch, (audio[0] + audio[1])[None])
        mask = irm(SN.stft(torch, mix), E)
        sent_e, clean_e = reanalysed(torch, mask * E, n), reanalysed(torch, E, n)
        sent_v, clean_v = reanalysed(torch, mask * V, n), reanalysed(torch, V, n)
        for kind, onset in events:
            start = round((onset - DT.PRE) / SN.HOP)
            length = round((DT.PRE + DT.POST) / SN.HOP)
            section = lambda Z: Z[:, start:start + length]
            ret = DT.per_frame(torch, section(sent_e), section(clean_e), len(t_axis), DT.FLOOR_DB)
            leak = DT.per_frame(torch, section(sent_v), section(clean_v), len(t_axis), DT.FLOOR_DB)
            before, after = float(np.nanmean(ret[pre])), float(np.nanmean(ret[post]))
            rows.append({"song": song, "kind": kind, "time_seconds": onset / SN.SR,
                         "ret_pre": before, "ret_post": after,
                         "duck": after / before if before > 0 else None,
                         "leak_0_2": float(np.nanmean(leak[(t_axis >= 0) & (t_axis < 2)]))})
        partial.write_text(json.dumps({"rows": rows}, indent=2, allow_nan=True) + "\n",
                           encoding="utf-8")
        print(f"events {index:2d}/{len(songs)} {song[:8]} n={len(events)}", flush=True)
    entries = [r for r in rows if r["kind"] == "entry"]
    summary = {
        "counts": {kind: sum(r["kind"] == kind for r in rows)
                   for kind in ("entry", "exit", "sustained")},
        "duck_median": median([r["duck"] for r in entries]),
        "duck_p10": percentile([r["duck"] for r in entries], 10),
        "ret_pre": median([r["ret_pre"] for r in entries]),
        "ret_post": median([r["ret_post"] for r in entries]),
        "leak_0_2": median([r["leak_0_2"] for r in entries]),
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
        sources = source_rows(torch, a.cache, split["dev"][:12])
        events = event_rows(torch, a.cache, split["dev"][:20],
                            a.out.with_suffix(".partial.json"))
    source = sources["summary"]
    entry = events["summary"]
    get = lambda data, key: data[key]["value"]
    gate = {
        "eligible_coverage": source["eligible"]["n"] == 12,
        "protected_coverage": all(source[name + "_over_eligible"]["n"] >= 8
                                  for name in ROLES[:3]),
        "entry_count": entry["counts"]["entry"] == 43,
        "eligible": get(source, "eligible") >= .432,
        "vocal": get(source, "vocal_over_eligible") <= .324,
        "drums": get(source, "drums_over_eligible") <= .0568,
        "bass": get(source, "bass_over_eligible") <= .0339,
        "entry_median": get(entry, "duck_median") >= .85,
        "entry_p10": get(entry, "duck_p10") >= .60,
        "absolute_pre": get(entry, "ret_pre") >= .25,
        "absolute_post": get(entry, "ret_post") >= .25,
    }
    result = {"scope": "privileged bounded real IRM; fixed DEV12 20 s sources and first DEV20 full cached-song events",
              "source": sources, "events": events, "gate": gate, "joint_pass": all(gate.values())}
    a.out.write_text(json.dumps(result, indent=2, allow_nan=True) + "\n", encoding="utf-8")
    print("source summary:", json.dumps(source), flush=True)
    print("entry summary:", json.dumps(entry), flush=True)
    print("gate:", json.dumps(gate), "joint_pass:", result["joint_pass"], flush=True)


if __name__ == "__main__":
    main()
