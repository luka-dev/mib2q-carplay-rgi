#!/usr/bin/env python3
"""C-render source-level stress; source attenuation is NOT Sound Check."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

import numpy as np
import soundfile as sf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--models", type=Path, nargs="+", required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--ids", nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    items = [item for item in manifest["items"] if item["id"] in args.ids]
    if {item["id"] for item in items} != set(args.ids):
        raise RuntimeError("unknown source IDs")
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for item in items:
        original, rate = sf.read(item["stereo"], dtype="float64", always_2d=True)
        if rate != 48000 or original.shape[1] != 2:
            raise RuntimeError("expected stereo 48k")
        for level in (0, -6, -12):
            source = args.output / f'{item["id"]}-{level}db.wav'
            sf.write(source, original * 10 ** (level / 20), rate, subtype="PCM_16")
            actual, _ = sf.read(source, always_2d=True)
            source_power = np.mean(actual ** 2)
            for index, model in enumerate(args.models):
                output = args.output / f'{item["id"]}-{level}db-model{index}.wav'
                result = subprocess.run([str(args.renderer.resolve()), "--model", str(model.resolve()),
                    "--float-render", "--surround", ".25", "--lfe", "assist", str(source), str(output)],
                    text=True, capture_output=True, check=True)
                pcm, _ = sf.read(output, always_2d=True)
                if pcm.shape != (len(actual), 6) or not np.isfinite(pcm).all():
                    raise RuntimeError("bad rendered PCM")
                rear_power = np.mean(pcm[:, 4:6] ** 2)
                rows.append({"id": item["id"], "model": str(model), "input_gain_db": level,
                    "rear_vs_input_db": 10 * math.log10((rear_power + 1e-30) / (source_power + 1e-30)),
                    "diagnostics": result.stderr})
        print(item["id"], flush=True)
    summaries = []
    for model in args.models:
        spans = []
        for item in items:
            levels = [row["rear_vs_input_db"] for row in rows
                      if row["model"] == str(model) and row["id"] == item["id"]]
            spans.append(max(levels) - min(levels))
        summaries.append({"model": str(model), "sha256": hashlib.sha256(model.read_bytes()).hexdigest(),
                          "mean_rear_transfer_span_db": float(np.mean(spans)),
                          "max_rear_transfer_span_db": max(spans)})
    report = {"rows": rows, "summary": summaries,
        "limits": ["C float renderer, actual S16 source attenuation; includes model AND guard effects.",
                   "Smaller level variation does not by itself prove better separation or listening quality."]}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(summaries), flush=True)


if __name__ == "__main__":
    main()
