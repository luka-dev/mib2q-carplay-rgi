#!/usr/bin/env python3
"""Train/export the development HSSR-160 semantic control model.

The script consumes MUSDB ``*.stem.mp4`` files without unpacking them into the
repository.  Stream order follows the MUSDB stem container contract:
mixture, drums, bass, other, vocals.  ``train/`` is used for optimization and
``train/`` is split artist-disjoint into optimization/validation subsets;
``test/`` is evaluated once after checkpoint selection.

The resulting model is a control model, not a waveform separator.  It predicts
six semantic probabilities, rear suitability, and lateral steering for each of
32 bands plus eight global controls.  The binary is loaded by hssr_neon.c.

MUSDB audio has research/non-commercial licensing constraints.  Generated
weights stay under ignored build/ and are DEVELOPMENT-ONLY unless the training
corpus is replaced by commercially cleared stems.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import pathlib
import random
import struct
import subprocess
import tempfile
import time
import zlib

import numpy as np
import torch
from torch import nn
from torch.utils.data import DataLoader, Dataset


SAMPLE_RATE = 48_000
FFT_SIZE = 512
HOP_SIZE = 128
CONTROL_HOPS = 4
FEATURES = 368
HIDDEN = 160
OUTPUTS = 264
BANDS = 32
CLASSES = 6
SEQUENCE = 64

BAND_EDGES_HZ = np.asarray(
    [
        0, 60, 90, 125, 170, 220, 280, 350,
        440, 550, 680, 830, 1000, 1200, 1450, 1750,
        2100, 2500, 3000, 3600, 4300, 5100, 6000, 7000,
        8200, 9600, 11200, 13000, 15000, 17500, 20000, 22000,
        24001,
    ],
    dtype=np.float32,
)

# Match the deterministic Object Focus presence curve used by hssr_dsp.c.
# The curve prevents an EDM routing target from teaching the network to move
# sub-bass or ultrasonic-only material merely because it is stereo-wide.
OBJECT_FOCUS_PRESENCE = np.asarray(
    [
        0.00, 0.00, 0.00, 0.00, 0.00, 0.05, 0.10, 0.20,
        0.35, 0.50, 0.70, 0.85, 1.00, 1.00, 1.00, 1.00,
        1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00,
        1.00, 0.95, 0.75, 0.50, 0.30, 0.15, 0.05, 0.00,
    ],
    dtype=np.float32,
)


def parse_args() -> argparse.Namespace:
    root = pathlib.Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--musdb-root",
        type=pathlib.Path,
        default=root / "build/reference-audio/musdb",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=root / "build/hssr",
    )
    parser.add_argument(
        "--cache-root",
        type=pathlib.Path,
        help=(
            "reuse a prepared MUSDB feature cache; defaults to OUTPUT/cache. "
            "Targets are upgraded at load time, so classic and edm-v2 runs "
            "can safely share the same cache"
        ),
    )
    parser.add_argument(
        "--routing-target",
        choices=("classic", "edm-v2", "universal-v5"),
        default="classic",
        help="rear-routing supervision profile (model dimensions are unchanged)",
    )
    parser.add_argument(
        "--loss-profile",
        choices=("balanced", "selection-v11", "cabin-v12"),
        default="balanced",
        help=(
            "selection-v11 applies an asymmetric vocal/bass false-positive "
            "penalty while retaining stronger positive-mask weighting"
        ),
    )
    parser.add_argument(
        "--init-checkpoint",
        type=pathlib.Path,
        help="warm-start from an existing HSSR development checkpoint",
    )
    parser.add_argument("--init-model", type=pathlib.Path,
                        help="warm-start from a validated quantized HSSRNEO1 binary")
    parser.add_argument("--cache-only", action="store_true",
                        help="train from explicit extra-cache roots without requiring MUSDB audio")
    parser.add_argument(
        "--extra-cache-root",
        type=pathlib.Path,
        action="append",
        default=[],
        help=(
            "append a prepared direct-surround supervision cache containing "
            "train/*.npz and validation/*.npz; may be repeated"
        ),
    )
    parser.add_argument("--epochs", type=int, default=18)
    parser.add_argument('--sequence',type=int,default=SEQUENCE)
    parser.add_argument('--burn-in',type=int,default=0)
    parser.add_argument('--control-delay',type=int,default=0,choices=[0,1])
    parser.add_argument('--runtime-aware',action='store_true')
    parser.add_argument('--device', choices=['auto','cpu','mps'], default='auto')
    parser.add_argument('--cpu-threads', type=int, default=0,
                        help='explicit CPU thread count; 0 preserves PyTorch default')
    parser.add_argument('--routing-head-only',action='store_true',
                        help='preserve GRUs and semantic/LFE heads when only two-stem pseudo-labels are available')
    parser.add_argument('--max-train-batches',type=int,default=0,
                        help='explicit pilot budget per epoch; 0 uses all chunks')
    parser.add_argument(
        "--early-stopping-patience",
        type=int,
        default=0,
        help="stop after this many non-improving validation epochs (0 disables)",
    )
    parser.add_argument("--batch-size", type=int, default=16)
    parser.add_argument("--learning-rate", type=float, default=1.5e-3)
    parser.add_argument("--seed", type=int, default=0x48535352)
    parser.add_argument("--max-train-tracks", type=int, default=0)
    parser.add_argument("--max-test-tracks", type=int, default=0)
    parser.add_argument("--prepare-only", action="store_true")
    return parser.parse_args()


def decode_stem_container(path: pathlib.Path) -> np.ndarray:
    """Return [5, samples, 2] float32 at 48 kHz."""
    with tempfile.TemporaryDirectory(prefix="hssr-decode-") as temporary:
        temporary_path = pathlib.Path(temporary)
        command = ["ffmpeg", "-nostdin", "-v", "error", "-i", str(path)]
        outputs: list[pathlib.Path] = []
        for stream in range(5):
            output = temporary_path / f"stream-{stream}.f32"
            outputs.append(output)
            command.extend(
                [
                    "-map", f"0:a:{stream}",
                    "-ar", str(SAMPLE_RATE),
                    "-ac", "2",
                    "-c:a", "pcm_f32le",
                    "-f", "f32le",
                    str(output),
                ]
            )
        subprocess.run(command, check=True)
        streams = [np.fromfile(output, dtype="<f4").reshape(-1, 2)
                   for output in outputs]
    samples = min(stream.shape[0] for stream in streams)
    return np.stack([stream[:samples] for stream in streams], axis=0)


def stft(stereo: np.ndarray) -> np.ndarray:
    """Return complex64 [frames, 2, 257]."""
    if stereo.shape[0] < FFT_SIZE:
        return np.empty((0, 2, FFT_SIZE // 2 + 1), dtype=np.complex64)
    window = np.sin(
        np.pi * (np.arange(FFT_SIZE, dtype=np.float32) + 0.5) / FFT_SIZE
    )
    channel_specs = []
    for channel in range(2):
        frames = np.lib.stride_tricks.sliding_window_view(
            stereo[:, channel], FFT_SIZE
        )[::HOP_SIZE]
        channel_specs.append(
            np.fft.rfft(frames * window[None, :], axis=1).astype(np.complex64)
        )
    return np.stack(channel_specs, axis=1)


def band_bins() -> list[np.ndarray]:
    frequencies = np.arange(FFT_SIZE // 2 + 1) * SAMPLE_RATE / FFT_SIZE
    return [
        np.flatnonzero(
            (frequencies >= BAND_EDGES_HZ[band])
            & (frequencies < BAND_EDGES_HZ[band + 1])
        )
        for band in range(BANDS)
    ]


def aggregate_bands(spec: np.ndarray) -> dict[str, np.ndarray]:
    count = spec.shape[0]
    result = {
        name: np.zeros((count, BANDS), dtype=np.float32)
        for name in (
            "pl", "pr", "cross_re", "cross_im", "mid", "side",
            "magnitude", "flatness", "flux",
        )
    }
    magnitude_bins = np.sqrt(
        0.5 * (np.abs(spec[:, 0]) ** 2 + np.abs(spec[:, 1]) ** 2)
    ).astype(np.float32)
    previous_magnitude = np.concatenate(
        [np.zeros_like(magnitude_bins[:1]), magnitude_bins[:-1]], axis=0
    )

    for band, bins in enumerate(band_bins()):
        if bins.size == 0:
            continue
        left = spec[:, 0, bins]
        right = spec[:, 1, bins]
        pl = np.sum(np.abs(left) ** 2, axis=1)
        pr = np.sum(np.abs(right) ** 2, axis=1)
        cross = np.sum(left * np.conj(right), axis=1)
        mid = 0.5 * (left + right)
        side = 0.5 * (left - right)
        mags = magnitude_bins[:, bins]
        positive = np.maximum(mags - previous_magnitude[:, bins], 0.0)

        result["pl"][:, band] = pl
        result["pr"][:, band] = pr
        result["cross_re"][:, band] = cross.real
        result["cross_im"][:, band] = cross.imag
        result["mid"][:, band] = np.sum(np.abs(mid) ** 2, axis=1)
        result["side"][:, band] = np.sum(np.abs(side) ** 2, axis=1)
        result["magnitude"][:, band] = np.sum(mags, axis=1)
        arithmetic = np.mean(mags + 1.0e-8, axis=1)
        geometric = np.exp(np.mean(np.log(mags + 1.0e-8), axis=1))
        result["flatness"][:, band] = geometric / (arithmetic + 1.0e-8)
        result["flux"][:, band] = np.sum(positive, axis=1) / (
            np.sum(mags, axis=1) + 1.0e-8
        )
    return result


def make_features(mixture: np.ndarray, spec: np.ndarray) -> np.ndarray:
    aggregates = aggregate_bands(spec)
    eps = 1.0e-8
    pl = aggregates["pl"]
    pr = aggregates["pr"]
    energy = pl + pr
    cross_re = aggregates["cross_re"]
    cross_im = aggregates["cross_im"]
    cross_abs = np.sqrt(cross_re * cross_re + cross_im * cross_im)
    coherence = np.clip(cross_abs / np.sqrt(pl * pr + eps), 0.0, 1.0)
    tail = np.zeros_like(energy)
    slow = np.zeros(BANDS, dtype=np.float32)
    for frame in range(energy.shape[0]):
        slow += 0.08 * (energy[frame] - slow)
        tail[frame] = np.clip(
            slow / (energy[frame] + slow + eps), 0.0, 1.0
        ) * (1.0 - np.clip(aggregates["flux"][frame], 0.0, 1.0))

    per_band = np.stack(
        [
            np.log(energy + eps),
            np.log((aggregates["side"] + eps) /
                   (aggregates["mid"] + eps)),
            0.5 * np.log((pl + eps) / (pr + eps)),
            coherence,
            np.clip(cross_re / (cross_abs + eps), -1.0, 1.0),
            np.clip(cross_im / (cross_abs + eps), -1.0, 1.0),
            np.clip(aggregates["flux"], 0.0, 4.0),
            1.0 - np.clip(aggregates["flatness"], 0.0, 1.0),
            tail,
        ],
        axis=2,
    ).reshape(spec.shape[0], BANDS * 9)

    magnitude = np.sqrt(
        0.5 * (np.abs(spec[:, 0]) ** 2 + np.abs(spec[:, 1]) ** 2)
    )
    shape = np.zeros((spec.shape[0], 64), dtype=np.float32)
    for index in range(64):
        begin = index * 4
        end = min(begin + 4, magnitude.shape[1])
        shape[:, index] = np.mean(np.log(magnitude[:, begin:end] + eps), axis=1)

    control_sample = np.minimum(
        (np.arange(spec.shape[0]) * HOP_SIZE + FFT_SIZE - 1),
        mixture.shape[0] - 1,
    )
    # Crest is evaluated on the causal 512-sample analysis window.
    crest = np.zeros(spec.shape[0], dtype=np.float32)
    for frame, end in enumerate(control_sample):
        begin = max(0, int(end) - FFT_SIZE + 1)
        block = mixture[begin:int(end) + 1]
        crest[frame] = np.max(np.abs(block)) / (
            np.sqrt(np.mean(block * block) + eps)
        )
    total = np.sum(energy, axis=1) + eps
    weighted_band = energy / total[:, None]
    frequencies = 0.5 * (BAND_EDGES_HZ[:-1] + BAND_EDGES_HZ[1:])
    pan = (
        (np.sqrt(pl + eps) - np.sqrt(pr + eps))
        / (np.sqrt(pl + eps) + np.sqrt(pr + eps))
    )
    pan_probability = energy / total[:, None]
    pan_entropy = -np.sum(
        pan_probability * np.log(pan_probability + eps), axis=1
    ) / math.log(BANDS)
    energy_delta = np.zeros(spec.shape[0], dtype=np.float32)
    energy_delta[1:] = np.abs(np.diff(np.log(total)))
    sub120 = np.flatnonzero(BAND_EDGES_HZ[1:] <= 125)
    low250 = np.flatnonzero(
        (BAND_EDGES_HZ[:-1] >= 125) & (BAND_EDGES_HZ[1:] <= 280)
    )
    globals_ = np.stack(
        [
            np.log(total),
            crest,
            np.sum(aggregates["mid"], axis=1) /
                (np.sum(aggregates["mid"] + aggregates["side"], axis=1) + eps),
            np.sum(aggregates["side"], axis=1) /
                (np.sum(aggregates["mid"] + aggregates["side"], axis=1) + eps),
            np.sum(coherence * energy, axis=1) / total,
            np.sum(energy[:, sub120], axis=1) / total,
            np.sum(energy[:, low250], axis=1) / total,
            np.sum(weighted_band * frequencies[None, :], axis=1) / 24000.0,
            np.sum(aggregates["flux"] * energy, axis=1) / total,
            np.mean(aggregates["flux"] > 0.20, axis=1),
            np.sum((1.0 - aggregates["flatness"]) * energy, axis=1) / total,
            np.sum((1.0 - coherence) * energy, axis=1) / total,
            np.sum(pan * energy, axis=1) / total,
            pan_entropy,
            np.zeros(spec.shape[0], dtype=np.float32),
            energy_delta,
        ],
        axis=1,
    )
    features = np.concatenate([per_band, shape, globals_], axis=1)
    if features.shape[1] != FEATURES:
        raise RuntimeError(f"feature width {features.shape[1]} != {FEATURES}")
    return np.nan_to_num(features, nan=0.0, posinf=20.0, neginf=-20.0)


def make_targets(stem_specs: np.ndarray) -> np.ndarray:
    """stem_specs is [4, frames, 2, bins] in drums,bass,other,vocals order."""
    eps = 1.0e-8
    frames = stem_specs.shape[1]
    stem_energy = np.zeros((4, frames, BANDS), dtype=np.float32)
    other_pl = np.zeros((frames, BANDS), dtype=np.float32)
    other_pr = np.zeros((frames, BANDS), dtype=np.float32)
    other_cross = np.zeros((frames, BANDS), dtype=np.complex64)
    other_flux = np.zeros((frames, BANDS), dtype=np.float32)

    for stem in range(4):
        aggregate = aggregate_bands(stem_specs[stem])
        stem_energy[stem] = aggregate["pl"] + aggregate["pr"]
        if stem == 2:
            other_pl = aggregate["pl"]
            other_pr = aggregate["pr"]
            other_cross = aggregate["cross_re"] + 1j * aggregate["cross_im"]
            other_flux = np.clip(aggregate["flux"], 0.0, 1.0)

    total = np.sum(stem_energy, axis=0) + eps
    ratio = stem_energy / total[None, :, :]
    drums, bass, other, vocals = ratio
    other_coherence = np.clip(
        np.abs(other_cross) / np.sqrt(other_pl * other_pr + eps), 0.0, 1.0
    )
    other_edge = np.abs(other_pl - other_pr) / (other_pl + other_pr + eps)
    diffuse = 1.0 - other_coherence
    fx_share = np.clip(diffuse * (1.0 - other_flux), 0.0, 0.75)
    background_share = np.clip(
        0.25 + 0.45 * diffuse + 0.30 * other_edge, 0.0, 1.0
    ) * (1.0 - fx_share)
    foreground_share = np.maximum(1.0 - fx_share - background_share, 0.0)

    classes = np.stack(
        [
            vocals,
            bass,
            drums,
            other * foreground_share,
            other * background_share,
            other * fx_share,
        ],
        axis=2,
    )
    classes /= np.sum(classes, axis=2, keepdims=True) + eps

    rearability = np.clip(
        np.power(other, 0.70)
        * np.power(1.0 - vocals, 2.0)
        * np.power(1.0 - bass, 1.5)
        * (1.0 - 0.65 * drums)
        * (0.20 + 0.80 * (background_share + fx_share)),
        0.0,
        1.0,
    )
    lateral = np.clip(
        (np.sqrt(other_pl + eps) - np.sqrt(other_pr + eps)) /
        (np.sqrt(other_pl + eps) + np.sqrt(other_pr + eps)),
        -1.0,
        1.0,
    )

    per_band = np.concatenate(
        [classes, rearability[:, :, None], lateral[:, :, None]], axis=2
    ).reshape(frames, BANDS * 8)
    stem_global = np.sum(stem_energy, axis=2)
    stem_global /= np.sum(stem_global, axis=0, keepdims=True) + eps
    global_rear = np.sum(rearability * total, axis=1) / np.sum(total, axis=1)
    class_entropy = (
        -np.sum(classes * np.log(classes + eps), axis=2)
        / math.log(CLASSES)
    )
    scene_stability = np.ones(frames, dtype=np.float32)
    if frames > 1:
        scene_stability[1:] = np.clip(
            1.0 - np.mean(np.abs(np.diff(rearability, axis=0)), axis=1),
            0.0,
            1.0,
        )
    fx_present = np.sum(classes[:, :, 5] * total, axis=1) / np.sum(total, axis=1)
    global_targets = np.stack(
        [
            stem_global[3],
            stem_global[1],
            np.mean(other_coherence, axis=1),
            1.0 - np.mean(class_entropy, axis=1),
            global_rear,
            scene_stability,
            fx_present,
            np.clip(1.0 - stem_global[3] - 0.5 * stem_global[1], 0.0, 1.0),
        ],
        axis=1,
    )
    result = np.concatenate([per_band, global_targets], axis=1)
    if result.shape[1] != OUTPUTS:
        raise RuntimeError(f"target width {result.shape[1]} != {OUTPUTS}")
    return np.nan_to_num(result, nan=0.0, posinf=1.0, neginf=0.0)


def smoothstep(values: np.ndarray, low: float, high: float) -> np.ndarray:
    normalized = np.clip((values - low) / (high - low), 0.0, 1.0)
    return normalized * normalized * (3.0 - 2.0 * normalized)


def apply_edm_v2_routing_targets(
    features: np.ndarray, targets: np.ndarray
) -> np.ndarray:
    """Teach rearability for spatially distinct EDM percussion/synth objects.

    MUSDB exposes only vocals/bass/drums/other.  In dense electronic music a
    synth chop, shaker, or impact is consequently often labelled ``drums`` and
    the original target suppresses it, even when its authored stereo geometry
    makes it a safe rear object.  This target upgrade deliberately leaves the
    six semantic labels intact and changes only rear suitability/steering.

    The target remains conservative: it requires a wide scene plus local ILD
    or coherent IPD evidence, applies the production presence curve, and keeps
    lead/bass protection.  It therefore teaches the existing output head what
    the DSP rescue currently has to infer by hand, without changing the model
    dimensions or target inference cost.
    """
    result = targets.copy()
    per_feature_band = features[:, :BANDS * 9].reshape(-1, BANDS, 9)
    per_target_band = result[:, :BANDS * 8].reshape(-1, BANDS, 8)

    classes = per_target_band[:, :, :CLASSES]
    lead = classes[:, :, 0]
    bass = classes[:, :, 1]
    drums = classes[:, :, 2]
    foreground = classes[:, :, 3]
    background = classes[:, :, 4]
    effects = classes[:, :, 5]

    ild = per_feature_band[:, :, 2]
    coherence = np.clip(per_feature_band[:, :, 3], 0.0, 1.0)
    sin_ipd = np.abs(per_feature_band[:, :, 5])
    flux = np.clip(per_feature_band[:, :, 6], 0.0, 1.0)
    scene_side_ratio = np.clip(features[:, BANDS * 9 + 64 + 3], 0.0, 1.0)

    scene_width = smoothstep(scene_side_ratio[:, None], 0.025, 0.16)
    ild_object = smoothstep(np.abs(np.tanh(0.5 * ild)), 0.025, 0.40)
    ipd_object = (
        smoothstep(sin_ipd, 0.12, 0.70)
        * smoothstep(coherence, 0.30, 0.90)
        * 0.60
    )
    spatial_object = np.maximum(ild_object, ipd_object)
    transient_support = 0.35 + 0.65 * smoothstep(flux, 0.02, 0.35)
    route_class = np.clip(
        drums + 0.35 * foreground + background + effects
        - 0.25 * lead - 0.35 * bass,
        0.0,
        1.0,
    )
    class_support = smoothstep(route_class, 0.10, 0.55)
    protection = np.power(1.0 - lead, 1.5) * np.power(1.0 - bass, 1.8)
    candidate = np.clip(
        0.82
        * scene_width
        * OBJECT_FOCUS_PRESENCE[None, :]
        * spatial_object
        * transient_support
        * class_support
        * protection,
        0.0,
        0.85,
    )

    old_rear = per_target_band[:, :, 6].copy()
    new_rear = np.maximum(old_rear, candidate)
    extra_fraction = np.clip(
        (new_rear - old_rear) / (new_rear + 1.0e-8), 0.0, 1.0
    )
    measured_pan = np.tanh(0.5 * ild)
    per_target_band[:, :, 6] = new_rear
    per_target_band[:, :, 7] = np.clip(
        (1.0 - extra_fraction) * per_target_band[:, :, 7]
        + extra_fraction * measured_pan,
        -1.0,
        1.0,
    )

    energy = np.exp(np.clip(per_feature_band[:, :, 0], -30.0, 30.0))
    global_rear = np.sum(new_rear * energy, axis=1) / (
        np.sum(energy, axis=1) + 1.0e-8
    )
    result[:, BANDS * 8 + 4] = np.maximum(
        result[:, BANDS * 8 + 4], global_rear
    )
    if result.shape[0] > 1:
        result[1:, BANDS * 8 + 5] = np.minimum(
            result[1:, BANDS * 8 + 5],
            np.clip(
                1.0 - np.mean(np.abs(np.diff(new_rear, axis=0)), axis=1),
                0.0,
                1.0,
            ),
        )
    return np.nan_to_num(result, nan=0.0, posinf=1.0, neginf=0.0)


def apply_universal_v5_routing_targets(
    features: np.ndarray, targets: np.ndarray
) -> np.ndarray:
    """Teach an automatic best-safe-object policy with nonzero coverage.

    The high-quality path remains the EDM/spatial target.  Universal adds a
    lower-confidence target for energetic mid/high TF regions when no clean
    lateral object exists.  Lead vocal and bass/kick remain strongly vetoed;
    the production renderer treats this target as a phase-distinct fallback,
    not as a claim that a studio-quality stem was recovered.
    """
    result = apply_edm_v2_routing_targets(features, targets)
    per_feature_band = features[:, :BANDS * 9].reshape(-1, BANDS, 9)
    per_target_band = result[:, :BANDS * 8].reshape(-1, BANDS, 8)
    classes = per_target_band[:, :, :CLASSES]
    lead = classes[:, :, 0]
    bass = classes[:, :, 1]
    drums = classes[:, :, 2]
    foreground = classes[:, :, 3]
    background = classes[:, :, 4]
    effects = classes[:, :, 5]

    log_energy = per_feature_band[:, :, 0]
    relative_log_energy = log_energy - np.max(log_energy, axis=1, keepdims=True)
    active = smoothstep(relative_log_energy, -7.0, -2.0)
    flux = np.clip(per_feature_band[:, :, 6], 0.0, 1.0)
    tail = np.clip(per_feature_band[:, :, 8], 0.0, 1.0)
    semantic = np.clip(
        background + effects + 0.35 * foreground + 0.20 * drums,
        0.0,
        1.0,
    )
    protection = np.power(1.0 - lead, 2.5) * np.power(1.0 - bass, 2.2)
    texture = 0.55 + 0.25 * smoothstep(flux, 0.02, 0.35) + 0.20 * tail
    fallback = np.clip(
        0.24
        * OBJECT_FOCUS_PRESENCE[None, :]
        * active
        * (0.35 + 0.65 * semantic)
        * protection
        * texture,
        0.0,
        0.30,
    )

    old_rear = per_target_band[:, :, 6].copy()
    new_rear = np.maximum(old_rear, fallback)
    per_target_band[:, :, 6] = new_rear

    # The fallback has no trustworthy authored pan.  Keep measured/teacher pan
    # where one exists; otherwise the deterministic renderer chooses a stable
    # frequency-alternating rear side and applies conjugate phase rotation.
    energy = np.exp(np.clip(log_energy, -30.0, 30.0))
    global_rear = np.sum(new_rear * energy, axis=1) / (
        np.sum(energy, axis=1) + 1.0e-8
    )
    result[:, BANDS * 8 + 4] = np.maximum(
        result[:, BANDS * 8 + 4], global_rear
    )
    if result.shape[0] > 1:
        result[1:, BANDS * 8 + 5] = np.minimum(
            result[1:, BANDS * 8 + 5],
            np.clip(
                1.0 - np.mean(np.abs(np.diff(new_rear, axis=0)), axis=1),
                0.0,
                1.0,
            ),
        )
    return np.nan_to_num(result, nan=0.0, posinf=1.0, neginf=0.0)


def cache_track(path: pathlib.Path, cache_path: pathlib.Path) -> None:
    streams = decode_stem_container(path)
    specs = np.stack([stft(stream) for stream in streams], axis=0)
    frames = min(spec.shape[0] for spec in specs)
    specs = specs[:, :frames]
    features = make_features(streams[0], specs[0])
    targets = make_targets(specs[1:5])
    select = np.arange(CONTROL_HOPS - 1, frames, CONTROL_HOPS)
    features = features[select].astype(np.float32)
    targets = targets[select].astype(np.float32)
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(cache_path, features=features, targets=targets)


def prepare_split(paths: list[pathlib.Path], cache_dir: pathlib.Path) -> list[pathlib.Path]:
    cached = []
    for index, source in enumerate(paths, start=1):
        output = cache_dir / f"{source.stem}.npz"
        cached.append(output)
        if output.exists():
            continue
        print(f"prepare {index}/{len(paths)} {source.name}", flush=True)
        cache_track(source, output)
    return cached


def split_train_validation(
    paths: list[pathlib.Path], seed: int
) -> tuple[list[pathlib.Path], list[pathlib.Path]]:
    """Make a deterministic artist-disjoint validation split."""
    if len(paths) < 2:
        return paths, paths
    artists = sorted({path.name.split(" - ", 1)[0] for path in paths})
    generator = random.Random(seed ^ 0x56414C49)
    generator.shuffle(artists)
    validation_count = max(1, int(round(len(artists) * 0.15)))
    validation_artists = set(artists[:validation_count])
    train = [path for path in paths
             if path.name.split(" - ", 1)[0] not in validation_artists]
    validation = [path for path in paths
                  if path.name.split(" - ", 1)[0] in validation_artists]
    if not train or not validation:
        return paths[:-1], paths[-1:]
    return train, validation


class SequenceDataset(Dataset):
    def __init__(self, files: list[pathlib.Path], mean: np.ndarray,
                 inv_std: np.ndarray, sequence: int = SEQUENCE,
                 routing_target: str = "classic"):
        self.tracks = []
        self.index = []
        self.mean = mean.astype(np.float32)
        self.inv_std = inv_std.astype(np.float32)
        self.sequence = sequence
        for track_index, path in enumerate(files):
            with np.load(path) as data:
                features = data["features"].astype(np.float32)
                targets = data["targets"].astype(np.float32)
                routing_locked = (
                    bool(data["routing_locked"])
                    if "routing_locked" in data else False
                )
            if routing_target == "edm-v2" and not routing_locked:
                targets = apply_edm_v2_routing_targets(features, targets)
            elif routing_target == "universal-v5" and not routing_locked:
                targets = apply_universal_v5_routing_targets(features, targets)
            self.tracks.append((features, targets))
            if features.shape[0] <= sequence:
                self.index.append((track_index, 0))
            else:
                for begin in range(0, features.shape[0] - sequence + 1,
                                   sequence // 2):
                    self.index.append((track_index, begin))

    def __len__(self) -> int:
        return len(self.index)

    def __getitem__(self, item: int) -> tuple[torch.Tensor, torch.Tensor]:
        track_index, begin = self.index[item]
        features, targets = self.tracks[track_index]
        x = features[begin:begin + self.sequence]
        y = targets[begin:begin + self.sequence]
        if x.shape[0] < self.sequence:
            pad = self.sequence - x.shape[0]
            x = np.pad(x, ((0, pad), (0, 0)))
            y = np.pad(y, ((0, pad), (0, 0)))
        x = np.clip((x - self.mean) * self.inv_std, -6.0, 6.0)
        return torch.from_numpy(x), torch.from_numpy(y)


class HssrNetwork(nn.Module):
    def __init__(self) -> None:
        super().__init__()
        self.gru1 = nn.GRU(FEATURES, HIDDEN, batch_first=True)
        self.gru2 = nn.GRU(HIDDEN, HIDDEN, batch_first=True)
        self.head = nn.Linear(HIDDEN, OUTPUTS)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.forward_state(x)[0]

    def forward_state(self,x,state=None):
        if getattr(self,'runtime_aware',False):
            from hssr_runtime_training import forward
            return forward(self,x,state)
        h1,h2=(None,None) if state is None else state
        x,h1=self.gru1(x,h1)
        x,h2=self.gru2(x,h2)
        return self.head(x),(h1,h2)


def freeze_except_routing_head(model: HssrNetwork) -> None:
    """Two-stem labels may tune routing/pan, never overwrite six-stem semantics."""
    model.gru1.requires_grad_(False)
    model.gru2.requires_grad_(False)
    row_mask = model.head.weight.new_zeros(OUTPUTS)
    row_mask[6:BANDS*8:8] = 1
    row_mask[7:BANDS*8:8] = 1
    model.head.weight.register_hook(lambda grad: grad * row_mask[:, None])
    model.head.bias.register_hook(lambda grad: grad * row_mask)


def loss_components(
    prediction: torch.Tensor,
    target: torch.Tensor,
    loss_profile: str = "balanced",
    runtime_aware: bool = False,
) -> dict[str, torch.Tensor]:
    pred_band = prediction[..., :BANDS * 8].reshape(
        *prediction.shape[:-1], BANDS, 8
    )
    target_band = target[..., :BANDS * 8].reshape(
        *target.shape[:-1], BANDS, 8
    )
    class_loss = -torch.sum(
        target_band[..., :6] * torch.log_softmax(pred_band[..., :6], dim=-1),
        dim=-1,
    ).mean()
    if loss_profile == "cabin-v12":
        # Runtime vocal protection also depends on the class posterior (its
        # object-rescue paths can bypass rearability). Penalizing only rear
        # false positives therefore does not protect those paths. Add a
        # numerically stable one-vs-rest vocal classification lesson.
        vocal_logit = pred_band[..., 0] - torch.logsumexp(pred_band[..., 1:6], dim=-1)
        # Vocal-negative caches use class mass as an activity/confidence
        # weight, not a normalized posterior. Zero mass means UNLABELLED,
        # never an explicit non-vocal lesson. Preserve that CE convention.
        class_mass = torch.sum(target_band[..., :6], dim=-1).detach()
        vocal_target = target_band[..., 0].detach() / class_mass.clamp_min(1.0e-8)
        vocal_error = nn.functional.binary_cross_entropy_with_logits(
            vocal_logit, vocal_target, reduction="none")
        class_loss = class_loss + torch.mean(
            vocal_error * class_mass * (1.0 + 3.0 * vocal_target))
    if runtime_aware:
        from hssr_runtime_training import sigmoid as runtime_sigmoid, tanh as runtime_tanh
        predicted_rear = runtime_sigmoid(pred_band[..., 6])
        predicted_pan = runtime_tanh(pred_band[..., 7])
        rear_element_loss = nn.functional.binary_cross_entropy(
            predicted_rear.clamp(1e-6, 1-1e-6), target_band[..., 6], reduction="none")
    else:
        predicted_rear = torch.sigmoid(pred_band[..., 6])
        predicted_pan = torch.tanh(pred_band[..., 7])
        rear_element_loss = nn.functional.binary_cross_entropy_with_logits(
            pred_band[..., 6], target_band[..., 6], reduction="none")
    # Rear-positive TF regions are rare compared with protected/empty bands.
    # Without weighting, predicting a very small rearability everywhere is a
    # deceptively good optimum (and exactly what the original EDM trace showed).
    rear_target = target_band[..., 6].detach()
    if loss_profile in ("selection-v11", "cabin-v12"):
        # A vocal false positive is far more objectionable than a missed rear
        # object.  Counterfactual mixtures make that boundary observable; the
        # asymmetric term makes the controller actually respect it.  Positive
        # masks remain strongly weighted so the trivial all-front solution is
        # not competitive.
        vocal_veto = target_band[..., 0].detach() * (1.0 - rear_target)
        bass_veto = target_band[..., 1].detach() * (1.0 - rear_target)
        rear_weight = (
            1.0 + 4.0 * rear_target + 5.0 * vocal_veto + 2.0 * bass_veto
        )
    else:
        rear_weight = 1.0 + 3.0 * rear_target
    rear_loss = torch.mean(rear_element_loss * rear_weight)
    pan_weight = rear_target
    pan_loss = torch.sum(
        (predicted_pan - target_band[..., 7]) ** 2
        * pan_weight
    ) / (torch.sum(pan_weight) + 1.0)
    global_loss = nn.functional.binary_cross_entropy_with_logits(
        prediction[..., BANDS * 8:], target[..., BANDS * 8:]
    )
    smooth_loss = torch.mean(
        torch.abs(predicted_rear[:, 1:] - predicted_rear[:, :-1])
    )
    if loss_profile in ("selection-v11", "cabin-v12"):
        total = class_loss + 2.4 * rear_loss + 0.5 * pan_loss + \
                0.5 * global_loss + 0.08 * smooth_loss
    else:
        total = class_loss + 2.0 * rear_loss + 0.5 * pan_loss + \
                0.5 * global_loss + 0.05 * smooth_loss
    return {
        "total": total,
        "class": class_loss,
        "rear": rear_loss,
        "pan": pan_loss,
        "global": global_loss,
        "smooth": smooth_loss,
    }


def compute_normalization(files: list[pathlib.Path]) -> tuple[np.ndarray, np.ndarray]:
    count = 0
    total = np.zeros(FEATURES, dtype=np.float64)
    total_square = np.zeros(FEATURES, dtype=np.float64)
    for path in files:
        with np.load(path) as data:
            features = data["features"].astype(np.float64)
        count += features.shape[0]
        total += np.sum(features, axis=0)
        total_square += np.sum(features * features, axis=0)
    mean = total / max(count, 1)
    variance = np.maximum(total_square / max(count, 1) - mean * mean, 1.0e-6)
    return mean.astype(np.float32), (1.0 / np.sqrt(variance)).astype(np.float32)


def evaluate(
    model: HssrNetwork,
    loader: DataLoader,
    device: torch.device,
    loss_profile: str = "balanced",
) -> dict[str, float]:
    sums: dict[str, float] = {}
    batches = 0
    model.eval()
    with torch.no_grad():
        for features, targets in loader:
            from hssr_runtime_training import supervised_prediction
            prediction,target=supervised_prediction(model,features.to(device),targets.to(device),
                getattr(model,'burn_in',0),getattr(model,'control_delay',0))
            values = loss_components(
                prediction, target, loss_profile, getattr(model, 'runtime_aware', False)
            )
            for name, value in values.items():
                sums[name] = sums.get(name, 0.0) + float(value.cpu())
            batches += 1
    return {name: value / max(batches, 1) for name, value in sums.items()}


def quantize_rows(weight: torch.Tensor) -> tuple[np.ndarray, np.ndarray]:
    array = weight.detach().cpu().numpy().astype(np.float32)
    scale = np.max(np.abs(array), axis=1) / 127.0
    scale = np.maximum(scale, 1.0e-8).astype(np.float32)
    quantized = np.clip(np.rint(array / scale[:, None]), -127, 127).astype(np.int8)
    return quantized, scale


def export_model(model: HssrNetwork, mean: np.ndarray, inv_std: np.ndarray,
                 path: pathlib.Path) -> dict[str, object]:
    matrices = [
        model.gru1.weight_ih_l0,
        model.gru1.weight_hh_l0,
        model.gru2.weight_ih_l0,
        model.gru2.weight_hh_l0,
        model.head.weight,
    ]
    quantized_and_scales = [quantize_rows(matrix) for matrix in matrices]
    weights = b"".join(item[0].tobytes(order="C")
                       for item in quantized_and_scales)
    scales = [item[1] for item in quantized_and_scales]
    biases = [
        model.gru1.bias_ih_l0.detach().cpu().numpy().astype("<f4"),
        model.gru1.bias_hh_l0.detach().cpu().numpy().astype("<f4"),
        model.gru2.bias_ih_l0.detach().cpu().numpy().astype("<f4"),
        model.gru2.bias_hh_l0.detach().cpu().numpy().astype("<f4"),
        model.head.bias.detach().cpu().numpy().astype("<f4"),
    ]
    floats = b"".join(
        array.astype("<f4").tobytes(order="C")
        for array in [mean, inv_std, *scales, *biases]
    )
    payload = weights + floats
    expected_weight_bytes = 449_280
    expected_float_count = 5_104
    expected_payload_bytes = 469_696
    if len(weights) != expected_weight_bytes:
        raise RuntimeError(f"weight bytes {len(weights)}")
    if len(floats) != expected_float_count * 4:
        raise RuntimeError(f"float bytes {len(floats)}")
    if len(payload) != expected_payload_bytes:
        raise RuntimeError(f"payload bytes {len(payload)}")
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    header = struct.pack(
        "<16I",
        0x52535348, 0x314F454E, 1, 64,
        FEATURES, HIDDEN, OUTPUTS,
        expected_weight_bytes, expected_float_count,
        expected_payload_bytes, crc,
        0, 0, 0, 0, 0,
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + payload)
    return {
        "path": str(path),
        "bytes": len(header) + len(payload),
        "payload_crc32": f"{crc:08x}",
        "parameters": sum(parameter.numel() for parameter in model.parameters()),
        "weight_bytes": len(weights),
        "float_count": expected_float_count,
    }


def import_model(path: pathlib.Path) -> tuple[HssrNetwork, np.ndarray, np.ndarray]:
    """Recover trainable row-dequantized weights, not a lost float checkpoint.

    Keeps the exact normalization and biases embedded in the deployed model.
    Torch inference is not bit-equivalent to the integer-activation C runtime.
    """
    raw = path.read_bytes()
    if len(raw) != 469760:
        raise ValueError("invalid HSSR model length")
    header = struct.unpack_from("<16I", raw)
    expected = (0x52535348, 0x314F454E, 1, 64, FEATURES, HIDDEN, OUTPUTS,
                449280, 5104, 469696)
    if header[:10] != expected or any(header[11:]) or zlib.crc32(raw[64:]) & 0xFFFFFFFF != header[10]:
        raise ValueError("invalid HSSR model header/CRC")
    floats = np.frombuffer(raw, dtype="<f4", offset=64 + 449280).copy()
    if not np.isfinite(floats).all() or np.any(floats[FEATURES:FEATURES * 2] <= 0):
        raise ValueError("invalid HSSR model normalization/floats")
    model = HssrNetwork()
    matrices = [model.gru1.weight_ih_l0, model.gru1.weight_hh_l0,
                model.gru2.weight_ih_l0, model.gru2.weight_hh_l0, model.head.weight]
    biases = [model.gru1.bias_ih_l0, model.gru1.bias_hh_l0,
              model.gru2.bias_ih_l0, model.gru2.bias_hh_l0, model.head.bias]
    byte_offset, float_offset = 64, FEATURES * 2
    with torch.no_grad():
        for matrix in matrices:
            rows, columns = matrix.shape
            scale = floats[float_offset:float_offset + rows]
            if np.any(scale <= 0):
                raise ValueError("invalid HSSR row scales")
            weights = np.frombuffer(raw, dtype=np.int8, count=rows * columns,
                                    offset=byte_offset).reshape(rows, columns)
            matrix.copy_(torch.from_numpy(weights.astype(np.float32) * scale[:, None]))
            byte_offset += rows * columns
            float_offset += rows
        for bias in biases:
            bias.copy_(torch.from_numpy(floats[float_offset:float_offset + bias.numel()]))
            float_offset += bias.numel()
    return model, floats[:FEATURES], floats[FEATURES:FEATURES * 2]


def main() -> int:
    args = parse_args()
    if args.sequence<2 or args.burn_in<0 or args.max_train_batches<0:
        raise SystemExit('invalid temporal training/budget options')
    random.seed(args.seed)
    np.random.seed(args.seed & 0xFFFFFFFF)
    torch.manual_seed(args.seed)
    args.output.mkdir(parents=True, exist_ok=True)

    train_sources = sorted((args.musdb_root / "train").glob("*.stem.mp4"))
    test_sources = sorted((args.musdb_root / "test").glob("*.stem.mp4"))
    if args.max_train_tracks:
        train_sources = train_sources[:args.max_train_tracks]
    if args.max_test_tracks:
        test_sources = test_sources[:args.max_test_tracks]
    if args.init_model is not None and args.init_checkpoint is not None:
        raise SystemExit("choose only one warm-start format")
    if args.cache_only and not args.extra_cache_root:
        raise SystemExit("--cache-only requires explicit --extra-cache-root")
    if not args.cache_only and (not train_sources or not test_sources):
        raise SystemExit("MUSDB train/test stem containers were not found")
    if args.cache_only:
        train_sources, test_sources = [], []

    train_sources, validation_sources = split_train_validation(
        train_sources, args.seed
    )
    cache_root = args.cache_root or (args.output / "cache")
    train_files = prepare_split(train_sources, cache_root / "train")
    validation_files = prepare_split(
        validation_sources, cache_root / "train"
    )
    test_files = prepare_split(test_sources, cache_root / "test")
    extra_train_files: list[pathlib.Path] = []
    extra_validation_files: list[pathlib.Path] = []
    for extra_root in args.extra_cache_root:
        train_extra = sorted((extra_root / "train").glob("*.npz"))
        validation_extra = sorted((extra_root / "validation").glob("*.npz"))
        if not train_extra:
            raise RuntimeError(f"no training caches found under {extra_root}")
        extra_train_files.extend(train_extra)
        extra_validation_files.extend(validation_extra)
    if args.cache_only:
        identities = []
        for files in (extra_train_files, extra_validation_files):
            source_ids = set()
            for path in files:
                with np.load(path) as cache:
                    if "source_sha256" not in cache:
                        raise RuntimeError(f"cache-only training needs source provenance: {path}")
                    source_ids.add(str(cache["source_sha256"].item()))
            identities.append(source_ids)
        if identities[0] & identities[1]:
            raise RuntimeError("source leakage between training and validation caches")
    warm_model = None
    checkpoint_data = None
    if args.init_model is not None:
        warm_model, mean, inv_std = import_model(args.init_model)
    elif args.init_checkpoint is not None:
        checkpoint_data = torch.load(args.init_checkpoint, map_location="cpu", weights_only=False)
        mean, inv_std = checkpoint_data["mean"], checkpoint_data["inv_std"]
    else:
        mean, inv_std = compute_normalization(train_files + extra_train_files)
    if not train_files + extra_train_files or not validation_files + extra_validation_files:
        raise SystemExit("nonempty, source-disjoint train and validation caches are required")
    np.savez(args.output / "normalization.npz", mean=mean, inv_std=inv_std)
    if args.prepare_only:
        print(
            f"prepared train={len(train_files)} "
            f"validation={len(validation_files)} heldout={len(test_files)}"
        )
        return 0

    train_dataset = SequenceDataset(
        train_files + extra_train_files,
        mean,
        inv_std,
        routing_target=args.routing_target,
        sequence=args.sequence+args.burn_in,
    )
    validation_dataset = SequenceDataset(
        validation_files + extra_validation_files,
        mean,
        inv_std,
        routing_target=args.routing_target,
        sequence=args.sequence+args.burn_in,
    )
    test_dataset = SequenceDataset(
        test_files, mean, inv_std, routing_target=args.routing_target,
        sequence=args.sequence+args.burn_in,
    )
    generator = torch.Generator().manual_seed(args.seed)
    train_loader = DataLoader(
        train_dataset, batch_size=args.batch_size, shuffle=True,
        num_workers=0, generator=generator,
    )
    validation_loader = DataLoader(
        validation_dataset, batch_size=args.batch_size,
        shuffle=False, num_workers=0
    )
    test_loader = DataLoader(
        test_dataset, batch_size=args.batch_size, shuffle=False, num_workers=0
    )
    if args.cpu_threads < 0:
        raise ValueError('CPU thread count must be nonnegative')
    if args.cpu_threads:
        torch.set_num_threads(args.cpu_threads)
    device = torch.device(("mps" if torch.backends.mps.is_available() else "cpu")
                          if args.device == 'auto' else args.device)
    model = (warm_model or HssrNetwork()).to(device)
    model.runtime_aware=args.runtime_aware
    model.burn_in=args.burn_in
    model.control_delay=args.control_delay
    if checkpoint_data is not None:
        # This is an explicitly supplied, locally generated development
        # checkpoint.  It also contains NumPy normalization arrays, which are
        # intentionally rejected by PyTorch 2.6's weights-only loader.
        model.load_state_dict(checkpoint_data["state_dict"])
        print(f"warm-start {args.init_checkpoint}", flush=True)
    if args.init_model is not None:
        print(f"warm-start row-dequantized {args.init_model}; normalization preserved", flush=True)
    if args.routing_head_only:
        freeze_except_routing_head(model)
    optimizer = torch.optim.AdamW(
        (p for p in model.parameters() if p.requires_grad), lr=args.learning_rate,
        weight_decay=0 if args.routing_head_only else 1e-4
    )
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(
        optimizer, T_max=max(args.epochs, 1), eta_min=min(1.0e-4, args.learning_rate * 0.1)
    )
    best_loss = float("inf")
    best_state = None
    history = []
    non_improving_epochs = 0
    started = time.time()
    baseline_validation = evaluate(model, validation_loader, device, args.loss_profile)
    best_epoch = 0
    if args.init_model or args.init_checkpoint:
        best_loss = baseline_validation["total"]
        best_state = {
            name: value.detach().cpu().clone()
            for name, value in model.state_dict().items()
        }
    print(json.dumps({"baseline_validation": baseline_validation}), flush=True)

    print(
        f"train device={device} tracks={len(train_files)} "
        f"validation={len(validation_files)} heldout={len(test_files)} "
        f"extra_train={len(extra_train_files)} "
        f"extra_validation={len(extra_validation_files)} "
        f"chunks={len(train_dataset)}",
        flush=True,
    )
    for epoch in range(1, args.epochs + 1):
        model.train()
        train_total = 0.0
        batches = 0
        for features, targets in train_loader:
            if args.max_train_batches and batches>=args.max_train_batches: break
            optimizer.zero_grad(set_to_none=True)
            from hssr_runtime_training import supervised_prediction
            prediction,target=supervised_prediction(model,features.to(device),targets.to(device),
                model.burn_in,model.control_delay)
            values = loss_components(
                prediction, target, args.loss_profile, model.runtime_aware
            )
            values["total"].backward()
            nn.utils.clip_grad_norm_(model.parameters(), 2.0)
            optimizer.step()
            train_total += float(values["total"].detach().cpu())
            batches += 1
            if batches % 16 == 0:
                print(f'epoch={epoch} train_batches={batches} elapsed={time.time()-started:.1f}s', flush=True)
        scheduler.step()
        validation = evaluate(
            model, validation_loader, device, args.loss_profile
        )
        record = {
            "epoch": epoch,
            "train_total": train_total / max(batches, 1),
            **{f"validation_{name}": value
               for name, value in validation.items()},
        }
        history.append(record)
        print(json.dumps(record, sort_keys=True), flush=True)
        if validation["total"] < best_loss:
            best_loss = validation["total"]
            best_epoch = epoch
            best_state = {
                name: value.detach().cpu().clone()
                for name, value in model.state_dict().items()
            }
            non_improving_epochs = 0
            torch.save(
                {"state_dict": best_state, "mean": mean, "inv_std": inv_std,
                 "epoch": epoch, "validation": validation},
                args.output / "best_so_far.pt",
            )
        else:
            non_improving_epochs += 1
            if (
                args.early_stopping_patience > 0
                and non_improving_epochs >= args.early_stopping_patience
            ):
                print(
                    f"early-stop epoch={epoch} best={best_loss:.8f} "
                    f"patience={args.early_stopping_patience}",
                    flush=True,
                )
                break

    if best_state is None:
        raise RuntimeError("training produced no checkpoint")
    model.load_state_dict(best_state)
    heldout = evaluate(model, test_loader, device, args.loss_profile) if test_files else None
    model.cpu()
    checkpoint = args.output / "hssr_development.pt"
    torch.save(
        {"state_dict": model.state_dict(), "mean": mean, "inv_std": inv_std},
        checkpoint,
    )
    export = export_model(model, mean, inv_std, args.output / "hssr_model.bin")
    manifest = {
        "development_only": True,
        "runtime_aware": args.runtime_aware,
        "routing_head_only": args.routing_head_only,
        "sequence": args.sequence, "burn_in": args.burn_in,
        "control_delay": args.control_delay,
        "max_train_batches": args.max_train_batches,
        "device": str(device), "cpu_threads": torch.get_num_threads(),
        "training_license_warning": (
            "MUSDB-derived checkpoint; do not ship commercially without legal clearance"
        ),
        "train_tracks": [path.name for path in train_sources],
        "validation_tracks": [path.name for path in validation_sources],
        "heldout_tracks": [path.name for path in test_sources],
        "feature_count": FEATURES,
        "routing_target": args.routing_target,
        "cache_only": args.cache_only,
        "init_model": str(args.init_model.resolve()) if args.init_model else None,
        "normalization_policy": "preserve warm-start normalization" if args.init_model or args.init_checkpoint else "training-only fitted",
        "loss_profile": args.loss_profile,
        "cache_root": str(cache_root.resolve()),
        "init_checkpoint": (
            str(args.init_checkpoint.resolve())
            if args.init_checkpoint is not None else None
        ),
        "extra_cache_roots": [
            str(path.resolve()) for path in args.extra_cache_root
        ],
        "extra_train_cache_count": len(extra_train_files),
        "extra_validation_cache_count": len(extra_validation_files),
        "extra_source_sha256": {
            "train": sorted(identities[0]), "validation": sorted(identities[1])
        } if args.cache_only else None,
        "hidden_size": HIDDEN,
        "output_count": OUTPUTS,
        "control_rate_hz": SAMPLE_RATE / (HOP_SIZE * CONTROL_HOPS),
        "epochs": args.epochs,
        "epochs_completed": len(history),
        "early_stopping_patience": args.early_stopping_patience,
        "batch_size": args.batch_size,
        "learning_rate": args.learning_rate,
        "seed": args.seed,
        "elapsed_seconds": time.time() - started,
        "history": history,
        "best_validation_total": best_loss,
        "best_epoch": best_epoch,
        "baseline_validation": baseline_validation,
        "heldout": heldout,
        "export": export,
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(export, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
