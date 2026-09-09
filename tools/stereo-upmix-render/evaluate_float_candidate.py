#!/usr/bin/env python3
"""Local model legacy/float A/B with explicit Sound Check protocol fixtures.

No training, uploads or vehicle control. Input-matrix metrics are NOT acoustic
quality scores. These metadata fixtures stress gain ordering; they need not
match the supplied music's actual sender metadata.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/soundamp-virtual-cabin"))
import soundamp_virtual_cabin as cabin


def fingerprint(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def score(path, matrix):
    pcm_metrics = cabin.ChannelMetrics(["L", "R", "C", "LFE", "Ls", "Rs"])
    bus_metrics = cabin.ChannelMetrics([f"bus{i}" for i in range(8)])
    with cabin.Pcm16WavReader(path) as reader:
        for block in reader.blocks():
            pcm_metrics.update(block)
            bus_metrics.update(cabin.apply_matrix(block, matrix))
    return {"pcm": pcm_metrics.report(), "input_matrix_buses": bus_metrics.report(),
            "input_matrix_aggregates": cabin.matrix_aggregates(bus_metrics)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ids", nargs="+")
    parser.add_argument("--legacy-reference", type=Path,
                        help="optional pre-change renderer for byte-exact legacy verification")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    items = [item for item in manifest["items"] if not args.ids or item["id"] in args.ids]
    if not items or (args.ids and set(args.ids) != {item["id"] for item in items}):
        parser.error("requested IDs must exist in the manifest")
    config = cabin.load_config(cabin.DEFAULT_CONFIG)
    matrix = cabin.linear_matrix(config)
    args.output.mkdir(parents=True, exist_ok=False)
    variants = {"legacy": [], "float-unity": ["--float-render"],
                "float-minus9": ["--float-render", "--soundcheck-target", "-16",
                                 "--soundcheck-params", "-6.5", "-1", "0.96875"],
                "float-plus1": ["--float-render", "--soundcheck-target", "-16",
                                "--soundcheck-params", "-18", "-1.8125", "-1.78125"]}
    rows = []
    for item in items:
        for name, flags in variants.items():
            target = args.output / item["id"] / f"{name}.wav"
            target.parent.mkdir(exist_ok=True)
            command = [str(args.renderer.resolve()), "--mode", "hssr-universal",
                       "--model", str(args.model.resolve()), "--lfe", "assist",
                       "--surround", "0.25", *flags, item["stereo"], str(target)]
            run = subprocess.run(command, check=True, capture_output=True, text=True)
            metrics = score(target, matrix)
            compatible = None
            if name == "legacy" and args.legacy_reference:
                reference = target.with_name("legacy-head-reference.wav")
                reference_command = [str(args.legacy_reference.resolve()), *command[1:-1], str(reference)]
                subprocess.run(reference_command, check=True, capture_output=True, text=True)
                compatible = fingerprint(target) == fingerprint(reference)
                if not compatible:
                    raise RuntimeError(f"legacy compatibility changed: {item['id']}")
            if name != "legacy":
                if any(channel["preclip_samples"] for channel in metrics["input_matrix_buses"]["channels"].values()):
                    raise RuntimeError(f"float candidate exceeds input-matrix headroom: {item['id']} {name}")
            rows.append({"id": item["id"], "variant": name,
                         "legacy_bit_exact_to_reference": compatible,
                         "source_sha256": fingerprint(Path(item["stereo"])),
                         "render": str(target), "diagnostics": run.stderr.strip(), **metrics})
            peak = max(channel["peak"] for channel in metrics["input_matrix_buses"]["channels"].values())
            print(f'{item["id"]} {name}: J525 input-matrix peak {peak:.6f} FS', flush=True)
    report = {"schema": 1, "model_sha256": fingerprint(args.model),
              "renderer_sha256": fingerprint(args.renderer),
              "cabin_sha256": fingerprint(cabin.DEFAULT_CONFIG),
              "operating_state": config["operating_state"], "rows": rows,
              "limits": ["All rendering variants in this report use the supplied model; this is not a cross-model quality score.",
                         "Float rail and J525 input-matrix buses share linked -1 dBFS sample protection; not true-peak protection.",
                         "Matrix feeds only the peak detector; no inverse OEM calibration is applied.",
                         "Unknown complex DSP/EQ/acoustics are not replaced by identity.",
                         "Sound Check fixtures are not asserted to describe these tracks."]}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
