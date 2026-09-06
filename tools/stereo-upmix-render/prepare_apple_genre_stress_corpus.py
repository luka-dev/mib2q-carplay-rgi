#!/usr/bin/env python3
"""Download official Apple previews for stereo-upmix genre stress tests.

The iTunes Search API and its unauthenticated 30-second preview URLs are used;
no Apple Music account, FairPlay content or screen/audio capture is involved.
Generated audio lives under ignored build/ storage and must not be committed or
redistributed.  Track IDs are fixed while metadata and preview URLs are looked
up afresh so expired CDN URLs do not make the corpus irreproducible.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import wave
from pathlib import Path
from typing import Any


SAMPLE_RATE = 48_000

TRACKS = (
    {
        "id": "rock-tool-schism",
        "group": "rock",
        "track_id": 1474185654,
        "artist": "TOOL",
        "title": "Schism",
        "stress_targets": ["polyrhythmic percussion", "panned guitar", "cymbal isolation"],
    },
    {
        "id": "rock-qotsa-no-one-knows",
        "group": "rock",
        "track_id": 1440759839,
        "artist": "Queens of the Stone Age",
        "title": "No One Knows",
        "stress_targets": ["dry drum transients", "dense guitar", "stable rear object"],
    },
    {
        "id": "rock-foo-fighters-everlong",
        "group": "rock",
        "track_id": 362133505,
        "artist": "Foo Fighters",
        "title": "Everlong",
        "stress_targets": ["dense double-tracked guitar", "continuous cymbals", "vocal safety"],
    },
    {
        "id": "rock-muse-hysteria",
        "group": "rock",
        "track_id": 1716097098,
        "artist": "Muse",
        "title": "Hysteria",
        "stress_targets": ["bass protection", "distorted guitar", "lead stability"],
    },
    {
        "id": "rap-kendrick-dna",
        "group": "rap",
        "track_id": 1440881990,
        "artist": "Kendrick Lamar",
        "title": "DNA.",
        "stress_targets": ["fast center vocal", "808 protection", "background FX"],
    },
    {
        "id": "rap-travis-sicko-mode",
        "group": "rap",
        "track_id": 1421658123,
        "artist": "Travis Scott",
        "title": "SICKO MODE",
        "stress_targets": ["scene changes", "adlibs", "wide effects", "sub protection"],
    },
    {
        "id": "rap-missy-get-ur-freak-on",
        "group": "rap",
        "track_id": 83134409,
        "artist": "Missy Elliott",
        "title": "Get Ur Freak On (Amended LP Version)",
        "stress_targets": ["sparse percussion", "center vocal", "short rear events"],
    },
    {
        "id": "rap-dj-shadow-nobody-speak",
        "group": "rap",
        "track_id": 1570361276,
        "artist": "DJ Shadow & Run The Jewels",
        "title": "Nobody Speak",
        "stress_targets": ["dual rap vocal", "horn sample", "drum separation"],
    },
    {
        "id": "dubstep-skrillex-bangarang",
        "group": "dubstep",
        "track_id": 491596647,
        "artist": "Skrillex",
        "title": "Bangarang (feat. Sirah)",
        "stress_targets": ["wobble bass", "hard stereo FX", "vocal chop", "drop transition"],
    },
    {
        "id": "dubstep-flux-i-cant-stop",
        "group": "dubstep",
        "track_id": 1769016658,
        "artist": "Flux Pavilion",
        "title": "I Can't Stop",
        "stress_targets": ["mono sub", "wide synth", "repeating transient object"],
    },
    {
        "id": "dubstep-nero-promises-remix",
        "group": "dubstep",
        "track_id": 1443218866,
        "artist": "Nero",
        "title": "Promises (Skrillex & Nero Remix)",
        "stress_targets": ["female vocal safety", "wide bass", "reverb and delay tails"],
    },
    {
        "id": "dubstep-knife-party-centipede",
        "group": "dubstep",
        "track_id": 530207562,
        "artist": "Knife Party",
        "title": "Centipede",
        "stress_targets": ["spoken center intro", "dense drop", "rapid stereo modulation"],
    },
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def curl_bytes(url: str) -> bytes:
    try:
        completed = subprocess.run(
            ["curl", "-fsSL", "--retry", "3", url],
            check=True,
            capture_output=True,
        )
    except FileNotFoundError as error:
        raise RuntimeError("curl is required") from error
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"download failed: {url}") from error
    return completed.stdout


def lookup_track(track: dict[str, Any]) -> dict[str, Any]:
    url = (
        "https://itunes.apple.com/lookup?country=US&entity=song&id="
        f"{track['track_id']}"
    )
    payload = json.loads(curl_bytes(url))
    matches = [
        item for item in payload.get("results", [])
        if int(item.get("trackId", -1)) == int(track["track_id"])
    ]
    if len(matches) != 1:
        raise RuntimeError(f"Apple lookup returned no unique result for {track['id']}")
    item = matches[0]
    if item.get("artistName") != track["artist"] or item.get("trackName") != track["title"]:
        raise RuntimeError(
            f"Apple metadata drift for {track['id']}: "
            f"{item.get('artistName')} / {item.get('trackName')}"
        )
    preview_url = item.get("previewUrl")
    if not isinstance(preview_url, str) or not preview_url.startswith(
        "https://audio-ssl.itunes.apple.com/"
    ):
        raise RuntimeError(f"official preview unavailable for {track['id']}")
    return item


def download_preview(url: str, destination: Path) -> None:
    if destination.is_file() and destination.stat().st_size > 0:
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(
            [
                "curl", "-fsSL", "--retry", "3", "--output",
                str(destination), url,
            ],
            check=True,
        )
    except FileNotFoundError as error:
        raise RuntimeError("curl is required") from error
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"preview download failed: {url}") from error


def decode_preview(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    command = [
        "ffmpeg", "-v", "error", "-nostdin", "-y",
        "-i", str(source), "-map", "0:a:0", "-vn",
        "-ac", "2", "-ar", str(SAMPLE_RATE),
        "-c:a", "pcm_s16le", "-map_metadata", "-1",
        str(destination),
    ]
    try:
        subprocess.run(command, check=True)
    except FileNotFoundError as error:
        raise RuntimeError("ffmpeg is required") from error
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"ffmpeg failed for {source}") from error


def wav_info(path: Path) -> tuple[int, int]:
    with wave.open(str(path), "rb") as stream:
        if stream.getnchannels() != 2 or stream.getsampwidth() != 2:
            raise RuntimeError(f"unexpected WAV format: {path}")
        if stream.getframerate() != SAMPLE_RATE:
            raise RuntimeError(f"unexpected WAV rate: {path}")
        return stream.getnframes(), stream.getframerate()


def prepare(preview_root: Path, output_root: Path) -> dict[str, Any]:
    items: list[dict[str, Any]] = []

    for track in TRACKS:
        metadata = lookup_track(track)
        preview_url = str(metadata["previewUrl"])
        preview_path = preview_root / f"{track['id']}.m4a"
        stereo_path = (output_root / str(track["id"]) / "stereo.wav").resolve()

        download_preview(preview_url, preview_path)
        decode_preview(preview_path, stereo_path)
        frames, _ = wav_info(stereo_path)
        items.append(
            {
                "id": track["id"],
                "group": track["group"],
                "artist": metadata["artistName"],
                "title": metadata["trackName"],
                "album": metadata.get("collectionName"),
                "apple_genre": metadata.get("primaryGenreName"),
                "track_id": metadata["trackId"],
                "apple_music_url": metadata.get("trackViewUrl"),
                "preview_url": preview_url,
                "preview_source": str(preview_path.resolve()),
                "preview_sha256": sha256(preview_path),
                "stereo": str(stereo_path),
                "stereo_sha256": sha256(stereo_path),
                "sample_rate": SAMPLE_RATE,
                "frames": frames,
                "duration_seconds": frames / SAMPLE_RATE,
                "stress_targets": track["stress_targets"],
            }
        )
        print(
            f"{track['group']:7s} {metadata['artistName']} - "
            f"{metadata['trackName']}: {frames / SAMPLE_RATE:.3f}s"
        )

    return {
        "schema": 1,
        "corpus_kind": "apple-public-preview-stereo-upmix-stress",
        "description": (
            "Official Apple public stereo previews selected to stress HSSR "
            "Object behavior in rock, rap and dubstep. No surround reference "
            "exists; this corpus is for safety, activity and A/B listening."
        ),
        "sample_rate": SAMPLE_RATE,
        "sample_format": "PCM signed 16-bit little-endian stereo",
        "rights": (
            "Local evaluation only; copyright remains with Apple and the "
            "rights holders. Do not commit or redistribute generated audio."
        ),
        "items": items,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = prepare(args.preview_root, args.output)
    manifest_path = args.output / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(manifest_path.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
