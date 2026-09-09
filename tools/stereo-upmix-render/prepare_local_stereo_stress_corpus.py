#!/usr/bin/env python3
"""Prepare deterministic excerpts from local stereo files for upmix testing.

Source files may live in the private repository testdata. Decoded 48 kHz stereo
WAV excerpts and their manifest are always written to ignored build/ storage.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import wave
from pathlib import Path


SAMPLE_RATE = 48_000


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def probe_duration(path: Path) -> float:
    command = [
        "ffprobe", "-v", "error", "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1", str(path),
    ]
    try:
        completed = subprocess.run(
            command, check=True, text=True, capture_output=True
        )
    except FileNotFoundError as error:
        raise RuntimeError("ffprobe is required") from error
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"ffprobe failed for {path}") from error
    return float(completed.stdout.strip())


def display_title(path: Path) -> str:
    title = re.sub(r"_spotdown\.org$", "", path.stem, flags=re.IGNORECASE)
    return title.replace("_", " ").strip()


def slug(text: str) -> str:
    ascii_words = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    if ascii_words:
        return ascii_words[:56]
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def excerpt_start(duration: float, excerpt_seconds: float, fraction: float) -> float:
    available = max(duration - excerpt_seconds, 0.0)
    return min(max(fraction * duration, 0.0), available)


def decode_excerpt(
    source: Path, destination: Path, start: float, excerpt_seconds: float
) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    command = [
        "ffmpeg", "-v", "error", "-nostdin", "-y",
        "-ss", f"{start:.6f}", "-i", str(source),
        "-t", f"{excerpt_seconds:.6f}", "-map", "0:a:0", "-vn",
        "-ac", "2", "-ar", str(SAMPLE_RATE), "-c:a", "pcm_s16le",
        "-map_metadata", "-1", str(destination),
    ]
    try:
        subprocess.run(command, check=True)
    except FileNotFoundError as error:
        raise RuntimeError("ffmpeg is required") from error
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"ffmpeg failed for {source}") from error


def wav_info(path: Path) -> int:
    with wave.open(str(path), "rb") as stream:
        if stream.getnchannels() != 2 or stream.getsampwidth() != 2:
            raise RuntimeError(f"unexpected WAV format: {path}")
        if stream.getframerate() != SAMPLE_RATE:
            raise RuntimeError(f"unexpected WAV rate: {path}")
        return stream.getnframes()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sources", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--excerpt-seconds", type=float, default=30.0)
    parser.add_argument(
        "--fractions", type=float, nargs="+", default=(0.20, 0.60),
        help="fractional positions in each track (default: 0.20 0.60)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.excerpt_seconds <= 0.0:
        raise RuntimeError("--excerpt-seconds must be positive")
    if not args.fractions or any(not 0.0 <= value <= 1.0 for value in args.fractions):
        raise RuntimeError("--fractions values must be between 0 and 1")

    args.output.mkdir(parents=True, exist_ok=True)
    items: list[dict[str, object]] = []
    used_ids: set[str] = set()

    for source_arg in args.sources:
        source = source_arg.expanduser().resolve()
        if not source.is_file():
            raise RuntimeError(f"source does not exist: {source}")
        duration = probe_duration(source)
        title = display_title(source)
        tagged = subprocess.run(['ffprobe','-v','error','-show_entries',
            'format_tags=artist,title,album,genre','-of','json',str(source)],
            check=True,text=True,capture_output=True)
        tags = {k.lower():v for k,v in json.loads(tagged.stdout).get('format',{}).get('tags',{}).items()}
        base_id = slug(title)
        source_digest = sha256(source)

        for index, fraction in enumerate(args.fractions, start=1):
            item_id = f"{base_id}-p{index}"
            if item_id in used_ids:
                item_id = f"{base_id}-{source_digest[:8]}-p{index}"
            used_ids.add(item_id)
            length = min(args.excerpt_seconds, duration)
            start = excerpt_start(duration, length, fraction)
            stereo = (args.output / item_id / "stereo.wav").resolve()
            decode_excerpt(source, stereo, start, length)
            frames = wav_info(stereo)
            items.append(
                {
                    "id": item_id,
                    "group": "private",
                    "artist": tags.get('artist', 'unknown'),
                    "source_tags": tags,
                    "title": f"{title} [{start:.1f}s]",
                    "source": str(source),
                    "source_sha256": source_digest,
                    "source_duration_seconds": duration,
                    "excerpt_start_seconds": start,
                    "stereo": str(stereo),
                    "stereo_sha256": sha256(stereo),
                    "sample_rate": SAMPLE_RATE,
                    "frames": frames,
                    "duration_seconds": frames / SAMPLE_RATE,
                    "stress_targets": [
                        "semantic rear activity",
                        "distinct SL/SR object",
                        "lead and bass protection",
                    ],
                }
            )
            print(f"{item_id}: {start:.2f}s + {frames / SAMPLE_RATE:.2f}s")

    manifest = {
        "schema": 1,
        "corpus_kind": "private-local-stereo-upmix-stress",
        "description": (
            "Private local stereo excerpts for HSSR activity and safety testing. "
            "There is no authored surround reference."
        ),
        "sample_rate": SAMPLE_RATE,
        "sample_format": "PCM signed 16-bit little-endian stereo",
        "rights": (
            "Private-repository evaluation asset. Do not publish or redistribute "
            "source audio or decoded excerpts."
        ),
        "items": items,
    }
    manifest_path = args.output / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(manifest_path.resolve())
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}")
        raise SystemExit(1)
