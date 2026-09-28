#!/usr/bin/env python3
"""Apply the precommitted oracle-audio renderer screen gate to finished CSVs."""

import argparse
import csv
import json
import math
import statistics
from pathlib import Path


POINTS = ("baseline", "rear_body0", "rear_air_off", "front_retention025", "front_retention075")
SHARE_LIMITS = {"guitar": (0.15, "min"), "other_keys": (0.15, "min"),
                "piano": (0.10, "min"), "bowed_strings": (0.14, "min"),
                "vocals": (0.02, "max"), "bass": (0.06, "max"), "drums": (0.08, "max")}
SLOPE_STEMS = ("guitar", "other_keys", "wind")


def median(rows, stem, field):
    values = [float(r[field]) for r in rows if r["stem"] == stem and math.isfinite(float(r[field]))]
    return statistics.median(values) if values else None


def read(path):
    with path.open(newline="", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise ValueError(f"empty stem CSV: {path}")
    return rows


def summary(rows):
    songs = {r["song"] for r in rows}
    swings = {r["song"] for r in rows if math.isfinite(float(r["range_1s"])) and float(r["range_1s"]) > 0.30}
    rms = {r["song"]: float(r["rear_rms_db"]) for r in rows}
    shares = {stem: median(rows, stem, "median_rear") for stem in SHARE_LIMITS}
    slopes = {stem: median(rows, stem, "slope_per_6dB") for stem in SLOPE_STEMS}
    return dict(songs=sorted(songs), swings=sorted(swings), rows=len(rows),
                rear_rms_db=statistics.median(rms.values()), shares=shares, slopes=slopes)


def passes(candidate, baseline):
    old, new = set(baseline["swings"]), set(candidate["swings"])
    gates = {
        "resolves_at_least_three": len(old - new) >= 3,
        "no_new_swing": not (new - old),
        "rear_rms_within_half_db": abs(candidate["rear_rms_db"] - baseline["rear_rms_db"]) <= 0.5,
    }
    for stem, (limit, direction) in SHARE_LIMITS.items():
        value = candidate["shares"][stem]
        gates[f"share_{stem}"] = value is not None and (value >= limit if direction == "min" else value <= limit)
    for stem in SLOPE_STEMS:
        value = candidate["slopes"][stem]
        gates[f"slope_{stem}"] = value is not None and abs(value) <= 0.05
    return gates


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--probe", type=Path, required=True)
    a = p.parse_args()
    manifest = json.loads((a.probe / "manifest.json").read_text(encoding="utf-8"))
    raw = {name: read(a.probe / name / "stems.csv") for name in POINTS}
    summaries = {name: summary(rows) for name, rows in raw.items()}
    base = summaries["baseline"]
    base_keys = {(r["song"], r["stem"]) for r in raw["baseline"]}
    expected = set(manifest["song_track_ids"])
    if set(base["songs"]) != expected or set(base["swings"]) != set(manifest["swing_songs"]):
        raise ValueError("fresh baseline no longer matches the frozen screen")
    if base["rows"] != 115:
        raise ValueError(f"expected 115 baseline stem rows, got {base['rows']}")
    results = {}
    for name in POINTS[1:]:
        item = summaries[name]
        if set(item["songs"]) != expected or {(r["song"], r["stem"]) for r in raw[name]} != base_keys:
            raise ValueError(f"{name} has an incomplete or different screen")
        gates = passes(item, base)
        results[name] = dict(summary=item, resolved=sorted(set(base["swings"]) - set(item["swings"])),
                             new=sorted(set(item["swings"]) - set(base["swings"])),
                             gates=gates, promote_to_full40=all(gates.values()))
    report = dict(scope="enriched 20-song oracle-audio mechanism screen only",
                  baseline=base, variants=results)
    (a.probe / "decision.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for name, item in results.items():
        failed = [key for key, ok in item["gates"].items() if not ok]
        print(f"{name}: swing {len(base['swings'])}->{len(item['summary']['swings'])}, "
              f"resolved {len(item['resolved'])}, new {len(item['new'])}, "
              f"full40 promotion {'YES' if not failed else 'NO'}; failed {', '.join(failed) or 'none'}")


if __name__ == "__main__":
    main()
