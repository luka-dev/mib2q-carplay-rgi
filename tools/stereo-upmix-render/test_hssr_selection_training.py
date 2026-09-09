#!/usr/bin/env python3
"""Focused regression tests for HSSR selection-target distillation."""

from __future__ import annotations

import unittest
import tempfile
import sys
import json
from unittest.mock import patch
from pathlib import Path

import numpy as np
import torch
import soundfile as sf

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from prepare_hssr_separator_supervision import selection_v11_targets
import prepare_hssr_separator_supervision as supervision
from train_hssr import (
    BANDS,
    FEATURES,
    FFT_SIZE,
    OUTPUTS,
    SequenceDataset,
    loss_components,
    HssrNetwork,
    export_model,
    import_model,
)


def one_bin_spec(bin_index: int, amplitude: float, frames: int = 8) -> np.ndarray:
    result = np.zeros((frames, 2, FFT_SIZE // 2 + 1), dtype=np.complex64)
    result[:, 0, bin_index] = amplitude
    result[:, 1, bin_index] = amplitude * np.exp(0.35j)
    return result


class SelectionTargetsTest(unittest.TestCase):
    def test_vocal_only_is_never_rear_routable(self) -> None:
        vocal = one_bin_spec(22, 1.0)
        targets = selection_v11_targets(vocal, np.zeros_like(vocal))
        bands = targets[:, :BANDS * 8].reshape(-1, BANDS, 8)
        self.assertEqual(targets.shape, (8, OUTPUTS))
        self.assertTrue(np.isfinite(targets).all())
        self.assertEqual(float(np.max(bands[:, :, 6])), 0.0)

    def test_clean_instrumental_midband_is_positive(self) -> None:
        instrumental = one_bin_spec(22, 1.0)
        targets = selection_v11_targets(
            np.zeros_like(instrumental), instrumental
        )
        bands = targets[:, :BANDS * 8].reshape(-1, BANDS, 8)
        self.assertGreater(float(np.max(bands[:, :, 6])), 0.25)

    def test_ambiguous_equal_energy_overlap_is_rejected(self) -> None:
        vocal = one_bin_spec(22, 1.0)
        instrumental = one_bin_spec(22, 1.0)
        targets = selection_v11_targets(vocal, instrumental)
        bands = targets[:, :BANDS * 8].reshape(-1, BANDS, 8)
        self.assertEqual(float(np.max(bands[:, :, 6])), 0.0)

    def test_sub_bass_is_rejected_even_when_instrumental(self) -> None:
        instrumental = one_bin_spec(1, 1.0)
        targets = selection_v11_targets(
            np.zeros_like(instrumental), instrumental
        )
        bands = targets[:, :BANDS * 8].reshape(-1, BANDS, 8)
        self.assertEqual(float(np.max(bands[:, :, 6])), 0.0)

    def test_active_class_probabilities_sum_to_one(self) -> None:
        vocal = one_bin_spec(22, 0.4)
        instrumental = one_bin_spec(22, 1.0)
        targets = selection_v11_targets(vocal, instrumental)
        bands = targets[:, :BANDS * 8].reshape(-1, BANDS, 8)
        active = np.sum(bands[:, :, :6], axis=2) > 0.0
        sums = np.sum(bands[:, :, :6], axis=2)[active]
        np.testing.assert_allclose(sums, 1.0, atol=2.0e-6)


class SelectionLossTest(unittest.TestCase):
    def test_cabin_unlabelled_bands_are_not_negative_vocal_labels(self):
        prediction = torch.zeros((1, 2, OUTPUTS), dtype=torch.float32)
        target = torch.zeros_like(prediction)
        self.assertEqual(float(loss_components(prediction, target, "cabin-v12")["class"]), 0)
        target[..., :BANDS * 8].reshape(1, 2, BANDS, 8)[..., 0] = .1
        prediction[..., :BANDS * 8].reshape(1, 2, BANDS, 8)[..., 0] = 4
        correct = loss_components(prediction, target, "cabin-v12")["class"]
        prediction[..., :BANDS * 8].reshape(1, 2, BANDS, 8)[..., 0] = -4
        wrong = loss_components(prediction, target, "cabin-v12")["class"]
        self.assertGreater(float(wrong), 5 * float(correct))

    def test_cabin_profile_penalizes_missed_vocal_class(self):
        prediction = torch.zeros((1, 2, OUTPUTS), dtype=torch.float32)
        target = torch.zeros_like(prediction)
        target[..., :BANDS * 8].reshape(1, 2, BANDS, 8)[..., 0] = 1.0
        prediction[..., :BANDS * 8].reshape(1, 2, BANDS, 8)[..., 0] = -4.0
        base = loss_components(prediction, target, "selection-v11")
        cabin = loss_components(prediction, target, "cabin-v12")
        self.assertGreater(float(cabin["class"]), 3 * float(base["class"]))
        torch.testing.assert_close(cabin["rear"], base["rear"])

    def test_selection_profile_penalizes_vocal_rear_false_positive(self) -> None:
        prediction = torch.zeros((1, 2, OUTPUTS), dtype=torch.float32)
        target = torch.zeros_like(prediction)
        target_band = target[..., :BANDS * 8].reshape(1, 2, BANDS, 8)
        target_band[..., 0] = 1.0
        prediction_band = prediction[..., :BANDS * 8].reshape(
            1, 2, BANDS, 8
        )
        prediction_band[..., 6] = 4.0
        balanced = loss_components(prediction, target, "balanced")["rear"]
        selective = loss_components(
            prediction, target, "selection-v11"
        )["rear"]
        self.assertGreater(float(selective), 5.5 * float(balanced))


class RoutingLockTest(unittest.TestCase):
    def test_locked_teacher_targets_skip_generic_routing_rewrite(self) -> None:
        features = np.zeros((64, FEATURES), dtype=np.float32)
        targets = np.zeros((64, OUTPUTS), dtype=np.float32)
        band = 16
        features[:, band * 9 + 2] = 1.0
        features[:, band * 9 + 3] = 0.8
        features[:, band * 9 + 6] = 0.3
        features[:, BANDS * 9 + 64 + 3] = 0.2
        target_bands = targets[:, :BANDS * 8].reshape(-1, BANDS, 8)
        target_bands[:, band, 2] = 1.0

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            locked = root / "locked.npz"
            unlocked = root / "unlocked.npz"
            np.savez(
                locked,
                features=features,
                targets=targets,
                routing_locked=np.asarray(True, dtype=np.bool_),
            )
            np.savez(unlocked, features=features, targets=targets)
            mean = np.zeros(FEATURES, dtype=np.float32)
            inv_std = np.ones(FEATURES, dtype=np.float32)
            locked_dataset = SequenceDataset(
                [locked], mean, inv_std, routing_target="edm-v2"
            )
            unlocked_dataset = SequenceDataset(
                [unlocked], mean, inv_std, routing_target="edm-v2"
            )
            locked_rear = locked_dataset.tracks[0][1][
                :, :BANDS * 8
            ].reshape(-1, BANDS, 8)[:, :, 6]
            unlocked_rear = unlocked_dataset.tracks[0][1][
                :, :BANDS * 8
            ].reshape(-1, BANDS, 8)[:, :, 6]
            self.assertEqual(float(np.max(locked_rear)), 0.0)
            self.assertGreater(float(np.max(unlocked_rear)), 0.0)


class BinaryWarmStartTest(unittest.TestCase):
    def test_quantized_model_recovers_normalization_and_biases(self):
        torch.manual_seed(17)
        model = HssrNetwork()
        mean = np.arange(FEATURES, dtype=np.float32) * 0.01
        inv = np.linspace(.3, 100, FEATURES, dtype=np.float32)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "model.bin"
            export_model(model, mean, inv, path)
            recovered, actual_mean, actual_inv = import_model(path)
            np.testing.assert_array_equal(actual_mean, mean)
            np.testing.assert_array_equal(actual_inv, inv)
            torch.testing.assert_close(recovered.head.bias, model.head.bias)
            self.assertLess(float(torch.max(torch.abs(recovered.head.weight - model.head.weight)).detach()), .001)
            path.write_bytes(path.read_bytes()[:-1] + b"x")
            with self.assertRaises(ValueError):
                import_model(path)


class LevelAugmentationTest(unittest.TestCase):
    def test_recomputes_features_preserves_targets_and_source_split(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            t = np.arange(96000, dtype=np.float32) / 48000
            vocal = np.stack([.2 * np.sin(2 * np.pi * 440 * t)] * 2, axis=1)
            inst = np.stack([.1 * np.sin(2 * np.pi * 1400 * t),
                             .17 * np.sin(2 * np.pi * 1500 * t)], axis=1)
            items = []
            for index in range(2):
                source = root / f"source{index}.wav"
                sf.write(source, vocal + inst, 48000, subtype="FLOAT")
                directory = root / "teacher" / str(index)
                directory.mkdir(parents=True)
                sf.write(directory / "stereo_(Vocals)_model_bs_roformer_test.wav", vocal, 48000, subtype="FLOAT")
                sf.write(directory / "stereo_(Instrumental)_model_bs_roformer_test.wav", inst, 48000, subtype="FLOAT")
                items.append({"id": str(index), "title": "synthetic", "stereo": str(source),
                              "source_sha256": f"synthetic-test-source-{index}"})
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({"items": items}))
            with patch.object(sys, "argv", ["prepare", "--manifest", str(manifest),
                    "--teacher-root", str(root / "teacher"), "--output", str(root / "cache"),
                    "--target-policy", "selection-v11", "--input-gains-db", "0", "-6", "-12"]):
                supervision.main()
            ids = []
            for split in ("train", "validation"):
                files = list((root / "cache" / split).glob("*.npz"))
                self.assertEqual(len(files), 3)
                levels = {}
                for path in files:
                    with np.load(path) as cache:
                        levels[float(cache["input_gain_db"])] = dict(cache)
                np.testing.assert_array_equal(levels[0]["targets"], levels[-12]["targets"])
                self.assertFalse(np.array_equal(levels[0]["features"], levels[-12]["features"]))
                self.assertTrue(np.isfinite(levels[-12]["features"]).all())
                ids.append(str(levels[0]["source_sha256"].item()))
            self.assertNotEqual(*ids)


if __name__ == "__main__":
    unittest.main()
