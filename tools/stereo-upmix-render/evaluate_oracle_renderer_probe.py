#!/usr/bin/env python3
"""Apply the precommitted oracle-audio renderer screen gate to finished CSVs."""

import argparse
import csv
import json
import math
import statistics
from pathlib import Path


PROBE_POINTS = ("baseline", "rear_body0", "rear_air_off", "front_retention025", "front_retention075")
FULL40_POINTS = ("baseline", "rear_air_off", "front_retention075")
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


def passes(candidate, baseline, scope):
    old, new = set(baseline["swings"]), set(candidate["swings"])
    gates = {"rear_rms_within_half_db": abs(candidate["rear_rms_db"] - baseline["rear_rms_db"]) <= 0.5}
    if scope == "probe":
        gates["resolves_at_least_three"] = len(old - new) >= 3
        gates["no_new_swing"] = not (new - old)
    elif scope == "full40":
        gates["any_stem_swing_at_most_five"] = len(new) <= 5
    else:
        gates["any_stem_swing_at_most_two"] = len(new) <= 2
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
    p.add_argument("--scope", choices=("probe", "full40", "test20", "production20"), default="probe")
    a = p.parse_args()
    manifest = json.loads((a.probe / "manifest.json").read_text(encoding="utf-8"))
    points = PROBE_POINTS if a.scope == "probe" else FULL40_POINTS
    raw = {name: read(a.probe / name / "stems.csv") for name in points}
    summaries = {name: summary(rows) for name, rows in raw.items()}
    base = summaries["baseline"]
    base_keys = {(r["song"], r["stem"]) for r in raw["baseline"]}
    expected = set(manifest["song_track_ids"])
    if set(base["songs"]) != expected or (a.scope in ("probe", "full40") and set(base["swings"]) != set(manifest["swing_songs"])):
        raise ValueError("fresh baseline no longer matches the frozen screen")
    expected_songs = 40 if a.scope == "full40" else 20
    expected_rows = 115 if a.scope == "probe" else 231 if a.scope == "full40" else None
    if len(base["songs"]) != expected_songs or (expected_rows is not None and base["rows"] != expected_rows):
        raise ValueError(f"unexpected baseline size: {len(base['songs'])} songs and {base['rows']} stem rows")
    results = {}
    verdict_key = {"probe": "promote_to_full40", "full40": "meets_full40_render_gates",
                   "test20": "meets_test20_render_gates",
                   "production20": "meets_production20_screen_gates"}[a.scope]
    for name in points[1:]:
        item = summaries[name]
        if set(item["songs"]) != expected or {(r["song"], r["stem"]) for r in raw[name]} != base_keys:
            raise ValueError(f"{name} has an incomplete or different screen")
        gates = passes(item, base, a.scope)
        results[name] = dict(summary=item, resolved=sorted(set(base["swings"]) - set(item["swings"])),
                             new=sorted(set(item["swings"]) - set(base["swings"])),
                             gates=gates, **{verdict_key: all(gates.values())})
    scope = {"probe": "enriched 20-song oracle-audio mechanism screen only",
             "full40": "full 40-song oracle-audio C render; causality, cue extraction, device and listening unproven",
             "test20": "independent 20-song MoisesDB test split, oracle audio; deployability and listening unproven",
             "production20": "enriched 20-song DEV mechanism screen with frozen Track A mask and no oracle injection; not an independent product validation"}[a.scope]
    report = dict(scope=scope,
                  baseline=base, variants=results)
    (a.probe / "decision.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for name, item in results.items():
        failed = [key for key, ok in item["gates"].items() if not ok]
        print(f"{name}: swing {len(base['swings'])}->{len(item['summary']['swings'])}, "
              f"resolved {len(item['resolved'])}, new {len(item['new'])}, "
              f"{verdict_key} {'YES' if not failed else 'NO'}; failed {', '.join(failed) or 'none'}")


if __name__ == "__main__":
    main()
