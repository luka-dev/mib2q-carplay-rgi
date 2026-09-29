#!/usr/bin/env python3
"""Fit a slow stereo operator to a frozen causal student's eligible estimate."""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_spatial_overlap_bound as SB
import diag_irm_overlap_bound as IRM
import diag_spatial_oracle as SO
import mask_net as MN
import sep_net as SN
import sep_net_fine_role as FR


def source_rows(torch, net, device, cache, songs, partial):
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
        estimate = net.separate_stft(Xfit.to(device)).cpu()
        cue_diag, cue_full = SO.fit_operators(torch, Xfit[0], estimate[0])
        _, truth_full = SB.fit(torch, train)
        S = SB.spectra(torch, test)
        operators = {"cue_diagonal": cue_diag, "cue_full": cue_full, "truth_full": truth_full}
        methods = {}
        for kind, operator in operators.items():
            energy = {name: SB.energy_ratio(torch, S[name], operator, "full", test.shape[-1])
                      for name in SB.ROLES}
            mix_power = sum(den for _, den in energy.values())
            trans = {name: num / den if den > 1e-6 * mix_power else None
                     for name, (num, den) in energy.items()}
            eligible = trans["eligible"]
            ratio = {name: trans[name] / eligible if eligible and trans[name] is not None else None
                     for name in SB.ROLES[:3]}
            methods[kind] = {"transmission": trans, "protected_over_eligible": ratio}
        rows.append({"song": song, "methods": methods})
        partial.write_text(json.dumps({"rows": rows}, indent=2) + "\n", encoding="utf-8")
        print(f"source {len(rows):2d}/{len(songs)} {song[:8]} cue E="
              f"{methods['cue_full']['transmission']['eligible']:.4f} "
              f"vocal/E={methods['cue_full']['protected_over_eligible']['vocal']}", flush=True)
    summary = {}
    for kind in operators:
        summary[kind] = {name: IRM.median([r["methods"][kind]["transmission"][name] for r in rows])
                         for name in SB.ROLES}
        summary[kind].update({name + "_over_eligible": IRM.median([
            r["methods"][kind]["protected_over_eligible"][name] for r in rows])
            for name in SB.ROLES[:3]})
    summary["cue_full_minus_cue_diagonal_bass_over_eligible"] = IRM.median([
        r["methods"]["cue_full"]["protected_over_eligible"]["bass"]
        - r["methods"]["cue_diagonal"]["protected_over_eligible"]["bass"]
        if r["methods"]["cue_full"]["protected_over_eligible"]["bass"] is not None
        and r["methods"]["cue_diagonal"]["protected_over_eligible"]["bass"] is not None else None
        for r in rows])
    return {"rows": rows, "summary": summary}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoint", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch, net, device = FR.load(str(a.checkpoint))
    if net.role_mode != "fine" or net.norm != "stream" or net.lookahead != 0 or net.bidir:
        raise ValueError("checkpoint is not the causal streaming fine-role arm")
    # The frozen source screen is CPU-only to avoid competing with the single GPU tenant.
    net.to("cpu")
    device = "cpu"
    split = MN.load_split(str(a.cache))
    a.out.parent.mkdir(parents=True, exist_ok=True)
    with torch.no_grad():
        source = source_rows(torch, net, device, a.cache, split["dev"][:12],
                             a.out.with_suffix(".partial.json"))
    s = source["summary"]["cue_full"]
    get = lambda key: s[key]["value"]
    delta = source["summary"]["cue_full_minus_cue_diagonal_bass_over_eligible"]["value"]
    gate = {
        "coverage": s["eligible"]["n"] == 12 and all(s[name + "_over_eligible"]["n"] >= 8
                                                   for name in SB.ROLES[:3]),
        "eligible": get("eligible") is not None and get("eligible") >= .432,
        "vocal": get("vocal_over_eligible") is not None and get("vocal_over_eligible") <= .324,
        "drums": get("drums_over_eligible") is not None and get("drums_over_eligible") <= .0568,
        "bass": get("bass_over_eligible") is not None and get("bass_over_eligible") <= .0339,
        "bass_better_than_cue_diagonal": delta is not None and delta < 0,
    }
    result = {"scope": "frozen review-37 causal cue fit on 0-29.9s; inspected DEV12 30.1-60s source gate; no entry/C/device/car claim",
              "source": source, "gate": gate, "source_pass": all(gate.values())}
    a.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("summary:", json.dumps(source["summary"]), flush=True)
    print("gate:", json.dumps(gate), "source_pass:", result["source_pass"], flush=True)


if __name__ == "__main__":
    main()
