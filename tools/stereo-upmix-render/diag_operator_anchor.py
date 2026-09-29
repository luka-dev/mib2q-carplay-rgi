#!/usr/bin/env python3
"""Matched source, full-song vocal entries and slow-operator use for review 42."""

import argparse
import json
import sys
from pathlib import Path

import numpy as np

import diag_role_selectivity as RS
import diag_transitions as DT
import mask_net as MN
import sep_net_operator_anchor as AN


LABELS = ("control", "anchored")


def median(values):
    good = [v for v in values if v is not None and np.isfinite(v)]
    return {"n": len(good), "value": float(np.median(good)) if good else None}


def percentile(values, p):
    good = [v for v in values if v is not None and np.isfinite(v)]
    return {"n": len(good), "value": float(np.percentile(good, p)) if good else None}


def source_attribution(cache, paths, output):
    previous_module, previous_argv = RS.SN, sys.argv
    RS.SN = AN
    sys.argv = ["diag_role_selectivity.py", str(cache), str(paths["control"]),
                str(paths["anchored"]), "--out", str(output)]
    try:
        RS.main()
    finally:
        RS.SN, sys.argv = previous_module, previous_argv
    return json.loads(output.read_text(encoding="utf-8"))


def as_vector(torch, net):
    diagonal, cross = net.last_matrix
    return torch.stack((diagonal[..., 0].real, diagonal[..., 0].imag,
                        diagonal[..., 1].real, diagonal[..., 1].imag,
                        cross[..., 0].real, cross[..., 0].imag,
                        cross[..., 1].real, cross[..., 1].imag), -1)


def mechanism(cache, paths, output):
    torch = AN._torch()
    split = MN.load_split(str(cache))
    rows = {label: [] for label in LABELS}
    for label in LABELS:
        _, net, dev = AN.load(str(paths[label]))
        with torch.no_grad():
            for song in split["dev"][:12]:
                path = cache / (song + ".npy")
                n = min(60 * AN.SR, MN.npy_header(str(path))[0][2])
                if n < 60 * AN.SR:
                    raise ValueError(f"mechanism song shorter than 60s: {song}")
                w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768,
                                    device=dev)
                X = AN.stft(torch, w.sum(0)[None])
                E = AN.stft(torch, w[6][None])
                net.separate_stft(X)
                final = as_vector(torch, net)
                slow = net.last_slow_vector
                fast = 2 * final - slow
                fraction = float(((fast - slow) ** 2).mean().sqrt()
                                 / (fast ** 2).mean().sqrt().clamp_min(1e-12))
                target, valid = AN.teacher_vector(torch, X, E)
                target_error = float(AN.operator_loss(torch, net, X, E)) if bool(valid.any()) else None
                rows[label].append({"song": song, "slow_fast_rms_fraction": fraction,
                                    "teacher_fit_valid": bool(valid.any()),
                                    "operator_target_loss": target_error})
                output.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
                print(f"mechanism {label} {song[:8]} slow/fast={fraction:.4f} "
                      f"target={target_error}", flush=True)
    summary = {label: {
        "slow_fast_rms_fraction": median([r["slow_fast_rms_fraction"] for r in rows[label]]),
        "operator_target_loss": median([r["operator_target_loss"] for r in rows[label]]),
        "valid_target_songs": sum(r["teacher_fit_valid"] for r in rows[label]),
    } for label in LABELS}
    return {"rows": rows, "summary": summary}


