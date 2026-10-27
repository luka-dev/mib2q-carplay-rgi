#!/usr/bin/env python3
"""Regression tests for the host-side SoundAmp virtual cabin."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import soundamp_virtual_cabin as cabin  # noqa: E402


class VirtualCabinTests(unittest.TestCase):
    def setUp(self) -> None:
        self.config = cabin.load_config(cabin.DEFAULT_CONFIG)

    def test_config_is_explicitly_3d_off_and_has_documented_topology(self) -> None:
        self.assertIs(self.config["operating_state"]["symphoria_3d_enabled"], False)
        self.assertEqual(len(self.config["speakers"]), 19)
        physical = self.config["physical_output_graph"]
        self.assertEqual(len(physical["output_circuits"]), 16)
        self.assertEqual(
            physical["status"],
            "speaker_circuits_proven_logical_bus_binding_inferred",
        )
        pins = [
            pin
            for circuit in physical["output_circuits"]
            for pin in circuit["pins"]
        ]
        self.assertEqual(len(pins), 32)
        self.assertEqual(len(set(pins)), 32)
        inferred_channels = [
            circuit["inferred_amplifier_channel"]
            for circuit in physical["output_circuits"]
        ]
        self.assertEqual(sorted(inferred_channels), list(range(1, 17)))

    def test_matrix_orientation_matches_recovered_v850_writer(self) -> None:
        gains = cabin.linear_matrix(self.config)
        source = np.zeros((1, 6), dtype=np.float64)
        source[0, 0] = 1.0
        buses = cabin.apply_matrix(source, gains)
        self.assertAlmostEqual(buses[0, 0], 10.0 ** (-2.0 / 20.0), places=12)
        self.assertAlmostEqual(buses[0, 2], 10.0 ** (-8.0 / 20.0), places=12)
        self.assertAlmostEqual(buses[0, 3], 10.0 ** (-8.0 / 20.0), places=12)
        self.assertAlmostEqual(buses[0, 1], 10.0 ** (-99.0 / 20.0), places=12)

    def test_all_matrix_columns_match_corrected_electronic_audit(self) -> None:
        from amp_dsp_chain import AmpChain
        actual=cabin.apply_matrix(np.eye(8),cabin.linear_matrix(self.config))
        np.testing.assert_allclose(actual,AmpChain().M8.T,rtol=0,atol=1e-7)
        # Independent asymmetric impulse: LFE bus receives Ls at-14dB,
        # while Ls bus receives LFE only at the-99dB floor.
        self.assertAlmostEqual(actual[4,3],10**(-14/20),places=12)
        self.assertAlmostEqual(actual[3,4],10**(-99/20),places=12)

    def test_stale_input_row_config_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            self.config['matrix']['storage_order']='rows_are_inputs_columns_are_outputs'
            path=Path(tmp)/'stale.json';path.write_text(json.dumps(self.config))
            with self.assertRaises(cabin.CabinError):cabin.load_config(path)
            with self.assertRaises(cabin.CabinError):cabin.linear_matrix(self.config)

    def test_matrix_renderer_writes_eight_bus_wav_and_report(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source_path = root / "source.wav"
            output_path = root / "buses.wav"
            samples = np.zeros((32, 6), dtype=np.float64)
            samples[0, 4] = 0.25
            with cabin.Pcm16WavWriter(source_path, 6, samples.shape[0], 0x60F) as writer:
                writer.write(samples)

            report = cabin.render_matrix(
                source_path, output_path, cabin.DEFAULT_CONFIG, None, block_frames=7
            )
            with cabin.Pcm16WavReader(output_path) as reader:
                rendered = np.concatenate(list(reader.blocks()), axis=0)
                self.assertEqual(reader.info.channels, 8)
                self.assertEqual(reader.info.frames, 32)
            self.assertAlmostEqual(
                rendered[0, 4], 0.25 * 10.0 ** (6.0 / 20.0), places=4
            )
            self.assertEqual(report["output_channel_order"][4], "Bus_Ls")
            self.assertEqual(report['model']['matrix_semantics_version'],2)
            self.assertTrue(cabin.default_report_path(output_path).exists())

    def test_documentary_circuit_renderer_preserves_bus_power(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source_path = root / "source.wav"
            output_path = root / "circuits.wav"
            samples = np.zeros((16, 6), dtype=np.float64)
            samples[0, 4] = 0.25
            with cabin.Pcm16WavWriter(source_path, 6, samples.shape[0], 0x60F) as writer:
                writer.write(samples)

            report = cabin.render_documentary_circuits(
                source_path,
                output_path,
                cabin.DEFAULT_CONFIG,
                None,
                block_frames=5,
            )
            with cabin.Pcm16WavReader(output_path) as reader:
                rendered = np.concatenate(list(reader.blocks()), axis=0)
                self.assertEqual(reader.info.channels, 16)
            route, names = cabin.documentary_fanout_matrix(self.config)
            expected_buses = cabin.apply_matrix(samples, cabin.linear_matrix(self.config))
            expected = cabin.apply_documentary_fanout(expected_buses, route)
            self.assertTrue(np.allclose(rendered, expected, atol=1.0 / 32768.0))
            self.assertEqual(report["output_circuit_order"], list(names))

            bus_power = np.sum(expected_buses * expected_buses, axis=0)
            circuit_power = np.sum(expected * expected, axis=0)
            for bus_index in range(8):
                destinations = np.flatnonzero(route[bus_index])
                if destinations.size:
                    self.assertAlmostEqual(
                        float(np.sum(circuit_power[destinations])),
                        float(bus_power[bus_index]),
                        places=12,
                    )

    def test_probe_is_safe_six_channel_and_sequential(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "probe.wav"
            manifest = cabin.make_probe(
                output,
                None,
                level_db=-30.0,
                sweep_seconds=0.25,
                tail_seconds=0.1,
                guard_seconds=0.1,
            )
            with cabin.Pcm16WavReader(output) as reader:
                self.assertEqual(reader.info.channels, 6)
                self.assertEqual(reader.info.channel_mask, 0x60F)
                samples = np.concatenate(list(reader.blocks()), axis=0)
            for entry in manifest["schedule"]:
                start = entry["sweep_start_frame"]
                stop = start + entry["sweep_frames"]
                active = np.max(np.abs(samples[start:stop]), axis=0) > 1e-4
                self.assertEqual(np.flatnonzero(active).tolist(), [entry["channel_index"]])

    def test_binaural_identity_response_does_not_apply_matrix_twice(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source_path = root / "source.wav"
            output_path = root / "ears.wav"
            samples = np.zeros((9, 6), dtype=np.float64)
            samples[2, 0] = 0.25
            samples[4, 1] = -0.125
            with cabin.Pcm16WavWriter(source_path, 6, samples.shape[0], 0x60F) as writer:
                writer.write(samples)

            responses: dict[str, object] = {}
            for index, name in enumerate(self.config["input_channels"][:6]):
                ir = np.zeros((1, 2), dtype=np.float64)
                if index == 0:
                    ir[0, 0] = 0.5
                if index == 1:
                    ir[0, 1] = 0.5
                ir_path = root / f"{name}.wav"
                with cabin.Pcm16WavWriter(ir_path, 2, 1) as writer:
                    writer.write(ir)
                responses[name] = {"file": ir_path.name}
            manifest_path = root / "brir.json"
            manifest_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "sample_rate": 48000,
                        "domain": "most_inputs",
                        "channels": self.config["input_channels"][:6],
                        "responses": responses,
                    }
                ),
                encoding="utf-8",
            )

            report = cabin.render_binaural(
                source_path,
                output_path,
                cabin.DEFAULT_CONFIG,
                manifest_path,
                None,
                gain_db=0.0,
                block_frames=4,
            )
            _info, rendered = cabin.read_all_wav(output_path)
            self.assertAlmostEqual(rendered[2, 0], 0.125, places=4)
            self.assertAlmostEqual(rendered[4, 1], -0.0625, places=4)
            self.assertEqual(report["brir_domain"], "most_inputs")
            self.assertIs(report['model']['matrix_applied'],False)


if __name__ == "__main__":
    unittest.main()
