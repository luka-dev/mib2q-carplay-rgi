#!/usr/bin/env python3
"""Decompose saved oracle-audio rear-share swings into front and rear gains.

Uses the already located one-second extreme windows. All gains are measured in
cells where the true stem dominates by at least 10 dB and are normalized by
that stem's own energy. This is a renderer diagnostic, not source attribution
or an in-car result.
"""

import argparse
import json
import math
from pathlib import Path

import numpy as np

import upmix_behavior_tests as behavior
import upmix_stem_tests as harness


def db_ratio(a, b):
    return 10 * math.log10((a + 1e-20) / (b + 1e-20))


def components(lead, rest, out6, time_s):
    lead_power = behavior.stft_pow(lead[0]) + behavior.stft_pow(lead[1])
    rest_power = behavior.stft_pow(rest[0]) + behavior.stft_pow(rest[1])
    front = sum(behavior.stft_pow(out6[c]) for c in (0, 1, 2))
    rear = sum(behavior.stft_pow(out6[c]) for c in (4, 5))
    n = min(len(lead_power), len(front))
    owned = lead_power[:n] > 10 * (rest_power[:n] + 1e-12)
    owned[:, :int(300 * behavior.N_FFT / behavior.SR)] = False
    dt = 5 * behavior.HOP / behavior.SR
    width = int(1 / dt) * 5
    result = []
    for t in time_s:
        start = round(t / dt) * 5
        stop = min(start + width, n)
        frames = []
        for s in range(start, stop, 5):
            own = owned[s:s + 5]
            if own.sum() <= 20:
                continue
            source = float(lead_power[s:s + 5][own].sum())
            f = float(front[s:s + 5][own].sum())
            r = float(rear[s:s + 5][own].sum())
            if source > 0 and f > 0 and r > 0:
                frames.append(dict(time_s=s * behavior.HOP / behavior.SR,
                                   owned_cells=int(own.sum()), source=source,
                                   front=f, rear=r, share=r / (f + r),
                                   front_per_source=f / source, rear_per_source=r / source))
        if len(frames) <= int(1 / dt) / 2:
            raise ValueError(f"too few valid 53-ms frames at {t:.2f}s")
        median_share = float(np.median([frame["share"] for frame in frames]))
        representative = min(frames, key=lambda frame: abs(frame["share"] - median_share))
        result.append(dict(time_s=t, median_share=median_share, valid_frames=len(frames),
                           median_rear_gain_db=float(np.median([
                               db_ratio(frame["rear"], frame["source"]) for frame in frames])),
                           median_front_gain_db=float(np.median([
                               db_ratio(frame["front"], frame["source"]) for frame in frames])),
                           representative=representative))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--oracle-run", type=Path, required=True)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--timing", type=Path, required=True)
    p.add_argument("--allow-share-change", action="store_true",
                   help="compare a saved renderer variant at the original baseline's swing windows")
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    track_ids = json.loads(a.manifest.read_text(encoding="utf-8"))["song_track_ids"]
    traces = json.loads(a.timing.read_text(encoding="utf-8"))
    if len(traces) != 10 or len({row["song"] for row in traces}) != 8:
        raise ValueError("expected the original ten traces across eight oracle swing songs")
    result = []
    for song in sorted({row["song"] for row in traces}):
        _, stems = harness.load_song(a.moisesdb / track_ids[song], 60.0)
        n = min(x.shape[1] for x in stems.values())
        mix = sum(x[:, :n] for x in stems.values())
        gain = 0.7 / (np.abs(mix).max() + 1e-9)
        out6 = behavior.read_wav6(a.oracle_run / f"{song}.6ch.wav")
        for row in (r for r in traces if r["song"] == song):
            stem = row["stem"]
            lead = stems[stem][:, :n] * gain
            rest = (mix - stems[stem][:, :n]) * gain
            low, high = components(lead, rest, out6, (row["low_s"], row["high_s"]))
            if not a.allow_share_change and (abs(low["median_share"] - row["low_share"]) > 0.01 or
                                             abs(high["median_share"] - row["high_share"]) > 0.01):
                raise ValueError(f"one-second median no longer matches saved timing: {song}/{stem}")
            low_rep, high_rep = low["representative"], high["representative"]
            rear_delta = high["median_rear_gain_db"] - low["median_rear_gain_db"]
            front_delta = high["median_front_gain_db"] - low["median_front_gain_db"]
            result.append(dict(song=song, stem=stem,
                               original_share_delta=row["high_share"] - row["low_share"],
                               measured_share_delta=high["median_share"] - low["median_share"],
                               rear_gain_delta_db=rear_delta,
                               front_gain_delta_db=front_delta,
                               depth_ratio_delta_db=rear_delta - front_delta,
                               representative_rear_gain_delta_db=db_ratio(
                                   high_rep["rear_per_source"], low_rep["rear_per_source"]),
                               representative_front_gain_delta_db=db_ratio(
                                   high_rep["front_per_source"], low_rep["front_per_source"]),
                               larger_change="rear" if abs(rear_delta) > abs(front_delta) else "front",
                               low=low, high=high))
        print(f"{song}: {sum(r['song'] == song for r in result)} traces", flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Decomposed {len(result)} oracle swing traces; {a.out}")


if __name__ == "__main__":
    main()