def event_rows(cache, paths, output):
    torch = AN._torch()
    split = MN.load_split(str(cache))
    songs = split["dev"][:20]
    axis = np.arange((DT.PRE + DT.POST) // DT.FR) * .5 - 4.0
    before, after = axis < 0, axis >= 0
    rows = {label: [] for label in LABELS}
    partial = output.with_suffix(".partial.json")
    for label in LABELS:
        _, net, dev = AN.load(str(paths[label]))
        print(f"{label}: {len(songs)} full-song inferences", flush=True)
        with torch.no_grad():
            for index, song in enumerate(songs, 1):
                path = cache / (song + ".npy")
                n = MN.npy_header(str(path))[0][2]
                w = MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768
                events = [(kind, onset) for kind, onset in DT.events(w[0] + w[1])
                          if onset - DT.PRE - DT.WARM >= 0 and onset + DT.POST <= n]
                audio = torch.as_tensor(w, device=dev)
                X = AN.stft(torch, audio.sum(0)[None])
                net.separate_stft(X)
                eligible = AN.stft(torch, audio[6][None])
                vocal = AN.stft(torch, (audio[0] + audio[1])[None])
                sent_e = AN.stft(torch, AN.istft(torch, net.apply_last_filter(eligible), n))
                sent_v = AN.stft(torch, AN.istft(torch, net.apply_last_filter(vocal), n))
                clean_e = AN.stft(torch, AN.istft(torch, eligible, n))
                clean_v = AN.stft(torch, AN.istft(torch, vocal, n))
                for kind, onset in events:
                    start = round((onset - DT.PRE) / AN.HOP)
                    length = round((DT.PRE + DT.POST) / AN.HOP)
                    cut = lambda z: z[:, start:start + length]
                    ret = DT.per_frame(torch, cut(sent_e), cut(clean_e), len(axis), DT.FLOOR_DB)
                    leak = DT.per_frame(torch, cut(sent_v), cut(clean_v), len(axis), DT.FLOOR_DB)
                    pre, post = float(np.nanmean(ret[before])), float(np.nanmean(ret[after]))
                    rows[label].append({"song": song, "kind": kind, "time_seconds": onset / AN.SR,
                                        "ret_pre": pre, "ret_post": post,
                                        "duck": post / pre if pre > 0 else None,
                                        "leak_0_2": float(np.nanmean(leak[(axis >= 0) & (axis < 2)]))})
                partial.write_text(json.dumps({"rows": rows}, indent=2, allow_nan=True) + "\n", encoding="utf-8")
                print(f"events {label} {index:2d}/{len(songs)} {song[:8]} n={len(events)}", flush=True)
    summary = {}
    for label in LABELS:
        entries = [r for r in rows[label] if r["kind"] == "entry"]
        summary[label] = {
            "n": len(entries),
            "duck_median": median([r["duck"] for r in entries]),
            "duck_p10": percentile([r["duck"] for r in entries], 10),
            "ret_pre": median([r["ret_pre"] for r in entries]),
            "ret_post": median([r["ret_post"] for r in entries]),
            "leak_0_2": median([r["leak_0_2"] for r in entries]),
        }
    result = {"scope": "same full cached-song inference and first-20-DEV events as reviews 35-38",
              "rows": rows, "entry_summary": summary}
    output.write_text(json.dumps(result, indent=2, allow_nan=True) + "\n", encoding="utf-8")
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("control", type=Path)
    p.add_argument("anchored", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    paths = {"control": a.control, "anchored": a.anchored}
    source = source_attribution(a.cache, paths, a.out / "source.json")
    mechanism_data = mechanism(a.cache, paths, a.out / "mechanism.json")
    events = event_rows(a.cache, paths, a.out / "transitions.json")
    control, candidate = source["anchor"]["median"], source["candidate"]["median"]
    ec, ea = events["entry_summary"]["control"], events["entry_summary"]["anchored"]
    mc, ma = mechanism_data["summary"]["control"], mechanism_data["summary"]["anchored"]
    get = lambda data, key: data[key]["value"]
    gate = {
        "source_coverage": candidate["eligible"]["n"] == 12 and all(
            candidate[name + "_over_eligible"]["n"] >= 8 for name in ("vocal", "drums", "bass")),
        "eligible": get(candidate, "eligible") is not None and get(candidate, "eligible") >= .432,
        "vocal": get(candidate, "vocal_over_eligible") is not None and get(candidate, "vocal_over_eligible") <= .324,
        "drums": get(candidate, "drums_over_eligible") is not None and get(candidate, "drums_over_eligible") <= .0568,
        "bass": get(candidate, "bass_over_eligible") is not None and get(candidate, "bass_over_eligible") <= .0339,
        "eligible_vs_control": get(candidate, "eligible") - get(control, "eligible") >= -.03,
        "protected_vs_control": all(get(candidate, name + "_over_eligible") -
                                    get(control, name + "_over_eligible") <= .01
                                    for name in ("vocal", "drums", "bass")),
        "entry_count": ec["n"] == ea["n"] == 43,
        "entry_median": get(ea, "duck_median") is not None and get(ea, "duck_median") >= .85,
        "entry_p10": get(ea, "duck_p10") is not None and get(ea, "duck_p10") >= .60,
        "entry_pre": get(ea, "ret_pre") is not None and get(ea, "ret_pre") >= .25,
        "entry_post": get(ea, "ret_post") is not None and get(ea, "ret_post") >= .25,
        "entry_p10_gain": get(ea, "duck_p10") - get(ec, "duck_p10") >= .04,
        "operator_target_valid": mc["valid_target_songs"] >= 8 and ma["valid_target_songs"] >= 8,
        "operator_target_improved": (get(ma, "operator_target_loss") is not None
                                     and get(mc, "operator_target_loss") is not None
                                     and get(ma, "operator_target_loss") < get(mc, "operator_target_loss")),
    }
    result = {"source": {"control": control, "anchored": candidate},
              "entry": {"control": ec, "anchored": ea},
              "mechanism": mechanism_data["summary"], "gate": gate,
              "promotion": all(gate.values())}
    (a.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("summary:", json.dumps(result), flush=True)


if __name__ == "__main__":
    main()
