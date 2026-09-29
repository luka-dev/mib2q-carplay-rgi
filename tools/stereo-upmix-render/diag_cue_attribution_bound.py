#!/usr/bin/env python3
"""Privileged ablation of protected contributions to a frozen causal spatial cue."""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_irm_overlap_bound as IRM
import diag_spatial_overlap_bound as SB
import diag_spatial_oracle as SO
import mask_net as MN
import sep_net as SN
import sep_net_fine_role as FR


ARMS = ("raw", "minus_vocal", "minus_drums", "minus_vocal_drums", "minus_all_protected")


def source_rows(torch, net, cache, songs, partial):
    rows = []
    for song in songs:
        path = cache / (song + ".npy")
        n = MN.npy_header(str(path))[0][2]
        if n < 60 * SN.SR:
            raise ValueError(f"source song shorter than 60 s: {song}")
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), 0, 60 * SN.SR)).astype(np.float32) / 32768)
        train = w[:, :, :SB.FIT - SB.GAP]
        test = w[:, :, SB.FIT + SB.GAP:60 * SN.SR]
        Xfit = SN.stft(torch, train.sum(0)[None])
        Y = net.separate_stft(Xfit)
        train_sources = SB.spectra(torch, train)
        attributed = {name: net.apply_last_filter(train_sources[name]) for name in SB.ROLES}
        closure = float(((Y - sum(attributed.values())).abs() ** 2).sum()
                        / (Y.abs() ** 2).sum().clamp_min(1e-20))
        targets = {
            "raw": Y,
            "minus_vocal": Y - attributed["vocal"],
            "minus_drums": Y - attributed["drums"],
            "minus_vocal_drums": Y - attributed["vocal"] - attributed["drums"],
            "minus_all_protected": attributed["eligible"],
        }
        S = SB.spectra(torch, test)
        methods = {}
        for kind in ARMS:
            _, operator = SO.fit_operators(torch, Xfit[0], targets[kind][0])
            energy = {name: SB.energy_ratio(torch, S[name], operator, "full", test.shape[-1])
                      for name in SB.ROLES}
            mix_power = sum(den for _, den in energy.values())
            trans = {name: num / den if den > 1e-6 * mix_power else None
                     for name, (num, den) in energy.items()}
            eligible = trans["eligible"]
            ratio = {name: trans[name] / eligible if eligible and trans[name] is not None else None
                     for name in SB.ROLES[:3]}
            methods[kind] = {"transmission": trans, "protected_over_eligible": ratio}
        rows.append({"song": song, "closure_nmse": closure, "methods": methods})
        partial.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
        print(f"source {len(rows):2d}/{len(songs)} {song[:8]} closure={closure:.2e} "
              f"V/E raw={methods['raw']['protected_over_eligible']['vocal']} "
              f"-V={methods['minus_vocal']['protected_over_eligible']['vocal']}", flush=True)
    summary = {"closure_nmse": IRM.median([r["closure_nmse"] for r in rows])}
    for kind in ARMS:
        summary[kind] = {name: IRM.median([r["methods"][kind]["transmission"][name] for r in rows])
                         for name in SB.ROLES}
        summary[kind].update({name + "_over_eligible": IRM.median([
            r["methods"][kind]["protected_over_eligible"][name] for r in rows])
            for name in SB.ROLES[:3]})
    return {"rows": rows, "summary": summary}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoint", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch, net, _ = FR.load(str(a.checkpoint))
    if net.role_mode != "fine" or net.norm != "stream" or net.lookahead != 0 or net.bidir:
        raise ValueError("checkpoint is not the causal streaming fine-role arm")
    net.to("cpu")
    split = MN.load_split(str(a.cache))
    a.out.parent.mkdir(parents=True, exist_ok=True)
    with torch.no_grad():
        source = source_rows(torch, net, a.cache, split["dev"][:12], a.out.with_suffix(".partial.json"))
    gates = {}
    for kind in ARMS:
        s = source["summary"][kind]
        get = lambda key: s[key]["value"]
        gate = {
            "coverage": s["eligible"]["n"] == 12 and all(s[name + "_over_eligible"]["n"] >= 8
                                                       for name in SB.ROLES[:3]),
            "eligible": get("eligible") is not None and get("eligible") >= .432,
            "vocal": get("vocal_over_eligible") is not None and get("vocal_over_eligible") <= .324,
            "drums": get("drums_over_eligible") is not None and get("drums_over_eligible") <= .0568,
            "bass": get("bass_over_eligible") is not None and get("bass_over_eligible") <= .0339,
        }
        gates[kind] = {"items": gate, "source_pass": all(gate.values())}
    closure_valid = max(r["closure_nmse"] for r in source["rows"]) <= 1e-8
    result = {"scope": "privileged subtraction from frozen review-37 cue on 0-29.9s fit; inspected DEV12 30.1-60s test",
              "source": source, "closure_valid": closure_valid, "gates": gates}
    a.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("summary:", json.dumps(source["summary"]), flush=True)
    print("closure_valid:", closure_valid, "gates:", json.dumps(gates), flush=True)


if __name__ == "__main__":
    main()
