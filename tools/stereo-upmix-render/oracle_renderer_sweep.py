#!/usr/bin/env python3
"""One MoisesDB load per song, five real-C oracle-audio renderer conditions.

The enriched 20-song set contains every swing in the original 40-song oracle
run. This is a mechanism screen. A winner must be rerun on the original full
40 and on the untouched 20 before it can support any product claim.
"""

import argparse
import csv
import json
import math
import subprocess
import sys
from pathlib import Path

import upmix_behavior_tests as behavior
import upmix_stem_tests as harness


POINTS = {
    "baseline": [],
    "rear_body0": ["--rear-body", "0"],
    "rear_air_off": ["--rear-air", "-40"],
    "front_retention025": ["--front-retention", "0.25"],
    "front_retention075": ["--front-retention", "0.75"],
}
CHECK_FIELDS = ("median_rear", "range_1s")


def cached_rows(folder, track_id):
    path = folder / f"{track_id}.rows.json"
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else None


def csv_rows(path):
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def verify_baseline(actual, expected, song):
    got = {r["stem"]: r for r in actual}
    want = {r["stem"]: r for r in expected if r["song"] == song}
    if got.keys() != want.keys():
        raise ValueError(f"baseline stem set changed for {song}: {got.keys()} vs {want.keys()}")
    for stem in got:
        for field in CHECK_FIELDS:
            x, y = float(got[stem][field]), float(want[stem][field])
            if math.isfinite(x) != math.isfinite(y) or (math.isfinite(x) and abs(x - y) > 0.005):
                raise ValueError(f"baseline C render differs for {song}/{stem}/{field}: {x:.4f} vs {y:.4f}")


def write_screen_csv(folder, track_ids):
    rows = []
    for track_id in track_ids:
        song_rows = cached_rows(folder, track_id)
        if not song_rows:
            raise ValueError(f"missing per-song render rows: {folder / (track_id + '.rows.json')}")
        rows.extend(song_rows)
    keys = ["song", "genre", "stem", "median_rear", "steady_std", "range_1s", "worst_half_s",
            "slope_per_6dB", "level_corr", "level_db", "frames"] + harness.BAND_KEYS + ["rear_rms_db"]
    with (folder / "stems.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=keys)
        writer.writeheader()
        writer.writerows(rows)
    return len(rows)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--render", type=Path, required=True)
    p.add_argument("--model", type=Path, required=True)
    p.add_argument("--guard", type=Path, required=True)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--expected-songs", type=int, default=20)
    p.add_argument("--points", default=",".join(POINTS),
                   help="comma-separated condition names, including baseline")
    p.add_argument("--skip-original-verify", action="store_true",
                   help="new test-split songs have no previous C baseline CSV")
    a = p.parse_args()
    names = [name.strip() for name in a.points.split(",")]
    if not names or names[0] != "baseline" or len(names) != len(set(names)) or any(name not in POINTS for name in names):
        p.error("--points must begin with baseline and contain distinct known conditions")
    selected_points = {name: POINTS[name] for name in names}
    manifest = json.loads((a.probe / "manifest.json").read_text(encoding="utf-8"))
    song_ids = manifest["song_track_ids"]
    if len(song_ids) != a.expected_songs:
        raise ValueError(f"expected {a.expected_songs} frozen songs, got {len(song_ids)}")
    original = None if a.skip_original_verify else csv_rows(a.probe / "baseline_stems.csv")
    if original is not None and {r["song"] for r in original} != set(song_ids):
        raise ValueError("baseline CSV does not match the frozen manifest")

    harness.INJECT = "oracle"
    harness.INJECT_SMOOTH = 0.0
    harness.ORACLE_REF = ""
    harness.MASK_DIR = ""
    harness.TEACHER_MASK = False
    harness.RESUME = True
    common = behavior.VARIANTS["shipped"] + ["--model", str(a.model), "--guard-model", str(a.guard),
                                              "--mask-drive", "1.0"]
    folders = {name: a.probe / name for name in selected_points}
    for folder in folders.values():
        folder.mkdir(parents=True, exist_ok=True)

    for index, (song, track_id) in enumerate(sorted(song_ids.items()), 1):
        missing = [name for name, folder in folders.items() if cached_rows(folder, track_id) is None]
        loaded = harness.load_song(a.moisesdb / track_id, 60.0) if missing else None
        for name, extra in selected_points.items():
            rows = harness.analyse_song(str(a.render), common + extra, str(folders[name]),
                                        str(a.moisesdb / track_id), 60.0, loaded=loaded)
            if not rows:
                raise RuntimeError(f"C render failed: {song} / {name}")
            if name == "baseline" and original is not None:
                verify_baseline(rows, original, song)
        print(f"{index}/{len(song_ids)} {song}", flush=True)

    baseline_csv = folders["baseline"] / "stems.csv"
    for name, folder in folders.items():
        count = write_screen_csv(folder, song_ids.values())
        print(f"{name}: {count} stem rows", flush=True)
        if name == "baseline":
            continue
        subprocess.run([sys.executable, str(Path(__file__).with_name("compare_rapid_pair.py")),
                        "--anchor-csv", str(baseline_csv), "--candidate-csv", str(folder / "stems.csv"),
                        "--out", str(a.probe / f"{name}_paired.json"), "--expected-songs", str(a.expected_songs)], check=True)


if __name__ == "__main__":
    main()
