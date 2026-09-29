#!/usr/bin/env python3
"""Review-35 matched source and full-song vocal-entry attribution screen."""

import argparse
import json
import sys
from pathlib import Path

import numpy as np

import diag_role_selectivity as RS
import diag_transitions as DT
import mask_net as MN
import sep_net_state as SN


def finite_median(values):
    v = np.asarray(values, dtype=float)
    v = v[np.isfinite(v)]
    return {"n": int(v.size), "value": float(np.median(v)) if v.size else None}


def finite_percentile(values, p):
    v = np.asarray(values, dtype=float)
    v = v[np.isfinite(v)]
    return {"n": int(v.size), "value": float(np.percentile(v, p)) if v.size else None}


def source_attribution(cache, reset, persistent, path):
    previous_module, previous_argv = RS.SN, sys.argv
    RS.SN = SN
    sys.argv = ["diag_role_selectivity.py", str(cache), str(reset), str(persistent),
                "--out", str(path)]
    try:
        RS.main()
    finally:
        RS.SN, sys.argv = previous_module, previous_argv
    return json.loads(path.read_text(encoding="utf-8"))


def mechanism_check(cache, checkpoints):
    torch = SN._torch()
    info = {}
    for mode, path in checkpoints.items():
        _, net, _ = SN.load(str(path))
        start = net.h2 + net.hg + (net.feat if net.dec_skip else 0)
        state_weights = torch.cat([
            layer.weight[:, start:start + SN.STATE_DIM].flatten()
            for layer in net.dec
        ])
        info[mode] = {
            "parameters": sum(p.numel() for p in net.parameters()),
            "estimated_neural_macs_per_second": net.macs * SN.FPS,
            "slow_decoder_weight_rms": float(state_weights.square().mean().sqrt()),
        }
    if (info["reset2"]["parameters"] != info["persistent"]["parameters"] or
            info["reset2"]["estimated_neural_macs_per_second"] !=
            info["persistent"]["estimated_neural_macs_per_second"]):
        raise ValueError("arm model size/compute differs")
    split = MN.load_split(str(cache))
    path = cache / (split["dev"][0] + ".npy")
    n = min(20 * SN.SR, MN.npy_header(str(path))[0][2])
    w = MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768
    _, net, dev = SN.load(str(checkpoints["persistent"]))
    X = SN.stft(torch, torch.as_tensor(w.sum(0)[None], device=dev))
    with torch.no_grad():
        net.state_mode = "persistent"
        full_output = net.separate_stft(X)
        full_state = net.last_slow_state
        net.state_mode = "reset2"
        reset_output = net.separate_stft(X)
        reset_state = net.last_slow_state
        start = round(2 * SN.SR / SN.HOP)
        info["same_checkpoint_mode_ablation"] = {
            "slow_state_rms_difference_after_2s": float(
                (full_state[:, start:] - reset_state[:, start:]).square().mean().sqrt()),
            "output_rms_difference_after_2s": float(
                (full_output[:, start:] - reset_output[:, start:]).abs().square().mean().sqrt()),
        }
    return info


def slice_event(torch, num, den, onset, n_frames):
    start = round((onset - DT.PRE) / SN.HOP)
    length = round((DT.PRE + DT.POST) / SN.HOP)
    return DT.per_frame(torch, num[:, start:start + length],
                        den[:, start:start + length], n_frames, DT.FLOOR_DB)


