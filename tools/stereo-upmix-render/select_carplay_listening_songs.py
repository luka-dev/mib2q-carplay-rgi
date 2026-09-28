#!/usr/bin/env python3
"""Find CarPlay listening passages in the owner's music via cached teacher stems.

This is a passage finder, not a quality metric or a listening result. It reads
the existing htdemucs four-stem cache on CPU and uses the vocal transitions
from diag_transitions.py (2-second, 10 dB crossings, quarter-second refine).
The cached "other" stem only says non-vocal/non-bass/non-drum material is
present; instrument identity still needs an ear check before the list is used.
"""
import argparse
import json
from pathlib import Path
import random
import re

import numpy as np

SR = 48000
HALF = SR // 2
STRIDE = 8
KINDS = ("entry", "exit", "instrumental", "dense", "exposed")


def powers(stem):
    """Stereo energy in 0.5-second blocks, sampled at 6 kHz for fast scanning."""
    count = stem.shape[-1] // HALF
    if not count:
        return np.empty(0)
    x = np.asarray(stem[:, :count * HALF:STRIDE], dtype=np.float32) / 32768.0
    return np.mean(x.reshape(2, count, HALF // STRIDE) ** 2, axis=(0, 2))


def db(power):
    return 10 * np.log10(np.maximum(power, 1e-12))


def stem_mean(p, start_s, end_s):
    lo, hi = max(0, int(start_s * 2)), min(len(p), int(end_s * 2))
    return float(p[lo:hi].mean()) if hi > lo else 0.0


def refined_crossing(vocal, coarse_s, kind):
    """The nearest directed quarter-second midpoint crossing, as in diag_transitions."""
    q = SR // 4
    a = max(0, int((coarse_s - 2) * SR))
    b = min(vocal.shape[-1], int((coarse_s + 2) * SR))
    a -= a % q
    count = (b - a) // q
    if count < 3:
        return None
    x = np.asarray(vocal[:, a:a + count * q:STRIDE], dtype=np.float32) / 32768
    lv = db(np.mean(x.reshape(2, count, q // STRIDE) ** 2, axis=(0, 2)))
    mid = 0.5 * (lv.max() + lv.min())
    if kind == "entry":
        choices = [i for i in range(1, count) if lv[i - 1] < mid <= lv[i]]
    else:
        choices = [i for i in range(1, count) if lv[i - 1] >= mid > lv[i]]
    return (a + min(choices, key=lambda i: abs(a + i * q - coarse_s * SR)) * q) / SR if choices else None


def transitions(vocal, vp, op, dp, bp):
    v2 = db(np.array([vp[i:i + 4].mean() for i in range(0, len(vp) - 3, 4)]))
    out = []
    duration = len(vp) / 2
    for k in range(1, len(v2)):
        change = v2[k] - v2[k - 1]
        kind = "entry" if change > 10 and v2[k] > -40 else "exit" if change < -10 and v2[k - 1] > -40 else None
        if kind is None:
            continue
        t = refined_crossing(vocal, k * 2, kind)
        if t is None or t < 12 or t > duration - 14:
            continue
        left, right = t - 4, t + 4
        other = min(stem_mean(op, left, t), stem_mean(op, t, right))
        protection = max(stem_mean(dp, left, right) + stem_mean(bp, left, right), 1e-9)
        if db(other) < -40:
            continue
        score = abs(change) + 3 * np.log10(max(other / protection, 1e-5))
        out.append(dict(kind=kind, center_s=round(t, 2), start_s=round(t - 10, 2),
                        end_s=round(t + 14, 2), score=round(float(score), 2),
                        vocal_change_db=round(float(change), 1), other_db=round(float(db(other)), 1)))
    return out


def scenes(vp, op, dp, bp):
    out = []
    duration = len(vp) / 2
    for center in range(12, int(duration) - 14, 2):
        v = stem_mean(vp, center - 6, center + 6)
        o = stem_mean(op, center - 6, center + 6)
        d = stem_mean(dp, center - 6, center + 6)
        b = stem_mean(bp, center - 6, center + 6)
        if o < 10 ** (-40 / 10):
            continue
        vd, od = float(db(v)), float(db(o))
        candidates = {
            "instrumental": od - 0.6 * max(vd + 45, 0) if vd < -42 else -1e3,
            "dense": min(vd, od, float(db(d))) if vd > -36 and od > -34 and float(db(d)) > -35 else -1e3,
            "exposed": vd + 3 * np.log10(max(v / (o + d + b + 1e-9), 1e-5)) if vd > -34 else -1e3,
        }
        for kind, score in candidates.items():
            if score > -100:
                out.append(dict(kind=kind, center_s=center, start_s=center - 10,
                                end_s=center + 14, score=round(float(score), 2),
                                vocal_db=round(vd, 1), other_db=round(od, 1), drum_db=round(float(db(d)), 1)))
    return out


def scan_one(meta, cache):
    path = cache / (meta.stem + ".npy")
    a = np.load(path, mmap_mode="r")
    if a.ndim != 3 or a.shape[:2] != (4, 2):
        return []
    vp, dp, bp, op = (powers(a[i]) for i in range(4))
    rows = transitions(a[0], vp, op, dp, bp) + scenes(vp, op, dp, bp)
    source = json.loads(meta.read_text(encoding="utf-8"))["source"]
    title_artist = re.sub(r"\.(mp3|m4a|flac|wav)$", "", source, flags=re.I)
    title_artist = re.sub(r"_[A-Za-z0-9]{20,}$", "", title_artist)
    for row in rows:
        row.update(cache_id=meta.stem, source=source, title_artist=title_artist,
                   duration_s=round(a.shape[-1] / SR, 1))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cache", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--limit", type=int, default=0, help="0 scans every complete four-stem cache")
    ap.add_argument("--seed", type=int, default=1609)
    a = ap.parse_args()
    files = [p for p in a.cache.glob("t*.json") if (a.cache / (p.stem + ".npy")).exists()]
    files.sort()
    if a.limit:
        rng = random.Random(a.seed)
        files = rng.sample(files, min(a.limit, len(files)))
    rows = []
    for i, meta in enumerate(files, 1):
        try:
            rows.extend(scan_one(meta, a.cache))
        except (OSError, ValueError, KeyError) as e:
            print(f"skip {meta.stem}: {type(e).__name__}", flush=True)
        if i % 25 == 0:
            print(f"scanned {i}/{len(files)}", flush=True)
    best = []
    per_song = []
    for kind in KINDS:
        ranked = sorted((r for r in rows if r["kind"] == kind), key=lambda r: r["score"], reverse=True)
        seen = set()
        for row in ranked:
            if row["cache_id"] not in seen:
                per_song.append(row)
                if len(seen) < 25:
                    best.append(row)
                seen.add(row["cache_id"])
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(dict(schema="carplay-passage-screen-v1", scanned=len(files),
                                     caveat="htdemucs other is not an instrument identity label; confirm by ear",
                                     candidates=best, per_song=per_song), indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"saved {len(best)} distinct-song/category rows to {a.out}", flush=True)


if __name__ == "__main__":
    main()
