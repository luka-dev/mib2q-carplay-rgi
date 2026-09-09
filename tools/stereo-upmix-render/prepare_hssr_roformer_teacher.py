#!/usr/bin/env python3
"""Prepare resumable BS-RoFormer vocal/instrumental teacher stems.

Run this with the dedicated ``hssr-teacher`` virtual environment.  The audio
and resulting stems are private development inputs and must remain under the
ignored build tree; only distilled feature/target caches may be consumed by
the small in-vehicle controller.
"""

from __future__ import annotations

import argparse
import json
import logging
import hashlib
import importlib.metadata
from pathlib import Path
import time

import soundfile as sf

from audio_separator.separator import Separator


MODEL = "model_bs_roformer_ep_317_sdr_12.9755.ckpt"
OUTPUT_SUFFIX = "model_bs_roformer_ep_317_sdr_12.wav"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--teacher-root", type=Path, required=True)
    parser.add_argument("--model-dir", type=Path, required=True)
    parser.add_argument("--model", default=MODEL)
    parser.add_argument("--limit", type=int, default=0)
    return parser.parse_args()


def output_pair(directory: Path) -> tuple[Path, Path]:
    return (
        directory / f"stereo_(Vocals)_{OUTPUT_SUFFIX}",
        directory / f"stereo_(Instrumental)_{OUTPUT_SUFFIX}",
    )


def valid_audio(path: Path) -> bool:
    if not path.is_file():
        return False
    try:
        info = sf.info(path)
    except RuntimeError:
        return False
    return info.samplerate == 48_000 and info.channels == 2 and info.frames > 0


def valid_pair(directory: Path) -> bool:
    vocals, instrumental = output_pair(directory)
    if not valid_audio(vocals) or not valid_audio(instrumental):
        return False
    vocal_info = sf.info(vocals)
    instrumental_info = sf.info(instrumental)
    return vocal_info.frames == instrumental_info.frames


def main() -> int:
    args = parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    items = list(manifest["items"])
    pending = [
        item for item in items
        if not valid_pair(args.teacher_root / str(item["id"]))
    ]
    ready_count = len(items) - len(pending)
    if args.limit:
        pending = pending[:args.limit]

    print(
        f"teacher corpus={len(items)} ready={ready_count} "
        f"pending={len(pending)}",
        flush=True,
    )
    if not pending:
        return 0

    separator = Separator(
        log_level=logging.WARNING,
        model_file_dir=str(args.model_dir),
        output_dir=str(args.teacher_root),
        output_format="WAV",
        sample_rate=48_000,
    )
    separator.load_model(args.model)
    model_path = args.model_dir / args.model
    model_sha256 = hashlib.sha256(model_path.read_bytes()).hexdigest()
    print(f"teacher device={separator.torch_device} model_sha256={model_sha256}", flush=True)

    started = time.monotonic()
    for index, item in enumerate(pending, start=1):
        item_id = str(item["id"])
        source = Path(str(item["stereo"]))
        if not valid_audio(source):
            raise RuntimeError(f"invalid 48 kHz stereo source: {source}")

        directory = args.teacher_root / item_id
        directory.mkdir(parents=True, exist_ok=True)
        separator.output_dir = str(directory)
        separator.model_instance.output_dir = str(directory)
        item_started = time.monotonic()
        print(
            f"separate {index}/{len(pending)} {item_id}",
            flush=True,
        )
        separator.separate(str(source))
        if not valid_pair(directory):
            raise RuntimeError(f"teacher did not produce a valid pair: {directory}")
        print(
            f"ready {item_id} in {time.monotonic() - item_started:.1f}s",
            flush=True,
        )

    summary = {
        "schema": 1,
        "kind": "hssr-bs-roformer-teacher",
        "development_only": True,
        "private_audio": True,
        "source_manifest": str(args.manifest.resolve()),
        "model": args.model,
        "model_sha256": model_sha256,
        "audio_separator_version": importlib.metadata.version("audio-separator"),
        "torch_version": importlib.metadata.version("torch"),
        "device": str(separator.torch_device),
        "sample_rate": 48_000,
        "items": [
            {
                "id": str(item["id"]),
                "source_sha256": str(item["source_sha256"]),
                "ready": valid_pair(args.teacher_root / str(item["id"])),
            }
            for item in items
        ],
        "elapsed_seconds": time.monotonic() - started,
    }
    args.teacher_root.mkdir(parents=True, exist_ok=True)
    (args.teacher_root / "roformer-manifest.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(
        f"complete ready={sum(item['ready'] for item in summary['items'])} "
        f"elapsed={summary['elapsed_seconds']:.1f}s",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
