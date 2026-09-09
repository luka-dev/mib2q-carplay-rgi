#!/usr/bin/env python3
"""Convert offline six-stem separation into HSSR semantic supervision.

This is a development-only distillation bridge.  Demucs supplies instrument
families; an optional higher-quality vocal stem supplies the lead-protection
mask.  The exported cache contains only HSSR features and targets, never audio.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
import soundfile as sf

from train_hssr import (
    BAND_EDGES_HZ,
    BANDS,
    CLASSES,
    CONTROL_HOPS,
    OBJECT_FOCUS_PRESENCE,
    SAMPLE_RATE,
    aggregate_bands,
    make_features,
    make_targets,
    stft,
)


STEMS = ("Bass", "Drums", "Guitar", "Other", "Piano", "Vocals")
DRUM_REAR_CURVE = np.asarray(
    [
        0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00,
        0.00, 0.00, 0.02, 0.05, 0.10, 0.18, 0.30, 0.45,
        0.62, 0.78, 0.90, 1.00, 1.00, 1.00, 1.00, 1.00,
        1.00, 0.95, 0.82, 0.65, 0.45, 0.25, 0.10, 0.00,
    ],
    dtype=np.float32,
)


def read_stereo(path: Path) -> np.ndarray:
    audio, rate = sf.read(path, dtype="float32", always_2d=True)
    if rate != SAMPLE_RATE or audio.shape[1] != 2:
        raise RuntimeError(f"expected 48 kHz stereo: {path}")
    return audio


def locate_stem(directory: Path, stem: str) -> Path:
    matches = list(directory.glob(f"*_({stem})_htdemucs_6s.wav"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one {stem} stem in {directory}")
    return matches[0]


def locate_demucs_stems(directory: Path) -> dict[str, Path] | None:
    result: dict[str, Path] = {}
    for stem in STEMS:
        matches = list(directory.glob(f"*_({stem})_htdemucs_6s.wav"))
        if len(matches) > 1:
            raise RuntimeError(f"expected at most one {stem} stem in {directory}")
        if matches:
            result[stem] = matches[0]
    if not result:
        return None
    if len(result) != len(STEMS):
        missing = sorted(set(STEMS) - set(result))
        raise RuntimeError(f"incomplete Demucs stems in {directory}: {missing}")
    return result


def locate_roformer_stem(directory: Path, stem: str) -> Path | None:
    matches = list(directory.glob(f"*_({stem})_model_bs_roformer*.wav"))
    if not matches:
        return None
    if len(matches) != 1:
        raise RuntimeError(f"expected at most one RoFormer {stem} stem in {directory}")
    return matches[0]


def split_sources(source_ids: list[str], validation_fraction: float) -> set[str]:
    ranked = sorted(
        set(source_ids),
        key=lambda value: hashlib.sha256(
            f"hssr-separator-validation:{value}".encode("utf-8")
        ).digest(),
    )
    count = max(1, round(len(ranked) * validation_fraction)) if len(ranked) > 1 else 0
    return set(ranked[:count])


def smoothstep(values: np.ndarray, low: float, high: float) -> np.ndarray:
    scaled = np.clip((values - low) / (high - low), 0.0, 1.0)
    return scaled * scaled * (3.0 - 2.0 * scaled)


def selection_v11_targets(
    vocal_spec: np.ndarray,
    instrumental_spec: np.ndarray,
) -> np.ndarray:
    """Distil a conservative direct instrumental-selection target.

    A 32-band controller cannot unmix two sources that fully overlap in the
    same time/frequency cell.  Accordingly the positive mask rises only when
    the RoFormer instrumental stem dominates a band.  Clean instrumental
    regions are labelled as routable, while ambiguous vocal overlap and the
    low-frequency body remain explicit vetoes.
    """
    eps = 1.0e-8
    vocal = aggregate_bands(vocal_spec)
    instrumental = aggregate_bands(instrumental_spec)
    vocal_energy = vocal["pl"] + vocal["pr"]
    instrumental_energy = instrumental["pl"] + instrumental["pr"]
    total = vocal_energy + instrumental_energy + eps
    vocal_share = np.clip(vocal_energy / total, 0.0, 1.0)
    instrumental_share = np.clip(instrumental_energy / total, 0.0, 1.0)

    amplitude_l = np.sqrt(instrumental["pl"] + eps)
    amplitude_r = np.sqrt(instrumental["pr"] + eps)
    pan = np.clip(
        (amplitude_l - amplitude_r) / (amplitude_l + amplitude_r + eps),
        -1.0,
        1.0,
    )
    cross_abs = np.sqrt(
        instrumental["cross_re"] ** 2 + instrumental["cross_im"] ** 2
    )
    coherence = np.clip(
        cross_abs
        / np.sqrt(instrumental["pl"] * instrumental["pr"] + eps),
        0.0,
        1.0,
    )
    diffuse = 1.0 - coherence
    transient = smoothstep(
        np.clip(instrumental["flux"], 0.0, 1.0), 0.025, 0.28
    )
    tonal = 1.0 - np.clip(instrumental["flatness"], 0.0, 1.0)
    spatial = smoothstep(np.abs(pan), 0.025, 0.45)

    centres = 0.5 * (BAND_EDGES_HZ[:-1] + BAND_EDGES_HZ[1:])
    bass_curve = 1.0 - smoothstep(centres[None, :], 170.0, 440.0)
    bass = instrumental_share * bass_curve
    non_low = instrumental_share * (1.0 - bass_curve)

    # Split the known non-vocal energy into the existing runtime taxonomy.
    # The shares sum to non_low, so the RoFormer vocal posterior remains an
    # exact class probability rather than being diluted by heuristic labels.
    drum_raw = 0.20 + 0.90 * transient
    foreground_raw = 0.35 + 0.65 * tonal * (1.0 - spatial)
    background_raw = 0.25 + 0.75 * np.clip(
        spatial + 0.50 * tonal, 0.0, 1.0
    )
    fx_raw = 0.15 + 0.85 * diffuse * (0.50 + 0.50 * transient)
    non_low_raw = drum_raw + foreground_raw + background_raw + fx_raw + eps
    drums = non_low * drum_raw / non_low_raw
    foreground = non_low * foreground_raw / non_low_raw
    background = non_low * background_raw / non_low_raw
    effects = non_low * fx_raw / non_low_raw
    classes = np.stack(
        (vocal_share, bass, drums, foreground, background, effects), axis=2
    )
    classes /= np.sum(classes, axis=2, keepdims=True) + eps

    # Dominance is the separation guard: 50/50 vocal/instrument overlap gets
    # no positive rear lesson; a clean instrumental cell receives an audible
    # target even if it is centred, with stronger targets for spatial ridges,
    # diffuse effects and transients.
    dominance = smoothstep(instrumental_share, 0.55, 0.96)
    object_evidence = np.maximum(
        spatial, np.maximum(0.65 * diffuse, 0.55 * transient)
    )
    rearability = np.clip(
        OBJECT_FOCUS_PRESENCE[None, :]
        * dominance
        * (0.30 + 0.62 * object_evidence)
        * (1.0 - 0.82 * bass_curve),
        0.0,
        0.92,
    )

    per_band = np.concatenate(
        (classes, rearability[:, :, None], pan[:, :, None]), axis=2
    ).reshape(len(vocal_spec), BANDS * 8)
    total_global = np.sum(total, axis=1) + eps
    instrumental_total = np.sum(instrumental_energy, axis=1) + eps
    lead_present = np.sum(vocal_energy, axis=1) / total_global
    bass_present = np.sum(instrumental_energy * bass_curve, axis=1) / total_global
    mean_coherence = np.sum(
        coherence * instrumental_energy, axis=1
    ) / instrumental_total
    class_entropy = -np.sum(classes * np.log(classes + eps), axis=2) / math.log(
        CLASSES
    )
    scene_confidence = 1.0 - np.sum(class_entropy * total, axis=1) / total_global
    global_rear = np.sum(rearability * total, axis=1) / total_global
    scene_stability = np.ones(len(vocal_spec), dtype=np.float32)
    if len(vocal_spec) > 1:
        scene_stability[1:] = np.clip(
            1.0 - np.mean(np.abs(np.diff(rearability, axis=0)), axis=1),
            0.0,
            1.0,
        )
    fx_present = np.sum(effects * total, axis=1) / total_global
    routing_safety = np.sum(
        dominance * (1.0 - bass_curve) * total, axis=1
    ) / total_global
    globals_ = np.stack(
        (
            lead_present,
            bass_present,
            mean_coherence,
            scene_confidence,
            global_rear,
            scene_stability,
            fx_present,
            routing_safety,
        ),
        axis=1,
    )
    result = np.concatenate((per_band, globals_), axis=1)
    return np.nan_to_num(result, nan=0.0, posinf=1.0, neginf=0.0).astype(
        np.float32
    )


def teacher_rear_targets(
    targets: np.ndarray,
    specs: dict[str, np.ndarray],
) -> np.ndarray:
    result = targets.copy()
    aggregates = {name: aggregate_bands(spec) for name, spec in specs.items()}
    energy = {
        name: value["pl"] + value["pr"] for name, value in aggregates.items()
    }
    total = sum(
        (energy[name] for name in STEMS),
        np.zeros_like(next(iter(energy.values()))),
    ) + 1.0e-8
    vocal = energy["Vocals"] / total
    roformer_available = "RoVocals" in energy and "RoInstrumental" in energy
    if roformer_available:
        roformer_total = energy["RoVocals"] + energy["RoInstrumental"] + 1.0e-8
        vocal = np.clip(energy["RoVocals"] / roformer_total, 0.0, 1.0)

        # Replace the coarse Demucs vocal probability while retaining its
        # relative non-vocal taxonomy.  This is the key teacher ensemble:
        # RoFormer protects voice, Demucs names the remaining instruments.
        band_target = result[:, :BANDS * 8].reshape(-1, BANDS, 8)
        classes = band_target[:, :, :CLASSES]
        non_vocal = classes[:, :, 1:]
        non_vocal /= np.sum(non_vocal, axis=2, keepdims=True) + 1.0e-8
        classes[:, :, 0] = vocal
        classes[:, :, 1:] = (1.0 - vocal[:, :, None]) * non_vocal
    bass = energy["Bass"] / total
    drums = energy["Drums"] / total
    safe_energy = energy["Guitar"] + energy["Other"] + energy["Piano"]
    safe = safe_energy / total

    protection = np.power(1.0 - vocal, 2.7) * np.power(1.0 - bass, 2.4)
    semantic_object = 0.78 * np.power(np.clip(safe, 0.0, 1.0), 0.62)
    if roformer_available:
        # This teacher target is allowed to use the full non-vocal mid/high
        # residue.  Bass/kick are still rejected below by Demucs bass and the
        # presence curve; upper percussion remains a bounded candidate.
        instrumental_object = 0.52 * np.power(1.0 - vocal, 0.70)
        semantic_object = np.maximum(semantic_object, instrumental_object)
    upper_percussion = (
        0.46
        * np.power(np.clip(drums, 0.0, 1.0), 0.72)
        * DRUM_REAR_CURVE[None, :]
    )
    # When the teacher sees no guitar/piano/other, it is still useful to teach
    # one bounded object: a non-bass upper drum layer.  This is the learned
    # counterpart of the universal DSP coverage fallback.
    coverage = (
        0.24
        * np.power(np.clip(drums + 0.35 * safe, 0.0, 1.0), 0.75)
        * DRUM_REAR_CURVE[None, :]
    )
    candidate = np.clip(
        np.maximum(semantic_object, np.maximum(upper_percussion, coverage))
        * OBJECT_FOCUS_PRESENCE[None, :]
        * protection,
        0.0,
        0.88,
    )

    band_target = result[:, :BANDS * 8].reshape(-1, BANDS, 8)
    old_rear = band_target[:, :, 6].copy()
    new_rear = np.maximum(old_rear, candidate)

    if roformer_available:
        candidate_pl = aggregates["RoInstrumental"]["pl"]
        candidate_pr = aggregates["RoInstrumental"]["pr"]
    else:
        candidate_pl = (
            aggregates["Guitar"]["pl"] + aggregates["Other"]["pl"]
            + aggregates["Piano"]["pl"]
            + DRUM_REAR_CURVE[None, :] * aggregates["Drums"]["pl"]
        )
        candidate_pr = (
            aggregates["Guitar"]["pr"] + aggregates["Other"]["pr"]
            + aggregates["Piano"]["pr"]
            + DRUM_REAR_CURVE[None, :] * aggregates["Drums"]["pr"]
        )
    measured_pan = (
        np.sqrt(candidate_pl + 1.0e-8) - np.sqrt(candidate_pr + 1.0e-8)
    ) / (
        np.sqrt(candidate_pl + 1.0e-8) + np.sqrt(candidate_pr + 1.0e-8)
    )
    teacher_weight = np.clip(
        (new_rear - old_rear) / (new_rear + 1.0e-8), 0.0, 1.0
    )
    band_target[:, :, 6] = new_rear
    band_target[:, :, 7] = np.clip(
        (1.0 - teacher_weight) * band_target[:, :, 7]
        + teacher_weight * measured_pan,
        -1.0,
        1.0,
    )

    global_rear = np.sum(new_rear * total, axis=1) / (
        np.sum(total, axis=1) + 1.0e-8
    )
    result[:, BANDS * 8 + 4] = np.maximum(
        result[:, BANDS * 8 + 4], global_rear
    )
    if len(new_rear) > 1:
        result[1:, BANDS * 8 + 5] = np.minimum(
            result[1:, BANDS * 8 + 5],
            np.clip(
                1.0 - np.mean(np.abs(np.diff(new_rear, axis=0)), axis=1),
                0.0,
                1.0,
            ),
        )
    return np.nan_to_num(result, nan=0.0, posinf=1.0, neginf=0.0)


def vocal_protection_targets(vocal_spec: np.ndarray) -> np.ndarray:
    """Build counterfactual targets that must never route an isolated voice.

    Mixture-only supervision can teach a useful instrumental route while still
    leaving the small controller uncertain when a vocal occupies the same
    time/frequency region.  RoFormer vocal stems provide clean negative
    examples: active bands are labelled lead and every rearability output is
    explicitly zero.  This changes training only; target inference remains the
    same 450k-parameter controller.
    """
    aggregates = aggregate_bands(vocal_spec)
    energy = aggregates["pl"] + aggregates["pr"] + 1.0e-8
    relative_db = 10.0 * np.log10(
        energy / (np.max(energy, axis=1, keepdims=True) + 1.0e-8)
    )
    activity = np.clip((relative_db + 9.0) / 7.0, 0.0, 1.0).astype(np.float32)

    result = np.zeros((len(vocal_spec), BANDS * 8 + 8), dtype=np.float32)
    bands = result[:, :BANDS * 8].reshape(-1, BANDS, 8)
    bands[:, :, 0] = activity       # lead/vocal class
    bands[:, :, 6] = 0.0            # never rear-route isolated voice
    bands[:, :, 7] = 0.0

    weighted_activity = np.sum(activity * energy, axis=1) / (
        np.sum(energy, axis=1) + 1.0e-8
    )
    globals_ = result[:, BANDS * 8:]
    globals_[:, 0] = weighted_activity  # lead present
    globals_[:, 3] = weighted_activity  # scene confidence
    globals_[:, 4] = 0.0                # rear activity target
    globals_[:, 5] = 1.0                # stable protection decision
    globals_[:, 7] = 0.0                # routing safety veto
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--teacher-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-native-teacher", action="store_true",
                        help="reject missing/stale teachers and bind cache provenance to native-rate float pairs")
    parser.add_argument("--validation-fraction", type=float, default=0.20)
    parser.add_argument("--input-gains-db", type=float, nargs="+", default=[0.0],
                        help="recompute source features at these nonpositive levels; targets remain semantic, not Sound Check gain")
    parser.add_argument(
        "--target-policy",
        choices=("semantic-v9", "selection-v11"),
        default="semantic-v9",
        help=(
            "semantic-v9 requires Demucs taxonomy; selection-v11 can use a "
            "RoFormer vocal/instrumental pair directly"
        ),
    )
    parser.add_argument(
        "--include-vocal-negatives",
        action="store_true",
        help="also emit isolated RoFormer-vocal caches with zero rear targets",
    )
    parser.add_argument(
        "--vocal-negative-fraction",
        type=float,
        default=1.0,
        help=(
            "fraction of each isolated-vocal excerpt to retain; the most "
            "energetic contiguous region is selected (default: 1.0)"
        ),
    )
    parser.add_argument(
        "--include-instrumental-positives",
        action="store_true",
        help="emit isolated RoFormer-instrumental caches with positive routing",
    )
    parser.add_argument(
        "--counterfactual-mixes",
        action="store_true",
        help=(
            "emit vocal-heavy and instrumental-heavy mixtures to sharpen the "
            "selection boundary"
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 0.0 <= args.validation_fraction < 0.5:
        raise RuntimeError("--validation-fraction must be in [0, 0.5)")
    if not 0.0 < args.vocal_negative_fraction <= 1.0:
        raise RuntimeError("--vocal-negative-fraction must be in (0, 1]")
    if (len(set(args.input_gains_db)) != len(args.input_gains_db)
            or any(not math.isfinite(g) or g < -24 or g > 0 for g in args.input_gains_db)):
        raise RuntimeError("input gains must be unique finite values in [-24, 0] dB")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if args.require_native_teacher and args.output.exists():
        raise RuntimeError("native teacher caches require a fresh output directory")
    source_ids = [str(item["source_sha256"]) for item in manifest["items"]]
    validation = split_sources(source_ids, args.validation_fraction)
    roformer_sources = {
        str(item["source_sha256"])
        for item in manifest["items"]
        if list(
            (args.teacher_root / str(item["id"])).glob(
                "*_(Vocals)_model_bs_roformer*.wav"
            )
        )
    }
    # Model selection must see at least one hard RoFormer-supervised song;
    # otherwise a lower generic validation loss can hide regression on the
    # exact non-vocal routing lesson being distilled.
    if roformer_sources and not (validation & roformer_sources):
        replacement = min(
            roformer_sources,
            key=lambda value: hashlib.sha256(
                f"hssr-roformer-validation:{value}".encode("utf-8")
            ).digest(),
        )
        if validation:
            validation.remove(sorted(validation)[-1])
        validation.add(replacement)
    rows: list[dict[str, object]] = []

    for item in manifest["items"]:
        item_id = str(item["id"])
        teacher_dir = args.teacher_root / item_id
        if not teacher_dir.is_dir():
            if args.require_native_teacher:
                raise RuntimeError(f"missing native teacher: {teacher_dir}")
            continue
        provenance_hash = "legacy-unverified"
        if args.require_native_teacher:
            from prepare_hssr_native_teacher import POLICY, digest, valid_pair
            provenance_path = teacher_dir / "teacher-provenance.json"
            identity = json.loads(provenance_path.read_text())["identity"]
            if (identity["policy"] != POLICY or
                    identity["source_sha256"] != digest(Path(item["stereo"])) or
                    not valid_pair(teacher_dir, identity)):
                raise RuntimeError(f"invalid native teacher provenance: {teacher_dir}")
            provenance_hash = digest(provenance_path)
        mixture = read_stereo(Path(str(item["stereo"])))
        demucs_paths = locate_demucs_stems(teacher_dir)
        roformer_vocals = locate_roformer_stem(teacher_dir, "Vocals")
        roformer_instrumental = locate_roformer_stem(teacher_dir, "Instrumental")
        if (roformer_vocals is None) != (roformer_instrumental is None):
            raise RuntimeError(f"incomplete RoFormer pair in {teacher_dir}")
        if args.target_policy == "semantic-v9" and demucs_paths is None:
            raise RuntimeError(f"semantic-v9 requires Demucs stems: {teacher_dir}")
        if args.target_policy == "selection-v11" and roformer_vocals is None:
            raise RuntimeError(f"selection-v11 requires RoFormer stems: {teacher_dir}")

        stems = (
            {name: read_stereo(path) for name, path in demucs_paths.items()}
            if demucs_paths is not None else {}
        )
        if roformer_vocals is not None and roformer_instrumental is not None:
            stems["RoVocals"] = read_stereo(roformer_vocals)
            stems["RoInstrumental"] = read_stereo(roformer_instrumental)
        frames = min([len(mixture), *(len(audio) for audio in stems.values())])
        if args.require_native_teacher and any(len(a) != len(mixture) for a in stems.values()):
            raise RuntimeError(f"native teacher alignment mismatch: {teacher_dir}")
        mixture = mixture[:frames]
        stems = {name: audio[:frames] for name, audio in stems.items()}

        mixture_spec = stft(mixture)
        stem_specs = {name: stft(audio) for name, audio in stems.items()}
        spec_frames = min(
            [len(mixture_spec), *(len(spec) for spec in stem_specs.values())]
        )
        mixture_spec = mixture_spec[:spec_frames]
        stem_specs = {name: spec[:spec_frames] for name, spec in stem_specs.items()}
        select = np.arange(CONTROL_HOPS - 1, spec_frames, CONTROL_HOPS)
        source_id = str(item["source_sha256"])
        split = "validation" if source_id in validation else "train"

        def emit_cache(
            cache_id: str,
            title: str,
            audio: np.ndarray,
            spec: np.ndarray,
            targets: np.ndarray,
            counterfactual: str | None = None,
        ) -> int:
            selected_targets = targets[select].astype(np.float32)
            begin, retained = 0, len(selected_targets)
            if counterfactual == "isolated-vocal-zero-rear":
                retained = min(retained, max(64, int(round(retained * args.vocal_negative_fraction))))
                if retained < len(selected_targets):
                    energy = np.convolve(selected_targets[:, BANDS * 8],
                                         np.ones(retained, dtype=np.float32), mode="valid")
                    begin = int(np.argmax(energy))
            selected_targets = selected_targets[begin:begin + retained]
            for gain_db in args.input_gains_db:
                gain = np.float32(10.0 ** (gain_db / 20.0))
                # Recompute ALL features from scaled waveform/spectrum. Merely
                # shifting log-energy misses epsilon floors/coherence/crest.
                features = make_features(audio * gain, spec * gain)[select]
                features = features[begin:begin + retained].astype(np.float32)
                level_id = cache_id if gain_db == 0 else f"{cache_id}-level{gain_db:g}db"
                output = args.output / split / f"{level_id}.npz"
                output.parent.mkdir(parents=True, exist_ok=True)
                np.savez_compressed(output, features=features, targets=selected_targets,
                    routing_locked=np.asarray(args.target_policy == "selection-v11", dtype=np.bool_),
                    teacher_provenance_sha256=np.asarray(provenance_hash),
                    source_sha256=np.asarray(source_id), input_gain_db=np.asarray(gain_db))
                row = {"id": level_id, "title": title, "source_sha256": source_id,
                       "split": split, "control_frames": int(len(features)),
                       "roformer_vocal_teacher": roformer_vocals is not None,
                       "demucs_teacher": demucs_paths is not None,
                       "target_policy": args.target_policy, "input_gain_db": gain_db,
                       "teacher_provenance_sha256": provenance_hash,
                       "cache": str(output.resolve())}
                if counterfactual is not None:
                    row["counterfactual"] = counterfactual
                rows.append(row)
            return retained

        if args.target_policy == "selection-v11":
            targets = selection_v11_targets(
                stem_specs["RoVocals"], stem_specs["RoInstrumental"]
            )
        else:
            combined_other = (
                stem_specs["Guitar"]
                + stem_specs["Other"]
                + stem_specs["Piano"]
            )
            targets = make_targets(
                np.stack(
                    (
                        stem_specs["Drums"], stem_specs["Bass"],
                        combined_other, stem_specs["Vocals"],
                    ),
                    axis=0,
                )
            )
            targets = teacher_rear_targets(targets, stem_specs)
        control_frames = emit_cache(
            item_id, str(item["title"]), mixture, mixture_spec, targets
        )

        if args.include_vocal_negatives and "RoVocals" in stems:
            vocal_audio = stems["RoVocals"]
            vocal_spec = stem_specs["RoVocals"]
            vocal_targets = vocal_protection_targets(vocal_spec)
            emit_cache(f"{item_id}-vocal-negative", f"{item['title']} [isolated vocal protection]",
                       vocal_audio, vocal_spec, vocal_targets, "isolated-vocal-zero-rear")
        if args.include_instrumental_positives and "RoInstrumental" in stems:
            instrumental_spec = stem_specs["RoInstrumental"]
            instrumental_targets = selection_v11_targets(
                np.zeros_like(instrumental_spec), instrumental_spec
            )
            emit_cache(
                f"{item_id}-instrumental-positive",
                f"{item['title']} [isolated instrumental selection]",
                stems["RoInstrumental"],
                instrumental_spec,
                instrumental_targets,
                "isolated-instrumental-positive-rear",
            )

        if args.counterfactual_mixes:
            if args.target_policy != "selection-v11":
                raise RuntimeError(
                    "--counterfactual-mixes requires --target-policy selection-v11"
                )
            for suffix, vocal_gain, instrumental_gain in (
                ("vocal-heavy", 1.0, 0.45),
                ("instrumental-heavy", 0.45, 1.0),
            ):
                mixed_audio = (
                    vocal_gain * stems["RoVocals"]
                    + instrumental_gain * stems["RoInstrumental"]
                )
                vocal_spec = vocal_gain * stem_specs["RoVocals"]
                instrumental_spec = (
                    instrumental_gain * stem_specs["RoInstrumental"]
                )
                mixed_spec = vocal_spec + instrumental_spec
                mixed_targets = selection_v11_targets(
                    vocal_spec, instrumental_spec
                )
                emit_cache(
                    f"{item_id}-{suffix}",
                    f"{item['title']} [{suffix} counterfactual]",
                    mixed_audio,
                    mixed_spec,
                    mixed_targets,
                    f"roformer-{suffix}-selection-boundary",
                )
        print(f"{split:10s} {item_id}: {control_frames} control frames")

    result = {
        "schema": 2,
        "kind": "hssr-separator-selection-supervision",
        "development_only": True,
        "private_audio": True,
        "teacher": "BS-RoFormer vocal/instrumental + optional htdemucs_6s",
        "target_policy": args.target_policy,
        "teacher_role": (
            "RoFormer direct instrumental dominance and vocal veto; Demucs "
            "taxonomy is retained only by the semantic-v9 compatibility policy"
        ),
        "isolated_vocal_negatives": args.include_vocal_negatives,
        "vocal_negative_fraction": args.vocal_negative_fraction,
        "isolated_instrumental_positives": args.include_instrumental_positives,
        "counterfactual_mixes": args.counterfactual_mixes,
        "source_manifest": str(args.manifest.resolve()),
        "source_manifest_sha256": hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
        "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "native_teacher_required": args.require_native_teacher,
        "items": rows,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "manifest.json").write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(
        f"prepared train={sum(row['split'] == 'train' for row in rows)} "
        f"validation={sum(row['split'] == 'validation' for row in rows)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}")
        raise SystemExit(1)
