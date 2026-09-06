#!/usr/bin/env python3
"""Convert verified 5.1 programmes into direct HSSR routing supervision.

The network still learns semantic classes from stem-labelled MUSDB.  This tool
adds the missing spatial lesson: it folds an authored six-channel excerpt to
the exact stereo signal seen by the upmixer, uses a frozen HSSR checkpoint for
soft semantic labels, and derives rearability/steering targets from the real
SL/SR channels.  Raw multichannel audio is never copied into the cache.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import subprocess
import time
from pathlib import Path

import numpy as np
import torch

from train_hssr import (
    BANDS,
    CLASSES,
    CONTROL_HOPS,
    FEATURES,
    HssrNetwork,
    OBJECT_FOCUS_PRESENCE,
    OUTPUTS,
    SAMPLE_RATE,
    aggregate_bands,
    make_features,
    smoothstep,
    stft,
)


SQRT_HALF = math.sqrt(0.5)


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source-manifest",
        type=Path,
        default=(
            root
            / "build/reference-audio/archive-surround-training-pool/manifest.json"
        ),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "build/hssr-archive-supervision",
    )
    parser.add_argument(
        "--teacher-checkpoint",
        type=Path,
        default=root / "build/hssr-edm-v2/hssr_development.pt",
    )
    parser.add_argument("--excerpt-seconds", type=float, default=20.0)
    parser.add_argument("--excerpts-per-file", type=int, default=1)
    parser.add_argument("--validation-fraction", type=float, default=0.15)
    parser.add_argument("--seed", type=int, default=0x41524348)
    parser.add_argument("--max-files", type=int, default=0)
    return parser.parse_args()


def channel_filter(layout: object) -> str:
    value = str(layout or "").lower()
    if value == "5.1":
        return "pan=5.1(side)|FL=FL|FR=FR|FC=FC|LFE=LFE|SL=BL|SR=BR"
    if value in {"5.1(side)", "5.1(side-downmix)"}:
        return "pan=5.1(side)|FL=FL|FR=FR|FC=FC|LFE=LFE|SL=SL|SR=SR"
    # For raw DTS and files whose layout metadata was dropped, FFmpeg's
    # decoded six-channel order is normally FL,FR,FC,LFE,BL,BR.
    return "pan=5.1(side)|c0=c0|c1=c1|c2=c2|c3=c3|c4=c4|c5=c5"


def decode_excerpt(path: Path, layout: object, start: float,
                   duration: float) -> np.ndarray:
    command = [
        "ffmpeg", "-nostdin", "-v", "error", "-ss", f"{start:.6f}",
        "-i", str(path), "-map", "0:a:0", "-t", f"{duration:.6f}",
        "-af", channel_filter(layout), "-ar", str(SAMPLE_RATE),
        "-c:a", "pcm_f32le", "-f", "f32le", "pipe:1",
    ]
    completed = subprocess.run(command, check=True, capture_output=True)
    decoded = np.frombuffer(completed.stdout, dtype="<f4")
    if decoded.size == 0 or decoded.size % 6:
        raise RuntimeError(f"malformed six-channel decode: {path}")
    return decoded.reshape(-1, 6).copy()


def canonical_fold(reference: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    fold = np.column_stack(
        (
            reference[:, 0] + SQRT_HALF * reference[:, 2]
            + SQRT_HALF * reference[:, 4],
            reference[:, 1] + SQRT_HALF * reference[:, 2]
            + SQRT_HALF * reference[:, 5],
        )
    ).astype(np.float32)
    peak = max(float(np.max(np.abs(fold))), 1.0e-8)
    gain = min(1.0, 0.90 / peak)
    return reference * gain, fold * gain


def teacher_targets(
    model: HssrNetwork,
    features: np.ndarray,
    mean: np.ndarray,
    inv_std: np.ndarray,
    device: torch.device,
) -> np.ndarray:
    normalized = np.clip((features - mean) * inv_std, -6.0, 6.0)
    tensor = torch.from_numpy(normalized[None].astype(np.float32)).to(device)
    with torch.no_grad():
        output = model(tensor)[0].cpu().numpy()
    band_output = output[:, :BANDS * 8].reshape(-1, BANDS, 8)
    logits = band_output[:, :, :CLASSES]
    logits -= np.max(logits, axis=2, keepdims=True)
    classes = np.exp(logits)
    classes /= np.sum(classes, axis=2, keepdims=True) + 1.0e-8
    rear = 1.0 / (1.0 + np.exp(-np.clip(band_output[:, :, 6], -20.0, 20.0)))
    pan = np.tanh(band_output[:, :, 7])
    globals_ = 1.0 / (
        1.0 + np.exp(-np.clip(output[:, BANDS * 8:], -20.0, 20.0))
    )
    band_target = np.concatenate(
        (classes, rear[:, :, None], pan[:, :, None]), axis=2
    ).reshape(-1, BANDS * 8)
    return np.concatenate((band_target, globals_), axis=1).astype(np.float32)


def apply_reference_rear_targets(
    targets: np.ndarray,
    fold_spec: np.ndarray,
    rear_spec: np.ndarray,
) -> np.ndarray:
    result = targets.copy()
    fold = aggregate_bands(fold_spec)
    rear = aggregate_bands(rear_spec)
    fold_energy = fold["pl"] + fold["pr"]
    rear_energy = rear["pl"] + rear["pr"]
    ratio = np.sqrt(rear_energy / (fold_energy + 1.0e-8))
    # Rearability is a routing control, not a request to reproduce the authored
    # rear level literally.  A conservative target transfers spatial ranking
    # while leaving final level to the bounded Object Focus renderer.
    authored = 0.50 * smoothstep(ratio, 0.06, 0.70)

    band_target = result[:, :BANDS * 8].reshape(-1, BANDS, 8)
    classes = band_target[:, :, :CLASSES]
    lead = classes[:, :, 0]
    bass = classes[:, :, 1]
    protection = np.power(1.0 - lead, 1.7) * np.power(1.0 - bass, 2.0)
    programme_threshold = np.maximum(
        np.median(fold_energy, axis=0, keepdims=True) * 1.0e-4,
        1.0e-10,
    )
    active = (fold_energy >= programme_threshold).astype(np.float32)
    authored *= OBJECT_FOCUS_PRESENCE[None, :] * protection * active

    old_rear = band_target[:, :, 6].copy()
    # Retain a quiet fraction of the semantic teacher where the authored mix
    # has no rear event, but make the actual SL/SR presentation authoritative.
    new_rear = np.maximum(0.65 * old_rear, authored)
    rear_pan = (
        np.sqrt(rear["pl"] + 1.0e-8) - np.sqrt(rear["pr"] + 1.0e-8)
    ) / (
        np.sqrt(rear["pl"] + 1.0e-8) + np.sqrt(rear["pr"] + 1.0e-8)
    )
    reference_weight = np.clip(
        authored / (authored + 0.65 * old_rear + 1.0e-8), 0.0, 1.0
    )
    band_target[:, :, 6] = np.clip(new_rear, 0.0, 0.90)
    band_target[:, :, 7] = np.clip(
        reference_weight * rear_pan
        + (1.0 - reference_weight) * band_target[:, :, 7],
        -1.0,
        1.0,
    )

    global_rear = np.sum(new_rear * fold_energy, axis=1) / (
        np.sum(fold_energy, axis=1) + 1.0e-8
    )
    result[:, BANDS * 8 + 4] = np.clip(global_rear, 0.0, 1.0)
    if result.shape[0] > 1:
        result[1:, BANDS * 8 + 5] = np.clip(
            1.0 - np.mean(np.abs(np.diff(new_rear, axis=0)), axis=1),
            0.0,
            1.0,
        )
    return np.nan_to_num(result, nan=0.0, posinf=1.0, neginf=0.0)


def safe_id(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "_", value).strip("_")[:100]


def split_identifiers(identifiers: list[str], fraction: float,
                      seed: int) -> set[str]:
    ranked = sorted(
        identifiers,
        key=lambda value: hashlib.sha256(
            f"{seed}:{value}".encode("utf-8")
        ).digest(),
    )
    count = max(1, round(len(ranked) * fraction)) if len(ranked) > 1 else 0
    return set(ranked[:count])


def excerpt_starts(duration: float, excerpt: float, count: int) -> list[float]:
    if duration <= excerpt:
        return [0.0]
    maximum = max(0.0, duration - excerpt)
    if count <= 1:
        return [0.5 * maximum]
    return [float(value) for value in np.linspace(0.0, maximum, count)]


def main() -> int:
    args = parse_args()
    source = json.loads(args.source_manifest.read_text(encoding="utf-8"))
    files = list(source.get("downloaded_files", []))
    if args.max_files:
        files = files[:args.max_files]
    if not files:
        raise SystemExit("source manifest contains no downloaded_files")
    if not 0.0 <= args.validation_fraction < 0.5:
        raise SystemExit("--validation-fraction must be in [0, 0.5)")

    checkpoint = torch.load(
        args.teacher_checkpoint, map_location="cpu", weights_only=False
    )
    mean = np.asarray(checkpoint["mean"], dtype=np.float32)
    inv_std = np.asarray(checkpoint["inv_std"], dtype=np.float32)
    if mean.shape != (FEATURES,) or inv_std.shape != (FEATURES,):
        raise RuntimeError("teacher normalization dimensions do not match HSSR")
    device = torch.device("mps" if torch.backends.mps.is_available() else "cpu")
    model = HssrNetwork().to(device)
    model.load_state_dict(checkpoint["state_dict"])
    model.eval()

    identifiers = sorted({str(item["archive_identifier"]) for item in files})
    validation = split_identifiers(
        identifiers, args.validation_fraction, args.seed
    )
    cache_items = []
    started = time.time()
    for file_index, item in enumerate(files, 1):
        path = Path(item["local_path"])
        duration = float(item.get("duration_seconds") or 0.0)
        if not path.is_file() or duration < 20.0:
            continue
        split = (
            "validation"
            if str(item["archive_identifier"]) in validation else "train"
        )
        starts = excerpt_starts(
            duration, args.excerpt_seconds, args.excerpts_per_file
        )
        for excerpt_index, start in enumerate(starts):
            try:
                reference = decode_excerpt(
                    path,
                    item.get("channel_layout"),
                    start,
                    min(args.excerpt_seconds, duration - start),
                )
                if reference.shape[0] < SAMPLE_RATE * 10:
                    continue
                reference, fold = canonical_fold(reference)
                fold_spec = stft(fold)
                rear_spec = stft(reference[:, 4:6])
                frames = min(fold_spec.shape[0], rear_spec.shape[0])
                fold_spec = fold_spec[:frames]
                rear_spec = rear_spec[:frames]
                features = make_features(fold, fold_spec)
                select = np.arange(CONTROL_HOPS - 1, frames, CONTROL_HOPS)
                features = features[select].astype(np.float32)
                targets = teacher_targets(
                    model, features, mean, inv_std, device
                )
                targets = apply_reference_rear_targets(
                    targets, fold_spec[select], rear_spec[select]
                )
            except (RuntimeError, subprocess.SubprocessError) as error:
                print(f"skip {path.name} at {start:.1f}s: {error}", flush=True)
                continue

            output_name = (
                f"{safe_id(str(item['archive_identifier']))}-"
                f"{file_index:04d}-{excerpt_index:02d}.npz"
            )
            output = args.output / split / output_name
            output.parent.mkdir(parents=True, exist_ok=True)
            np.savez_compressed(
                output,
                features=features,
                targets=targets,
            )
            cache_items.append(
                {
                    "archive_identifier": item["archive_identifier"],
                    "source_file": str(path.resolve()),
                    "source_remote_url": item["remote_url"],
                    "license_url": item["license_url"],
                    "split": split,
                    "start_seconds": start,
                    "duration_seconds": reference.shape[0] / SAMPLE_RATE,
                    "frames": int(features.shape[0]),
                    "cache": str(output.resolve()),
                }
            )
            print(
                f"cache {len(cache_items)} {split} "
                f"{item['archive_identifier']} start={start:.1f}s",
                flush=True,
            )

    manifest = {
        "schema": 1,
        "kind": "hssr-direct-surround-supervision",
        "development_only": True,
        "source_manifest": str(args.source_manifest.resolve()),
        "teacher_checkpoint": str(args.teacher_checkpoint.resolve()),
        "teacher_role": "soft semantic labels only",
        "spatial_target": (
            "canonical 5.1 fold input with direct authored SL/SR "
            "rearability and lateral steering"
        ),
        "train_identifiers": sorted(set(identifiers) - validation),
        "validation_identifiers": sorted(validation),
        "elapsed_seconds": time.time() - started,
        "items": cache_items,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        f"prepared caches={len(cache_items)} "
        f"train={sum(item['split'] == 'train' for item in cache_items)} "
        f"validation={sum(item['split'] == 'validation' for item in cache_items)}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
