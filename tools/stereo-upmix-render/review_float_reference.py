#!/usr/bin/env python3
"""Screen a render-only V11 reference bundle; makes no training-gain claim."""
import argparse
import hashlib
import json
from pathlib import Path

V11_SHA = "ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = json.loads(args.report.read_text())
    if report["model_sha256"] != V11_SHA:
        raise RuntimeError("reference-only screen requires exact V11")
    rows = report["rows"]
    ids = {row["id"] for row in rows}
    if len(ids) != 14 or len(rows) != 56:
        raise RuntimeError("expected complete 14-excerpt/four-variant regression")
    for item in ids:
        variants = {r["variant"] for r in rows if r["id"] == item}
        if variants != {"legacy", "float-unity", "float-minus9", "float-plus1"}:
            raise RuntimeError("missing render variant")
    legacy = all(r["legacy_bit_exact_to_reference"] is True for r in rows if r["variant"] == "legacy")
    over = sum(c["preclip_samples"] for r in rows if r["variant"] != "legacy"
               for c in r["input_matrix_buses"]["channels"].values())
    result = {"kind": "render-only-V11-reference", "model_sha256": V11_SHA,
        "renderer_sha256": report["renderer_sha256"],
        "offline_screen_passed": legacy and over == 0,
        "legacy_bit_exact": legacy, "float_matrix_over_range_samples": over,
        "report_sha256": {str(args.report): hashlib.sha256(args.report.read_bytes()).hexdigest()},
        "new_trained_model_selected": False, "vehicle_approved": False,
        "limitations": ["Retains V11 because trained pilots failed separation screens or offered no meaningful gain.",
                       "Only render compatibility and known-matrix headroom are screened here; not acoustic quality.",
                       "Float dynamics require listening. No complete Virtual Cabin/acoustic simulation."]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
