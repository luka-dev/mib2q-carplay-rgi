#!/usr/bin/env python3
"""Build controlled 5.1 references from open MUSDB18/MedleyDB stems.

The resulting 5.1 files are test fixtures, not artist-authored surround
masters.  Each stereo input is produced by an explicit ITU-style fold-down of
its corresponding fixture, so every backend sees exactly the same lossy
observation of a known spatial target.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np


SAMPLE_RATE = 48_000
SQRT_HALF = math.sqrt(0.5)
CHANNELS = ("FL", "FR", "FC", "LFE", "SL", "SR")


MUSDB_SELECTIONS = (
    ("80s Rock", "Music Delta - 80s Rock"),
    ("Beatles-style", "Music Delta - Beatles"),
    ("Britpop", "Music Delta - Britpop"),
    ("Disco", "Music Delta - Disco"),
    ("Country", "Music Delta - Country1"),
    ("Gospel", "Music Delta - Gospel"),
    ("Grunge", "Music Delta - Grunge"),
)

MEDLEYDB_SELECTIONS = (
    ("Singer/Songwriter", "LizNelson_Rainfall", 60.0, 12.0),
    ("World/Folk", "Phoenix_ScotchMorris", 40.0, 12.0),
)


def run_ffmpeg_decode(
    path: Path,
    *,
    audio_stream: int = 0,
    start: float = 0.0,
    duration: float | None = None,
) -> np.ndarray:
    command = ["ffmpeg", "-v", "error", "-nostdin"]
    if start:
        command += ["-ss", f"{start:.6f}"]
    command += ["-i", str(path), "-map", f"0:a:{audio_stream}"]
    if duration is not None:
        command += ["-t", f"{duration:.6f}"]
    command += [
        "-ac",
        "2",
        "-ar",
        str(SAMPLE_RATE),
        "-c:a",
        "pcm_f32le",
        "-f",
        "f32le",
        "pipe:1",
    ]
    try:
        completed = subprocess.run(command, check=True, capture_output=True)
    except FileNotFoundError as error:
        raise RuntimeError("ffmpeg is required to prepare the corpus") from error
    except subprocess.CalledProcessError as error:
        message = error.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"ffmpeg failed for {path}: {message}") from error

    decoded = np.frombuffer(completed.stdout, dtype="<f4")
    if decoded.size == 0 or decoded.size % 2:
        raise RuntimeError(f"ffmpeg returned malformed stereo PCM for {path}")
    return decoded.reshape(-1, 2).astype(np.float64)


def run_ffmpeg_5_1(
    path: Path, *, start: float, duration: float
) -> np.ndarray:
    command = [
        "ffmpeg",
        "-v",
        "error",
        "-nostdin",
        "-ss",
        f"{start:.6f}",
        "-i",
        str(path),
        "-t",
        f"{duration:.6f}",
        "-ar",
        str(SAMPLE_RATE),
        "-ac",
        "6",
        "-channel_layout",
        "5.1",
        "-c:a",
        "pcm_f32le",
        "-f",
        "f32le",
        "pipe:1",
    ]
    try:
        completed = subprocess.run(command, check=True, capture_output=True)
    except FileNotFoundError as error:
        raise RuntimeError("ffmpeg is required to prepare the corpus") from error
    except subprocess.CalledProcessError as error:
        message = error.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"ffmpeg failed for {path}: {message}") from error

    decoded = np.frombuffer(completed.stdout, dtype="<f4")
    if decoded.size == 0 or decoded.size % 6:
        raise RuntimeError(f"ffmpeg returned malformed 5.1 PCM for {path}")
    return decoded.reshape(-1, 6).astype(np.float64)


def write_pcm16(path: Path, samples: np.ndarray) -> None:
    if samples.ndim != 2:
        raise ValueError("audio must have shape [frames, channels]")
    channels = samples.shape[1]
    channel_masks = {2: 0x00000003, 6: 0x0000060F}
    if channels not in channel_masks:
        raise ValueError(f"no WAVE channel mask for {channels} channels")
    quantized = np.clip(np.rint(samples * 32767.0), -32768, 32767).astype("<i2")
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = quantized.tobytes()
    block_align = channels * 2
    riff_size = 60 + len(payload)
    if riff_size > 0xFFFFFFFF:
        raise ValueError("audio is too large for RIFF/WAVE")

    # WAVE_FORMAT_EXTENSIBLE with the PCM subtype makes channel order explicit
    # to both test tools and media players.
    pcm_guid = bytes.fromhex("0100000000001000800000aa00389b71")
    header = b"".join(
        (
            b"RIFF",
            struct.pack("<I", riff_size),
            b"WAVEfmt ",
            struct.pack("<IHHIIHHHHI", 40, 0xFFFE, channels, SAMPLE_RATE,
                        SAMPLE_RATE * block_align, block_align, 16, 22, 16,
                        channel_masks[channels]),
            pcm_guid,
            b"data",
            struct.pack("<I", len(payload)),
        )
    )
    with path.open("wb") as output:
        output.write(header)
        output.write(payload)


def lowpass_lfe(signal: np.ndarray) -> np.ndarray:
    """Cheap fixture-only fourth-order 80 Hz low pass."""
    coefficient = math.exp(-2.0 * math.pi * 80.0 / SAMPLE_RATE)
    result = signal.copy()
    for _ in range(4):
        state = 0.0
        stage = np.empty_like(result)
        for index, value in enumerate(result):
            state = (1.0 - coefficient) * value + coefficient * state
            stage[index] = state
        result = stage
    return result


def side_pair(stereo: np.ndarray) -> np.ndarray:
    side = 0.5 * (stereo[:, 0] - stereo[:, 1])
    return np.column_stack((side, -side))


def make_musdb_reference(stems: dict[str, np.ndarray]) -> np.ndarray:
    frame_count = min(stem.shape[0] for stem in stems.values())
    drums = stems["drums"][:frame_count]
    bass = stems["bass"][:frame_count]
    other = stems["other"][:frame_count]
    vocals = stems["vocals"][:frame_count]

    reference = np.zeros((frame_count, 6), dtype=np.float64)
    reference[:, 0:2] = 0.70 * drums + 0.52 * bass
    reference[:, 2] = 0.80 * np.mean(vocals, axis=1)
    reference[:, 4:6] = 0.78 * other + 0.20 * side_pair(vocals)
    bass_mono = np.mean(bass, axis=1) + 0.20 * np.mean(drums, axis=1)
    reference[:, 3] = 0.42 * lowpass_lfe(bass_mono)
    return reference


def mono(stereo: np.ndarray) -> np.ndarray:
    return np.mean(stereo, axis=1)


def pan(signal: np.ndarray, position: float) -> np.ndarray:
    """Equal-power pan of a mono signal; position is -1 left to +1 right."""
    angle = (position + 1.0) * math.pi / 4.0
    return np.column_stack((math.cos(angle) * signal, math.sin(angle) * signal))


def make_medleydb_reference(track: str, stems: list[np.ndarray]) -> np.ndarray:
    frame_count = min(stem.shape[0] for stem in stems)
    values = [mono(stem[:frame_count]) for stem in stems]
    reference = np.zeros((frame_count, 6), dtype=np.float64)

    if track == "LizNelson_Rainfall":
        if len(values) != 5:
            raise RuntimeError(f"expected five stems for {track}, got {len(values)}")
        lead, backing_l, backing_r, acoustic, electric = values
        reference[:, 0:2] = 0.72 * pan(acoustic, -0.28) + 0.42 * pan(electric, 0.35)
        reference[:, 2] = 0.78 * lead
        reference[:, 4:6] = (
            0.58 * pan(backing_l, -0.82)
            + 0.58 * pan(backing_r, 0.82)
            + 0.34 * pan(electric, 0.62)
        )
        reference[:, 3] = 0.20 * lowpass_lfe(acoustic)
    elif track == "Phoenix_ScotchMorris":
        if len(values) != 4:
            raise RuntimeError(f"expected four stems for {track}, got {len(values)}")
        guitar, flute, violin, main_system = values
        reference[:, 0:2] = 0.72 * pan(guitar, -0.18) + 0.30 * pan(main_system, 0.18)
        reference[:, 2] = 0.58 * flute
        reference[:, 4:6] = 0.68 * pan(violin, 0.55) + 0.35 * pan(main_system, -0.58)
        reference[:, 3] = 0.20 * lowpass_lfe(guitar + 0.25 * main_system)
    else:
        raise RuntimeError(f"no MedleyDB routing recipe for {track}")
    return reference


def fold_down(reference: np.ndarray) -> np.ndarray:
    return np.column_stack(
        (
            reference[:, 0] + SQRT_HALF * reference[:, 2] + SQRT_HALF * reference[:, 4],
            reference[:, 1] + SQRT_HALF * reference[:, 2] + SQRT_HALF * reference[:, 5],
        )
    )


def normalize_reference(reference: np.ndarray) -> tuple[np.ndarray, np.ndarray, float]:
    folded = fold_down(reference)
    peak = max(float(np.max(np.abs(reference))), float(np.max(np.abs(folded))), 1e-12)
    gain = min(1.0, 0.90 / peak)
    return reference * gain, folded * gain, gain


def find_musdb_track(root: Path, title: str) -> Path:
    matches = list(root.glob(f"**/{title}.stem.mp4"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one MUSDB track {title!r}, found {len(matches)}")
    return matches[0]


def medley_stem_paths(root: Path, track: str) -> list[Path]:
    matches = sorted(root.glob(f"**/{track}_STEMS/{track}_STEM_*.wav"))
    if not matches:
        raise RuntimeError(f"no MedleyDB stems found for {track!r}")
    return matches


def add_item(
    items: list[dict[str, object]],
    output_root: Path,
    *,
    slug: str,
    title: str,
    genre: str,
    dataset: str,
    evaluation_group: str,
    reference: np.ndarray,
    source_path: Path,
) -> None:
    reference, folded, gain = normalize_reference(reference)
    item_root = output_root / slug
    reference_path = item_root / "reference_5_1.wav"
    fold_path = item_root / "fold_down_stereo.wav"
    write_pcm16(reference_path, reference)
    write_pcm16(fold_path, folded)
    items.append(
        {
            "id": slug,
            "title": title,
            "genre": genre,
            "dataset": dataset,
            "evaluation_group": evaluation_group,
            "source": str(source_path),
            "reference_5_1": str(reference_path),
            "fold_down_stereo": str(fold_path),
            "sample_rate": SAMPLE_RATE,
            "frames": int(reference.shape[0]),
            "duration_seconds": reference.shape[0] / SAMPLE_RATE,
            "reference_gain": gain,
            "channel_order": list(CHANNELS),
            "fold_down": "L=FL+sqrt(1/2)*FC+sqrt(1/2)*SL; R=FR+sqrt(1/2)*FC+sqrt(1/2)*SR; LFE omitted",
        }
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--musdb-root", type=Path, required=True)
    parser.add_argument("--medleydb-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--authored-5-1",
        type=Path,
        help="optional authored 5.1 FLAC/WAV to add as a separate external check",
    )
    parser.add_argument("--authored-start", type=float, default=780.0)
    parser.add_argument("--authored-duration", type=float, default=30.0)
    parser.add_argument(
        "--musdb-validation-count",
        type=int,
        default=0,
        help="add a deterministic held-out subset of other MUSDB sample tracks",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    items: list[dict[str, object]] = []

    for genre, title in MUSDB_SELECTIONS:
        source = find_musdb_track(args.musdb_root, title)
        streams = {
            "drums": run_ffmpeg_decode(source, audio_stream=1),
            "bass": run_ffmpeg_decode(source, audio_stream=2),
            "other": run_ffmpeg_decode(source, audio_stream=3),
            "vocals": run_ffmpeg_decode(source, audio_stream=4),
        }
        add_item(
            items,
            args.output,
            slug=title.lower().replace(" ", "-").replace("'", ""),
            title=title,
            genre=genre,
            dataset="MUSDB18-7",
            evaluation_group="genre-development",
            reference=make_musdb_reference(streams),
            source_path=source,
        )

    if args.musdb_validation_count < 0:
        raise RuntimeError("MUSDB validation count must be non-negative")
    selected_titles = {title for _genre, title in MUSDB_SELECTIONS}
    validation_candidates = [
        path
        for path in args.musdb_root.glob("**/*.stem.mp4")
        if path.name.removesuffix(".stem.mp4") not in selected_titles
    ]
    validation_candidates.sort(
        key=lambda path: hashlib.sha256(path.name.encode("utf-8")).digest()
    )
    if args.musdb_validation_count > len(validation_candidates):
        raise RuntimeError(
            f"requested {args.musdb_validation_count} validation tracks, "
            f"but only {len(validation_candidates)} are available"
        )
    for source in validation_candidates[: args.musdb_validation_count]:
        title = source.name.removesuffix(".stem.mp4")
        streams = {
            "drums": run_ffmpeg_decode(source, audio_stream=1),
            "bass": run_ffmpeg_decode(source, audio_stream=2),
            "other": run_ffmpeg_decode(source, audio_stream=3),
            "vocals": run_ffmpeg_decode(source, audio_stream=4),
        }
        add_item(
            items,
            args.output,
            slug="validation-" + hashlib.sha256(title.encode("utf-8")).hexdigest()[:12],
            title=title,
            genre="MUSDB held-out",
            dataset="MUSDB18-7",
            evaluation_group="held-out",
            reference=make_musdb_reference(streams),
            source_path=source,
        )

    for genre, track, start, duration in MEDLEYDB_SELECTIONS:
        source_paths = medley_stem_paths(args.medleydb_root, track)
        stems = [
            run_ffmpeg_decode(path, start=start, duration=duration)
            for path in source_paths
        ]
        add_item(
            items,
            args.output,
            slug=track.lower().replace("_", "-"),
            title=track,
            genre=genre,
            dataset="MedleyDB Sample",
            evaluation_group="genre-development",
            reference=make_medleydb_reference(track, stems),
            source_path=source_paths[0].parent.parent,
        )

    if args.authored_5_1 is not None:
        if args.authored_start < 0.0 or args.authored_duration <= 0.0:
            raise RuntimeError("authored start must be non-negative and duration positive")
        reference = run_ffmpeg_5_1(
            args.authored_5_1,
            start=args.authored_start,
            duration=args.authored_duration,
        )
        add_item(
            items,
            args.output,
            slug="authored-sintel-5-1",
            title="Sintel authored 5.1 excerpt",
            genre="Film score/effects",
            dataset="Xiph.org test media",
            evaluation_group="authored",
            reference=reference,
            source_path=args.authored_5_1,
        )

    manifest = {
        "schema": 1,
        "description": "Controlled 5.1 fixtures derived from open multitracks, plus an optional authored 5.1 external check.",
        "items": items,
    }
    manifest_path = args.output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(items)} reference fixtures to {args.output}")
    print(manifest_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
