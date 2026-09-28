#!/usr/bin/env python3
"""Locate one-second rear-depth extremes in the full-harness oracle-audio run.

This uses the existing C renders, true MoisesDB stems, and the same rear-share
tracker as upmix_stem_tests. It is a mechanism diagnostic, not the official
source-attribution transition score or a deployable result.
"""

import argparse
import csv
import json
import math
from pathlib import Path

import numpy as np

import upmix_behavior_tests as behavior
import upmix_stem_tests as harness
from diag_transitions import events


SR = behavior.SR


def rms_db(x):
    return 20 * math.log10(math.sqrt(float(np.mean(x * x)) + 1e-12))


def one_second_extremes(share, weight, dt):
    valid = np.where((weight > 20) & np.isfinite(share), share, np.nan)
    width = int(1 / dt)
    offset = int(2 / dt)
    windows = []
    for start in range(offset, len(valid) - width, width):
        block = valid[start:start + width]
        if np.isfinite(block).sum() > width / 2:
            windows.append((start * dt, float(np.nanmedian(block))))
    if not windows:
        return None
    lo = min(windows, key=lambda x: x[1])
    hi = max(windows, key=lambda x: x[1])
    return lo, hi


def audio_db(x, t):
    start = round(t * SR)
    return rms_db(x[:, start:start + SR])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--oracle-run", type=Path, required=True)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    manifest = json.loads(a.manifest.read_text(encoding="utf-8"))
    song_ids = manifest["song_track_ids"]
    swings = set(manifest["swing_songs"])
    with (a.oracle_run / "stems.csv").open(newline="", encoding="utf-8-sig") as f:
        original = [r for r in csv.DictReader(f) if r["song"] in swings and float(r["range_1s"]) > 0.30]
    if {r["song"] for r in original} != swings:
        raise ValueError("manifest swings do not match the original C results")

    output = []
    for song in sorted(swings):
        meta, stems = harness.load_song(a.moisesdb / song_ids[song], 60.0)
        n = min(x.shape[1] for x in stems.values())
        stems = {name: audio[:, :n] for name, audio in stems.items()}
        mix = sum(stems.values())
        gain = 0.7 / (np.abs(mix).max() + 1e-9)
        out6 = behavior.read_wav6(a.oracle_run / f"{song}.6ch.wav")
        vocal = stems.get("vocals")
        entries = [] if vocal is None else [s / SR for kind, s in events(vocal) if kind == "entry"]
        for row in (r for r in original if r["song"] == song):
            stem = row["stem"]
            lead = stems[stem] * gain
            rest = (mix - stems[stem]) * gain
            share, weight, dt = behavior.rear_share(lead, rest, out6)
            extreme = one_second_extremes(share, weight, dt)
            if extreme is None:
                raise ValueError(f"no tracked windows for {song} / {stem}")
            (low_t, low), (high_t, high) = extreme
            measured = float(row["range_1s"])
            if abs(high - low - measured) > 0.02:
                raise ValueError(f"tracker mismatch for {song} / {stem}: {high-low:.3f} vs {measured:.3f}")
            output.append(dict(song=song, stem=stem, range_1s=measured,
                               low_s=round(low_t, 2), low_share=round(low, 3),
                               high_s=round(high_t, 2), high_share=round(high, 3),
                               vocal_db_low=None if vocal is None else round(audio_db(vocal, low_t), 1),
                               vocal_db_high=None if vocal is None else round(audio_db(vocal, high_t), 1),
                               stem_db_low=round(audio_db(stems[stem], low_t), 1),
                               stem_db_high=round(audio_db(stems[stem], high_t), 1),
                               nearest_entry_to_low_s=None if not entries else round(min(abs(e-low_t) for e in entries), 2),
                               nearest_entry_to_high_s=None if not entries else round(min(abs(e-high_t) for e in entries), 2)))
        print(f"{song}: {sum(r['song'] == song for r in output)} swinging stems", flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(f"Located {len(output)} swinging stem traces across {len(swings)} songs; {a.out}")


if __name__ == "__main__":
    main()
