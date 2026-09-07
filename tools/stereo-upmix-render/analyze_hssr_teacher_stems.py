#!/usr/bin/env python3
"""Audit six-stem teacher output and make a private oracle upmix.

The oracle is deliberately not a production algorithm: it uses offline Demucs
stems to answer whether a clearly separated, safe rear object exists.  Audio
and reports should be written below ignored build/ storage only.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy import signal


STEMS = ("Bass", "Drums", "Guitar", "Other", "Piano", "Vocals")
SAFE_STEMS = ("Guitar", "Other", "Piano")
SQRT_HALF = math.sqrt(0.5)
SAMPLE_RATE = 48_000


def read_stereo(path: Path) -> np.ndarray:
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != SAMPLE_RATE or audio.shape[1] != 2:
        raise RuntimeError(f"expected 48 kHz stereo: {path}")
    return audio


def rms(audio: np.ndarray) -> float:
    return math.sqrt(float(np.mean(audio * audio)) + 1.0e-30)


def level_db(numerator: float, denominator: float) -> float:
    return 20.0 * math.log10((numerator + 1.0e-30) / (denominator + 1.0e-30))


def correlation(audio: np.ndarray) -> float:
    left = audio[:, 0] - np.mean(audio[:, 0])
    right = audio[:, 1] - np.mean(audio[:, 1])
    denominator = math.sqrt(float(left @ left) * float(right @ right))
    if denominator <= 1.0e-30:
        return 0.0
    return float((left @ right) / denominator)


def side_ratio(audio: np.ndarray) -> float:
    mid = 0.5 * (audio[:, 0] + audio[:, 1])
    side = 0.5 * (audio[:, 0] - audio[:, 1])
    mid_power = float(mid @ mid)
    side_power = float(side @ side)
    return side_power / (mid_power + side_power + 1.0e-30)


def highpass(audio: np.ndarray, cutoff: float) -> np.ndarray:
    sos = signal.butter(4, cutoff, btype="highpass", fs=SAMPLE_RATE, output="sos")
    return signal.sosfiltfilt(sos, audio, axis=0)


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


def locate_stems(directory: Path) -> dict[str, Path]:
    result = {}
    for stem in STEMS:
        matches = list(directory.glob(f"*_({stem})_*.wav"))
        if len(matches) != 1:
            raise RuntimeError(f"expected one {stem} stem in {directory}")
        result[stem] = matches[0]
    return result


def make_oracle(mixture: np.ndarray, stems: dict[str, np.ndarray]) -> tuple[np.ndarray, np.ndarray]:
    # Keep vocal, bass and the body of the main drums in front.  The offline
    # teacher exposes guitar/piano/other (usually synths and effects) directly;
    # only a quiet high-passed drum layer is allowed into the oracle rear.
    candidate = sum((stems[name] for name in SAFE_STEMS), np.zeros_like(mixture))
    candidate += 0.20 * highpass(stems["Drums"], 1800.0)
    candidate = highpass(candidate, 160.0)
    rear = 0.25 * candidate
    front = mixture - SQRT_HALF * rear
    output = np.column_stack(
        (front[:, 0], front[:, 1], np.zeros(len(front)), np.zeros(len(front)),
         rear[:, 0], rear[:, 1])
    )

    peak = float(np.max(np.abs(soundamp_buses(output))))
    ceiling = 10.0 ** (-0.5 / 20.0)
    if peak > ceiling:
        output *= ceiling / peak
    return output, candidate


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--teacher-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    item_by_id = {str(item["id"]): item for item in manifest["items"]}
    rows: list[dict[str, object]] = []
    args.output.mkdir(parents=True, exist_ok=True)

    for teacher_dir in sorted(path for path in args.teacher_root.iterdir() if path.is_dir()):
        item = item_by_id.get(teacher_dir.name)
        if item is None:
            continue
        mixture = read_stereo(Path(str(item["stereo"])))
        stem_paths = locate_stems(teacher_dir)
        stem_audio = {name: read_stereo(path) for name, path in stem_paths.items()}
        frames = min([len(mixture), *(len(audio) for audio in stem_audio.values())])
        mixture = mixture[:frames]
        stem_audio = {name: audio[:frames] for name, audio in stem_audio.items()}
        mixture_power = float(np.sum(mixture * mixture)) + 1.0e-30
        powers = {name: float(np.sum(audio * audio)) for name, audio in stem_audio.items()}
        power_sum = sum(powers.values()) + 1.0e-30
        reconstruction = sum(stem_audio.values(), np.zeros_like(mixture))
        reconstruction_snr = 10.0 * math.log10(
            mixture_power
            / (float(np.sum((mixture - reconstruction) ** 2)) + 1.0e-30)
        )

        oracle, candidate = make_oracle(mixture, stem_audio)
        item_output = args.output / teacher_dir.name
        item_output.mkdir(parents=True, exist_ok=True)
        sf.write(item_output / "teacher-rear-candidate.wav", candidate, SAMPLE_RATE,
                 subtype="PCM_16")
        sf.write(item_output / "teacher-oracle-5.1.wav", oracle, SAMPLE_RATE,
                 subtype="PCM_16")

        front_power = float(np.sum(oracle[:, :2] ** 2))
        rear_power = float(np.sum(oracle[:, 4:6] ** 2))
        safe_power = sum(powers[name] for name in SAFE_STEMS)
        metrics = {
            name.lower(): {
                "energy_fraction": powers[name] / power_sum,
                "level_vs_mix_db": level_db(rms(stem_audio[name]), rms(mixture)),
                "side_ratio": side_ratio(stem_audio[name]),
                "lr_correlation": correlation(stem_audio[name]),
            }
            for name in STEMS
        }
        strongest_safe = max(SAFE_STEMS, key=lambda name: powers[name])
        row = {
            "id": teacher_dir.name,
            "title": item["title"],
            "teacher_reconstruction_snr_db": reconstruction_snr,
            "safe_stem_energy_fraction": safe_power / power_sum,
            "strongest_safe_stem": strongest_safe.lower(),
            "strongest_safe_stem_fraction": powers[strongest_safe] / power_sum,
            "oracle_rear_energy_fraction": rear_power / (front_power + rear_power + 1.0e-30),
            "oracle_rear_front_db": level_db(math.sqrt(rear_power), math.sqrt(front_power)),
            "oracle_soundamp_peak_dbfs": 20.0 * math.log10(
                max(float(np.max(np.abs(soundamp_buses(oracle)))), 1.0e-30)
            ),
            "stems": metrics,
            "rear_candidate": str((item_output / "teacher-rear-candidate.wav").resolve()),
            "oracle_5_1": str((item_output / "teacher-oracle-5.1.wav").resolve()),
        }
        rows.append(row)
        print(
            f"{teacher_dir.name}: safe={100.0 * safe_power / power_sum:.1f}% "
            f"strongest={strongest_safe} "
            f"oracle-rear={100.0 * float(row['oracle_rear_energy_fraction']):.2f}%"
        )

    result = {
        "schema": 1,
        "kind": "private-hssr-six-stem-teacher-audit",
        "development_only": True,
        "teacher": "htdemucs_6s",
        "manifest": str(args.manifest.resolve()),
        "rows": rows,
    }
    (args.output / "results.json").write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    lines = [
        "# Private six-stem teacher audit", "",
        "Offline teacher/oracle results only; these are not target-device metrics.",
        "Audio is private and must not be committed or redistributed.", "",
        "| track | safe stems % | strongest safe stem % | oracle rear % | rear/front dB | reconstruction SNR dB | matrix peak dBFS |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['title']} | {100.0 * float(row['safe_stem_energy_fraction']):.1f} "
            f"| {row['strongest_safe_stem']} {100.0 * float(row['strongest_safe_stem_fraction']):.1f} "
            f"| {100.0 * float(row['oracle_rear_energy_fraction']):.2f} "
            f"| {float(row['oracle_rear_front_db']):.2f} "
            f"| {float(row['teacher_reconstruction_snr_db']):.2f} "
            f"| {float(row['oracle_soundamp_peak_dbfs']):.2f} |"
        )
    lines.append("")
    (args.output / "report.md").write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}")
        raise SystemExit(1)
