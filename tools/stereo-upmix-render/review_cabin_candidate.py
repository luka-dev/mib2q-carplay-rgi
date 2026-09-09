#!/usr/bin/env python3
"""Offline non-regression screen, not automatic vehicle promotion.

Thresholds chosen before examining the V12 candidate: retain >=90% of baseline
instrumental transfer, allow <=10% vocal leakage increase, retain >=95% of
instrumental/vocal selectivity on both validation and heldout diagnostics.
These are engineering screens, not confidence intervals or listening scores.
"""
import argparse
import json
import hashlib
from pathlib import Path


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("validation-baseline", "validation-candidate", "heldout-baseline",
                 "heldout-candidate", "level-validation", "level-render", "float-report", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    checks = []
    def check(name, value, limit, comparison):
        passed = value <= limit if comparison == "<=" else value >= limit
        checks.append({"name": name, "value": value, "comparison": comparison,
                       "limit": limit, "passed": passed})
    for split in ("validation", "heldout"):
        baseline = read(getattr(args, split + "_baseline"))
        candidate = read(getattr(args, split + "_candidate"))
        for stem, threshold, comparison in (("vocals", 1.1, "<="), ("instrumental", .9, ">=")):
            key = "mean_mix_rear_contribution"
            ratio = candidate["aggregate"][stem][key] / baseline["aggregate"][stem][key]
            check(f"{split}_{stem}_transfer_ratio", ratio, threshold, comparison)
        ratio = candidate["selection"]["mix_contribution_ratio"] / baseline["selection"]["mix_contribution_ratio"]
        check(f"{split}_selectivity_ratio", ratio, .95, ">=")
    levels = read(args.level_validation)["rows"]
    models = list(dict.fromkeys(row["model"] for row in levels))
    if len(models) != 2:
        raise RuntimeError("expected baseline then candidate in level validation")
    for level in (0, -6, -12):
        loss = [next(r["metrics"]["total"] for r in levels
                     if r["model"] == model and r["input_gain_db"] == level) for model in models]
        check(f"level_{level}_loss_ratio", loss[1] / loss[0], 1.01, "<=")
    summaries = read(args.level_render)["summary"]
    if len(summaries) != 2:
        raise RuntimeError("expected two C-render model summaries")
    check("C_mean_level_variation_ratio", summaries[1]["mean_rear_transfer_span_db"] /
          max(summaries[0]["mean_rear_transfer_span_db"], 1e-9), 1.05, "<=")
    float_report = read(args.float_report)
    candidate_sha = float_report["model_sha256"]
    if any(row["sha256"] != candidate_sha for row in levels if row["model"] == models[1]):
        raise RuntimeError("candidate weights disagree between reports")
    if summaries[1]["sha256"] != candidate_sha:
        raise RuntimeError("candidate C-render weights disagree")
    for path in (args.validation_candidate, args.heldout_candidate):
        stem_report = read(path)
        if stem_report.get("model_sha256") != candidate_sha:
            raise RuntimeError("stem report missing/mismatching candidate fingerprint; re-run it")
        if stem_report.get("renderer_sha256") != float_report["renderer_sha256"]:
            raise RuntimeError("stem/mix renderer fingerprint mismatch")
    rows = float_report["rows"]
    over = sum(c["preclip_samples"] for row in rows if row["variant"] != "legacy"
               for c in row["input_matrix_buses"]["channels"].values())
    check("float_known_matrix_over_range_samples", over, 0, "<=")
    result = {"offline_screen_passed": all(c["passed"] for c in checks),
              "model_sha256": candidate_sha,
              "renderer_sha256": float_report["renderer_sha256"],
              "report_sha256": {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                                for key, path in vars(args).items() if key != "output"},
              "vehicle_approved": False, "checks": checks,
              "limitations": ["No acoustic/listening or live head-unit deadline validation.",
                              "Stem-isolated inference is not exact leakage attribution in mixtures."]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
