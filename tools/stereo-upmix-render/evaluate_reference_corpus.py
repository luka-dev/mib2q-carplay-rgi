#!/usr/bin/env python3
"""Render and score stereo upmix backends against controlled 5.1 fixtures."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy import signal


BACKENDS = (
    "hsr-td", "hsr-td-active", "music-stage", "hssr-neon", "hssr-active",
    "hssr-object", "hssr-object-v2", "hssr-object-focus",
    "ambient-lite", "ambient-hq",
)
SQRT_HALF = math.sqrt(0.5)
MAX_LAG = 2048


def read_audio(path: Path, expected_channels: int) -> np.ndarray:
    samples, sample_rate = sf.read(path, dtype="float64", always_2d=True)
    if sample_rate != 48_000 or samples.shape[1] != expected_channels:
        raise RuntimeError(
            f"{path}: expected 48 kHz/{expected_channels} channels, "
            f"got {sample_rate} Hz/{samples.shape[1]} channels"
        )
    return samples


def pearson(left: np.ndarray, right: np.ndarray) -> float:
    left = left.ravel() - float(np.mean(left))
    right = right.ravel() - float(np.mean(right))
    denominator = math.sqrt(float(np.dot(left, left)) * float(np.dot(right, right)))
    if denominator <= 1e-20:
        return 0.0
    return float(np.dot(left, right) / denominator)


def stereo_best_lag(target: np.ndarray, estimate: np.ndarray) -> int:
    correlation = sum(
        signal.correlate(estimate[:, channel], target[:, channel], mode="full", method="fft")
        for channel in range(2)
    )
    lags = signal.correlation_lags(estimate.shape[0], target.shape[0], mode="full")
    keep = np.abs(lags) <= MAX_LAG
    return int(lags[keep][np.argmax(np.abs(correlation[keep]))])


def mono_best_lag(target: np.ndarray, estimate: np.ndarray) -> int:
    correlation = signal.correlate(estimate, target, mode="full", method="fft")
    lags = signal.correlation_lags(estimate.size, target.size, mode="full")
    keep = np.abs(lags) <= MAX_LAG
    return int(lags[keep][np.argmax(np.abs(correlation[keep]))])


def align(target: np.ndarray, estimate: np.ndarray, lag: int) -> tuple[np.ndarray, np.ndarray]:
    if lag > 0:
        target = target[:-lag]
        estimate = estimate[lag:]
    elif lag < 0:
        target = target[-lag:]
        estimate = estimate[:lag]
    frame_count = min(target.shape[0], estimate.shape[0])
    return target[:frame_count], estimate[:frame_count]


def si_sdr(target: np.ndarray, estimate: np.ndarray) -> float:
    target = target.ravel()
    estimate = estimate.ravel()
    target_energy = float(np.dot(target, target))
    if target_energy <= 1e-20:
        return -120.0
    scale = float(np.dot(estimate, target)) / target_energy
    wanted = scale * target
    residual = estimate - wanted
    return 10.0 * math.log10(
        (float(np.dot(wanted, wanted)) + 1e-20)
        / (float(np.dot(residual, residual)) + 1e-20)
    )


def raw_snr(target: np.ndarray, estimate: np.ndarray) -> float:
    error = estimate - target
    return 10.0 * math.log10(
        (float(np.sum(target * target)) + 1e-20)
        / (float(np.sum(error * error)) + 1e-20)
    )


def energy_envelope(stereo: np.ndarray, frames: int = 1024, hop: int = 512) -> np.ndarray:
    if stereo.shape[0] < frames:
        return np.array([math.sqrt(float(np.mean(stereo * stereo)))])
    values = []
    for start in range(0, stereo.shape[0] - frames + 1, hop):
        block = stereo[start : start + frames]
        values.append(math.sqrt(float(np.mean(block * block)) + 1e-20))
    return np.asarray(values)


def interchannel_correlation(stereo: np.ndarray) -> float:
    return pearson(stereo[:, 0], stereo[:, 1])


def fold_down(surround: np.ndarray) -> np.ndarray:
    return np.column_stack(
        (
            surround[:, 0] + SQRT_HALF * surround[:, 2] + SQRT_HALF * surround[:, 4],
            surround[:, 1] + SQRT_HALF * surround[:, 2] + SQRT_HALF * surround[:, 5],
        )
    )


def score(
    reference: np.ndarray, stereo: np.ndarray, prediction: np.ndarray,
    front_gain: float,
) -> dict[str, object]:
    frame_count = min(reference.shape[0], stereo.shape[0], prediction.shape[0])
    reference = reference[:frame_count]
    stereo = stereo[:frame_count]
    prediction = prediction[:frame_count]
    target_rear = reference[:, 4:6]
    predicted_rear = prediction[:, 4:6]

    lag = stereo_best_lag(target_rear, predicted_rear)
    aligned_target, aligned_prediction = align(target_rear, predicted_rear, lag)
    target_power = float(np.mean(aligned_target * aligned_target))
    predicted_power = float(np.mean(aligned_prediction * aligned_prediction))
    target_envelope = energy_envelope(aligned_target)
    predicted_envelope = energy_envelope(aligned_prediction)
    envelope_count = min(target_envelope.size, predicted_envelope.size)

    reference_non_lfe_power = np.mean(reference[:, [0, 1, 2, 4, 5]] ** 2, axis=0)
    prediction_non_lfe_power = np.mean(prediction[:, [0, 1, 2, 4, 5]] ** 2, axis=0)
    reference_rear_fraction = float(
        np.sum(reference_non_lfe_power[3:]) / (np.sum(reference_non_lfe_power) + 1e-20)
    )
    predicted_rear_fraction = float(
        np.sum(prediction_non_lfe_power[3:]) / (np.sum(prediction_non_lfe_power) + 1e-20)
    )

    target_lfe = reference[:, 3]
    predicted_lfe = prediction[:, 3]
    lfe_lag = mono_best_lag(target_lfe, predicted_lfe)
    aligned_lfe_target, aligned_lfe_prediction = align(
        target_lfe[:, np.newaxis], predicted_lfe[:, np.newaxis], lfe_lag
    )
    lfe_target_envelope = energy_envelope(aligned_lfe_target)
    lfe_prediction_envelope = energy_envelope(aligned_lfe_prediction)
    lfe_envelope_count = min(lfe_target_envelope.size, lfe_prediction_envelope.size)
    lfe_target_power = float(np.mean(aligned_lfe_target * aligned_lfe_target))
    lfe_prediction_power = float(np.mean(aligned_lfe_prediction * aligned_lfe_prediction))
    lfe_reference_active = lfe_target_power > 1e-10

    bus_l = 0.794328 * prediction[:, 0] + 0.398107 * prediction[:, 3] + 0.707946 * prediction[:, 4]
    bus_r = 0.794328 * prediction[:, 1] + 0.398107 * prediction[:, 3] + 0.707946 * prediction[:, 5]
    bus_c = 0.501187 * (prediction[:, 0] + prediction[:, 1]) + 0.316228 * prediction[:, 3]
    bus_lfe = 1.412538 * prediction[:, 3]
    bus_ls = 0.199526 * prediction[:, 3] + 1.995262 * prediction[:, 4]
    bus_rs = 0.199526 * prediction[:, 3] + 1.995262 * prediction[:, 5]
    visible_bus_peak = max(
        float(np.max(np.abs(bus)))
        for bus in (bus_l, bus_r, bus_c, bus_lfe, bus_ls, bus_rs)
    )

    scaled_stereo = stereo * front_gain
    predicted_front = prediction[:, 0:2]
    front_lag = stereo_best_lag(scaled_stereo, predicted_front)
    aligned_front_target, aligned_front = align(
        scaled_stereo, predicted_front, front_lag
    )
    predicted_fold = fold_down(prediction)
    fold_lag = stereo_best_lag(scaled_stereo, predicted_fold)
    aligned_fold_target, aligned_fold = align(
        scaled_stereo, predicted_fold, fold_lag
    )

    return {
        "rear_best_lag_frames": lag,
        "rear_waveform_correlation": pearson(aligned_target, aligned_prediction),
        "rear_si_sdr_db": si_sdr(aligned_target, aligned_prediction),
        "rear_envelope_correlation": pearson(
            target_envelope[:envelope_count], predicted_envelope[:envelope_count]
        ),
        "rear_energy_ratio": math.sqrt((predicted_power + 1e-20) / (target_power + 1e-20)),
        "reference_rear_energy_fraction": reference_rear_fraction,
        "predicted_rear_energy_fraction": predicted_rear_fraction,
        "reference_rear_lr_correlation": interchannel_correlation(aligned_target),
        "predicted_rear_lr_correlation": interchannel_correlation(aligned_prediction),
        "rear_lr_correlation_error": abs(
            interchannel_correlation(aligned_prediction)
            - interchannel_correlation(aligned_target)
        ),
        "front_best_lag_frames": front_lag,
        "front_preservation_snr_db": raw_snr(
            aligned_front_target, aligned_front
        ),
        "foldback_best_lag_frames": fold_lag,
        "foldback_snr_db": raw_snr(aligned_fold_target, aligned_fold),
        "lfe_reference_active": lfe_reference_active,
        "lfe_best_lag_frames": lfe_lag if lfe_reference_active else None,
        "lfe_envelope_correlation": (
            pearson(
                lfe_target_envelope[:lfe_envelope_count],
                lfe_prediction_envelope[:lfe_envelope_count],
            )
            if lfe_reference_active
            else None
        ),
        "lfe_si_sdr_db": (
            si_sdr(aligned_lfe_target, aligned_lfe_prediction)
            if lfe_reference_active
            else None
        ),
        "lfe_energy_ratio": (
            math.sqrt(
                (lfe_prediction_power + 1e-20) / (lfe_target_power + 1e-20)
            )
            if lfe_reference_active
            else None
        ),
        "visible_soundamp_bus_peak": visible_bus_peak,
    }


def render(
    renderer: Path,
    stereo: Path,
    output: Path,
    backend: str,
    lfe: str,
    surround_gain: float,
    hssr_model: Path | None,
) -> None:
    command = [
        str(renderer),
        "--mode",
        backend,
        "--lfe",
        lfe,
        "--surround",
        f"{surround_gain:.6f}",
    ]
    if backend in ("hssr-neon", "hssr-active", "hssr-object",
                   "hssr-object-v2", "hssr-object-focus"):
        if hssr_model is None:
            raise RuntimeError(f"{backend} requires --hssr-model")
        command.extend(["--model", str(hssr_model)])
    command.extend([str(stereo), str(output)])
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode:
        raise RuntimeError(
            f"renderer failed for {backend}/{stereo}:\n{completed.stderr.strip()}"
        )


def mean(rows: list[dict[str, object]], backend: str, key: str) -> float:
    values = [
        float(row[key])
        for row in rows
        if row["backend"] == backend and row.get(key) is not None
    ]
    return float(np.mean(values))


def maximum(rows: list[dict[str, object]], backend: str, key: str) -> float:
    values = [
        float(row[key])
        for row in rows
        if row["backend"] == backend and row.get(key) is not None
    ]
    return max(values)


def metric(row: dict[str, object], key: str, digits: int = 3) -> str:
    value = row.get(key)
    return "n/a" if value is None else f"{float(value):.{digits}f}"


def markdown_report(
    manifest: dict[str, object],
    rows: list[dict[str, object]],
    backends: tuple[str, ...],
    lfe: str,
    surround_gain: float,
) -> str:
    controlled_items = [
        item
        for item in manifest["items"]
        if item.get(
            "include_in_aggregate", item["dataset"] != "Xiph.org test media"
        )
    ]
    controlled_ids = {item["id"] for item in controlled_items}
    controlled_rows = [row for row in rows if row["id"] in controlled_ids]
    aggregate_groups: list[tuple[str, str, list[dict[str, object]]]] = []
    for group, label in (
        ("genre-development", "Development/tuning song-fixture aggregate"),
        ("held-out", "Held-out validation song-fixture aggregate"),
        ("authored-music", "Authored-music aggregate"),
        ("apple-public-preview", "Apple public-preview aggregate"),
    ):
        group_rows = [
            row for row in controlled_rows if row.get("evaluation_group") == group
        ]
        if group_rows:
            aggregate_groups.append((group, label, group_rows))
    if not aggregate_groups:
        aggregate_groups.append(
            ("controlled", "Controlled song-fixture aggregate", controlled_rows)
        )

    corpus_kind = manifest.get("corpus_kind")
    if corpus_kind == "apple-public-preview-authored-surround":
        report_title = "# Apple public-preview stereo-to-5.1 evaluation"
        corpus_intro = [
            "This corpus uses official Apple public AAC previews as the stereo inputs.",
            "Each preview is waveform-aligned to a publicly downloadable publisher-authored",
            "5.0/Atmos sample of the exact same recording. The stereo and surround editions",
            "may be separately mastered, so waveform metrics are comparative rather than",
            "a claim that Apple supplied the surround reference.",
        ]
        lfe_intro = (
            "Only the Atmos trumpet reference contains authored LFE; "
            f"the candidate LFE output is `{lfe}`."
        )
    elif corpus_kind == "archive-authored-surround":
        report_title = "# Archive.org six-channel music evaluation"
        corpus_intro = [
            "Every source was verified as a real six-channel stream with ffprobe.",
            "The evaluator uses a deterministic rear-active excerpt and its exact canonical",
            "stereo fold-down. This makes the channel reference exact even when a separately",
            "mixed stereo edition is unavailable. Source licenses remain recorded per item.",
        ]
        lfe_intro = (
            "Authored LFE is omitted from the canonical stereo fold; "
            f"the candidate LFE output is `{lfe}`."
        )
    elif corpus_kind == "authored-surround":
        report_title = "# HSR-TD controlled 5.1 fold-down evaluation"
        corpus_intro = [
            "This corpus contains publisher-authored 5.0/Atmos music excerpts.",
            "The technical EBU channel-identification item is reported per-track but excluded",
            "from the music aggregate. Every evaluated stereo input is the documented canonical",
            "fold-down of its exact surround reference; separately mastered publisher stereo",
            "files are retained in the dataset but are not used for objective scoring.",
        ]
        lfe_intro = f"LFE is omitted from that fold and the candidate LFE output is `{lfe}`."
    else:
        report_title = "# HSR-TD controlled 5.1 fold-down evaluation"
        corpus_intro = [
            "The main corpus contains controlled 5.1 fixtures rendered from open multitrack",
            "stems, not artist-authored surround masters. An optional Xiph.org row is a real",
            "authored film master and is kept out of the song-fixture aggregates. Development",
            "fixtures used for tuning and held-out fixtures used only for validation are reported",
            "separately. Every stereo input is the documented fold-down of its reference.",
        ]
        lfe_intro = f"LFE is omitted from that fold and the candidate LFE output is `{lfe}`."

    lines = [
        report_title,
        "",
        *corpus_intro,
        lfe_intro,
        "The digital center is not scored because the MHI hook deliberately leaves C at zero",
        "for the vehicle SoundAmp to derive.",
        "",
        f"All backends use rear gain `{surround_gain:.3f}` and LFE `{lfe}`. Higher envelope correlation,",
        "waveform correlation, SI-SDR, front SNR, and fold-back SNR are better; lower",
        "rear L/R correlation error is better. Energy ratio is descriptive, not a score.",
        "",
    ]
    for group_index, (_group, label, group_rows) in enumerate(aggregate_groups):
        if group_index:
            lines.append("")
        lines += [
            f"## {label}",
            "",
            "| backend | rear envelope corr | rear waveform corr | rear SI-SDR dB | rear L/R corr error | rear energy ratio | front SNR dB | fold-back SNR dB |",
            "|---|---:|---:|---:|---:|---:|---:|---:|",
        ]
        for backend in backends:
            lines.append(
                f"| {backend} | {mean(group_rows, backend, 'rear_envelope_correlation'):.3f} "
                f"| {mean(group_rows, backend, 'rear_waveform_correlation'):.3f} "
                f"| {mean(group_rows, backend, 'rear_si_sdr_db'):.2f} "
                f"| {mean(group_rows, backend, 'rear_lr_correlation_error'):.3f} "
                f"| {mean(group_rows, backend, 'rear_energy_ratio'):.3f} "
                f"| {mean(group_rows, backend, 'front_preservation_snr_db'):.2f} "
                f"| {mean(group_rows, backend, 'foldback_snr_db'):.2f} |"
            )

        if lfe == "assist":
            if corpus_kind == "apple-public-preview-authored-surround":
                lfe_explanation = [
                    "Only the Atmos trumpet item has authored reference LFE.",
                    "Its score measures whether restrained bass assist follows that channel;",
                    "the other five items contribute only to the SoundAmp peak statistics.",
                ]
            else:
                lfe_explanation = [
                    "LFE is absent from the stereo fold-down as a discrete channel, so these",
                    "figures measure how well restrained bass assist follows the reference bass",
                    "envelope; they are not evidence that the original LFE was recovered.",
                ]
            lines += [
                "",
                f"### {label.removesuffix(' aggregate')} LFE aggregate",
                "",
                *lfe_explanation,
                "",
                "| backend | LFE envelope corr | LFE SI-SDR dB | LFE energy ratio | SoundAmp bus peak mean/max |",
                "|---|---:|---:|---:|---:|",
            ]
            for backend in backends:
                lines.append(
                    f"| {backend} | {mean(group_rows, backend, 'lfe_envelope_correlation'):.3f} "
                    f"| {mean(group_rows, backend, 'lfe_si_sdr_db'):.2f} "
                    f"| {mean(group_rows, backend, 'lfe_energy_ratio'):.3f} "
                    f"| {mean(group_rows, backend, 'visible_soundamp_bus_peak'):.3f}/"
                    f"{maximum(group_rows, backend, 'visible_soundamp_bus_peak'):.3f} |"
                )

    lines += [
        "",
        "## Per track",
        "",
        "| genre | track | backend | env corr | waveform corr | SI-SDR dB | L/R corr error | energy ratio | lag frames | fold-back SNR dB |",
        "|---|---|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['genre']} | {row['title']} | {row['backend']} "
            f"| {float(row['rear_envelope_correlation']):.3f} "
            f"| {float(row['rear_waveform_correlation']):.3f} "
            f"| {float(row['rear_si_sdr_db']):.2f} "
            f"| {float(row['rear_lr_correlation_error']):.3f} "
            f"| {float(row['rear_energy_ratio']):.3f} "
            f"| {int(row['rear_best_lag_frames'])} "
            f"| {float(row['foldback_snr_db']):.2f} |"
        )

    if lfe == "assist":
        lines += [
            "",
            "## Per-track LFE",
            "",
            "| genre | track | backend | envelope corr | SI-SDR dB | energy ratio | lag frames | SoundAmp bus peak |",
            "|---|---|---|---:|---:|---:|---:|---:|",
        ]
        for row in rows:
            lines.append(
                f"| {row['genre']} | {row['title']} | {row['backend']} "
                f"| {metric(row, 'lfe_envelope_correlation')} "
                f"| {metric(row, 'lfe_si_sdr_db', 2)} "
                f"| {metric(row, 'lfe_energy_ratio')} "
                f"| {metric(row, 'lfe_best_lag_frames', 0)} "
                f"| {float(row['visible_soundamp_bus_peak']):.3f} |"
            )

    lines += [
        "",
        "## Interpretation limits",
        "",
        "A stereo fold-down is non-invertible: no upmixer can recover source channels",
        "uniquely. These measurements are regression and comparative evidence only.",
        "Waveform metrics also penalize intentional decorrelation in the ambience backends.",
        "Listening tests and on-vehicle measurements remain required.",
        "",
        f"Controlled song entries: {len(controlled_ids)}; total entries: {len(manifest['items'])}; backend renders: {len(rows)}.",
        "",
    ]
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--backends",
        nargs="+",
        choices=BACKENDS,
        default=list(BACKENDS),
    )
    parser.add_argument("--lfe", choices=("off", "assist"), default="off")
    parser.add_argument("--surround", type=float, default=0.18)
    parser.add_argument("--hssr-model", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 0.0 <= args.surround <= 0.25:
        raise RuntimeError("--surround must be between 0.0 and 0.25")
    backends = tuple(args.backends)
    if ({"hssr-neon", "hssr-active", "hssr-object", "hssr-object-v2",
         "hssr-object-focus"} &
            set(backends)) and args.hssr_model is None:
        raise RuntimeError("--hssr-model is required for HSSR backends")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    args.output.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []

    for item in manifest["items"]:
        reference_path = Path(item["reference_5_1"])
        stereo_path = Path(item["fold_down_stereo"])
        reference = read_audio(reference_path, 6)
        stereo = read_audio(stereo_path, 2)
        for backend in backends:
            render_path = args.output / item["id"] / f"{backend}.wav"
            render_path.parent.mkdir(parents=True, exist_ok=True)
            render(
                args.renderer.resolve(),
                stereo_path,
                render_path,
                backend,
                args.lfe,
                args.surround,
                args.hssr_model.resolve() if args.hssr_model else None,
            )
            prediction = read_audio(render_path, 6)
            front_gain = 30802.0 / 32768.0 if args.lfe == "assist" \
                else 32670.0 / 32768.0
            metrics = score(reference, stereo, prediction, front_gain)
            rows.append(
                {
                    "id": item["id"],
                    "title": item["title"],
                    "genre": item["genre"],
                    "dataset": item["dataset"],
                    "evaluation_group": item.get(
                        "evaluation_group", "genre-development"
                    ),
                    "backend": backend,
                    **metrics,
                }
            )
            print(
                f"{item['genre']}: {backend}: "
                f"env={metrics['rear_envelope_correlation']:.3f} "
                f"SI-SDR={metrics['rear_si_sdr_db']:.2f} dB"
            )

    result = {
        "schema": 1,
        "configuration": {
            "backends": list(backends),
            "sample_rate": 48_000,
            "surround_gain": args.surround,
            "front_gain": (
                30802.0 / 32768.0 if args.lfe == "assist"
                else 32670.0 / 32768.0
            ),
            "lfe": args.lfe,
            "maximum_alignment_lag_frames": MAX_LAG,
            "center_scored": False,
            "hssr_model": (
                str(args.hssr_model.resolve()) if args.hssr_model else None
            ),
        },
        "results": rows,
    }
    json_path = args.output / "results.json"
    markdown_path = args.output / "report.md"
    json_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    markdown_path.write_text(
        markdown_report(manifest, rows, backends, args.lfe, args.surround),
        encoding="utf-8",
    )
    print(json_path)
    print(markdown_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
