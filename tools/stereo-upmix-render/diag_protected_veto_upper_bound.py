#!/usr/bin/env python3
"""True-stem pure-protected veto on a frozen mixture-inferred matrix estimate.

Privileged upper bound only: true aligned sources choose the gate at evaluation
time. Source transmission uses reanalyzed audio, matching the usual gate; vocal
entries use the unchanged event detector and frame scoring.
"""

import argparse
import json
from pathlib import Path

import numpy as np

import diag_transitions as DT
import mask_net as MN
import sep_net as SN


SOURCES = ("vocal", "drums", "bass", "eligible")


def gate_from_truth(torch, spectra):
    powers = torch.stack([(spectra[name].abs() ** 2).sum(-1) for name in SOURCES], -1)
    fraction = powers / powers.sum(-1).clamp_min(1e-8)[..., None]
    pure_protected = fraction[..., :3].max(-1).values >= 0.8
    return torch.where(pure_protected, 0.25, 1.0)[..., None]


def med(values):
    vals = [v for v in values if v is not None and np.isfinite(v)]
    return {"n": len(vals), "value": float(np.median(vals)) if vals else None}


def source_screen(torch, net, cache):
    split = MN.load_split(str(cache))
    rows = []
    for song in split["dev"][:12]:
        path = cache / (song + ".npy")
        N = MN.npy_header(str(path))[0][2]
        n = min(20 * SN.SR, N)
        start = MN.dev_excerpt(str(path), n) if N > n else 0
        w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n)).astype(np.float32) / 32768)
        with torch.no_grad():
            spectra = {
                "vocal": SN.stft(torch, (w[0] + w[1])[None]),
                "drums": SN.stft(torch, (w[2] + w[3] + w[4])[None]),
                "bass": SN.stft(torch, w[5][None]),
                "eligible": SN.stft(torch, w[6][None]),
            }
            X = SN.stft(torch, w.sum(0)[None])
            net.separate_stft(X)
            gate = gate_from_truth(torch, spectra)
            energy = {"baseline": {}, "oracle_veto": {}}
            for source, S in spectra.items():
                clean = SN.stft(torch, SN.istft(torch, S, n))
                base = net.apply_last_filter(S)
                for label, Y in (("baseline", base), ("oracle_veto", gate * base)):
                    sent = SN.stft(torch, SN.istft(torch, Y, n))
                    energy[label][source] = (float((sent.abs() ** 2).sum()),
                                             float((clean.abs() ** 2).sum()))
        mix_energy = sum(v[1] for v in energy["baseline"].values())
        record = {"song": song, "transmission": {}, "protected_over_eligible": {}}
        for label in energy:
            tr = {source: num / den if den > 1e-6 * mix_energy else None
                  for source, (num, den) in energy[label].items()}
            ratio = {source: tr[source] / tr["eligible"]
                     if tr[source] is not None and tr["eligible"] else None
                     for source in SOURCES[:3]}
            record["transmission"][label] = tr
            record["protected_over_eligible"][label] = ratio
        rows.append(record)
    summary = {}
    for label in ("baseline", "oracle_veto"):
        summary[label] = {
            "transmission": {source: med([r["transmission"][label][source] for r in rows])
                             for source in SOURCES},
            "protected_over_eligible": {
                source: med([r["protected_over_eligible"][label][source] for r in rows])
                for source in SOURCES[:3]
            },
        }
        t = summary[label]["transmission"]
        q = summary[label]["protected_over_eligible"]
        print(f"{label} eligible={t['eligible']['value']:.4f} "
              f"vocal/E={q['vocal']['value']:.4f} drums/E={q['drums']['value']:.4f} "
              f"bass/E={q['bass']['value']:.4f}", flush=True)
    return {"rows": rows, "summary": summary}


