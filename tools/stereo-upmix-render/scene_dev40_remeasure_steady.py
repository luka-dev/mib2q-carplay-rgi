#!/usr/bin/env python3
"""Re-measure a finished harness run's 6-ch renders against the MoisesDB stems with two extra
stability columns: range of 1 s medians from 2 s (= harness range_1s), from 5 s (steady range, the
stage has formed) and the start-up rise (median of 5-10 s minus median of 2-4 s).
Usage: remeasure_steady.py RUN_DIR MOISESDB OUT_CSV [JOBS]"""
import os, sys, json, csv, glob, re, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import upmix_stem_tests as U, upmix_behavior_tests as T
run, moises, out_csv = sys.argv[1], sys.argv[2], sys.argv[3]
jobs = int(sys.argv[4]) if len(sys.argv) > 4 else 3
def one(sid):
    rows_p = os.path.join(run, sid + ".rows.json"); comp_p = os.path.join(run, sid + ".complete.json")
    rows = json.load(open(rows_p)) if os.path.exists(rows_p) else json.load(open(comp_p)).get("rows", [])
    if not rows: return []
    tag = rows[0]["song"]
    p6 = os.path.join(run, sid + ".6ch.wav")
    if not os.path.exists(p6): p6 = os.path.join(run, tag + ".6ch.wav")
    if not os.path.exists(p6): return []
    meta, stems = U.load_song(os.path.join(moises, sid), 60.0)
    n = min(x.shape[1] for x in stems.values()); stems = {k: v[:, :n] for k, v in stems.items()}
    mix = sum(stems.values()); k = 0.7 / (np.abs(mix).max() + 1e-9)
    out6 = T.read_wav6(p6)[:, :n]
    res = []
    for name, stem in stems.items():
        lead = stem * k; rest = (mix - stem) * k
        if (lead ** 2).sum() < 1e-3 * (mix ** 2).sum(): continue
        share, w, dt = T.rear_share(lead, rest, out6)
        s = np.where((w > 20) & ~np.isnan(share), share, np.nan)
        w1 = int(1 / dt)
        def meds(t0):
            body = s[int(t0 / dt):]
            return [np.nanmedian(body[i:i + w1]) for i in range(0, len(body) - w1, w1) if np.sum(~np.isnan(body[i:i + w1])) > w1 / 2]
        m2, m5 = meds(2.0), meds(5.0)
        early = [np.nanmedian(s[int(t / dt):int((t + 1) / dt)]) for t in (2, 3)]
        mid = [np.nanmedian(s[int(t / dt):int((t + 1) / dt)]) for t in range(5, 10)]
        res.append({"song": tag, "stem": name, "median_rear": float(np.nanmedian(s)),
                    "range_2s": (max(m2) - min(m2)) if m2 else float("nan"),
                    "range_5s": (max(m5) - min(m5)) if m5 else float("nan"),
                    "rise": (np.nanmedian(mid) - np.nanmedian(early)) if (m5 and not np.all(np.isnan(early))) else float("nan")})
    print(tag, len(res), flush=True)
    return res
sids = sorted({os.path.basename(p).split(".")[0] for p in glob.glob(os.path.join(run, "*.rows.json")) + glob.glob(os.path.join(run, "*.complete.json")) if "-" in os.path.basename(p)})
from concurrent.futures import ThreadPoolExecutor
with ThreadPoolExecutor(jobs) as ex:
    allrows = [r for rs in ex.map(one, sids) for r in rs]
with open(out_csv, "w", newline="") as fh:
    wr = csv.DictWriter(fh, ["song", "stem", "median_rear", "range_2s", "range_5s", "rise"]); wr.writeheader(); wr.writerows(allrows)
print("rows", len(allrows), "songs", len(sids))
