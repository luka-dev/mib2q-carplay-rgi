#!/usr/bin/env python3
"""Freeze an enriched renderer screen from the original 40-song oracle run.

Every original any-stem swing is included, plus deterministic nonswinging
controls. The screen is for mechanism diagnosis only; a winner must be tested
on the original full 40 and the untouched 20-song holdout.
"""

import argparse
import csv
import json
import random
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--oracle-run", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--controls", type=int, default=12)
    p.add_argument("--seed", type=int, default=1609)
    a = p.parse_args()
    if a.controls < 0:
        p.error("--controls must be nonnegative")

    with (a.oracle_run / "stems.csv").open(newline="", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))
    songs = {r["song"] for r in rows}
    swings = {r["song"] for r in rows if float(r["range_1s"]) > 0.30}
    if len(songs) != 40 or not swings:
        raise ValueError(f"expected 40 original oracle songs and swings, got {len(songs)} and {len(swings)}")
    controls = sorted(songs - swings)
    if a.controls > len(controls):
        p.error(f"only {len(controls)} nonswinging controls available")
    picked_controls = sorted(random.Random(a.seed).sample(controls, a.controls))
    selected = swings | set(picked_controls)

    track_ids = {}
    for path in a.oracle_run.glob("*.rows.json"):
        cached = json.loads(path.read_text(encoding="utf-8"))
        if not cached:
            continue
        title = cached[0]["song"]
        if title in track_ids:
            raise ValueError(f"duplicate cached song: {title}")
        track_ids[title] = path.name.removesuffix(".rows.json")
    missing = selected - track_ids.keys()
    if missing:
        raise ValueError(f"original oracle cache lacks selected songs: {sorted(missing)}")

    a.out.mkdir(parents=True, exist_ok=True)
    split = {track_ids[song]: "probe" for song in sorted(selected)}
    (a.out / "split.json").write_text(json.dumps({"split": split}, indent=2) + "\n", encoding="utf-8")
    with (a.out / "baseline_stems.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(r for r in rows if r["song"] in selected)
    manifest = {
        "source": str(a.oracle_run),
        "scope": "enriched mechanism screen, not an unbiased validation set",
        "swing_threshold": "any stem range_1s > 0.30",
        "original_songs": len(songs),
        "swing_songs": sorted(swings),
        "controls": picked_controls,
        "track_ids": split,
    }
    (a.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Frozen {len(swings)} swing songs + {len(picked_controls)} controls = {len(selected)}")


if __name__ == "__main__":
    main()
