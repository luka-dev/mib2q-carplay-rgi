#!/usr/bin/env python3
"""Render a stereo-only stress corpus and report activity/safety metrics.

Unlike evaluate_reference_corpus.py, this evaluator has no authored surround
target.  It therefore does not claim separation accuracy: it measures rear
activity, left/right distinction, stereo compatibility and the visible
SoundAmp matrix peaks for comparative A/B testing.
"""

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


BACKENDS = ("hssr-universal",)
HSSR_BACKENDS = {"hssr-universal"}
SQRT_HALF = math.sqrt(0.5)
MAX_LAG = 2048
BLOCK_FRAMES = 960  # 20 ms at 48 kHz.


def read_audio(path: Path, expected_channels: int) -> np.ndarray:
    samples, sample_rate = sf.read(path, dtype="float64", always_2d=True)
    if sample_rate != 48_000 or samples.shape[1] != expected_channels:
        raise RuntimeError(
            f"{path}: expected 48 kHz/{expected_channels} channels, "
            f"got {sample_rate} Hz/{samples.shape[1]} channels"
        )
    return samples


def resolve_item_path(manifest_path: Path, value: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = manifest_path.parent / path
    if not path.is_file():
        raise RuntimeError(f"audio file does not exist: {path}")
    return path.resolve()


def stereo_best_lag(target: np.ndarray, estimate: np.ndarray) -> int:
    correlation = sum(
        signal.correlate(
            estimate[:, channel], target[:, channel], mode="full", method="fft"
        )
        for channel in range(2)
    )
    lags = signal.correlation_lags(estimate.shape[0], target.shape[0], mode="full")
    keep = np.abs(lags) <= MAX_LAG
    return int(lags[keep][np.argmax(np.abs(correlation[keep]))])


def align(
    target: np.ndarray, estimate: np.ndarray, lag: int
) -> tuple[np.ndarray, np.ndarray]:
    if lag > 0:
        target = target[:-lag]
        estimate = estimate[lag:]
    elif lag < 0:
        target = target[-lag:]
        estimate = estimate[:lag]
    frames = min(target.shape[0], estimate.shape[0])
    return target[:frames], estimate[:frames]


def snr_db(reference: np.ndarray, estimate: np.ndarray) -> float:
    error = estimate - reference
    return 10.0 * math.log10(
        (float(np.sum(reference * reference)) + 1e-30)
        / (float(np.sum(error * error)) + 1e-30)
    )


def correlation(left: np.ndarray, right: np.ndarray) -> float:
    left = left.ravel() - float(np.mean(left))
    right = right.ravel() - float(np.mean(right))
    denominator = math.sqrt(float(np.dot(left, left)) * float(np.dot(right, right)))
    if denominator <= 1e-30:
        return 0.0
    return float(np.dot(left, right) / denominator)


def rms(samples: np.ndarray) -> float:
    return math.sqrt(float(np.mean(samples * samples)) + 1e-30)


def level_db(numerator: float, denominator: float) -> float:
    return 20.0 * math.log10((numerator + 1e-30) / (denominator + 1e-30))


def soundamp_buses(output: np.ndarray) -> np.ndarray:
    fl, fr, _fc, lfe, sl, sr = output.T
    return np.column_stack(
        (
            0.794328 * fl + 0.398107 * lfe + 0.707946 * sl,
            0.794328 * fr + 0.398107 * lfe + 0.707946 * sr,
            0.501187 * (fl + fr) + 0.316228 * lfe,
            1.412538 * lfe,
            0.199526 * lfe + 1.995262 * sl,
            0.199526 * lfe + 1.995262 * sr,
        )
    )


def block_side_dominance(rear: np.ndarray) -> tuple[float, float, float]:
    block_count = rear.shape[0] // BLOCK_FRAMES
    if block_count == 0:
        return 0.0, 0.0, 0.0
    blocks = rear[: block_count * BLOCK_FRAMES].reshape(
        block_count, BLOCK_FRAMES, 2
    )
    energy = np.mean(blocks * blocks, axis=1)
    total = np.sum(energy, axis=1)
    peak = float(np.max(total))
    active = total > max(peak * 1e-4, 1e-20)
    if not np.any(active):
        return 0.0, 0.0, 0.0
    balance = np.abs(
        10.0 * np.log10((energy[active, 0] + 1e-30) / (energy[active, 1] + 1e-30))
    )
    return (
        float(np.median(balance)),
        float(np.percentile(balance, 90.0)),
        float(np.mean(balance >= 3.0)),
    )


def score(stereo: np.ndarray, output: np.ndarray, front_gain: float) -> dict[str, float | int]:
    frames = min(stereo.shape[0], output.shape[0])
    stereo = stereo[:frames]
    output = output[:frames]
    front = output[:, 0:2]
    lfe = output[:, 3:4]
    rear = output[:, 4:6]

    front_power = float(np.sum(front * front))
    lfe_power = float(np.sum(lfe * lfe))
    rear_power = float(np.sum(rear * rear))
    total_power = front_power + lfe_power + rear_power + 1e-30
    rear_fraction = rear_power / total_power
    lfe_fraction = lfe_power / total_power

    scaled_stereo = front_gain * stereo
    front_lag = stereo_best_lag(scaled_stereo, front)
    aligned_target, aligned_front = align(scaled_stereo, front, front_lag)

    fold = np.column_stack(
        (
            output[:, 0] + SQRT_HALF * output[:, 2] + SQRT_HALF * output[:, 4],
            output[:, 1] + SQRT_HALF * output[:, 2] + SQRT_HALF * output[:, 5],
        )
    )
    fold_lag = stereo_best_lag(scaled_stereo, fold)
    aligned_fold_target, aligned_fold = align(scaled_stereo, fold, fold_lag)

    median_balance, p90_balance, dominant_fraction = block_side_dominance(rear)
    buses = soundamp_buses(output)
    matrix_peak = float(np.max(np.abs(buses)))
    digital_peak = float(np.max(np.abs(output)))

    return {
        "rear_energy_fraction": rear_fraction,
        "rear_front_db": level_db(rms(rear), rms(front)),
        "lfe_energy_fraction": lfe_fraction,
        "lfe_front_db": level_db(rms(lfe), rms(front)),
        "lfe_peak_dbfs": 20.0 * math.log10(
            max(float(np.max(np.abs(lfe))), 1e-30)
        ),
        "rear_lr_waveform_correlation": correlation(rear[:, 0], rear[:, 1]),
        "rear_block_balance_median_db": median_balance,
        "rear_block_balance_p90_db": p90_balance,
        "rear_blocks_over_3db_fraction": dominant_fraction,
        "front_best_lag_frames": front_lag,
        "front_preservation_snr_db": snr_db(aligned_target, aligned_front),
        "foldback_best_lag_frames": fold_lag,
        "foldback_snr_db": snr_db(aligned_fold_target, aligned_fold),
        "digital_peak": digital_peak,
        "soundamp_matrix_peak": matrix_peak,
        "soundamp_matrix_peak_dbfs": 20.0 * math.log10(max(matrix_peak, 1e-30)),
        "clipped_sample_count": int(np.count_nonzero(np.abs(output) >= 1.0)),
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
        "--mode", backend,
        "--lfe", lfe,
        "--surround", f"{surround_gain:.6f}",
    ]
    if backend in HSSR_BACKENDS:
        if hssr_model is None:
            raise RuntimeError(f"{backend} requires --hssr-model")
        command.extend(("--model", str(hssr_model)))
    command.extend((str(stereo), str(output)))
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode:
        raise RuntimeError(
            f"renderer failed for {backend}/{stereo}:\n{completed.stderr.strip()}"
        )


def values(
    rows: list[dict[str, object]], backend: str, key: str
) -> list[float]:
    return [
        float(row[key]) for row in rows
        if row["backend"] == backend and row.get(key) is not None
    ]


def mean(rows: list[dict[str, object]], backend: str, key: str) -> float:
    return float(np.mean(values(rows, backend, key)))


def maximum(rows: list[dict[str, object]], backend: str, key: str) -> float:
    return max(values(rows, backend, key))


def markdown_report(
    manifest: dict[str, object],
    rows: list[dict[str, object]],
    backends: tuple[str, ...],
    lfe: str,
    surround_gain: float,
) -> str:
    lines = [
        "# Stereo upmix stress test",
        "",
        "This corpus has no authored surround reference. The metrics below compare rear",
        "activity, SL/SR distinction, stereo compatibility and the visible SoundAmp",
        "matrix headroom; they do not measure source-separation accuracy or Atmos fidelity.",
        "Listening in the vehicle remains the deciding test.",
        "",
        f"All renders use rear gain `{surround_gain:.3f}` and LFE `{lfe}`.",
        "Rear fraction and rear/front level indicate audibility. Block balance indicates",
        "whether local events favour one rear side instead of becoming the same wash in",
        "both channels. More is not automatically better; front/fold SNR and peaks are",
        "the safety checks.",
        "",
    ]

    groups = tuple(dict.fromkeys(str(item["group"]) for item in manifest["items"]))
    for group in (*groups, "all"):
        group_rows = rows if group == "all" else [row for row in rows if row["group"] == group]
        lines += [
            f"## {group}",
            "",
            "| backend | rear energy % | rear/front dB | LFE energy % | LFE/front dB | rear L/R corr | block balance median/P90 dB | blocks >3 dB % | front SNR dB | fold SNR dB | SoundAmp peak max dBFS |",
            "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
        ]
        for backend in backends:
            lines.append(
                f"| {backend} "
                f"| {100.0 * mean(group_rows, backend, 'rear_energy_fraction'):.2f} "
                f"| {mean(group_rows, backend, 'rear_front_db'):.2f} "
                f"| {100.0 * mean(group_rows, backend, 'lfe_energy_fraction'):.2f} "
                f"| {mean(group_rows, backend, 'lfe_front_db'):.2f} "
                f"| {mean(group_rows, backend, 'rear_lr_waveform_correlation'):.3f} "
                f"| {mean(group_rows, backend, 'rear_block_balance_median_db'):.2f}/"
                f"{mean(group_rows, backend, 'rear_block_balance_p90_db'):.2f} "
                f"| {100.0 * mean(group_rows, backend, 'rear_blocks_over_3db_fraction'):.1f} "
                f"| {mean(group_rows, backend, 'front_preservation_snr_db'):.2f} "
                f"| {mean(group_rows, backend, 'foldback_snr_db'):.2f} "
                f"| {maximum(group_rows, backend, 'soundamp_matrix_peak_dbfs'):.2f} |"
            )
        lines.append("")

    lines += [
        "## Per track",
        "",
        "| genre | track | backend | rear % | rear/front dB | L/R corr | balance P90 dB | front/fold SNR dB | SoundAmp peak dBFS | clipped samples |",
        "|---|---|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['group']} | {row['artist']} — {row['title']} | {row['backend']} "
            f"| {100.0 * float(row['rear_energy_fraction']):.2f} "
            f"| {float(row['rear_front_db']):.2f} "
            f"| {float(row['rear_lr_waveform_correlation']):.3f} "
            f"| {float(row['rear_block_balance_p90_db']):.2f} "
            f"| {float(row['front_preservation_snr_db']):.2f}/"
            f"{float(row['foldback_snr_db']):.2f} "
            f"| {float(row['soundamp_matrix_peak_dbfs']):.2f} "
            f"| {int(row['clipped_sample_count'])} |"
        )

    lines += [
        "",
        "## Limits",
        "",
        "A mastered stereo mix is non-invertible and this corpus contains no true stems",
        "or authored rear channels. High rear energy can be either a useful isolated object",
        "or unwanted vocal/bass leakage. Use the generated pairs for level-matched listening",
        "and use the controlled 5.1 corpus for objective spatial-reference scoring.",
        "",
        f"Tracks: {len(manifest['items'])}; renders: {len(rows)}.",
        "",
    ]
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--backends", nargs="+", choices=BACKENDS,
        default=BACKENDS,
    )
    parser.add_argument("--lfe", choices=("off", "assist"), default="assist")
    parser.add_argument("--surround", type=float, default=0.25)
    parser.add_argument("--hssr-model", type=Path)
    parser.add_argument(
        "--discard-renders",
        action="store_true",
        help="delete generated 5.1 WAVs after scoring to conserve disk space",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 0.0 <= args.surround <= 0.25:
        raise RuntimeError("--surround must be between 0.0 and 0.25")
    if not args.renderer.is_file():
        raise RuntimeError(f"renderer does not exist: {args.renderer}")
    backends = tuple(args.backends)
    if HSSR_BACKENDS & set(backends):
        if args.hssr_model is None or not args.hssr_model.is_file():
            raise RuntimeError("--hssr-model must name an existing model for HSSR")

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    corpus_kind = manifest.get("corpus_kind")
    if corpus_kind not in {
        "apple-public-preview-stereo-upmix-stress",
        "private-local-stereo-upmix-stress",
    }:
        raise RuntimeError("manifest is not a supported stereo-only stress corpus")

    args.output.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    front_gain = 30100.0 / 32768.0 if args.lfe == "assist" else 32670.0 / 32768.0
    hssr_model = args.hssr_model.resolve() if args.hssr_model else None

    for item in manifest["items"]:
        stereo_path = resolve_item_path(args.manifest.resolve(), item["stereo"])
        stereo = read_audio(stereo_path, 2)
        for backend in backends:
            render_path = args.output / item["id"] / f"{backend}.wav"
            render_path.parent.mkdir(parents=True, exist_ok=True)
            render(
                args.renderer.resolve(), stereo_path, render_path, backend,
                args.lfe, args.surround, hssr_model,
            )
            output = read_audio(render_path, 6)
            metrics = score(stereo, output, front_gain)
            if args.discard_renders:
                render_path.unlink()
            rows.append(
                {
                    "id": item["id"],
                    "group": item["group"],
                    "artist": item["artist"],
                    "title": item["title"],
                    "stress_targets": item["stress_targets"],
                    "backend": backend,
                    **metrics,
                }
            )
            print(
                f"{item['group']:7s} {backend:17s} {item['artist']} — {item['title']}: "
                f"rear={100.0 * float(metrics['rear_energy_fraction']):.2f}% "
                f"balance-p90={float(metrics['rear_block_balance_p90_db']):.2f} dB "
                f"matrix={float(metrics['soundamp_matrix_peak_dbfs']):.2f} dBFS"
            )

    result = {
        "schema": 1,
        "description": (
            "Stereo-only HSSR stress metrics; no authored surround reference and no "
            "source-separation accuracy claim."
        ),
        "configuration": {
            "backends": list(backends),
            "sample_rate": 48_000,
            "surround_gain": args.surround,
            "front_gain": front_gain,
            "lfe": args.lfe,
            "maximum_alignment_lag_frames": MAX_LAG,
            "hssr_model": str(hssr_model) if hssr_model else None,
            "renders_retained": not args.discard_renders,
        },
        "results": rows,
    }
    json_path = args.output / "results.json"
    report_path = args.output / "report.md"
    json_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    report_path.write_text(
        markdown_report(manifest, rows, backends, args.lfe, args.surround),
        encoding="utf-8",
    )
    print(json_path)
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
