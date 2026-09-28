#!/usr/bin/env python3
"""Paired stem-harness comparison; require explicit scope before certification.

Both CSVs must cover the same songs. Bootstrap units are songs, not stem rows.
The report preserves absolute operating points alongside paired differences so
that a quieter mask cannot appear better merely by suppressing everything.
"""
import argparse
import csv
import json
import math
from pathlib import Path

import numpy as np

STEMS = ("vocals", "guitar", "piano", "other_keys", "bowed_strings",
         "wind", "other", "bass", "drums", "percussion")
METRICS = ("median_rear", "slope_per_6dB", "rear_nse", "rear_rms_db")


def read(path):
    with path.open(newline="", encoding="utf-8-sig") as f:
        records = list(csv.DictReader(f))
    if not records or not {"song", "stem", "median_rear", "range_1s"}.issubset(records[0]):
        raise ValueError(f"missing required stem-harness columns: {path}")
    out = {}
    for r in records:
        key = (r["song"], r["stem"])
        if key in out:
            raise ValueError(f"duplicate song/stem {key}: {path}")
        out[key] = r
    return out


def number(r, field):
    try:
        x = float(r.get(field, ""))
        return x if math.isfinite(x) else None
    except (TypeError, ValueError):
        return None


def med_ci(pairs, rng, trials):
    a = np.asarray(pairs, np.float64)
    base, cand = np.median(a, axis=0)
    delta = cand - base
    paired = float(np.median(a[:, 1] - a[:, 0]))
    idx = rng.integers(0, len(a), size=(trials, len(a)))
    sampled = np.median((a[:, 1] - a[:, 0])[idx], axis=1)
    lo, hi = np.quantile(sampled, (0.05, 0.95))
    return dict(n=len(a), anchor_median=round(float(base), 4),
                candidate_median=round(float(cand), 4),
                difference_of_medians=round(float(delta), 4),
                paired_median_delta=round(paired, 4),
                paired_median_delta_ci90=[round(float(lo), 4), round(float(hi), 4)])


def swing_songs(rows, songs):
    return {s for (s, _), r in rows.items() if s in songs and (number(r, "range_1s") or 0) > 0.30}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--anchor-csv", type=Path, required=True)
    p.add_argument("--candidate-csv", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--bootstrap", type=int, default=5000)
    p.add_argument("--seed", type=int, default=1609)
    p.add_argument("--expected-songs", type=int, default=0,
                   help="fail if the paired harness has any other song count")
    a = p.parse_args()
    anchor, candidate = read(a.anchor_csv), read(a.candidate_csv)
    anchor_songs = {s for s, _ in anchor}
    candidate_songs = {s for s, _ in candidate}
    if anchor_songs != candidate_songs:
        raise ValueError(f"screens differ: only anchor={sorted(anchor_songs - candidate_songs)}, "
                         f"only candidate={sorted(candidate_songs - anchor_songs)}")
    songs = sorted(anchor_songs)
    if a.expected_songs and len(songs) != a.expected_songs:
        raise ValueError(f"expected {a.expected_songs} paired songs, found {len(songs)}")
    rng = np.random.default_rng(a.seed)
    metrics = {}
    for stem in STEMS:
        fields = {}
        for field in METRICS:
            pairs = []
            for song in songs:
                k = (song, stem)
                if k not in anchor or k not in candidate:
                    continue
                x, y = number(anchor[k], field), number(candidate[k], field)
                if x is not None and y is not None:
                    pairs.append((x, y))
            if pairs:
                fields[field] = med_ci(pairs, rng, a.bootstrap)
        if fields:
            metrics[stem] = fields
    bs, cs = swing_songs(anchor, set(songs)), swing_songs(candidate, set(songs))
    result = dict(scope="paired stem harness; held-out, causality, device budget and listening are separate gates",
                  anchor=str(a.anchor_csv), candidate=str(a.candidate_csv),
                  songs=len(songs), rows_anchor=len(anchor), rows_candidate=len(candidate),
                  any_stem_swing=dict(anchor=len(bs), candidate=len(cs),
                                      new=sorted(cs - bs), resolved=sorted(bs - cs)),
                  by_stem=metrics)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{len(songs)} paired songs; any-stem swing {len(bs)} -> {len(cs)}, "
          f"new {len(cs - bs)}, resolved {len(bs - cs)}")
    for stem in STEMS:
        item = metrics.get(stem, {}).get("median_rear")
        if item:
            print(f"{stem:15} n={item['n']:2} rear {item['anchor_median']:.3f} -> "
                  f"{item['candidate_median']:.3f}; paired delta {item['paired_median_delta']:+.3f} "
                  f"90% CI {item['paired_median_delta_ci90']}")


if __name__ == "__main__":
    main()
