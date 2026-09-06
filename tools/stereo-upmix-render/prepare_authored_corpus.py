#!/usr/bin/env python3
"""Prepare normalized authored-surround excerpts for the upmix evaluator.

The source manifest points at publisher-supplied stereo/5.0 pairs, a Dolby
Atmos sample, and an EBU ADM channel-identification file.  This tool decodes
the surround presentation to the evaluator's fixed FL, FR, FC, LFE, SL, SR
order, chooses a deterministic rear-active excerpt, and creates the exact
stereo fold-down that every upmixer must observe.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

from prepare_reference_corpus import (
    CHANNELS,
    SAMPLE_RATE,
    normalize_reference,
    write_pcm16,
)


SURROUND_PAN_5_0 = (
    "pan=5.1(side)|FL=FL|FR=FR|FC=FC|LFE=0*FL|SL=SL|SR=SR"
)
SURROUND_PAN_5_1 = (
    "pan=5.1(side)|FL=FL|FR=FR|FC=FC|LFE=LFE|SL=SL|SR=SR"
)
SURROUND_PAN_5_1_BACK = (
    "pan=5.1(side)|FL=FL|FR=FR|FC=FC|LFE=LFE|SL=BL|SR=BR"
)
SURROUND_PAN_INDEXED = (
    "pan=5.1(side)|c0=c0|c1=c1|c2=c2|c3=c3|c4=c4|c5=c5"
)


class UnusableReferenceError(RuntimeError):
    """The source decodes but has no spatial target for rear evaluation."""


def run_ffmpeg(path: Path, *, channels: int, audio_filter: str | None) -> np.ndarray:
    command = [
        "ffmpeg",
        "-v",
        "error",
        "-nostdin",
        "-i",
        str(path),
        "-map",
        "0:a:0",
    ]
    if audio_filter is not None:
        command += ["-af", audio_filter]
    else:
        command += ["-ac", str(channels)]
    command += [
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
    if decoded.size == 0 or decoded.size % channels:
        raise RuntimeError(f"ffmpeg returned malformed {channels}-channel PCM for {path}")
    return decoded.reshape(-1, channels).copy()


def source_path(manifest_path: Path, value: str) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = manifest_path.parent / path
    if not path.is_file():
        raise RuntimeError(f"source file does not exist: {path}")
    return path.resolve()


def surround_filter(item: dict[str, object]) -> str:
    surround = item["surround"]
    channels = int(surround.get("channels", surround.get("channels_reported_by_ffmpeg", 0)))
    if channels == 5:
        return SURROUND_PAN_5_0
    if channels != 6:
        raise RuntimeError(f"{item['id']}: unsupported source channel count {channels}")
    if surround.get("layout_reported_by_ffmpeg") == "unknown":
        # EBU AP_00010003 / AT_00010001..06 is ordered
        # M+030, M-030, M+000, LFE, M+110, M-110.
        return SURROUND_PAN_INDEXED
    if surround.get("layout_reported_by_ffmpeg") == "5.1":
        # FFmpeg's plain 5.1 layout uses back-left/back-right, whereas the MHI
        # renderer and evaluator use side-left/side-right channel names.
        return SURROUND_PAN_5_1_BACK
    return SURROUND_PAN_5_1


def block_power(samples: np.ndarray, channels: tuple[int, ...], hop_frames: int) -> np.ndarray:
    block_count = samples.shape[0] // hop_frames
    if block_count == 0:
        return np.array([float(np.mean(samples[:, channels] ** 2))])
    power = np.zeros(block_count, dtype=np.float64)
    usable = block_count * hop_frames
    for channel in channels:
        values = samples[:usable, channel].reshape(block_count, hop_frames)
        power += np.mean(values.astype(np.float64) ** 2, axis=1)
    return power / len(channels)


def choose_excerpt(
    reference: np.ndarray, duration_seconds: float, hop_seconds: float
) -> tuple[int, int, float]:
    wanted_frames = round(duration_seconds * SAMPLE_RATE)
    if reference.shape[0] <= wanted_frames:
        return 0, reference.shape[0], 0.0

    hop_frames = round(hop_seconds * SAMPLE_RATE)
    if hop_frames <= 0:
        raise RuntimeError("excerpt hop must be positive")
    window_blocks = max(1, round(duration_seconds / hop_seconds))
    programme = block_power(reference, (0, 1, 2, 4, 5), hop_frames)
    rear = block_power(reference, (4, 5), hop_frames)
    if programme.size < window_blocks:
        return 0, wanted_frames, 0.0

    programme_sum = np.convolve(programme, np.ones(window_blocks), mode="valid")
    rear_sum = np.convolve(rear, np.ones(window_blocks), mode="valid")
    # Geometric mean rejects loud front-only passages and quiet rear-only tails.
    scores = np.sqrt(np.maximum(programme_sum, 0.0) * np.maximum(rear_sum, 0.0))
    block = int(np.argmax(scores))
    start = block * hop_frames
    return start, min(wanted_frames, reference.shape[0] - start), float(scores[block])


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalize_stereo(stereo: np.ndarray) -> tuple[np.ndarray, float]:
    peak = max(float(np.max(np.abs(stereo))), 1e-12)
    gain = min(1.0, 0.90 / peak)
    return stereo * gain, gain


def energy_fraction(reference: np.ndarray, selected: tuple[int, ...]) -> float:
    non_lfe = np.mean(reference[:, (0, 1, 2, 4, 5)] ** 2, axis=0)
    selected_power = float(np.sum(np.mean(reference[:, selected] ** 2, axis=0)))
    return selected_power / (float(np.sum(non_lfe)) + 1e-20)


def prepare_item(
    source_manifest: Path,
    output_root: Path,
    item: dict[str, object],
    excerpt_duration: float,
    excerpt_hop: float,
) -> dict[str, object]:
    surround_info = item["surround"]
    surround_source = source_path(source_manifest, str(surround_info["path"]))
    decoded = run_ffmpeg(
        surround_source,
        channels=6,
        audio_filter=surround_filter(item),
    )
    full_rear_power = float(np.mean(decoded[:, (4, 5)] ** 2))
    if full_rear_power <= 1e-10:
        raise UnusableReferenceError(
            f"{item['id']}: source has no usable SL/SR programme "
            f"(rear power {full_rear_power:.3e})"
        )
    start, frames, selection_score = choose_excerpt(
        decoded, excerpt_duration, excerpt_hop
    )
    reference = decoded[start : start + frames]

    publisher_source: Path | None = None
    publisher_excerpt: np.ndarray | None = None
    publisher_gain: float | None = None
    if item.get("stereo") is not None:
        publisher_source = source_path(source_manifest, str(item["stereo"]["path"]))
        publisher = run_ffmpeg(publisher_source, channels=2, audio_filter=None)
        if publisher.shape[0] < start + frames:
            frames = min(frames, publisher.shape[0] - start)
            if frames <= 0:
                raise RuntimeError(f"{item['id']}: publisher stereo does not overlap excerpt")
            reference = reference[:frames]
        publisher_excerpt, publisher_gain = normalize_stereo(
            publisher[start : start + frames]
        )

    reference, folded, reference_gain = normalize_reference(reference)
    item_root = output_root / str(item["id"])
    reference_path = (item_root / "reference_5_1.wav").resolve()
    fold_path = (item_root / "fold_down_stereo.wav").resolve()
    write_pcm16(reference_path, reference)
    write_pcm16(fold_path, folded)

    publisher_path: Path | None = None
    if publisher_excerpt is not None:
        publisher_path = (item_root / "publisher_stereo.wav").resolve()
        write_pcm16(publisher_path, publisher_excerpt)

    lfe_power = float(np.mean(reference[:, 3] ** 2))
    prepared = {
        "id": item["id"],
        "title": item["title"],
        "genre": item["genre"],
        "instrumentation": item.get("instrumentation"),
        "dataset": item.get(
            "dataset",
            "EBU QC authored ADM"
            if str(item["id"]).startswith("ebu-")
            else "Pavlik Records authored surround",
        ),
        "evaluation_group": item.get(
            "evaluation_group",
            "technical" if str(item["id"]).startswith("ebu-") else "authored-music",
        ),
        "include_in_aggregate": item.get(
            "include_in_aggregate", not str(item["id"]).startswith("ebu-")
        ),
        "source_url": item.get("source_url"),
        "license": item.get("license"),
        "source": str(surround_source),
        "source_sha256": surround_info["sha256"],
        "source_stereo": str(publisher_source) if publisher_source else None,
        "reference_5_1": str(reference_path),
        "reference_5_1_sha256": sha256(reference_path),
        "fold_down_stereo": str(fold_path),
        "fold_down_stereo_sha256": sha256(fold_path),
        "publisher_stereo": str(publisher_path) if publisher_path else None,
        "publisher_stereo_sha256": sha256(publisher_path) if publisher_path else None,
        "sample_rate": SAMPLE_RATE,
        "sample_format": "PCM signed 16-bit little-endian",
        "frames": int(reference.shape[0]),
        "duration_seconds": reference.shape[0] / SAMPLE_RATE,
        "source_excerpt_start_seconds": start / SAMPLE_RATE,
        "selection_score": selection_score,
        "selection": "maximum 1-second-hop geometric mean of programme and rear power",
        "reference_gain": reference_gain,
        "publisher_stereo_gain": publisher_gain,
        "channel_order": list(CHANNELS),
        "fold_down": "L=FL+sqrt(1/2)*FC+sqrt(1/2)*SL; R=FR+sqrt(1/2)*FC+sqrt(1/2)*SR; LFE omitted",
        "reference_rear_energy_fraction": energy_fraction(reference, (4, 5)),
        "reference_lfe_active": lfe_power > 1e-10,
        "reference_lfe_power": lfe_power,
    }
    return prepared


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--excerpt-duration", type=float, default=30.0)
    parser.add_argument("--excerpt-hop", type=float, default=1.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.excerpt_duration <= 0.0 or args.excerpt_hop <= 0.0:
        raise RuntimeError("excerpt duration and hop must be positive")
    source_manifest = args.source_manifest.resolve()
    manifest = json.loads(source_manifest.read_text(encoding="utf-8"))
    output_root = args.output.resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    items = []
    exclusions = []
    for item in manifest["items"]:
        try:
            prepared = prepare_item(
                source_manifest,
                output_root,
                item,
                args.excerpt_duration,
                args.excerpt_hop,
            )
        except UnusableReferenceError as error:
            exclusions.append({"id": item["id"], "reason": str(error)})
            print(f"excluded {error}")
            continue
        items.append(prepared)
        print(
            f"{prepared['id']}: start={prepared['source_excerpt_start_seconds']:.1f}s "
            f"duration={prepared['duration_seconds']:.1f}s "
            f"rear={prepared['reference_rear_energy_fraction']:.3f} "
            f"lfe={'yes' if prepared['reference_lfe_active'] else 'no'}"
        )

    output_manifest = {
        "schema": 1,
        "corpus_kind": manifest.get("corpus_kind", "authored-surround"),
        "description": manifest.get(
            "output_description",
            "Deterministic rear-active excerpts from publisher-authored 5.0/Atmos "
            "music and an EBU 5.1 channel-identification reference.",
        ),
        "source_manifest": str(source_manifest),
        "sample_rate": SAMPLE_RATE,
        "sample_format": "PCM signed 16-bit little-endian",
        "excerpt_duration_seconds": args.excerpt_duration,
        "excerpt_hop_seconds": args.excerpt_hop,
        "channel_order": list(CHANNELS),
        "items": items,
        "excluded_items": exclusions,
    }
    manifest_path = output_root / "manifest.json"
    manifest_path.write_text(
        json.dumps(output_manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"wrote {len(items)} surround fixtures to {output_root}")
    print(manifest_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, TypeError, ValueError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
