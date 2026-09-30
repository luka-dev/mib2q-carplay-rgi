#!/usr/bin/env python3
"""Summarize existing real-C rear-band CSVs without reading audio or models.

These are rear/(front+rear) ratios in source-dominant cells, not isolated
source transmissions. Missing coverage stays missing. No promotion gate is
introduced by this descriptive report.
"""

import argparse
import ast
import csv
import hashlib
import json
import math
from pathlib import Path
from statistics import median


BANDS = (
    ("rear_300_1k", 300, 1000),
    ("rear_1_2k", 1000, 2500),
    ("rear_2_4k", 2500, 4000),
    ("rear_4_6k", 4000, 6000),
    ("rear_6_10k", 6000, 10000),
    ("rear_10_16k", 10000, 16000),
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    rows = {}
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        required = {"song", "stem", *(name for name, _, _ in BANDS)}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f"{path}: missing columns {required - set(reader.fieldnames or [])}")
        for row in reader:
            key = (row["song"], row["stem"])
            if key in rows:
                raise ValueError(f"{path}: duplicate song/stem {key}")
            values = {}
            for name, _, _ in BANDS:
                value = float(row[name])
                if math.isnan(value):
                    values[name] = None
                elif not math.isfinite(value) or not 0 <= value <= 1:
                    raise ValueError(f"{path}: invalid rear fraction at {key}/{name}")
                else:
                    values[name] = value
            rows[key] = values
    return rows


def stats(values):
    finite = [v for v in values if v is not None]
    return {"median": median(finite) if finite else None,
            "finite_rows": len(finite), "missing_rows": len(values) - len(finite)}


def report(inputs, reference, expected_songs, expected_rows):
    # Read syntax only: do not import or mutate the live harness's dependencies.
    definition = Path(__file__).with_name("upmix_stem_tests.py")
    tree = ast.parse(definition.read_text())
    actual = next(ast.literal_eval(node.value) for node in tree.body
                  if isinstance(node, ast.Assign)
                  and any(isinstance(t, ast.Name) and t.id == "BANDS" for t in node.targets))
    if actual != [(lo, hi) for _, lo, hi in BANDS]:
        raise ValueError("Harness frequency boundaries changed; review this reporter")
    datasets = {label: load(path) for label, path in inputs.items()}
    ref = datasets[reference]
    keys = set(ref)
    if len(keys) != expected_rows or len({key[0] for key in keys}) != expected_songs:
        raise ValueError("Reference song/row count differs from the declared harness")
    if any(set(rows) != keys for rows in datasets.values()):
        raise ValueError("Input song/stem identities differ; paired comparison is invalid")
    output = {
        "schema": "upmix_saved_rear_spectrum_v1",
        "definition": "actual C rear power / (front + rear) power in dry-source-dominant cells",
        "limitations": [
            "Whole-excerpt band power ratios differ from temporal median_rear.",
            "Source dominance is an attribution proxy, not isolated audible source extraction.",
            "A ratio of these medians is not a fraction of source energy retained.",
            "NaN coverage is excluded and counted, never filled with zero.",
            "No attack/sustain, ear, new candidate or promotion result is inferred.",
        ],
        "reference": reference,
        "bands": [{"column": name, "lo_hz": lo, "hi_hz_exclusive": hi}
                  for name, lo, hi in BANDS],
        "band_definition_source": {"path": str(definition), "sha256": sha(definition)},
        "songs": expected_songs, "stem_rows": expected_rows,
        "song_stem_keys_sha256": hashlib.sha256(
            json.dumps(sorted(keys), ensure_ascii=False).encode()).hexdigest(),
        "inputs": {},
    }
    for label, rows in datasets.items():
        stem_stats = {}
        for stem in sorted({key[1] for key in keys}):
            stem_keys = sorted(key for key in keys if key[1] == stem)
            bands = {}
            for name, _, _ in BANDS:
                pairs = [(rows[k][name], ref[k][name]) for k in stem_keys]
                paired = [(v, r) for v, r in pairs if v is not None and r is not None]
                item = stats([rows[k][name] for k in stem_keys])
                item.update({
                    "paired_rows": len(paired),
                    "coverage_mismatch_rows": sum((v is None) != (r is None) for v, r in pairs),
                    "paired_delta_median": median(v - r for v, r in paired) if paired else None,
                    "paired_nonworse_rows": sum(v >= r for v, r in paired),
                })
                bands[name] = item
            stem_stats[stem] = {"total_rows": len(stem_keys), "bands": bands}
        output["inputs"][label] = {"path": str(inputs[label]), "sha256": sha(inputs[label]),
                                   "stems": stem_stats}
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", required=True, metavar="LABEL=CSV")
    parser.add_argument("--reference", required=True)
    parser.add_argument("--expected-songs", type=int, default=40)
    parser.add_argument("--expected-rows", type=int, default=231)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inputs = {}
    for value in args.input:
        label, path = value.split("=", 1)
        if not label or label in inputs:
            parser.error("Input labels must be nonempty and unique")
        inputs[label] = Path(path)
    if args.reference not in inputs:
        parser.error("Reference must be one of the input labels")
    result = report(inputs, args.reference, args.expected_songs, args.expected_rows)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps({"output": str(args.output), "songs": result["songs"],
                      "rows": result["stem_rows"], "labels": list(inputs)}))


if __name__ == "__main__":
    main()