def entry_screen(torch, net, cache):
    split = MN.load_split(str(cache))
    rows = []
    nf = (DT.PRE + DT.POST) // DT.FR
    t_axis = np.arange(nf) * 0.5 - 4.0
    pre, post = t_axis < 0, t_axis >= 0
    for song in split["dev"][:20]:
        path = cache / (song + ".npy")
        N = MN.npy_header(str(path))[0][2]
        w = MN.expand_slots(MN.read_window(str(path), 0, N)).astype(np.float32) / 32768
        evs = [(k, s) for k, s in DT.events(w[0] + w[1])
               if k == "entry" and s - DT.PRE - DT.WARM >= 0 and s + DT.POST <= N]
        for _, onset in evs:
            a0, a1 = onset - DT.PRE - DT.WARM, onset + DT.POST
            seg = torch.as_tensor(w[:, :, a0:a1])
            with torch.no_grad():
                X = SN.stft(torch, seg.sum(0)[None])
                net.separate_stft(X)
                spectra = {
                    "vocal": SN.stft(torch, (seg[0] + seg[1])[None]),
                    "drums": SN.stft(torch, (seg[2] + seg[3] + seg[4])[None]),
                    "bass": SN.stft(torch, seg[5][None]),
                    "eligible": SN.stft(torch, seg[6][None]),
                }
                gate = gate_from_truth(torch, spectra)
                cutf = X.shape[1] - SN.stft(torch, seg.sum(0)[None, :, DT.WARM:]).shape[1]
                reanalyze = lambda Z: SN.stft(torch, SN.istft(torch, Z, a1 - a0))[:, cutf:]
                clean_e = reanalyze(spectra["eligible"])
                clean_v = reanalyze(spectra["vocal"])
                base_e = net.apply_last_filter(spectra["eligible"])
                base_v = net.apply_last_filter(spectra["vocal"])
                values = {}
                for label, g in (("baseline", 1.0), ("oracle_veto", gate)):
                    ret = DT.per_frame(torch, reanalyze(g * base_e), clean_e, nf, DT.FLOOR_DB)
                    leak = DT.per_frame(torch, reanalyze(g * base_v), clean_v, nf, DT.FLOOR_DB)
                    pre_ret, post_ret = np.nanmean(ret[pre]), np.nanmean(ret[post])
                    values[label] = {
                        "eligible_pre": float(pre_ret), "eligible_post": float(post_ret),
                        "post_over_pre": float(post_ret / pre_ret) if pre_ret > 0 else None,
                        "vocal_0_2": float(np.nanmean(leak[(t_axis >= 0) & (t_axis < 2)])),
                    }
            rows.append({"song": song, "onset_s": float(onset / SN.SR), "values": values})
    summary = {}
    for label in ("baseline", "oracle_veto"):
        ducks = [r["values"][label]["post_over_pre"] for r in rows
                 if r["values"][label]["post_over_pre"] is not None]
        summary[label] = {
            "n": len(ducks), "retention_median": float(np.median(ducks)),
            "retention_p10": float(np.percentile(ducks, 10)),
            "vocal_0_2_median": med([r["values"][label]["vocal_0_2"] for r in rows]),
        }
        v = summary[label]
        print(f"{label} entries n={v['n']} retention median/p10 "
              f"{v['retention_median']:.3f}/{v['retention_p10']:.3f} "
              f"vocal 0-2 s={v['vocal_0_2_median']['value']:.3f}", flush=True)
    return {"rows": rows, "summary": summary}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("checkpoint", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    torch, net, dev = SN.load(str(a.checkpoint))
    if net.mask_mode != "matrix2" or dev != "cpu":
        raise ValueError("requires frozen matrix2 checkpoint on CPU")
    source = source_screen(torch, net, a.cache)
    entry = entry_screen(torch, net, a.cache)
    c = source["summary"]["oracle_veto"]
    e = entry["summary"]["oracle_veto"]
    gates = {
        "eligible": c["transmission"]["eligible"]["value"] >= 0.432,
        "vocal": c["protected_over_eligible"]["vocal"]["value"] <= 0.324,
        "drums": c["protected_over_eligible"]["drums"]["value"] <= 0.0568,
        "bass": c["protected_over_eligible"]["bass"]["value"] <= 0.0339,
        "entry_median": e["retention_median"] >= 0.85,
        "entry_p10": e["retention_p10"] >= 0.60,
    }
    print("gates " + " ".join(f"{name}={passed}" for name, passed in gates.items()), flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({
        "scope": "privileged true-stem pure-protected veto, fixed DEV12/DEV20; not a product or independent validation",
        "checkpoint": str(a.checkpoint), "gate": "0.25 amplitude on >=0.8 pure-protected cells; 1 elsewhere",
        "source": source, "entry": entry, "gates": gates,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
