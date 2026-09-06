#!/usr/bin/env python3
"""Prepare Apple public-preview inputs aligned to open authored surround samples.

The Apple inputs are the official, unauthenticated 30-second AAC previews from
the iTunes Search API.  Each preview below was matched by waveform correlation
to the publisher's freely downloadable stereo sample (or, for the Atmos-only
trumpet item, to a stereo fold of the public Atmos sample).  The resulting
fixtures therefore compare the exact Apple catalogue recording against a
time-aligned authored 5.0/Atmos reference without accessing FairPlay media.

The audio is for local evaluation only.  Copyright remains with Apple/the
publisher and generated audio must not be committed or redistributed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import numpy as np

from prepare_authored_corpus import source_path, surround_filter
from prepare_reference_corpus import fold_down, normalize_reference, write_pcm16


SAMPLE_RATE = 48_000

# source_start_frame_48k is the sample-accurate result of correlating decoded
# Apple AAC against the public publisher sample after both were resampled to
# 48 kHz.  The repeated 2,158,899 value is about 44.977 seconds into the
# publisher sample; the non-integer-looking location includes the AAC/MP4
# presentation offset visible after decoding.
MATCHES = (
    {
        "id": "apple-pavlik-01-dohnanyi",
        "source_id": "pavlik-01-dohnanyi",
        "apple_album": "Ernő Dohnányi: Piano Concertos NOS 1 & 2",
        "apple_track": "Piano Concerto No. 1 in E Minor, Op. 5: I. Adagio maestoso – Cadenza - Poco meno - Allegro",
        "apple_artist": "Slovak State Philharmonic Orchestra, Kosice, Ladislav Fanzowitz & Zbynek Müller",
        "collection_id": 1692864673,
        "track_id": 1692864860,
        "preview_name": "dohnanyi-concerto-t1.m4a",
        "preview_url": "https://audio-ssl.itunes.apple.com/itunes-assets/AudioPreview221/v4/c1/fd/68/c1fd68a6-7e6f-a6d9-0e0e-006a0223aef0/mzaf_3366991936095797401.plus.aac.p.m4a",
        "source_start_frame_48k": 2158899,
        "correlation": 0.974486,
    },
    {
        "id": "apple-pavlik-03-signum-credo",
        "source_id": "pavlik-03-choir-quartet",
        "apple_album": "Signum Magnum",
        "apple_track": "Credo",
        "apple_artist": "Ondrej Olos",
        "collection_id": 1600203090,
        "track_id": 1600203094,
        "preview_name": "signum-credo-t1.m4a",
        "preview_url": "https://audio-ssl.itunes.apple.com/itunes-assets/AudioPreview116/v4/04/e3/1f/04e31f17-4252-2f7c-6cbe-c30237426218/mzaf_2920171080801551292.plus.aac.p.m4a",
        "source_start_frame_48k": 1935640,
        "correlation": 0.994971,
    },
    {
        "id": "apple-pavlik-04-widmung",
        "source_id": "pavlik-04-solo-piano",
        "apple_album": "Ernő Dohnanyi: Piano Works, Vol. 3",
        "apple_track": "Winterreigen - 10 Bagatellen, Op. 13: I. Widmung",
        "apple_artist": "Ladislav Fanzowitz",
        "collection_id": 1604903453,
        "track_id": 1604903862,
        "preview_name": "solo-widmung-t1.m4a",
        "preview_url": "https://audio-ssl.itunes.apple.com/itunes-assets/AudioPreview126/v4/48/6a/d4/486ad4f0-499a-7524-1809-6a02ff45a584/mzaf_4682020732308497891.plus.aac.p.m4a",
        "source_start_frame_48k": 2158899,
        "correlation": 0.999906,
    },
    {
        "id": "apple-pavlik-05-schumann",
        "source_id": "pavlik-05-violin-orchestra",
        "apple_album": "Beethoven / Schumann / Pala / Lejava",
        "apple_track": "Cello concerto in A Minor, Op. 129 (Arr. for Violin & Ensemble by Marian Lejava): III. Sehr lebhaft",
        "apple_artist": "Milan Pala, Ensemble Opera Diversa & Marian Lejava",
        "collection_id": 1758677132,
        "track_id": 1758677448,
        "preview_name": "schumann-d2-t3.m4a",
        "preview_url": "https://audio-ssl.itunes.apple.com/itunes-assets/AudioPreview211/v4/9a/45/f5/9a45f543-cca1-f7c1-3bbf-b0df7c739c42/mzaf_12074919419992868543.plus.aac.p.m4a",
        "source_start_frame_48k": 2158899,
        "correlation": 0.981759,
    },
    {
        "id": "apple-pavlik-06-perroni",
        "source_id": "pavlik-06-cello-baroque",
        "apple_album": "Vienna 1728",
        "apple_track": "Concerto per il Violoncello ‘Suonato dallo stesso doppo' il Te Deum’ in D Minor: I. Grave. Allegro. Grave. Allegro",
        "apple_artist": "Michal Stahel & Solamente Naturali",
        "collection_id": 1222320953,
        "track_id": 1222320962,
        "preview_name": "perroni-t1.m4a",
        "preview_url": "https://audio-ssl.itunes.apple.com/itunes-assets/AudioPreview221/v4/2e/cd/2f/2ecd2f6b-0fa0-a5c9-3bd2-d09b269828e8/mzaf_5883969213924158385.plus.aac.p.m4a",
        "source_start_frame_48k": 0,
        "correlation": 0.997956,
    },
    {
        "id": "apple-pavlik-07-trumpet",
        "source_id": "pavlik-07-atmos-trumpet",
        "apple_album": "Jozef Podprocký: Koncerty",
        "apple_track": "Concerto for Trumpet and Orchestra, Op. 70: III. Allegro pressante",
        "apple_artist": "Various Artists",
        "collection_id": 1858623107,
        "track_id": 1858623599,
        "preview_name": "trumpet-op70-t6.m4a",
        "preview_url": "https://audio-ssl.itunes.apple.com/itunes-assets/AudioPreview211/v4/57/f5/6d/57f56dfb-55ad-dc98-cbd6-4574d588f32f/mzaf_14868168990558940714.plus.aac.p.m4a",
        "source_start_frame_48k": 2159155,
        "correlation": 0.819239,
    },
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    if destination.is_file() and destination.stat().st_size > 0:
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    command = [
        "curl",
        "-fsSL",
        "--retry",
        "3",
        "--output",
        str(destination),
        url,
    ]
    try:
        subprocess.run(command, check=True)
    except FileNotFoundError as error:
        raise RuntimeError("curl is required to download Apple public previews") from error
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"could not download Apple preview: {url}") from error


def decode_audio(
    path: Path,
    *,
    channels: int,
    audio_filter: str | None = None,
    start_frame: int | None = None,
    duration_frames: int | None = None,
) -> np.ndarray:
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
    if start_frame is not None:
        command += ["-ss", f"{start_frame / SAMPLE_RATE:.9f}"]
    if duration_frames is not None:
        command += ["-t", f"{duration_frames / SAMPLE_RATE:.9f}"]
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
    return decoded.reshape(-1, channels).astype(np.float64)


def normalize_stereo(stereo: np.ndarray) -> tuple[np.ndarray, float]:
    peak = max(float(np.max(np.abs(stereo))), 1e-12)
    gain = min(1.0, 0.90 / peak)
    return stereo * gain, gain


def prepare(
    source_manifest_path: Path,
    preview_root: Path,
    output_root: Path,
) -> dict[str, object]:
    source_manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))
    source_items = {item["id"]: item for item in source_manifest["items"]}
    prepared_items: list[dict[str, object]] = []

    for match in MATCHES:
        source_item = source_items[str(match["source_id"])]
        preview_path = preview_root / str(match["preview_name"])
        download(str(match["preview_url"]), preview_path)
        stereo = decode_audio(preview_path, channels=2)

        surround_info = source_item["surround"]
        surround_path = source_path(
            source_manifest_path,
            str(surround_info["path"]),
        )
        reference = decode_audio(
            surround_path,
            channels=6,
            audio_filter=surround_filter(source_item),
            start_frame=int(match["source_start_frame_48k"]),
            duration_frames=stereo.shape[0],
        )
        frame_count = min(stereo.shape[0], reference.shape[0])
        stereo, stereo_gain = normalize_stereo(stereo[:frame_count])
        reference, reference_fold, reference_gain = normalize_reference(
            reference[:frame_count]
        )

        item_root = output_root / str(match["id"])
        stereo_path = (item_root / "apple_preview_stereo.wav").resolve()
        reference_path = (item_root / "reference_5_1.wav").resolve()
        reference_fold_path = (item_root / "reference_fold_down_stereo.wav").resolve()
        write_pcm16(stereo_path, stereo)
        write_pcm16(reference_path, reference)
        write_pcm16(reference_fold_path, reference_fold)

        prepared_items.append(
            {
                "id": match["id"],
                "title": match["apple_track"],
                "genre": source_item["genre"],
                "instrumentation": source_item["instrumentation"],
                "dataset": "Apple public preview aligned to Pavlik Records authored surround",
                "evaluation_group": "apple-public-preview",
                "include_in_aggregate": True,
                "apple_album": match["apple_album"],
                "apple_artist": match["apple_artist"],
                "apple_collection_id": match["collection_id"],
                "apple_track_id": match["track_id"],
                "apple_music_url": (
                    f"https://music.apple.com/us/album/{match['collection_id']}"
                    f"?i={match['track_id']}"
                ),
                "apple_preview_url": match["preview_url"],
                "apple_preview_source": str(preview_path.resolve()),
                "apple_preview_sha256": sha256(preview_path),
                "alignment_reference": (
                    "publisher stereo" if source_item.get("stereo") else "Atmos stereo fold"
                ),
                "alignment_correlation": match["correlation"],
                "source_start_frame_48k": match["source_start_frame_48k"],
                "source_start_seconds": int(match["source_start_frame_48k"]) / SAMPLE_RATE,
                "sample_rate": SAMPLE_RATE,
                "frames": frame_count,
                "duration_seconds": frame_count / SAMPLE_RATE,
                "channel_order": ["FL", "FR", "FC", "LFE", "SL", "SR"],
                "reference_5_1": str(reference_path),
                "reference_5_1_sha256": sha256(reference_path),
                # The evaluator intentionally sees the Apple catalogue preview,
                # not the mathematically convenient reference fold-down.
                "fold_down_stereo": str(stereo_path),
                "fold_down_stereo_sha256": sha256(stereo_path),
                "reference_fold_down_stereo": str(reference_fold_path),
                "reference_fold_down_stereo_sha256": sha256(reference_fold_path),
                "apple_stereo_gain": stereo_gain,
                "reference_gain": reference_gain,
            }
        )
        print(
            f"{match['id']}: {frame_count / SAMPLE_RATE:.3f}s, "
            f"alignment corr={match['correlation']:.6f}"
        )

    return {
        "schema": 1,
        "corpus_kind": "apple-public-preview-authored-surround",
        "description": (
            "Official Apple public AAC previews aligned by waveform correlation "
            "to publicly downloadable publisher-authored 5.0/Atmos samples of "
            "the exact same recordings. Apple stereo and surround references "
            "may be separately mastered; waveform scores are comparative only."
        ),
        "source_manifest": str(source_manifest_path.resolve()),
        "sample_rate": SAMPLE_RATE,
        "sample_format": "PCM signed 16-bit little-endian",
        "channel_order": ["FL", "FR", "FC", "LFE", "SL", "SR"],
        "rights": (
            "Local evaluation only; copyright remains with Apple and Pavlik Records; "
            "do not commit or redistribute the audio."
        ),
        "items": prepared_items,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-manifest", type=Path, required=True)
    parser.add_argument("--preview-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.source_manifest.is_file():
        raise RuntimeError(f"source manifest does not exist: {args.source_manifest}")
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = prepare(
        args.source_manifest.resolve(),
        args.preview_root.resolve(),
        args.output.resolve(),
    )
    manifest_path = args.output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(manifest_path.resolve())
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
