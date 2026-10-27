#!/usr/bin/env python3
"""Same summary as the Track A matched40 complete.json, from a harness stems.csv:
median over songs of per-stem median_rear; swings = songs with any stem range_1s > 0.30;
rear = mean rear_rms_db over songs. Usage: summarise_dev40.py NAME=stems.csv [NAME=stems.csv ...]"""
import csv, sys, numpy as np
STEMS = ["vocals", "bass", "drums", "guitar", "other_keys", "piano", "bowed_strings", "wind", "percussion", "other"]
def load(p):
    rows = list(csv.DictReader(open(p)))
    f = lambda v: float(v) if v not in ("", "nan", "None") else np.nan
    return [{**r, "median_rear": f(r["median_rear"]), "range_1s": f(r["range_1s"]), "rear_rms_db": f(r["rear_rms_db"])} for r in rows]
tables = [(a.split("=", 1)[0], load(a.split("=", 1)[1])) for a in sys.argv[1:]]
stems = [s for s in STEMS if any(r["stem"] == s for _, rows in tables for r in rows)]
print("| run | songs | rows | " + " | ".join(stems) + " | swings | rear dBFS |")
print("|---|---|---|" + "---|" * len(stems) + "---|---|")
for name, rows in tables:
    songs = sorted({r["song"] for r in rows})
    sw = sorted({r["song"] for r in rows if r["range_1s"] > 0.30})
    rear = np.nanmean([next(r["rear_rms_db"] for r in rows if r["song"] == s) for s in songs])
    cells = []
    for s in stems:
        v = [r["median_rear"] for r in rows if r["stem"] == s and not np.isnan(r["median_rear"])]
        cells.append(f"{np.median(v):.3f} ({len(v)})" if v else "-")
    print(f"| {name} | {len(songs)} | {len(rows)} | " + " | ".join(cells) + f" | {len(sw)}/{len(songs)} | {rear:.1f} |")
    print(f"  swings {name}: " + "; ".join(sw) if sw else f"  swings {name}: none")

# per-band medians (where the rear share lives inside an instrument)
BANDS = ["rear_300_1k", "rear_1_2k", "rear_2_4k", "rear_4_6k", "rear_6_10k", "rear_10_16k"]
print("\nrear share by band (median over songs): " + " ".join(b[5:] for b in BANDS))
for name, rows in tables:
    for s in ("vocals", "drums", "guitar", "other_keys", "other"):
        rs = [r for r in rows if r["stem"] == s]
        if not rs: continue
        f = lambda v: float(v) if v not in ("", "nan", "None") else np.nan
        print(f"  {name:8s} {s:11s} " + " ".join(f"{np.nanmedian([f(r[b]) for r in rs]):5.2f}" for b in BANDS))
