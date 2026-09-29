#!/usr/bin/env python3
"""Review 43 fixed DEV screen: prior fine-role 4k versus cross-song 4k."""

import argparse
import json
from pathlib import Path

import diag_dual_state as DD
import diag_fine_role as FD
import sep_net_fine_role as FR


def value(data, key):
    return data[key]["value"]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache", type=Path)
    p.add_argument("baseline", type=Path)
    p.add_argument("crosssong", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    checkpoints = {"coarse": a.baseline, "fine": a.crosssong}
    quality = FD.quality(a.cache, checkpoints, a.out / "quality.json")
    DD.SN = FR
    source = DD.source_attribution(a.cache, a.baseline, a.crosssong,
                                   a.out / "source.json")
    entry = DD.event_rows(a.cache,
                          {"reset2": a.baseline, "persistent": a.crosssong},
                          a.out / "transitions.json")
    entry["arm_labels"] = {"reset2": "same-song baseline", "persistent": "cross-song true stems"}
    (a.out / "transitions.json").write_text(json.dumps(entry, indent=2, allow_nan=True) + "\n",
                                               encoding="utf-8")
    base, candidate = source["anchor"]["median"], source["candidate"]["median"]
    eb, ec = entry["entry_summary"]["reset2"], entry["entry_summary"]["persistent"]
    qb, qc = quality["summary"]["coarse"], quality["summary"]["fine"]
    gate = {
        "source_coverage": candidate["eligible"]["n"] == 12 and all(
            candidate[name + "_over_eligible"]["n"] >= 8
            for name in ("vocal", "drums", "bass")),
        "eligible": value(candidate, "eligible") >= .432,
        "vocal": value(candidate, "vocal_over_eligible") <= .324,
        "drums": value(candidate, "drums_over_eligible") <= .0568,
        "bass": value(candidate, "bass_over_eligible") <= .0339,
        "eligible_vs_baseline": value(candidate, "eligible") - value(base, "eligible") >= -.03,
        "protected_vs_baseline": all(
            value(candidate, name + "_over_eligible") -
            value(base, name + "_over_eligible") <= .01
            for name in ("vocal", "drums", "bass")),
        "entry_count": eb["n"] == ec["n"] == 43,
        "entry_median": value(ec, "duck_median") >= .85,
        "entry_p10": value(ec, "duck_p10") >= .60,
        "entry_pre": value(ec, "ret_pre") >= .25,
        "entry_post": value(ec, "ret_post") >= .25,
        "entry_p10_gain": value(ec, "duck_p10") - value(eb, "duck_p10") >= .04,
        "role_auc_retained": value(qc, "auc") >= value(qb, "auc") - .05,
        "mixed_correlation_gain": value(qc, "mixed_correlation") -
                                  value(qb, "mixed_correlation") >= .10,
    }
    summary = {"scope": "first 12 inspected DEV excerpts and first 20 inspected DEV full songs;"
                        " existing 4k same-song control", "quality": quality["summary"],
               "source": {"baseline": base, "crosssong": candidate},
               "entry": {"baseline": eb, "crosssong": ec},
               "gate": gate, "promotion": all(gate.values())}
    (a.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
