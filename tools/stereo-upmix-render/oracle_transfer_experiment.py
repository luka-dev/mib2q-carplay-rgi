#!/usr/bin/env python3
"""Measure ideal rear-object transfer policies against the vehicle matrix.

This is deliberately an oracle experiment: the authored reference rear pair is
treated as if a perfect separator had recovered it from the canonical stereo
fold.  It therefore tests routing topology, not source-separation quality.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

from prepare_reference_corpus import write_pcm16


SAMPLE_RATE = 48_000
SQRT_HALF = math.sqrt(0.5)
TARGET_MATRIX_PEAK = 10.0 ** (-0.5 / 20.0)

# Measured visible SoundAmp matrix.
MATRIX_FRONT = 0.794328
MATRIX_FRONT_LFE = 0.398107
MATRIX_FRONT_REAR = 0.707946
MATRIX_CENTER_FRONT = 0.501187
MATRIX_CENTER_LFE = 0.316228
MATRIX_LFE = 1.412538
MATRIX_REAR_LFE = 0.199526
MATRIX_REAR = 1.995262

# Canonical transfer preserves L = FL + sqrt(1/2) * SL.  To additionally
# cancel a fully transferred oracle rear component at the physical front bus,
# front subtraction must remove both its original stereo-fold contribution and
# the matrix's new rear-to-front crossfeed.
PHYSICAL_ALPHA = SQRT_HALF + MATRIX_FRONT_REAR / MATRIX_FRONT
COMPROMISE_ALPHA = 0.5 * (SQRT_HALF + PHYSICAL_ALPHA)

POLICIES = {
    "additive": 0.0,
    "canonical": SQRT_HALF,
    "compromise": COMPROMISE_ALPHA,
    "physical": PHYSICAL_ALPHA,
}


def read_audio(path: Path, channels: int) -> np.ndarray:
    command = [
        "ffmpeg",
        "-v",
        "error",
        "-nostdin",
        "-i",
        str(path),
        "-map",
        "0:a:0",
        "-ar",
        str(SAMPLE_RATE),
        "-ac",
        str(channels),
        "-c:a",
        "pcm_f32le",
        "-f",
        "f32le",
        "pipe:1",
    ]
    try:
        completed = subprocess.run(command, check=True, capture_output=True)
    except FileNotFoundError as error:
        raise RuntimeError("ffmpeg is required for the oracle experiment") from error
    except subprocess.CalledProcessError as error:
        message = error.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"ffmpeg failed for {path}: {message}") from error
    decoded = np.frombuffer(completed.stdout, dtype="<f4")
    if decoded.size == 0 or decoded.size % channels:
        raise RuntimeError(f"ffmpeg returned malformed {channels}-channel PCM for {path}")
    return decoded.reshape(-1, channels).astype(np.float64)


def resolve_item_path(manifest_path: Path, value: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = manifest_path.parent / path
    if not path.is_file():
        raise RuntimeError(f"audio file does not exist: {path}")
    return path.resolve()


def render(stereo: np.ndarray, oracle_rear: np.ndarray, scale: float, alpha: float) -> np.ndarray:
    frames = min(stereo.shape[0], oracle_rear.shape[0])
    stereo = stereo[:frames]
    rear = scale * oracle_rear[:frames]
    output = np.zeros((frames, 6), dtype=np.float64)
    output[:, 0:2] = stereo - alpha * rear
    output[:, 4:6] = rear
    # FC is zero because the SoundAmp derives center.  LFE is excluded so this
    # experiment measures rear transfer independently of bass assist.
    return output


def soundamp_buses(output: np.ndarray) -> np.ndarray:
    fl, fr, _fc, lfe, sl, sr = output.T
    return np.column_stack(
        (
            MATRIX_FRONT * fl + MATRIX_FRONT_LFE * lfe + MATRIX_FRONT_REAR * sl,
            MATRIX_FRONT * fr + MATRIX_FRONT_LFE * lfe + MATRIX_FRONT_REAR * sr,
            MATRIX_CENTER_FRONT * (fl + fr) + MATRIX_CENTER_LFE * lfe,
            MATRIX_LFE * lfe,
            MATRIX_REAR_LFE * lfe + MATRIX_REAR * sl,
            MATRIX_REAR_LFE * lfe + MATRIX_REAR * sr,
        )
    )


def rms(samples: np.ndarray) -> float:
    return math.sqrt(float(np.mean(samples * samples)) + 1e-30)


def level_db(numerator: float, denominator: float) -> float:
    return 20.0 * math.log10((numerator + 1e-30) / (denominator + 1e-30))


def snr_db(reference: np.ndarray, estimate: np.ndarray) -> float:
    frames = min(reference.shape[0], estimate.shape[0])
    reference = reference[:frames]
    estimate = estimate[:frames]
    return 10.0 * math.log10(
        (float(np.sum(reference * reference)) + 1e-30)
        / (float(np.sum((reference - estimate) ** 2)) + 1e-30)
    )


def isolated_coefficients(scale: float, alpha: float) -> tuple[float, float, float]:
    # The authored rear target already contributed sqrt(1/2)*target to the
    # canonical stereo input.  Routing scale*target to SL/SR changes the
    # physical front and rear coefficients as follows.
    front = MATRIX_FRONT * (SQRT_HALF - alpha * scale) + MATRIX_FRONT_REAR * scale
    rear = MATRIX_REAR * scale
    fold_error = (SQRT_HALF - alpha) * scale
    return front, rear, fold_error


def score(stereo: np.ndarray, output: np.ndarray, scale: float, alpha: float) -> dict[str, float]:
    buses = soundamp_buses(output)
    fold = np.column_stack(
        (
            output[:, 0] + SQRT_HALF * output[:, 2] + SQRT_HALF * output[:, 4],
            output[:, 1] + SQRT_HALF * output[:, 2] + SQRT_HALF * output[:, 5],
        )
    )
    matrix_peak = float(np.max(np.abs(buses)))
    digital_peak = float(np.max(np.abs(output)))
    safety_gain = min(1.0, TARGET_MATRIX_PEAK / max(matrix_peak, 1e-30))
    isolated_front, isolated_rear, fold_error = isolated_coefficients(scale, alpha)
    return {
        "alpha": alpha,
        "rear_scale": scale,
        "digital_peak": digital_peak,
        "matrix_peak": matrix_peak,
        "matrix_peak_dbfs": 20.0 * math.log10(max(matrix_peak, 1e-30)),
        "common_safety_gain": safety_gain,
        "common_safety_gain_db": 20.0 * math.log10(safety_gain),
        "programme_front_bus_rms": rms(buses[:, (0, 1)]),
        "programme_rear_bus_rms": rms(buses[:, (4, 5)]),
        "programme_rear_front_db": level_db(
            rms(buses[:, (4, 5)]), rms(buses[:, (0, 1)])
        ),
        "foldback_snr_db": snr_db(stereo, fold),
        "isolated_front_bus_coefficient": isolated_front,
        "isolated_rear_bus_coefficient": isolated_rear,
        "isolated_rear_front_db": level_db(abs(isolated_rear), abs(isolated_front)),
        "foldback_error_coefficient": fold_error,
    }


def write_wav(path: Path, output: np.ndarray, gain: float) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    write_pcm16(path, np.clip(output * gain, -1.0, 1.0))


def aggregate(rows: list[dict[str, object]], policy: str, scale: float, key: str) -> float:
    values = [
        float(row[key])
        for row in rows
        if row["policy"] == policy and math.isclose(float(row["rear_scale"]), scale)
    ]
    return float(np.mean(values))


def maximum(rows: list[dict[str, object]], policy: str, scale: float, key: str) -> float:
    values = [
        float(row[key])
        for row in rows
        if row["policy"] == policy and math.isclose(float(row["rear_scale"]), scale)
    ]
    return max(values)


def report(rows: list[dict[str, object]], scales: list[float], audio_scales: list[float]) -> str:
    lines = [
        "# Oracle rear-transfer experiment",
        "",
        "The authored SL/SR pair is treated as a perfectly separated oracle object.",
        "This measures routing topology only; it does not measure source-separation quality.",
        "FC and LFE are zero. All listening WAVs for a given track/scale use one common",
        "gain chosen from the worst policy, preserving level matching while keeping every",
        "predicted SoundAmp bus below -0.5 dBFS.",
        "",
        "Policies:",
        "",
        f"- `additive`: alpha = 0",
        f"- `canonical`: alpha = sqrt(1/2) = {SQRT_HALF:.6f}; exact canonical fold-back",
        f"- `compromise`: alpha = {COMPROMISE_ALPHA:.6f}",
        f"- `physical`: alpha = sqrt(1/2) + 0.707946/0.794328 = {PHYSICAL_ALPHA:.6f}",
        "",
        "The extra sqrt(1/2) in the physical policy is required because the oracle rear",
        "component is already present in the input stereo fold. Using only 0.707946/0.794328",
        "would cancel new rear crossfeed but leave the original folded component in front.",
        "",
        "## Aggregate transfer curve",
        "",
        "| rear scale | policy | isolated rear/front dB | programme rear/front dB | fold-back SNR dB | matrix peak mean/max dBFS | listening common gain mean dB |",
        "|---:|---|---:|---:|---:|---:|---:|",
    ]
    for scale in scales:
        for policy in POLICIES:
            lines.append(
                f"| {scale:.2f} | {policy} "
                f"| {aggregate(rows, policy, scale, 'isolated_rear_front_db'):.2f} "
                f"| {aggregate(rows, policy, scale, 'programme_rear_front_db'):.2f} "
                f"| {aggregate(rows, policy, scale, 'foldback_snr_db'):.2f} "
                f"| {aggregate(rows, policy, scale, 'matrix_peak_dbfs'):.2f}/"
                f"{maximum(rows, policy, scale, 'matrix_peak_dbfs'):.2f} "
                f"| {aggregate(rows, policy, scale, 'listening_common_gain_db'):.2f} |"
            )

    lines += [
        "",
        "## Interpretation",
        "",
        "- `additive` retains the entire folded rear object in front and adds a rear copy.",
        "- `canonical` transfers part of the object while preserving the canonical stereo fold.",
        "- `physical` cancels the fully transferred oracle object at the measured front bus",
        "  when rear scale is 1.0, but deliberately changes stereo fold-back.",
        "- `compromise` trades fold compatibility for stronger physical relocation.",
        "- Whole-program rear/front level is not isolated-object localization; both are shown.",
        "",
        f"Listening WAV scales: {', '.join(f'{value:.2f}' for value in audio_scales)}.",
        "Use gain-matched blind listening before choosing a transfer coefficient.",
        "",
    ]
    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--rear-scales",
        type=float,
        nargs="+",
        default=[0.10, 0.15, 0.20, 0.25, 0.50, 1.00],
    )
    parser.add_argument(
        "--audio-scales",
        type=float,
        nargs="+",
        default=[0.25, 1.00],
        help="rear scales for which gain-matched listening WAVs are written",
    )
    parser.add_argument("--include-technical", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    scales = sorted(set(args.rear_scales) | set(args.audio_scales))
    if any(scale <= 0.0 for scale in scales):
        raise RuntimeError("rear scales must be positive")

    manifest_path = args.manifest.resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    output_root = args.output.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []

    items = [
        item
        for item in manifest["items"]
        if args.include_technical or item.get("include_in_aggregate", True)
    ]
    for item in items:
        reference_path = resolve_item_path(manifest_path, item["reference_5_1"])
        stereo_path = resolve_item_path(manifest_path, item["fold_down_stereo"])
        reference = read_audio(reference_path, 6)
        stereo = read_audio(stereo_path, 2)
        frames = min(reference.shape[0], stereo.shape[0])
        reference = reference[:frames]
        stereo = stereo[:frames]
        oracle_rear = reference[:, (4, 5)]

        for scale in scales:
            rendered = {
                policy: render(stereo, oracle_rear, scale, alpha)
                for policy, alpha in POLICIES.items()
            }
            policy_scores = {
                policy: score(stereo, output, scale, POLICIES[policy])
                for policy, output in rendered.items()
            }
            # One gain for all policies preserves level matching.  It is based
            # on the highest predicted SoundAmp peak at this track/scale.
            worst_peak = max(value["matrix_peak"] for value in policy_scores.values())
            common_gain = min(1.0, TARGET_MATRIX_PEAK / max(worst_peak, 1e-30))

            for policy, metrics in policy_scores.items():
                row = {
                    "id": item["id"],
                    "title": item["title"],
                    "policy": policy,
                    **metrics,
                    "listening_common_gain": common_gain,
                    "listening_common_gain_db": 20.0 * math.log10(common_gain),
                }
                rows.append(row)
                if scale in args.audio_scales:
                    audio_path = (
                        output_root
                        / str(item["id"])
                        / f"rear-{scale:.2f}"
                        / f"{policy}.wav"
                    )
                    write_wav(audio_path, rendered[policy], common_gain)

        print(f"{item['id']}: {frames / SAMPLE_RATE:.1f}s, {len(scales)} scales")

    result = {
        "schema": 1,
        "experiment": "oracle-rear-transfer",
        "manifest": str(manifest_path),
        "sample_rate": SAMPLE_RATE,
        "center": "zero",
        "lfe": "off",
        "target_matrix_peak_dbfs": -0.5,
        "soundamp_matrix": {
            "front": MATRIX_FRONT,
            "front_lfe": MATRIX_FRONT_LFE,
            "front_rear": MATRIX_FRONT_REAR,
            "center_front": MATRIX_CENTER_FRONT,
            "center_lfe": MATRIX_CENTER_LFE,
            "lfe": MATRIX_LFE,
            "rear_lfe": MATRIX_REAR_LFE,
            "rear": MATRIX_REAR,
        },
        "policies": POLICIES,
        "rear_scales": scales,
        "audio_scales": args.audio_scales,
        "results": rows,
    }
    json_path = output_root / "results.json"
    report_path = output_root / "report.md"
    json_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    report_path.write_text(report(rows, scales, args.audio_scales), encoding="utf-8")
    print(json_path)
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, TypeError, ValueError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
