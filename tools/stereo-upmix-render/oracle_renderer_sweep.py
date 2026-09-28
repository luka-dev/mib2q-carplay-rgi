#!/usr/bin/env python3
"""One MoisesDB load per song across real-C renderer conditions.

The source can be isolated oracle-eligible audio, an ideal bounded real-mask
audio estimate, a frozen sep_net student estimate, or Track A's frozen
deployable mask binary with no injection.
A 20-song enriched set is a mechanism screen;
the full DEV and independent test sets provide broader validation.
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
    "rear_air_off_gain0427": ["--rear-air", "-40", "--surround", "0.427"],
    "front_retention025": ["--front-retention", "0.25"],
    "front_retention075": ["--front-retention", "0.75"],
    "front_retention070": ["--front-retention", "0.70"],
    "direct_object": [],
    "direct_no_bed": [],
    "direct_no_bed_body0": ["--rear-body", "0"],
    "direct_no_bed_body0_cal": ["--rear-body", "0", "--surround", "0.3235"],
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
    p.add_argument("--render-overrides", default="",
                   help="comma-separated point=renderer.exe overrides for host-only architecture comparisons")
    p.add_argument("--model", type=Path, required=True)
    p.add_argument("--guard", type=Path, required=True)
    p.add_argument("--moisesdb", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--source", choices=("oracle", "irm", "role_power", "student", "production"), default="oracle")
    p.add_argument("--student-ckpt", type=Path,
                   help="sep_net checkpoint for --source student; same frozen estimate in every C condition")
    p.add_argument("--student-gain", type=float, default=1.0,
                   help="positive scalar on student eligible audio before both C conditions (for matched rear level)")
    p.add_argument("--mask-bin", type=Path, help="Track A's exported mask binary for --source production")
    p.add_argument("--expected-songs", type=int, default=20)
    p.add_argument("--points", default=",".join(POINTS),
                   help="comma-separated condition names, including baseline")
    p.add_argument("--skip-original-verify", action="store_true",
                   help="new test-split songs have no previous C baseline CSV")
    a = p.parse_args()
    if a.source == "production" and (a.mask_bin is None or not a.skip_original_verify):
        p.error("--source production requires --mask-bin and --skip-original-verify")
    if a.source == "production" and not a.mask_bin.is_file():
        p.error(f"missing production mask binary: {a.mask_bin}")
    if a.source == "student" and (a.student_ckpt is None or not a.student_ckpt.is_file()):
        p.error("--source student requires an existing --student-ckpt")
    if a.source != "student" and a.student_ckpt is not None:
        p.error("--student-ckpt is only valid with --source student")
    if not math.isfinite(a.student_gain) or a.student_gain <= 0 or (a.source != "student" and a.student_gain != 1.0):
        p.error("--student-gain must be positive and requires --source student")
    names = [name.strip() for name in a.points.split(",")]
    if not names or names[0] != "baseline" or len(names) != len(set(names)) or any(name not in POINTS for name in names):
        p.error("--points must begin with baseline and contain distinct known conditions")
    selected_points = {name: POINTS[name] for name in names}
    overrides = {}
    if a.render_overrides:
        for item in a.render_overrides.split(","):
            name, separator, path = item.partition("=")
            if not separator or name not in selected_points or name == "baseline" or not path:
                p.error("--render-overrides needs distinct non-baseline point=path entries")
            if name in overrides:
                p.error(f"duplicate renderer override: {name}")
            overrides[name] = path
    for name in ("direct_object", "direct_no_bed", "direct_no_bed_body0",
                 "direct_no_bed_body0_cal"):
        if name in selected_points and name not in overrides:
            p.error(f"{name} requires a separately named host binary in --render-overrides")
    manifest = json.loads((a.probe / "manifest.json").read_text(encoding="utf-8"))
    song_ids = manifest["song_track_ids"]
    if len(song_ids) != a.expected_songs:
        raise ValueError(f"expected {a.expected_songs} frozen songs, got {len(song_ids)}")
    original = None if a.skip_original_verify else csv_rows(a.probe / "baseline_stems.csv")
    if original is not None and {r["song"] for r in original} != set(song_ids):
        raise ValueError("baseline CSV does not match the frozen manifest")

    harness.INJECT = (a.source if a.source in ("oracle", "irm", "role_power") else
                      f"student:{a.student_ckpt}" if a.source == "student" else "")
    harness.INJECT_SMOOTH = 0.0
    harness.INJECT_GAIN = a.student_gain
    harness.ORACLE_REF = ""
    harness.MASK_DIR = ""
    harness.TEACHER_MASK = False
    harness.RESUME = True
    if a.source in ("oracle", "irm", "role_power", "student"):
        common = behavior.VARIANTS["shipped"] + ["--model", str(a.model), "--guard-model", str(a.guard),
                                                  "--mask-drive", "1.0"]
    else:
        common = behavior.VARIANTS["cand_noair"] + ["--model", str(a.model), "--guard-model", str(a.guard),
                                                       "--mask-model", str(a.mask_bin), "--mask-drive-prod", "1.5",
                                                       "--vocal-gate", "0.5", "--rear-air", "-9"]
    folders = {name: a.probe / name for name in selected_points}
    for folder in folders.values():
        folder.mkdir(parents=True, exist_ok=True)

    for index, (song, track_id) in enumerate(sorted(song_ids.items()), 1):
        missing = [name for name, folder in folders.items() if cached_rows(folder, track_id) is None]
        loaded = harness.load_song(a.moisesdb / track_id, 60.0) if missing else None
        for name, extra in selected_points.items():
            rows = harness.analyse_song(overrides.get(name, str(a.render)), common + extra, str(folders[name]),
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