def event_rows(cache, checkpoints, output):
    torch = SN._torch()
    split = MN.load_split(str(cache))
    songs = split["dev"][:20]
    t_axis = np.arange((DT.PRE + DT.POST) // DT.FR) * 0.5 - 4.0
    before, after = t_axis < 0, t_axis >= 0
    data = {"scope": "full-song causal inference; original vocal event detector; fixed first 20 inspected DEV songs",
            "rows": {"reset2": [], "persistent": []}}
    partial = output.with_suffix(".partial.json")
    for mode, model_path in checkpoints.items():
        _, net, dev = SN.load(str(model_path))
        print(f"{mode}: {len(songs)} full-song inferences", flush=True)
        with torch.no_grad():
            for index, song in enumerate(songs, 1):
                path = cache / (song + ".npy")
                n = MN.npy_header(str(path))[0][2]
                w = MN.expand_slots(MN.read_window(str(path), 0, n)).astype(np.float32) / 32768
                events = [(kind, onset) for kind, onset in DT.events(w[0] + w[1])
                          if onset - DT.PRE - DT.WARM >= 0 and onset + DT.POST <= n]
                audio = torch.as_tensor(w, device=dev)
                mix = audio.sum(0)[None]
                X = SN.stft(torch, mix)
                net.separate_stft(X)
                state = net.last_slow_state
                eligible = SN.stft(torch, audio[6][None])
                vocal = SN.stft(torch, (audio[0] + audio[1])[None])
                filtered_eligible = SN.stft(torch, SN.istft(torch, net.apply_last_filter(eligible), n))
                filtered_vocal = SN.stft(torch, SN.istft(torch, net.apply_last_filter(vocal), n))
                clean_eligible = SN.stft(torch, SN.istft(torch, eligible, n))
                clean_vocal = SN.stft(torch, SN.istft(torch, vocal, n))
                for kind, onset in events:
                    ret = slice_event(torch, filtered_eligible, clean_eligible,
                                      onset, len(t_axis))
                    leak = slice_event(torch, filtered_vocal, clean_vocal,
                                       onset, len(t_axis))
                    pre, post = float(np.nanmean(ret[before])), float(np.nanmean(ret[after]))
                    state_frame = min(round(onset / SN.HOP), state.shape[1] - 1)
                    row = {"song": song, "kind": kind, "time_seconds": onset / SN.SR,
                           "ret_pre": pre, "ret_post": post,
                           "duck": post / pre if pre > 0 else None,
                           "leak_0_2": float(np.nanmean(leak[(t_axis >= 0) & (t_axis < 2)])),
                           "leak_steady": float(np.nanmean(leak[t_axis >= 4])),
                           "state_rms": float(state[:, state_frame].square().mean().sqrt())}
                    data["rows"][mode].append(row)
                partial.write_text(json.dumps(data, indent=2, allow_nan=True) + "\n",
                                   encoding="utf-8")
                print(f"  {index:2d}/{len(songs)} {song[:8]} events={len(events)}", flush=True)
                del (audio, mix, X, state, eligible, vocal, filtered_eligible,
                     filtered_vocal, clean_eligible, clean_vocal)
    entries = {}
    for mode, rows in data["rows"].items():
        items = [r for r in rows if r["kind"] == "entry"]
        entries[mode] = {
            "n": len(items),
            "duck_median": finite_median([r["duck"] for r in items]),
            "duck_p10": finite_percentile([r["duck"] for r in items], 10),
            "ret_pre": finite_median([r["ret_pre"] for r in items]),
            "ret_post": finite_median([r["ret_post"] for r in items]),
            "leak_0_2": finite_median([r["leak_0_2"] for r in items]),
            "state_rms": finite_median([r["state_rms"] for r in items]),
        }
    if entries["reset2"]["n"] != entries["persistent"]["n"]:
        raise ValueError("matched event counts differ")
    data["entry_summary"] = entries
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path)
    parser.add_argument("reset", type=Path)
    parser.add_argument("persistent", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    a = parser.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    checkpoints = {"reset2": a.reset, "persistent": a.persistent}
    mechanism = mechanism_check(a.cache, checkpoints)
    (a.out / "mechanism.json").write_text(json.dumps(mechanism, indent=2) + "\n",
                                            encoding="utf-8")
    print("mechanism:", json.dumps(mechanism), flush=True)
    source = source_attribution(a.cache, a.reset, a.persistent,
                                a.out / "source.json")
    transitions = event_rows(a.cache, checkpoints,
                             a.out / "transitions.json")
    reset = source["anchor"]["median"]
    candidate = source["candidate"]["median"]
    er, ep = transitions["entry_summary"]["reset2"], transitions["entry_summary"]["persistent"]
    get = lambda data, key: data[key]["value"]
    gate = {
        "eligible": get(candidate, "eligible") >= .432,
        "vocal": get(candidate, "vocal_over_eligible") <= .324,
        "drums": get(candidate, "drums_over_eligible") <= .0568,
        "bass": get(candidate, "bass_over_eligible") <= .0339,
        "entry_median": get(ep, "duck_median") >= .85,
        "entry_p10": get(ep, "duck_p10") >= .60,
        "absolute_pre": get(ep, "ret_pre") >= .25,
        "absolute_post": get(ep, "ret_post") >= .25,
        "improve_p10_vs_reset": get(ep, "duck_p10") - get(er, "duck_p10") >= .04,
        "retain_eligible_vs_reset": get(candidate, "eligible") - get(reset, "eligible") >= -.03,
        "protected_vs_reset": all(
            get(candidate, name + "_over_eligible") -
            get(reset, name + "_over_eligible") <= .01
            for name in ("vocal", "drums", "bass")),
        "entry_count": ep["n"] == 43,
        "mechanism_state_used": (
            mechanism["persistent"]["slow_decoder_weight_rms"] > 1e-5
            and mechanism["same_checkpoint_mode_ablation"]["slow_state_rms_difference_after_2s"] > 1e-5
            and mechanism["same_checkpoint_mode_ablation"]["output_rms_difference_after_2s"] > 1e-7),
    }
    transitions["gate"] = gate
    transitions["promotion"] = all(gate.values())
    (a.out / "transitions.json").write_text(json.dumps(transitions, indent=2, allow_nan=True) + "\n",
                                               encoding="utf-8")
    print("entry summaries:", json.dumps(transitions["entry_summary"], indent=2), flush=True)
    print("gate:", json.dumps(gate), "promotion:", transitions["promotion"], flush=True)


if __name__ == "__main__":
    main()
