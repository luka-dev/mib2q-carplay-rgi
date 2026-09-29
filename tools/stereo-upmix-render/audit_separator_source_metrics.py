#!/usr/bin/env python3
"""Read-only source audit with exact interference and full filter attribution.

Leaves historical diag_gap.py and its reports unchanged. Applies the filter
inferred from the mixture to each aligned source; supports diagonal, matrix2
and deep2 outputs. JSON includes each song and each band's energy denominator.
"""
import argparse
import json
import os
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN

BANDS = (("<250", 0, 11), ("250-1k", 11, 43), ("1-3k", 43, 128),
         ("3-8k", 128, 342), ("8-14k", 342, 598), (">14k", 598, 1025))


def vocal_owned(torch, vocal, interference):
    """Ownership uses the actual nonvocal spectrum, including its phase."""
    return vocal.abs().square() > 10 * (interference.abs().square() + 1e-9)


def ratio(numerator, denominator):
    d = float(denominator.sum())
    return float(numerator.sum()) / d if d > 1e-9 else None


def finite_median(values):
    values = [x for x in values if x is not None and np.isfinite(x)]
    return float(np.median(values)) if values else None


def audit(cache, checkpoint, split_name, songs, seconds):
    torch, net, device = SN.load(str(checkpoint))
    split = MN.load_split(str(cache))
    ids = split[split_name][:songs]
    rows = []
    with torch.no_grad():
        for song in ids:
            path = cache / (song + ".npy")
            n = min(int(seconds * SN.SR), MN.npy_header(str(path))[0][2])
            start = MN.dev_excerpt(str(path), n)
            w = torch.as_tensor(MN.expand_slots(MN.read_window(str(path), start, n))
                                    .astype(np.float32) / 32768, device=device)
            mix = w.sum(0)[None]
            sources = dict(eligible=w[list(SN.ELIGIBLE_SLOTS)].sum(0)[None],
                           vocal=(w[0] + w[1])[None],
                           drums=(w[2] + w[3] + w[4])[None], bass=w[5][None])
            X = SN.stft(torch, mix)
            net.separate_stft(X)
            reanalyse = lambda z: SN.stft(torch, SN.istft(torch, z, n))
            refs = {k: reanalyse(SN.stft(torch, v)) for k, v in sources.items()}
            sent = {k: reanalyse(net.apply_last_filter(SN.stft(torch, v)))
                    for k, v in sources.items()}
            powers = {k: v.abs().square() for k, v in refs.items()}
            sent_powers = {k: v.abs().square() for k, v in sent.items()}
            R = reanalyse(X - SN.stft(torch, sources["vocal"]))
            owned = vocal_owned(torch, refs["vocal"], R)
            old_owned = powers["vocal"] > 10 * (
                reanalyse(X).abs().square() - powers["vocal"]).clamp(min=0) + 1e-9
            closure = sum(sent.values()) - reanalyse(net.apply_last_filter(X))
            row = dict(song=song, excerpt_start_s=start / SN.SR,
                       transmission={k: ratio(sent_powers[k], powers[k]) for k in sources},
                       vocal_owned_tx=ratio(sent_powers["vocal"] * owned,
                                            powers["vocal"] * owned),
                       historical_vocal_owned_tx=ratio(sent_powers["vocal"] * old_owned,
                                                       powers["vocal"] * old_owned),
                       vocal_owned_coverage=float(owned.float().mean()),
                       historical_vocal_owned_coverage=float(old_owned.float().mean()),
                       closure_nse=ratio(closure.abs().square(),
                                         reanalyse(net.apply_last_filter(X)).abs().square()),
                       bands={})
            for label, lo, hi in BANDS:
                source_energy = float(powers["eligible"][:, :, lo:hi].sum())
                output_energy = float(sent_powers["eligible"][:, :, lo:hi].sum())
                row["bands"][label] = dict(source_energy=source_energy,
                    output_energy=output_energy,
                    transmission=output_energy / source_energy if source_energy > 1e-9 else None)
            rows.append(row)
    summary = dict(transmission={k: finite_median([r["transmission"][k] for r in rows])
                                 for k in sources},
                   vocal_owned_tx=finite_median([r["vocal_owned_tx"] for r in rows]),
                   historical_vocal_owned_tx=finite_median(
                       [r["historical_vocal_owned_tx"] for r in rows]), bands={})
    for label, _, _ in BANDS:
        inputs = sum(r["bands"][label]["source_energy"] for r in rows)
        outputs = sum(r["bands"][label]["output_energy"] for r in rows)
        summary["bands"][label] = dict(
            energy_weighted_db=10 * np.log10(max(outputs / inputs, 1e-12)) if inputs > 1e-9 else None,
            per_song_median_tx=finite_median([r["bands"][label]["transmission"] for r in rows]),
            largest_song_input_fraction=max(r["bands"][label]["source_energy"] for r in rows) / inputs
                                        if inputs > 1e-9 else None)
    return dict(checkpoint=str(checkpoint), split=split_name, songs=len(rows),
                device=device, norm=getattr(net, "norm", None), summary=summary, rows=rows)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--checkpoint", type=Path, action="append", required=True)
    p.add_argument("--split", choices=("train", "dev", "test"), default="dev")
    p.add_argument("--songs", type=int, default=12)
    p.add_argument("--seconds", type=float, default=20)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--cpu", action="store_true")
    args = p.parse_args()
    if args.songs < 1 or args.seconds <= 0:
        p.error("positive songs and seconds required")
    if args.cpu:
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
    result = dict(scope="source attribution on selection songs; no product certification",
                  models=[audit(args.cache, ck, args.split, args.songs, args.seconds)
                          for ck in args.checkpoint])
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    for model in result["models"]:
        print(json.dumps({k: v for k, v in model.items() if k != "rows"}, allow_nan=False))


if __name__ == "__main__":
    main()
