#!/usr/bin/env python3
"""Measure HSSR rear transfer separately on offline teacher stems."""

from __future__ import annotations

import argparse
import json
import hashlib
import math
import subprocess
from pathlib import Path

import numpy as np
import soundfile as sf


TEACHERS = {
    "demucs": {
        "suffix": "htdemucs_6s",
        "stems": ("Bass", "Drums", "Guitar", "Other", "Piano", "Vocals"),
    },
    "roformer": {
        "suffix": "model_bs_roformer_ep_317_sdr_12",
        "stems": ("Instrumental", "Vocals"),
    },
}


def read_audio(path: Path, channels: int) -> np.ndarray:
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != 48_000 or audio.shape[1] != channels:
        raise RuntimeError(f"unexpected audio format: {path}")
    return audio


def rms(audio: np.ndarray) -> float:
    return math.sqrt(float(np.mean(audio * audio)) + 1.0e-30)


def db(numerator: float, denominator: float) -> float:
    return 20.0 * math.log10((numerator + 1.0e-30) / (denominator + 1.0e-30))


def locate_stem(directory: Path, stem: str, suffix: str) -> Path:
    matches = list(directory.glob(f"*_({stem})_{suffix}.wav"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one {stem} stem in {directory}")
    return matches[0]


def render(
    renderer: Path, model: Path, backend: str, source: Path, output: Path,
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(renderer), "--mode", backend, "--lfe", "off",
        "--surround", "0.25", "--model", str(model), str(source), str(output),
    ]
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode:
        raise RuntimeError(completed.stderr.strip())


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--teacher-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--backend", choices=("hssr-universal",), default="hssr-universal",
    )
    parser.add_argument(
        "--teacher", choices=tuple(TEACHERS), default="demucs",
        help="stem family to evaluate (default: demucs)",
    )
    parser.add_argument("--ids", nargs="*")
    parser.add_argument(
        "--discard-renders",
        action="store_true",
        help="delete generated 5.1 WAVs after scoring to conserve disk space",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    item_by_id = {str(item["id"]): item for item in manifest["items"]}
    selected = set(args.ids or item_by_id)
    teacher = TEACHERS[args.teacher]
    stems = teacher["stems"]
    suffix = str(teacher["suffix"])
    rows: list[dict[str, object]] = []

    for item_id in sorted(selected):
        item = item_by_id[item_id]
        teacher_dir = args.teacher_root / item_id
        mixture = read_audio(Path(str(item["stereo"])), 2)
        mixture_rms = rms(mixture)
        for stem in stems:
            source = locate_stem(teacher_dir, stem, suffix)
            stem_audio = read_audio(source, 2)
            relative_level = db(rms(stem_audio), mixture_rms)
            output_path = args.output / item_id / f"{stem.lower()}.wav"
            render(
                args.renderer.resolve(), args.model.resolve(), args.backend,
                source, output_path,
            )
            output = read_audio(output_path, 6)
            if args.discard_renders:
                output_path.unlink()
            front_power = float(np.sum(output[:, :2] ** 2))
            rear_power = float(np.sum(output[:, 4:6] ** 2))
            rows.append(
                {
                    "id": item_id,
                    "title": item["title"],
                    "stem": stem.lower(),
                    "stem_level_vs_mix_db": relative_level,
                    "active": relative_level >= -40.0,
                    "rear_energy_fraction": rear_power
                    / (front_power + rear_power + 1.0e-30),
                    "rear_front_db": 10.0 * math.log10(
                        (rear_power + 1.0e-30) / (front_power + 1.0e-30)
                    ),
                }
            )
        print(item_id)

    aggregate: dict[str, dict[str, float | int]] = {}
    for stem in (name.lower() for name in stems):
        active = [row for row in rows if row["stem"] == stem and row["active"]]
        if not active:
            continue
        aggregate[stem] = {
            "active_excerpts": len(active),
            "mean_rear_energy_fraction": float(
                np.mean([row["rear_energy_fraction"] for row in active])
            ),
            "p90_rear_energy_fraction": float(
                np.percentile(
                    [row["rear_energy_fraction"] for row in active], 90.0
                )
            ),
            "mean_mix_rear_contribution": float(
                np.mean(
                    [
                        float(row["rear_energy_fraction"])
                        * 10.0 ** (float(row["stem_level_vs_mix_db"]) / 10.0)
                        for row in active
                    ]
                )
            ),
        }
    selection: dict[str, float] | None = None
    if "instrumental" in aggregate and "vocals" in aggregate:
        instrumental = aggregate["instrumental"]
        vocals = aggregate["vocals"]
        unweighted_ratio = (
            float(instrumental["mean_rear_energy_fraction"])
            / (float(vocals["mean_rear_energy_fraction"]) + 1.0e-30)
        )
        contribution_ratio = (
            float(instrumental["mean_mix_rear_contribution"])
            / (float(vocals["mean_mix_rear_contribution"]) + 1.0e-30)
        )
        selection = {
            "instrumental_to_vocal_rear_ratio": unweighted_ratio,
            "instrumental_to_vocal_rear_db": 10.0 * math.log10(
                unweighted_ratio + 1.0e-30
            ),
            "mix_contribution_ratio": contribution_ratio,
            "mix_contribution_ratio_db": 10.0 * math.log10(
                contribution_ratio + 1.0e-30
            ),
        }

    result = {
        "schema": 1,
        "kind": "hssr-offline-teacher-stem-routing",
        "development_only": True,
        "model_sha256": hashlib.sha256(args.model.read_bytes()).hexdigest(),
        "renderer_sha256": hashlib.sha256(args.renderer.read_bytes()).hexdigest(),
        "source_manifest": str(args.manifest.resolve()),
        "backend": args.backend,
        "teacher": args.teacher,
        "renders_retained": not args.discard_renders,
        "aggregate": aggregate,
        "selection": selection,
        "rows": rows,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "results.json").write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    lines = [
        f"# HSSR routing by {args.teacher} teacher stem", "",
        "Only stems at or above -40 dB relative to their mixture enter the mean.", "",
        (
            "`mix rear contribution` weights the routed fraction by the "
            "stem's level relative to the mixture; it is a better leakage "
            "indicator than an unweighted mean over quiet stems."
        ), "",
        (
            "| stem | active excerpts | mean rear % | mean rear/front dB "
            "| mean mix rear contribution % |"
        ),
        "|---|---:|---:|---:|---:|",
    ]
    for stem in (name.lower() for name in stems):
        active = [row for row in rows if row["stem"] == stem and row["active"]]
        if not active:
            continue
        mix_contribution = np.mean(
            [
                float(row["rear_energy_fraction"])
                * 10.0 ** (float(row["stem_level_vs_mix_db"]) / 10.0)
                for row in active
            ]
        )
        lines.append(
            f"| {stem} | {len(active)} "
            f"| {100.0 * float(np.mean([row['rear_energy_fraction'] for row in active])):.2f} "
            f"| {float(np.mean([row['rear_front_db'] for row in active])):.2f} "
            f"| {100.0 * float(mix_contribution):.3f} |"
        )
    if selection is not None:
        lines += [
            "",
            "## Instrumental selection",
            "",
            (
                "Mean instrumental/vocal rear ratio: "
                f"**{selection['instrumental_to_vocal_rear_ratio']:.1f}:1 "
                f"({selection['instrumental_to_vocal_rear_db']:.2f} dB)**."
            ),
            (
                "Mixture-level weighted contribution ratio: "
                f"**{selection['mix_contribution_ratio']:.1f}:1 "
                f"({selection['mix_contribution_ratio_db']:.2f} dB)**."
            ),
        ]
    lines += [
        "", "## Per excerpt", "",
        "| track | stem | stem vs mix dB | rear % | rear/front dB |",
        "|---|---|---:|---:|---:|",
    ]
    for row in rows:
        if row["active"]:
            lines.append(
                f"| {row['title']} | {row['stem']} "
                f"| {float(row['stem_level_vs_mix_db']):.2f} "
                f"| {100.0 * float(row['rear_energy_fraction']):.2f} "
                f"| {float(row['rear_front_db']):.2f} |"
            )
    lines.append("")
    (args.output / "report.md").write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, RuntimeError) as error:
        print(f"error: {error}")
        raise SystemExit(1)
